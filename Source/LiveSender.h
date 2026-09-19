#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "AudioCore.h"
#include <mutex>
#include <thread>
#include <memory>
namespace lsl {
struct MediaPeer;
class LiveSender {
public:
 enum class State { Offline, Connecting, Ready, Starting, Live, Error };
 struct View {State state=State::Offline;juce::String message="Paste a playlist link and private connection key.",title;int listeners=0;bool relay=false;};
 explicit LiveSender(AudioTap&);
 ~LiveSender();
 void connect(const juce::String& link,const juce::String& key);
 void start();
 void disconnect();
 void stop() noexcept; // Lock-free; safe from the host callback.
 View view()const;
 std::uint32_t capture()const noexcept{return captureGeneration.load(std::memory_order_acquire);}
 std::atomic<std::uint64_t> framesSent{0};
private:
 enum class Action { None, Disconnect, Connect, Start };
 struct Command {Action action=Action::None;std::uint32_t revision=0;juce::String link,key;};
 AudioTap& tap;
 std::atomic<bool> shuttingDown{false};
 std::atomic<std::uint32_t> revision{1},captureGeneration{0};
 std::atomic<std::int64_t> lastAudioTime{0},httpDeadline{0};
 std::atomic<bool> mediaFailed{false};
 mutable std::mutex stateMutex,peerMutex,httpMutex;
 View snapshot;
 Command pending;
 std::vector<std::shared_ptr<MediaPeer>> peers;
 std::shared_ptr<juce::WebInputStream> request;
 std::thread controlThread,mediaThread;
 void controlLoop();
 void mediaLoop();
 void setView(State,const juce::String&);
 void closePeers();
 void cancelRequest();
 struct Response {int status=0;juce::var json;bool ok()const{return status>=200&&status<300&&json.isObject();}};
 Response post(const juce::String& action,const juce::String& key,const juce::StringPairArray& fields);
};
}
