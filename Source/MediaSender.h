#pragma once
#include "AudioCore.h"
#include "TlsRelay.h"
#include <sstream>
#include <rtc/rtc.hpp>
#include <opus.h>
#include <samplerate.h>
#include <memory>
#include <vector>
#include <stdexcept>
#include <chrono>
namespace lsl {
// libdatachannel 0.23.2 lets transport=tcp override the turns scheme.
// RFC 7065 turns over TCP still requires TLS.
inline rtc::IceServer parseIceServer(const std::string& url) {
 rtc::IceServer server(url);
 if(url.rfind("turns:",0)==0||url.rfind("TURNS:",0)==0)
  server.relayType=rtc::IceServer::RelayType::TurnTls;
 return server;
}
// Owned and called by the media worker. It never touches the host's buffers.
class MusicEncoder {
 OpusEncoder* encoder=nullptr;
 SRC_STATE* resampler=nullptr;
 std::array<float,1920> frame{};
 std::array<float,8192> converted{};
 int filled=0;
 double rate=0;
 std::uint32_t epoch=0, generation=0;
 std::uint64_t expectedFrame=0;
public:
 MusicEncoder(){
  int error=0;encoder=opus_encoder_create(48000,2,OPUS_APPLICATION_AUDIO,&error);
  if(error!=OPUS_OK||!encoder)throw std::runtime_error("Cannot create music encoder");
  resampler=src_new(SRC_SINC_FASTEST,2,&error);
  if(!resampler){opus_encoder_destroy(encoder);encoder=nullptr;throw std::runtime_error("Cannot create resampler");}
  opus_encoder_ctl(encoder,OPUS_SET_BITRATE(192000));
  opus_encoder_ctl(encoder,OPUS_SET_VBR(1));
  opus_encoder_ctl(encoder,OPUS_SET_COMPLEXITY(8));
  opus_encoder_ctl(encoder,OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));
  opus_encoder_ctl(encoder,OPUS_SET_FORCE_CHANNELS(2));
  opus_encoder_ctl(encoder,OPUS_SET_DTX(0));
 }
 ~MusicEncoder(){if(encoder)opus_encoder_destroy(encoder);if(resampler)src_delete(resampler);}
 MusicEncoder(const MusicEncoder&)=delete;
 void reset(){src_reset(resampler);opus_encoder_ctl(encoder,OPUS_RESET_STATE);filled=0;rate=0;}
 template<class Callback> void process(const Packet& p,Callback send){
  if(p.sampleRate<8000||p.sampleRate>192000||p.frames>packetFrames)throw std::runtime_error("Unsupported audio sample rate");
  if(p.sampleRate!=rate||p.epoch!=epoch||p.generation!=generation||p.firstFrame!=expectedFrame){reset();rate=p.sampleRate;epoch=p.epoch;generation=p.generation;}
  expectedFrame=p.firstFrame+p.frames;
  std::array<float,packetFrames*2> input=p.interleaved;
  for(std::size_t i=0;i<p.frames*2;++i)input[i]=std::isfinite(input[i])?std::clamp(input[i],-1.f,1.f):0.f;
  const float* samples=input.data();long frames=p.frames;
  if(rate!=48000){
   SRC_DATA data{};data.data_in=input.data();data.input_frames=p.frames;data.data_out=converted.data();data.output_frames=4096;data.src_ratio=48000/rate;
   if(src_process(resampler,&data)!=0||data.input_frames_used!=(long)p.frames)throw std::runtime_error("Resampling failed");
   samples=converted.data();frames=data.output_frames_gen;
  }
  for(long i=0;i<frames;++i){frame[2*filled]=samples[2*i];frame[2*filled+1]=samples[2*i+1];
   if(++filled==960){std::array<unsigned char,1500> bytes{};int n=opus_encode_float(encoder,frame.data(),960,bytes.data(),(opus_int32)bytes.size());filled=0;if(n<0)throw std::runtime_error("Audio encoding failed");send(bytes.data(),(size_t)n);}
  }
 }
};
struct MediaPeer {
#ifdef __APPLE__
 std::vector<std::unique_ptr<TlsRelay>> tlsRelays;
#endif
 std::shared_ptr<rtc::PeerConnection> pc;
 std::shared_ptr<rtc::Track> track;
 std::shared_ptr<rtc::RtpPacketizationConfig> rtp;
 std::chrono::steady_clock::time_point created=std::chrono::steady_clock::now();
 bool answered=false;
 std::string id;
 ~MediaPeer(){close();}
 void close() noexcept {try{if(pc)pc->close();}catch(...){}}
 bool connected()const{return pc->state()==rtc::PeerConnection::State::Connected&&track->isOpen();}
 MediaPeer(const rtc::Configuration& config,const std::string& offer,std::string peerId):id(std::move(peerId)){
  rtc::Description remote(offer,rtc::Description::Type::Offer);
  if(remote.mediaCount()!=1)throw std::runtime_error("Listener must offer one audio track");
  auto entry=remote.media(0);auto media=std::get_if<rtc::Description::Media*>(&entry);
  if(!media||(*media)->type()!="audio"||(*media)->direction()!=rtc::Description::Direction::RecvOnly)throw std::runtime_error("Listener offer must be receive-only audio");
  int payload=-1;for(int pt:(*media)->payloadTypes()){auto map=(*media)->rtpMap(pt);if(map&&map->format=="opus"&&map->clockRate==48000&&map->encParams=="2"){payload=pt;break;}}
  if(payload<0)throw std::runtime_error("Listener does not offer stereo Opus");
  auto conf=config;conf.disableAutoNegotiation=true;
#ifdef __APPLE__
  for(auto& server:conf.iceServers)if(server.type==rtc::IceServer::Type::Turn&&server.relayType==rtc::IceServer::RelayType::TurnTls){
   auto bridge=std::make_unique<TlsRelay>(server.hostname,server.port?server.port:5349);
   server.hostname="127.0.0.1";server.port=bridge->port();server.relayType=rtc::IceServer::RelayType::TurnUdp;
   tlsRelays.push_back(std::move(bridge));
  }
#endif
  pc=std::make_shared<rtc::PeerConnection>(conf);
  const auto ssrc=(uint32_t)(std::chrono::steady_clock::now().time_since_epoch().count()&0xffffffffu)|1u;
  rtc::Description::Audio audio((*media)->mid(),rtc::Description::Direction::SendOnly);
  audio.addOpusCodec(payload,"minptime=10;stereo=1;sprop-stereo=1;maxaveragebitrate=192000;useinbandfec=0");
  audio.addSSRC(ssrc,"lindell","lindell-mix","stereo");
  track=pc->addTrack(audio);rtp=std::make_shared<rtc::RtpPacketizationConfig>(ssrc,"lindell",(uint8_t)payload,48000);
  auto packetizer=std::make_shared<rtc::OpusRtpPacketizer>(rtp);
  packetizer->addToChain(std::make_shared<rtc::RtcpSrReporter>(rtp));
  track->setMediaHandler(packetizer);
  pc->setRemoteDescription(remote);pc->setLocalDescription(rtc::Description::Type::Answer);
 }
 std::string diagnostic()const{
  std::ostringstream out;out<<"ICE "<<pc->iceState()<<"; gathering "<<pc->gatheringState();
  auto description=pc->localDescription();int relay=0,other=0;
  if(description)for(const auto& candidate:description->candidates()){if(candidate.type()==rtc::Candidate::Type::Relayed)++relay;else ++other;}
  out<<"; candidates "<<other<<" direct / "<<relay<<" relay";
#ifdef __APPLE__
  for(const auto& tunnel:tlsRelays)out<<"; TLS "<<(tunnel->state()==TlsRelay::State::Ready?"verified":tunnel->state()==TlsRelay::State::Failed?"failed":"connecting");
#endif
  if(answered)out<<"; answer sent";
  rtc::Candidate local,remote;if(pc->getSelectedCandidatePair(&local,&remote))out<<"; route "<<((local.type()==rtc::Candidate::Type::Relayed||remote.type()==rtc::Candidate::Type::Relayed)?"relay":"direct");
  return out.str();
 }
 std::string answer()const{if(pc->gatheringState()!=rtc::PeerConnection::GatheringState::Complete)return {};auto d=pc->localDescription();return d?std::string(*d):std::string{};}
 void send(const unsigned char* bytes,size_t n){
  rtp->timestamp+=960;
  if(connected()&&track->bufferedAmount()<16000){track->send(reinterpret_cast<const rtc::byte*>(bytes),n);}
 }
};
}
