#pragma once
// TURN over TLS stream framing (RFC 5766 section 11). Each MediaPeer owns
// one bridge and one TURN allocation; no credentials are stored or logged here.
#ifdef __APPLE__
#include <Security/SecureTransport.h>
#include <Security/Security.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <atomic>
#include <thread>
#include <vector>
#include <array>
#include <algorithm>
#include <string>
#include <chrono>
#include <stdexcept>
#include <cstring>
#include <cerrno>
namespace lsl {
class TlsRelay {
public:
 enum class State { Connecting, Ready, Failed, Closed };
 TlsRelay(std::string host,unsigned short port):hostname(std::move(host)),remotePort(port){
  udp=::socket(AF_INET,SOCK_DGRAM,0);if(udp<0)throw std::runtime_error("Relay socket failed");
  sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
  if(::bind(udp,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))<0){::close(udp);throw std::runtime_error("Relay bind failed");}
  socklen_t len=sizeof(addr);getsockname(udp,reinterpret_cast<sockaddr*>(&addr),&len);localPort=ntohs(addr.sin_port);
  fcntl(udp,F_SETFL,O_NONBLOCK);worker=std::thread([this]{run();});
 }
 ~TlsRelay(){stopping=true;if(worker.joinable())worker.join();::close(udp);}
 TlsRelay(const TlsRelay&)=delete;
 unsigned short port()const{return localPort;}
 State state()const{return status.load();}
private:
 std::string hostname;unsigned short remotePort=0,localPort=0;int udp=-1,tcp=-1;
 std::atomic<bool> stopping{false};std::atomic<State> status{State::Connecting};std::thread worker;
 using Clock=std::chrono::steady_clock;
 static OSStatus readSocket(SSLConnectionRef ref,void* data,size_t* count){
  auto self=static_cast<const TlsRelay*>(ref);auto n=::recv(self->tcp,data,*count,0);
  if(n>0){auto wanted=*count;*count=static_cast<size_t>(n);return *count==wanted?noErr:errSSLWouldBlock;}
  *count=0;if(n==0)return errSSLClosedGraceful;
  return errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR?errSSLWouldBlock:errSSLClosedAbort;
 }
 static OSStatus writeSocket(SSLConnectionRef ref,const void* data,size_t* count){
  auto self=static_cast<const TlsRelay*>(ref);auto n=::send(self->tcp,data,*count,0);
  if(n>=0){auto wanted=*count;*count=static_cast<size_t>(n);return *count==wanted?noErr:errSSLWouldBlock;}
  *count=0;return errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR?errSSLWouldBlock:errSSLClosedAbort;
 }
 bool connectSocket(){
  addrinfo hints{};hints.ai_socktype=SOCK_STREAM;hints.ai_family=AF_UNSPEC;addrinfo* list=nullptr;
  if(getaddrinfo(hostname.c_str(),std::to_string(remotePort).c_str(),&hints,&list)!=0)return false;
  bool connected=false;const auto deadline=Clock::now()+std::chrono::seconds(5);
  for(auto a=list;a&&!stopping&&Clock::now()<deadline;a=a->ai_next){
   tcp=::socket(a->ai_family,a->ai_socktype,a->ai_protocol);if(tcp<0)continue;
   int yes=1;setsockopt(tcp,SOL_SOCKET,SO_NOSIGPIPE,&yes,sizeof(yes));fcntl(tcp,F_SETFL,O_NONBLOCK);
   int rc=::connect(tcp,a->ai_addr,a->ai_addrlen);
   if(rc==0)connected=true;
   else if(errno==EINPROGRESS){while(!stopping&&Clock::now()<deadline){pollfd fd{tcp,POLLOUT,0};if(poll(&fd,1,50)>0){int error=0;socklen_t size=sizeof(error);connected=getsockopt(tcp,SOL_SOCKET,SO_ERROR,&error,&size)==0&&error==0;break;}}}
   if(connected)break;::close(tcp);tcp=-1;
  }
  freeaddrinfo(list);return connected;
 }
 bool writeFrame(SSLContextRef ssl,const unsigned char* bytes,size_t size){
  const auto deadline=Clock::now()+std::chrono::seconds(2);size_t offset=0;
  while(!stopping&&Clock::now()<deadline){size_t written=0;auto result=SSLWrite(ssl,bytes+offset,size-offset,&written);offset+=written;
   if(result!=noErr&&result!=errSSLWouldBlock)return false;
   if(offset==size&&result==noErr)return true;
   std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }return false;
 }
 void run()noexcept{
  SSLContextRef ssl=nullptr;
  try{
   if(!connectSocket())throw std::runtime_error("connect");
   ssl=SSLCreateContext(kCFAllocatorDefault,kSSLClientSide,kSSLStreamType);if(!ssl)throw std::runtime_error("TLS init");
   // Use macOS system trust, hostname validation and TLS 1.2 minimum.
   if(SSLSetIOFuncs(ssl,readSocket,writeSocket)!=noErr||SSLSetConnection(ssl,this)!=noErr||SSLSetPeerDomainName(ssl,hostname.data(),hostname.size())!=noErr||SSLSetProtocolVersionMin(ssl,kTLSProtocol12)!=noErr)throw std::runtime_error("TLS config");
   auto deadline=Clock::now()+std::chrono::seconds(6);OSStatus result=errSSLWouldBlock;
   while(!stopping&&Clock::now()<deadline&&(result=SSLHandshake(ssl))==errSSLWouldBlock)std::this_thread::sleep_for(std::chrono::milliseconds(5));
   if(result!=noErr)throw std::runtime_error("TLS handshake or trust");
   status=State::Ready;sockaddr_in source{};bool haveSource=false;std::vector<unsigned char> incoming;incoming.reserve(65536);
   while(!stopping){
    std::array<unsigned char,65536> bytes{};sockaddr_in from{};socklen_t fromLen=sizeof(from);
    auto n=recvfrom(udp,bytes.data(),bytes.size(),0,reinterpret_cast<sockaddr*>(&from),&fromLen);
    if(n>0){
     if(!haveSource){source=from;haveSource=true;}
     if(from.sin_port==source.sin_port&&from.sin_addr.s_addr==source.sin_addr.s_addr){
      size_t size=static_cast<size_t>(n);
      if(size<4)continue;
      // ChannelData on a stream requires 32-bit padding; STUN is already aligned.
      if((bytes[0]&0xc0)==0x40){size_t padded=(size+3)&~size_t(3);if(padded>bytes.size())continue;std::fill(bytes.begin()+size,bytes.begin()+padded,0);size=padded;}
      if(!writeFrame(ssl,bytes.data(),size))throw std::runtime_error("TLS write");
     }
    }
    size_t read=0;result=SSLRead(ssl,bytes.data(),16384,&read);
    if(result!=noErr&&result!=errSSLWouldBlock)throw std::runtime_error("TLS read");
    if(read){incoming.insert(incoming.end(),bytes.begin(),bytes.begin()+read);if(incoming.size()>131072)throw std::runtime_error("frame size");}
    while(incoming.size()>=4){
     const bool channel=(incoming[0]&0xc0)==0x40;
     if(!channel&&(incoming[0]&0xc0)!=0)throw std::runtime_error("frame type");
     size_t payload=(static_cast<size_t>(incoming[2])<<8)|incoming[3];
     size_t length=payload+(channel?4:20),padded=channel?(length+3)&~size_t(3):length;
     if(padded>65536)throw std::runtime_error("frame limit");
     if(incoming.size()<padded)break;
     if(haveSource)sendto(udp,incoming.data(),length,0,reinterpret_cast<sockaddr*>(&source),sizeof(source));
     incoming.erase(incoming.begin(),incoming.begin()+padded);
    }
    pollfd fds[2]={{udp,POLLIN,0},{tcp,POLLIN,0}};poll(fds,2,3);
   }
  }catch(...){status=State::Failed;}
  if(ssl)CFRelease(ssl);if(tcp>=0)::close(tcp);tcp=-1;
  if(stopping)status=State::Closed;
 }
};
}
#endif
