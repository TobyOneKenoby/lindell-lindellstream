#pragma once
#include "PluginProcessor.h"
class LindellLiveEditor final:public juce::AudioProcessorEditor,private juce::Timer {
public:
 explicit LindellLiveEditor(LindellLiveProcessor&);
 ~LindellLiveEditor()override;
 void paint(juce::Graphics&)override;
 void resized()override;
private:
 void timerCallback()override;
 void meter(juce::Graphics&,int,int);
 void pasteInto(juce::TextEditor&);
 LindellLiveProcessor& processor;
 juce::LookAndFeel_V4 look;
 juce::TextEditor link,key;
 juce::TextButton connect{"CONNECT"},live{"GO LIVE"},reset{"RESET CLIPS"},pasteLink{"PASTE"},pasteKey{"PASTE"};
 juce::TextButton copyDiagnostics{"COPY DIAGNOSTICS"};
 juce::ToggleButton relayTest{"Test TLS relay (TCP 443)"};
 juce::Label status,title;
 juce::TooltipWindow tips{this,600};
 std::array<float,2> displayPeak{},displayRms{};
 std::uint32_t lastHeartbeat=0,lastRevision=0;
 int idleTicks=0;
 lsl::LiveSender::View current;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LindellLiveEditor)
};
