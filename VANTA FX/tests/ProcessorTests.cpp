#include "../Source/Plugin.cpp"
#include <iostream>
#include <cstdlib>
static void require(bool ok,const char* msg){if(!ok){std::cerr<<"FAIL: "<<msg<<std::endl;std::exit(1);}}
int main(){
 juce::ScopedJuceInitialiser_GUI gui;
 FxProcessor p; p.prepareToPlay(48000,256);
 for(int e=0;e<9;++e){auto* choice=p.state.getParameter("engine");choice->setValueNotifyingHost(choice->convertTo0to1(float(e)));
  std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());require(editor!=nullptr,"native editor creation");editor->setVisible(true);auto image=editor->createComponentSnapshot(editor->getLocalBounds());require(image.isValid(),"editor snapshot");
  juce::AudioBuffer<float> audio(2,256);juce::MidiBuffer midi;for(int n=0;n<30;++n){audio.clear();if(n==0)audio.setSample(0,0,1);p.processBlock(audio,midi);for(int c=0;c<2;++c)for(int k=0;k<256;++k)require(std::isfinite(audio.getSample(c,k)),"processor finite audio");}
  auto* param=p.state.getParameter(FxProcessor::id(e,0));param->setValueNotifyingHost(.3f);
 }
 juce::MemoryBlock data;p.getStateInformation(data);FxProcessor restored;restored.setStateInformation(data.getData(),int(data.getSize()));
 require(restored.state.getRawParameterValue("engine")->load()==8,"restore engine selection");
 for(int e=0;e<9;++e)require(std::abs(restored.state.getParameter(FxProcessor::id(e,0))->getValue()-.3f)<.02f,"restore inactive engine controls");
 std::cout<<"FX editor, processing and project-state tests passed"<<std::endl;
}
