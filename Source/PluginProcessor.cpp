#include "PluginProcessor.h"
#include "PluginEditor.h"
LindellLiveProcessor::LindellLiveProcessor():AudioProcessor(BusesProperties().withInput("Mix",juce::AudioChannelSet::stereo(),true).withOutput("Mix",juce::AudioChannelSet::stereo(),true)),sender(tap){setLatencySamples(0);}
LindellLiveProcessor::~LindellLiveProcessor(){sender.stop();}
void LindellLiveProcessor::prepareToPlay(double rate,int){sender.stop();tap.prepare(rate);}
void LindellLiveProcessor::releaseResources(){sender.stop();}
bool LindellLiveProcessor::isBusesLayoutSupported(const BusesLayout& b)const{return b.getMainInputChannelSet()==juce::AudioChannelSet::stereo()&&b.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();}
void LindellLiveProcessor::processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer&){
 if(audio.getNumChannels()<2)return;
 auto generation=sender.capture();
 if(isNonRealtime()){sender.stop();generation=0;}
 tap.process(audio.getReadPointer(0),audio.getReadPointer(1),audio.getNumSamples(),generation!=0,generation);
}
void LindellLiveProcessor::processBlockBypassed(juce::AudioBuffer<float>& audio,juce::MidiBuffer&){sender.stop();if(audio.getNumChannels()>=2)tap.process(audio.getReadPointer(0),audio.getReadPointer(1),audio.getNumSamples(),false);}
juce::AudioProcessorEditor* LindellLiveProcessor::createEditor(){return new LindellLiveEditor(*this);}
juce::String LindellLiveProcessor::getPlaylistLink()const{std::lock_guard<std::mutex> lock(destinationMutex);return playlistLink;}
juce::String LindellLiveProcessor::setPlaylistLink(const juce::String& input){auto parsed=lsl::parsePlaylistLink(input.toStdString());sender.disconnect();{std::lock_guard<std::mutex> lock(destinationMutex);playlistLink=juce::String(parsed.canonical);destinationRevision++;}return juce::String(parsed.error);}
void LindellLiveProcessor::getStateInformation(juce::MemoryBlock& dest){juce::XmlElement state("LindellStreamsLive");state.setAttribute("schema",2);state.setAttribute("playlistLink",getPlaylistLink());copyXmlToBinary(state,dest);}
void LindellLiveProcessor::setStateInformation(const void* data,int size){sender.disconnect();auto state=getXmlFromBinary(data,size);juce::String link;if(state&&state->hasTagName("LindellStreamsLive")&&state->getIntAttribute("schema")==2)link=state->getStringAttribute("playlistLink");setPlaylistLink(link);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new LindellLiveProcessor();}
