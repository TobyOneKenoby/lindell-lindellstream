#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "AudioCore.h"
#include "LiveSender.h"
#include "PlaylistLink.h"
#include <mutex>
class LindellLiveProcessor final : public juce::AudioProcessor {
public:
 LindellLiveProcessor();
 ~LindellLiveProcessor() override;
 const juce::String getName()const override{return "Lindell Streams Live";}
 void prepareToPlay(double,int)override;
 void releaseResources()override;
 bool isBusesLayoutSupported(const BusesLayout&)const override;
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&)override;
 void processBlockBypassed(juce::AudioBuffer<float>&,juce::MidiBuffer&)override;
 bool hasEditor()const override{return true;}
 juce::AudioProcessorEditor* createEditor()override;
 bool acceptsMidi()const override{return false;}
 bool producesMidi()const override{return false;}
 double getTailLengthSeconds()const override{return 0;}
 int getNumPrograms()override{return 1;}
 int getCurrentProgram()override{return 0;}
 void setCurrentProgram(int)override{}
 const juce::String getProgramName(int)override{return {};}
 void changeProgramName(int,const juce::String&)override{}
 void getStateInformation(juce::MemoryBlock&)override;
 void setStateInformation(const void*,int)override;
 juce::String getPlaylistLink()const;
 juce::String setPlaylistLink(const juce::String&);
 lsl::AudioTap tap;
 lsl::LiveSender sender;
 std::atomic<std::uint32_t> destinationRevision{0};
private:
 mutable std::mutex destinationMutex;
 juce::String playlistLink;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LindellLiveProcessor)
};
