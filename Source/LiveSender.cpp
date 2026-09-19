#include "LiveSender.h"
#include "MediaSender.h"
#include "PlaylistLink.h"
#include <map>
#include <set>
namespace lsl {
namespace {
using Clock=std::chrono::steady_clock;
std::int64_t nowMs(){return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();}
bool isKey(const juce::String& key){return key.length()==64&&key.containsOnly("0123456789abcdef");}
rtc::Configuration iceConfig(const juce::var& data,bool& relay){
 rtc::Configuration c;c.disableAutoNegotiation=true;relay=false;
 if(auto entries=data.getArray())for(const auto& entry:*entries){
  juce::Array<juce::var> urls;auto u=entry["urls"];if(u.isArray())urls=*u.getArray();else urls.add(u);
  for(const auto& v:urls){auto s=v.toString();if(!(s.startsWith("stun:")||s.startsWith("turn:")))continue;
   // libjuice supports UDP STUN/TURN, not TURN TCP/TLS.
   if(s.contains("transport=tcp"))continue;
   rtc::IceServer server(s.toStdString());server.username=entry["username"].toString().toStdString();server.password=entry["credential"].toString().toStdString();
   if(server.type==rtc::IceServer::Type::Turn)relay=true;c.iceServers.push_back(std::move(server));
  }
 }
 return c;
}
}
LiveSender::LiveSender(AudioTap& t):tap(t){
 // Do not log SDP, ICE credentials, private links or connection keys.
 controlThread=std::thread([this]{controlLoop();});mediaThread=std::thread([this]{mediaLoop();});
}
LiveSender::~LiveSender(){stop();shuttingDown=true;cancelRequest();if(controlThread.joinable())controlThread.join();if(mediaThread.joinable())mediaThread.join();closePeers();}
void LiveSender::stop()noexcept{captureGeneration.store(0,std::memory_order_release);revision.fetch_add(1,std::memory_order_acq_rel);}
LiveSender::View LiveSender::view()const{std::lock_guard<std::mutex> lock(stateMutex);return snapshot;}
void LiveSender::setView(State s,const juce::String& message){std::lock_guard<std::mutex> lock(stateMutex);snapshot.state=s;snapshot.message=message;if(s!=State::Live)snapshot.listeners=0;}
void LiveSender::connect(const juce::String& link,const juce::String& key){
 stop();auto parsed=parsePlaylistLink(link.toStdString());
 if(!parsed||!isKey(key.trim())){setView(State::Error,!parsed?juce::String(parsed.error):"Enter the 64-character private connection key from the playlist Live panel.");return;}
 std::lock_guard<std::mutex> lock(stateMutex);pending={Action::Connect,revision.load(),juce::String(parsed.canonical),key.trim()};snapshot.state=State::Connecting;snapshot.message="Verifying playlist with Lindell Streams...";
}
void LiveSender::disconnect(){stop();std::lock_guard<std::mutex> lock(stateMutex);pending={Action::Disconnect,revision.load(),{}, {}};snapshot.state=State::Offline;snapshot.title.clear();snapshot.message="Destination changed. Connect to verify it.";}
void LiveSender::start(){
 std::lock_guard<std::mutex> lock(stateMutex);if(snapshot.state!=State::Ready)return;
 captureGeneration=0;pending.action=Action::Start;pending.revision=revision.fetch_add(1)+1;snapshot.state=State::Starting;snapshot.message="Opening live session...";
}
void LiveSender::closePeers(){std::vector<std::shared_ptr<MediaPeer>> old;{std::lock_guard<std::mutex> lock(peerMutex);old.swap(peers);}for(auto& p:old)p->close();}
void LiveSender::cancelRequest(){std::shared_ptr<juce::WebInputStream> stream;{std::lock_guard<std::mutex> lock(httpMutex);stream=request;}if(stream)stream->cancel();}
LiveSender::Response LiveSender::post(const juce::String& action,const juce::String& key,const juce::StringPairArray& fields){
 juce::String body;for(int i=0;i<fields.size();++i){if(i)body+="&";body+=juce::URL::addEscapeChars(fields.getAllKeys()[i],true)+"="+juce::URL::addEscapeChars(fields.getAllValues()[i],true);}
 auto url=juce::URL("https://lindell-streams.com/?action=livePlugin"+action).withPOSTData(body);
 auto stream=std::make_shared<juce::WebInputStream>(url,false);
 stream->withCustomRequestCommand("POST").withNumRedirectsToFollow(0).withConnectionTimeout(3000).withExtraHeaders("Authorization: Bearer "+key+"\r\nContent-Type: application/x-www-form-urlencoded\r\n");
 {std::lock_guard<std::mutex> lock(httpMutex);request=stream;httpDeadline=nowMs()+3500;}
 Response result;
 if(!shuttingDown&&stream->connect(nullptr)){
  result.status=stream->getStatusCode();juce::MemoryOutputStream bytes;char chunk[4096];
  while(!shuttingDown&&!stream->isExhausted()&&bytes.getDataSize()<1048576){int n=stream->read(chunk,sizeof(chunk));if(n<=0)break;bytes.write(chunk,(size_t)n);}
  if(!stream->isError()&&bytes.getDataSize()<1048576)result.json=juce::JSON::parse(bytes.toString());
 }
 {std::lock_guard<std::mutex> lock(httpMutex);request.reset();httpDeadline=0;}
 return result;
}
void LiveSender::controlLoop(){
 std::uint32_t active=0;juce::String session,key,link;bool verified=false;rtc::Configuration config;
 std::map<std::string,std::shared_ptr<MediaPeer>> members;std::int64_t nextPoll=0,nextIceRefresh=0;
 auto finish=[&]{captureGeneration=0;closePeers();members.clear();if(session.isNotEmpty()&&!shuttingDown){juce::StringPairArray f;f.set("session",session);post("Stop",key,f);}session.clear();};
 try {
 while(!shuttingDown){
  const auto wanted=revision.load();
  if(wanted!=active){
   finish();active=wanted;Command c;{std::lock_guard<std::mutex> lock(stateMutex);c=pending;}
   if(c.revision!=active||c.action==Action::None){setView(verified?State::Ready:State::Offline,verified?"Stopped. Press Go Live to start a new session.":"Offline. Connect a playlist to continue.");}
   else {
    if(c.action==Action::Disconnect){verified=false;key.clear();link.clear();setView(State::Offline,"Connect the playlist before broadcasting.");continue;}
    key=c.key;link=c.link;juce::StringPairArray f;f.set("link",link);
    if(c.action==Action::Connect){verified=false;auto response=post("Resolve",key,f);
     if(revision!=active)continue;
     if(!response.ok()||response.json["protocol"].toString()!="lindell-live-webrtc-v1"){
      setView(State::Error,response.status==401?"Connection key expired or invalid. Create a new key on the website.":response.status==403?"This key does not belong to the pasted playlist.":"Cannot connect to the Live API (HTTP "+juce::String(response.status)+"). Check the website Live update and your connection.");
     }else{bool relay=false;config=iceConfig(response.json["iceServers"],relay);verified=true;{std::lock_guard<std::mutex> lock(stateMutex);snapshot.title=response.json["title"].toString().substring(0,120);snapshot.relay=relay;}setView(State::Ready,relay?"Connected. Ready to broadcast.":"Connected. No UDP relay configured; off-site listening may fail.");}
    }else if(c.action==Action::Start&&verified){
     // Encoder must be usable before advertising a publisher to listeners.
     MusicEncoder checkEncoder;
     auto response=post("Start",key,f);session=response.json["session"].toString();
     if(revision!=active){finish();continue;}
     if(!response.ok()||session.length()!=32){session.clear();setView(State::Error,response.status==409?"This playlist already has a broadcaster. End it on the website first.":"Could not start. If the request timed out, wait 25 seconds before retrying.");}
     else{bool relay=false;config=iceConfig(response.json["iceServers"],relay);{std::lock_guard<std::mutex> lock(stateMutex);snapshot.relay=relay;}lastAudioTime=nowMs();mediaFailed=false;captureGeneration=active;nextPoll=0;nextIceRefresh=nowMs()+300000;setView(State::Live,"On air - waiting for a listener.");}
    }
   }
  }
  if(session.isNotEmpty()&&revision==active){
   if(mediaFailed||nowMs()-lastAudioTime.load()>3000){stop();finish();active=revision.load();setView(State::Error,mediaFailed?"Audio sender stopped after a transport error. Reconnect to retry.":"Broadcast stopped: the host stopped delivering audio.");continue;}
   if(nowMs()>=nextIceRefresh){juce::StringPairArray f;f.set("link",link);auto response=post("Resolve",key,f);if(revision!=active)continue;
    if(!response.ok()){stop();finish();active=revision.load();verified=false;setView(State::Error,"Connection authorization could not be refreshed. Reconnect to continue.");continue;}bool relay=false;config=iceConfig(response.json["iceServers"],relay);nextIceRefresh=nowMs()+300000;
   }
   if(nowMs()>=nextPoll){juce::StringPairArray f;f.set("session",session);auto response=post("Poll",key,f);if(revision!=active)continue;
    if(!response.ok()||!response.json["peers"].isArray()){stop();finish();active=revision.load();verified=false;setView(State::Error,"Live connection ended or network lost. Reconnect to continue.");continue;}
    nextPoll=nowMs()+1000;std::set<std::string> present;
    auto& entries=*response.json["peers"].getArray();if(entries.size()>8)throw std::runtime_error("Listener capacity exceeded");
    for(const auto& entry:entries){auto id=entry["id"].toString().toStdString();if(id.size()!=32)continue;present.insert(id);
     if(!members.count(id)){try{auto offer=entry["offer"].toString();if(offer.length()>60000)continue;members[id]=std::make_shared<MediaPeer>(config,offer.toStdString(),id);}catch(...){/* Bad peer must not interrupt other listeners. */}}
    }
    for(auto i=members.begin();i!=members.end();){if(!present.count(i->first)){i->second->close();i=members.erase(i);}else ++i;}
    {std::lock_guard<std::mutex> lock(peerMutex);peers.clear();for(auto& pair:members)peers.push_back(pair.second);}
   }
   for(auto& item:members){auto& peer=item.second;if(!peer->answered){auto answer=peer->answer();if(!answer.empty()){juce::StringPairArray f;f.set("session",session);f.set("peer",item.first);f.set("answer",juce::String(answer));auto response=post("Answer",key,f);if(revision!=active)break;if(response.ok())peer->answered=true;else if(response.status==404){peer->answered=true;peer->close();}else{stop();}break;}}}
   int listeners=0;for(auto& item:members)if(item.second->connected())++listeners;
   {std::lock_guard<std::mutex> lock(stateMutex);if(revision==active){snapshot.listeners=listeners;snapshot.message=listeners>0?"On air - "+juce::String(listeners)+" connected listener(s).":"On air - waiting for a listener.";}}
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
 }
 }catch(...){captureGeneration=0;setView(State::Error,"Sender stopped after an internal error. Remove and reinsert the plugin.");}
 finish();
}
void LiveSender::mediaLoop(){
 try{MusicEncoder encoder;std::uint32_t lastGeneration=0;Packet packet;
 while(!shuttingDown){
  if(httpDeadline.load()>0&&nowMs()>httpDeadline.load())cancelRequest();
  const auto g=captureGeneration.load();if(g!=lastGeneration){encoder.reset();lastGeneration=g;}
  int drained=0;std::vector<Packet> recent;
  while(drained++<32&&tap.queue.pop(packet)){if(g&&packet.generation==g)recent.push_back(packet);}
  // After any worker stall, discard all but the newest ~100 ms. Never burst stale music.
  size_t first=0;double ms=0;for(size_t i=recent.size();i>0;--i){ms+=1000.0*recent[i-1].frames/recent[i-1].sampleRate;if(ms>100){first=i;break;}}
  for(size_t i=first;i<recent.size();++i){if(captureGeneration!=g||revision!=g)break;lastAudioTime=nowMs();
   encoder.process(recent[i],[&](const unsigned char* bytes,size_t n){if(captureGeneration!=g||revision!=g)return;std::vector<std::shared_ptr<MediaPeer>> destinations;{std::lock_guard<std::mutex> lock(peerMutex);destinations=peers;}
    for(auto& peer:destinations){if(captureGeneration!=g||revision!=g)break;try{peer->send(bytes,n);}catch(...){peer->close();}}framesSent.fetch_add(960);
   });
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
 }
 }catch(...){mediaFailed=true;captureGeneration=0;}
}
}
