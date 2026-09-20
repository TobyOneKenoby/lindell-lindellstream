#include "../Source/MediaSender.h"
#include <fstream>
#include <iostream>
#include <thread>
#include <cstdlib>
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static lsl::Packet tone(std::uint64_t first,double rate,int frames){lsl::Packet p;p.frames=(uint32_t)frames;p.sampleRate=rate;p.firstFrame=first;p.generation=7;p.epoch=1;for(int i=0;i<frames;++i){p.interleaved[(size_t)i*2]=.25f*(float)std::sin(6.283185307179586*440*(double)(first+i)/rate);p.interleaved[(size_t)i*2+1]=.25f*(float)std::sin(6.283185307179586*880*(double)(first+i)/rate);}return p;}
int main(int argc,char** argv){try{
 if(argc==4&&std::string(argv[1])=="--browser"){
  std::ifstream input(argv[2]);std::string offer((std::istreambuf_iterator<char>(input)),{});rtc::Configuration config;config.disableAutoNegotiation=true;
  if(std::getenv("LSL_TEST_TLS_RELAY")){config.iceServers.push_back(lsl::parseIceServer("turns:localhost:18444?transport=tcp"));config.iceServers.back().username="fixture";config.iceServers.back().password="fixture-password";config.iceTransportPolicy=rtc::TransportPolicy::Relay;}
  auto peer=std::make_shared<lsl::MediaPeer>(config,offer,"test");
  auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);std::string answer;
  while((answer=peer->answer()).empty()&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(10));check(!answer.empty(),"ICE gathering timed out");std::cerr<<peer->diagnostic()<<std::endl;std::ofstream(argv[3])<<answer;
  deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);while(!peer->connected()&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(10));std::cerr<<peer->diagnostic()<<std::endl;check(peer->connected(),"Browser did not connect");
  if(std::getenv("LSL_TEST_TLS_RELAY")){rtc::Candidate local,remote;check(peer->pc->getSelectedCandidatePair(&local,&remote)&&local.type()==rtc::Candidate::Type::Relayed,"TLS test must use a TURN relay candidate");std::cout<<"PASS verified TLS TURN allocation and selected relay route\n";}
  lsl::MusicEncoder encoder;auto next=std::chrono::steady_clock::now();for(std::uint64_t n=0;n<48000*10;n+=480){encoder.process(tone(n,48000,480),[&](const unsigned char* data,size_t size){peer->send(data,size);});next+=std::chrono::milliseconds(10);std::this_thread::sleep_until(next);}peer->close();std::cout<<"PASS browser sender\n";return 0;
 }
 for(double rate:{44100.,48000.,88200.,96000.,192000.}){
  lsl::MusicEncoder encoder;int error=0;auto* decoder=opus_decoder_create(48000,2,&error);check(error==OPUS_OK,"Opus decoder");double energy[2]{},difference=0;int received=0;
  for(std::uint64_t n=0;n<(uint64_t)rate;n+=480){auto p=tone(n,rate,480);encoder.process(p,[&](const unsigned char* bytes,size_t size){float decoded[1920];int frames=opus_decode_float(decoder,bytes,(opus_int32)size,decoded,960,0);check(frames==960,"Decode 20ms");received+=frames;for(int i=0;i<frames;++i){for(int c=0;c<2;++c){check(std::isfinite(decoded[2*i+c]),"Finite output");energy[c]+=decoded[2*i+c]*decoded[2*i+c];}double d=decoded[2*i]-decoded[2*i+1];difference+=d*d;}});}
  opus_decoder_destroy(decoder);check(received>46000&&received<50000,"Resampled duration");check(energy[0]/received>.02&&energy[1]/received>.02,"Both channels audible");check(difference/received>.03,"Stereo not downmixed");
 }
 std::cout<<"PASS: Opus encoding, decoding, stereo separation and sinc resampling at five rates\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
