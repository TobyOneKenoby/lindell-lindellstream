#include "PluginEditor.h"
namespace {const juce::Colour ink(0xff163447),cyan(0xff85d8ff),muted(0xff587083);float db(float v){return juce::Decibels::gainToDecibels(v,-60.f);}}
LindellLiveEditor::LindellLiveEditor(LindellLiveProcessor& p):AudioProcessorEditor(p),processor(p){
 look.setColour(juce::TextButton::buttonColourId,ink);look.setColour(juce::TextButton::textColourOffId,juce::Colours::white);setLookAndFeel(&look);
 for(auto* b:{&connect,&live,&reset})addAndMakeVisible(*b);
 for(auto* t:{&link,&key}){addAndMakeVisible(*t);t->setColour(juce::TextEditor::backgroundColourId,juce::Colour(0xfff6f9fb));t->setColour(juce::TextEditor::textColourId,ink);t->setColour(juce::TextEditor::outlineColourId,juce::Colour(0xffa2b9c7));t->setFont(juce::Font(juce::FontOptions(13.f)));t->setSelectAllWhenFocused(true);}
 link.setText(processor.getPlaylistLink(),false);link.setTextToShowWhenEmpty("https://lindell-streams.com/?share=...",muted);link.setInputRestrictions(2048);link.setTitle("Playlist share link");
 key.setPasswordCharacter('*');key.setInputRestrictions(64,"0123456789abcdefABCDEF");key.setTextToShowWhenEmpty("Private key from the playlist Live panel",muted);key.setTitle("Private connection key");
 key.setTooltip("Kept only in memory; never saved in your DAW project. Create this key in your playlist's Live panel.");
 for(auto* label:{&status,&title}){addAndMakeVisible(*label);label->setColour(juce::Label::textColourId,ink);}
 status.setFont(juce::Font(juce::FontOptions(12.f)));title.setFont(juce::Font(juce::FontOptions(15.f,juce::Font::bold)));
 link.onTextChange=[this]{processor.sender.disconnect();};key.onTextChange=[this]{processor.sender.disconnect();};
 connect.onClick=[this]{auto error=processor.setPlaylistLink(link.getText());if(error.isEmpty())processor.sender.connect(processor.getPlaylistLink(),key.getText().toLowerCase());else status.setText(error,juce::dontSendNotification);};
 live.onClick=[this]{auto s=processor.sender.view().state;if(s==lsl::LiveSender::State::Live||s==lsl::LiveSender::State::Starting)processor.sender.stop();else processor.sender.start();};
 reset.onClick=[this]{for(auto& c:processor.tap.clipped)c=false;};
 setSize(740,510);lastRevision=processor.destinationRevision;startTimerHz(30);timerCallback();
}
LindellLiveEditor::~LindellLiveEditor(){stopTimer();setLookAndFeel(nullptr);}
void LindellLiveEditor::resized(){link.setBounds(43,153,431,34);key.setBounds(43,218,316,34);connect.setBounds(370,218,104,34);title.setBounds(39,268,439,30);live.setBounds(43,312,431,49);status.setBounds(39,372,439,58);reset.setBounds(544,405,153,27);}
void LindellLiveEditor::timerCallback(){
 auto h=processor.tap.heartbeat.load();idleTicks=h==lastHeartbeat?std::min(idleTicks+1,1000):0;lastHeartbeat=h;
 for(int c=0;c<2;++c){displayPeak[c]=idleTicks>5?displayPeak[c]*.85f:processor.tap.peak[c].load();displayRms[c]=idleTicks>5?displayRms[c]*.85f:processor.tap.rms[c].load();}
 auto r=processor.destinationRevision.load();if(r!=lastRevision){lastRevision=r;link.setText(processor.getPlaylistLink(),false);}
 current=processor.sender.view();using S=lsl::LiveSender::State;bool broadcasting=current.state==S::Live||current.state==S::Starting;
 live.setButtonText(broadcasting?"STOP BROADCAST":"GO LIVE");live.setEnabled(broadcasting||current.state==S::Ready);
 live.setColour(juce::TextButton::buttonColourId,broadcasting?juce::Colour(0xffab3844):ink);
 connect.setEnabled(!broadcasting&&current.state!=S::Connecting);link.setEnabled(!broadcasting);key.setEnabled(!broadcasting);
 status.setText(current.message,juce::dontSendNotification);title.setText(current.title.isEmpty()?"No playlist connected":current.title,juce::dontSendNotification);repaint();
}
void LindellLiveEditor::meter(juce::Graphics& g,int c,int x){
 g.setColour(ink);g.setFont(14.f);g.drawText(c==0?"L":"R",x,120,34,22,juce::Justification::centred);
 g.setColour(juce::Colour(0xff102635));g.fillRoundedRectangle((float)x,159,34,209,5);
 auto fraction=juce::jlimit(0.f,1.f,(db(displayPeak[c])+60)/60);
 for(int i=0;i<32;++i){auto col=i>29?juce::Colour(0xffff7272):i>26?juce::Colour(0xffffd180):cyan;g.setColour(col.withAlpha((float)i/32<fraction?1.f:.09f));g.fillRect(x+5,359-i*6,24,4);}
 float rms=juce::jlimit(0.f,1.f,(db(displayRms[c])+60)/60);g.setColour(juce::Colours::white);g.fillRect(x+3,359-(int)(rms*190),28,2);
 g.setColour(processor.tap.clipped[c]?juce::Colour(0xffed5555):muted);g.fillRoundedRectangle((float)x,147,34,5,2);
 g.setColour(ink);g.setFont(12.f);g.drawText(juce::String(db(displayPeak[c]),1),x-10,375,54,22,juce::Justification::centred);
}
void LindellLiveEditor::paint(juce::Graphics& g){
 g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff2f5f7),0,0,juce::Colour(0xffaebcc6),0,510,false));g.fillAll();g.setColour(juce::Colours::white.withAlpha(.10f));for(int y=0;y<510;y+=3)g.drawHorizontalLine(y,0,740);
 g.setColour(ink.withAlpha(.3f));g.drawRoundedRectangle(1,1,738,508,12,2);
 g.setColour(juce::Colour(0xffcfe4ff));g.fillRoundedRectangle(30,29,53,53,15);g.setColour(ink);int heights[]={6,20,32,17,27,6};for(int i=0;i<6;++i)g.fillRoundedRectangle(40.f+i*6,55-heights[i]*.4f,3,heights[i]*.8f,1.5f);
 g.setFont(juce::Font(juce::FontOptions(30.f,juce::Font::bold)));g.drawText("lindellstreams",99,26,340,42,juce::Justification::centredLeft);g.setFont(12.f);g.drawText("L I V E  /  M I X  B U S  S E N D",101,70,360,22,juce::Justification::centredLeft);
 g.setColour(juce::Colour(0xffe1e8ed));g.fillRoundedRectangle(28,117,461,323,10);g.setColour(ink);g.setFont(11.f);g.drawText("PLAYLIST SHARE LINK",43,128,431,21,juce::Justification::left);g.drawText("PRIVATE CONNECTION KEY",43,194,431,21,juce::Justification::left);
 meter(g,0,554);meter(g,1,654);g.setFont(10.f);for(int value:{0,-12,-24,-36,-48,-60})g.drawText(juce::String(value),602,152+(int)(-value/60.f*209),33,15,juce::Justification::centred);
 g.setFont(11.f);g.drawText("STEREO / PEAK + RMS",531,92,184,20,juce::Justification::centred);
 bool onAir=current.state==lsl::LiveSender::State::Live;g.setColour(onAir?juce::Colour(0xffe05454):muted);g.fillEllipse(557,52,8,8);g.setColour(ink);g.setFont(12.f);g.drawText(onAir?"ON AIR":"OFFLINE",575,44,117,24,juce::Justification::left);
 g.setFont(11.f);g.drawText("Host audio unchanged  |  Stream: stereo Opus, 48 kHz / 192 kbps target",30,458,680,20,juce::Justification::left);g.setColour(muted);g.setFont(10.f);g.drawText("Close editor: broadcast continues. Use STOP to end.   /   TEST BUILD 0.3",30,480,680,17,juce::Justification::left);
}
