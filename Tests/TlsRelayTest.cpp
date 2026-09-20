#include "../Source/TlsRelay.h"
#include <iostream>
#include <algorithm>
using namespace std::chrono_literals;
static void check(bool ok){if(!ok)throw std::runtime_error("TLS relay transport test failed");}
int main(){try{
 {lsl::TlsRelay relay("localhost",18443);auto end=std::chrono::steady_clock::now()+10s;
 while(relay.state()==lsl::TlsRelay::State::Connecting&&std::chrono::steady_clock::now()<end)std::this_thread::sleep_for(10ms);
 check(relay.state()==lsl::TlsRelay::State::Ready);
 int sock=socket(AF_INET,SOCK_DGRAM,0);check(sock>=0);timeval timeout{3,0};setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);addr.sin_port=htons(relay.port());
 // STUN followed by unaligned ChannelData: TLS fixture fragments replies.
 std::vector<std::vector<unsigned char>> frames={{0,1,0,0,0x21,0x12,0xa4,0x42,1,2,3,4,5,6,7,8,9,10,11,12},{0x40,1,0,5,1,2,3,4,5}};
 for(int repeat=0;repeat<4;++repeat)for(const auto& frame:frames){check(sendto(sock,frame.data(),frame.size(),0,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))==(int)frame.size());unsigned char data[100];auto n=recv(sock,data,sizeof(data),0);check(n==(int)frame.size()&&std::equal(frame.begin(),frame.end(),data));}
 close(sock);}
 {lsl::TlsRelay invalid("127.0.0.1",18443);auto end=std::chrono::steady_clock::now()+10s;while(invalid.state()==lsl::TlsRelay::State::Connecting&&std::chrono::steady_clock::now()<end)std::this_thread::sleep_for(10ms);check(invalid.state()==lsl::TlsRelay::State::Failed);}
 std::cout<<"PASS TLS trust, hostname rejection, STUN/ChannelData framing and fragmented replies\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
