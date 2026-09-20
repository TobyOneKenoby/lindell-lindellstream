#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include <iostream>
#include <stdexcept>
#include <cstring>
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;try{
 check(argc==2,"Expected VST3 path");juce::VST3PluginFormat format;juce::OwnedArray<juce::PluginDescription> types;format.findAllTypesForFile(types,argv[1]);check(types.size()==1,"VST3 discovery");
 juce::AudioPluginFormatManager manager;manager.addFormat(new juce::VST3PluginFormat);juce::String error;auto p=manager.createPluginInstance(*types[0],48000,512,error);check(p!=nullptr,"VST3 instantiate");check(p->getTotalNumInputChannels()==2&&p->getTotalNumOutputChannels()==2,"Stereo buses");
 for(double rate:{44100.,48000.,88200.,96000.}){p->prepareToPlay(rate,2048);for(int size:{32,256,1301,2048}){juce::AudioBuffer<float> b(2,size);juce::MidiBuffer midi;for(int c=0;c<2;++c)for(int i=0;i<size;++i)b.setSample(c,i,(float)std::sin(i*.13+c)*1.1f);juce::AudioBuffer<float> original;original.makeCopyOf(b);p->processBlock(b,midi);for(int c=0;c<2;++c)check(std::memcmp(b.getReadPointer(c),original.getReadPointer(c),sizeof(float)*(size_t)size)==0,"Plugin must preserve audio bytes");p->processBlockBypassed(b,midi);for(int c=0;c<2;++c)check(std::memcmp(b.getReadPointer(c),original.getReadPointer(c),sizeof(float)*(size_t)size)==0,"Bypass unchanged");}p->releaseResources();}
 juce::MemoryBlock state;p->getStateInformation(state);check(state.getSize()>0,"Plugin state");p->setStateInformation(state.getData(),(int)state.getSize());auto editor=std::unique_ptr<juce::AudioProcessorEditor>(p->createEditorIfNeeded());check(editor!=nullptr,"VST3 editor");editor.reset();
 LindellLiveProcessor direct;direct.prepareToPlay(48000,512);direct.sender.start();check(direct.sender.capture()==0,"Never start without authorization");
 direct.setPlaylistLink("https://lindell-streams.com/?share="+juce::String::repeatedString("a",64));direct.getStateInformation(state);direct.setStateInformation(state.getData(),(int)state.getSize());check(direct.sender.capture()==0,"Recall must remain offline");
 direct.setPlaylistLink("");
 auto preview=std::unique_ptr<juce::AudioProcessorEditor>(direct.createEditor());preview->setVisible(true);
 auto* link=dynamic_cast<juce::TextEditor*>(preview->findChildWithID("playlistLink"));
 auto* key=dynamic_cast<juce::TextEditor*>(preview->findChildWithID("connectionKey"));
 auto* pasteLink=dynamic_cast<juce::TextButton*>(preview->findChildWithID("pastePlaylistLink"));
 auto* pasteKey=dynamic_cast<juce::TextButton*>(preview->findChildWithID("pasteConnectionKey"));
 check(link&&key&&pasteLink&&pasteKey,"Clipboard controls exist");
 const auto oldClipboard=juce::SystemClipboard::getTextFromClipboard();
 const auto testLink="https://lindell-streams.com/?share="+juce::String::repeatedString("a",64);
 const auto testKey=juce::String::repeatedString("b",64);
 juce::SystemClipboard::copyTextToClipboard(testLink);pasteLink->onClick();check(link->getText()==testLink,"Playlist Paste button reads system clipboard");
 juce::SystemClipboard::copyTextToClipboard(testKey);pasteKey->onClick();check(key->getText()==testKey,"Private key Paste button reads system clipboard");
 check(key->getPasswordCharacter()!='\0',"Connection key stays masked");
 link->selectAll();juce::SystemClipboard::copyTextToClipboard(testLink);
 check(link->keyPressed(juce::KeyPress('v',juce::ModifierKeys::commandModifier,0)),"Text editor handles Command-V");
 check(link->getText()==testLink,"Command-V pastes text");
 check(direct.sender.capture()==0,"Pasting cannot start broadcast");
 juce::SystemClipboard::copyTextToClipboard(oldClipboard);link->clear();key->clear();
 juce::Image image(juce::Image::ARGB,preview->getWidth(),preview->getHeight(),true,juce::SoftwareImageType());{juce::Graphics g(image);preview->paintEntireComponent(g,true);}check(image.getPixelAt(10,100).getBrightness()>.2f,"Nonblank preview");juce::File file=juce::File::getCurrentWorkingDirectory().getChildFile("Lindell-Streams-Preview.png");auto stream=file.createOutputStream();check(stream!=nullptr,"Preview file");juce::PNGImageFormat png;check(png.writeImageToStream(image,*stream),"Preview write");
 std::cout<<"PASS: VST3 load, byte-exact pass-through/bypass, four rates, four block sizes, state, authorization gate, editor, system clipboard buttons and Command-V\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
