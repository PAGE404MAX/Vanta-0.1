#include <JuceHeader.h>
#include "Engine.h"
class FxEditor;
class FxProcessor : public juce::AudioProcessor {
public:
 juce::AudioProcessorValueTreeState state;
 vfx::Engine engine; std::array<std::atomic<float>,256> scope{};std::atomic<unsigned> write{0};
 static juce::String id(int e,int p){return "fx"+juce::String(e)+"_"+juce::String(p);}
 static juce::AudioProcessorValueTreeState::ParameterLayout layout(){
  juce::AudioProcessorValueTreeState::ParameterLayout l;
  juce::StringArray names;for(auto& s:vfx::specs())names.add(s.name);
  l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"engine",1},"Engine",names,0));
  l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"mix",1},"Mix",0.f,1.f,.5f));
  l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"trim",1},"Output",-24.f,6.f,0.f));
  l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass",1},"Bypass",false));
  for(int e=0;e<9;++e){const auto& s=vfx::specs()[size_t(e)];for(int p=0;p<int(s.controls.size());++p){auto& c=s.controls[size_t(p)];l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id(e,p),1},juce::String(s.name)+" / "+c.name,juce::NormalisableRange<float>(c.min,c.max,c.step),c.initial));}for(int k=0;k<2;++k){juce::StringArray opts;for(auto n:k==0?s.modes:s.colors)opts.add(n);l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{id(e,100+k),1},juce::String(s.name)+(k==0?" Algorithm":" Character"),opts,0));}}
  return l;
 }
 FxProcessor():AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),state(*this,nullptr,"VantaFX",layout()){for(auto& x:scope)x.store(0);}
 const juce::String getName() const override{return "VANTA FX";}
 void prepareToPlay(double sr,int) override{engine.prepare(sr);}
 void releaseResources() override{}
 bool isBusesLayoutSupported(const BusesLayout& b) const override{return b.getMainInputChannelSet()==juce::AudioChannelSet::stereo()&&b.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();}
 void processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&) override{juce::ScopedNoDenormals dn;int e=juce::jlimit(0,8,int(state.getRawParameterValue("engine")->load()));std::array<float,16> vals{};for(int p=0;p<int(vfx::specs()[size_t(e)].controls.size());++p)vals[size_t(p)]=state.getRawParameterValue(id(e,p))->load();int mode=int(state.getRawParameterValue(id(e,100))->load()),color=int(state.getRawParameterValue(id(e,101))->load());float mix=state.getRawParameterValue("mix")->load(),trim=state.getRawParameterValue("trim")->load();bool bypass=state.getRawParameterValue("bypass")->load()>.5f;
  auto* l=b.getWritePointer(0);auto* r=b.getWritePointer(1);unsigned pos=write.load(std::memory_order_relaxed);for(int i=0;i<b.getNumSamples();++i){engine.tick(l[i],r[i],e,vals.data(),mode,color,mix,trim,bypass);if(i%16==0)scope[(pos++)%256].store((l[i]+r[i])*.5f,std::memory_order_relaxed);}write.store(pos,std::memory_order_release);
 }
 bool acceptsMidi() const override{return false;}bool producesMidi() const override{return false;}bool isMidiEffect() const override{return false;}double getTailLengthSeconds() const override{return 60;}
 int getNumPrograms() override{return 1;}int getCurrentProgram() override{return 0;}void setCurrentProgram(int) override{}const juce::String getProgramName(int) override{return "Init";}void changeProgramName(int,const juce::String&) override{}
 bool hasEditor() const override{return true;}juce::AudioProcessorEditor* createEditor() override;
 void getStateInformation(juce::MemoryBlock& b) override{auto xml=state.copyState().createXml();copyXmlToBinary(*xml,b);}
 void setStateInformation(const void* d,int n) override{auto xml=getXmlFromBinary(d,n);if(xml&&xml->hasTagName("VantaFX"))state.replaceState(juce::ValueTree::fromXml(*xml));}
};
class FxEditor : public juce::AudioProcessorEditor,private juce::Timer {
 FxProcessor& p;juce::LookAndFeel_V4 look;
 std::array<juce::TextButton,9> tabs;juce::TextButton bypass{"BYPASS"},reset{"RESET ENGINE"};juce::ComboBox mode,color;juce::Slider mix,trim;
 using SA=juce::AudioProcessorValueTreeState::SliderAttachment;using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
 std::unique_ptr<SA> mixA,trimA;std::unique_ptr<BA> bypassA;std::unique_ptr<CA> modeA,colorA;
 struct Knob {juce::Slider slider;juce::Label label;std::unique_ptr<SA> attachment;};std::vector<std::unique_ptr<Knob>> knobs;int selected=-1;
 void setEngine(int e){auto* param=p.state.getParameter("engine");param->beginChangeGesture();param->setValueNotifyingHost(param->convertTo0to1(float(e)));param->endChangeGesture();show(e);}
 void show(int e){selected=e;knobs.clear();modeA.reset();colorA.reset();mode.clear();color.clear();auto& s=vfx::specs()[size_t(e)];for(int k=0;k<int(s.modes.size());++k)mode.addItem(s.modes[size_t(k)],k+1);for(int k=0;k<int(s.colors.size());++k)color.addItem(s.colors[size_t(k)],k+1);modeA=std::make_unique<CA>(p.state,FxProcessor::id(e,100),mode);colorA=std::make_unique<CA>(p.state,FxProcessor::id(e,101),color);
 for(int k=0;k<int(s.controls.size());++k){auto n=std::make_unique<Knob>();auto& c=s.controls[size_t(k)];n->label.setText(c.name,juce::dontSendNotification);n->label.setJustificationType(juce::Justification::centred);n->label.setFont(12.f);n->slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);n->slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,110,22);n->slider.setTextValueSuffix(c.unit);addAndMakeVisible(n->slider);addAndMakeVisible(n->label);n->attachment=std::make_unique<SA>(p.state,FxProcessor::id(e,k),n->slider);knobs.push_back(std::move(n));}for(int i=0;i<9;++i)tabs[size_t(i)].setToggleState(i==e,juce::dontSendNotification);resized();repaint();
 }
 void timerCallback() override{int e=juce::jlimit(0,8,int(p.state.getRawParameterValue("engine")->load()));if(e!=selected)show(e);repaint(28,177,835,160);}
public:
 FxEditor(FxProcessor& proc):AudioProcessorEditor(proc),p(proc){look.setColour(juce::Slider::rotarySliderFillColourId,juce::Colour(0xfff04438));look.setColour(juce::Slider::thumbColourId,juce::Colour(0xfff4e6df));look.setColour(juce::Slider::rotarySliderOutlineColourId,juce::Colour(0xff552024));look.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff8b1216));setLookAndFeel(&look);
 for(int e=0;e<9;++e){auto& b=tabs[size_t(e)];b.setButtonText(vfx::specs()[size_t(e)].name);b.onClick=[this,e]{setEngine(e);};addAndMakeVisible(b);}for(auto* c:std::array<juce::Component*,6>{&bypass,&reset,&mode,&color,&mix,&trim})addAndMakeVisible(c);
 bypass.setClickingTogglesState(true);bypassA=std::make_unique<BA>(p.state,"bypass",bypass);for(auto* s:{&mix,&trim}){s->setSliderStyle(juce::Slider::LinearHorizontal);s->setTextBoxStyle(juce::Slider::TextBoxBelow,false,150,20);}mixA=std::make_unique<SA>(p.state,"mix",mix);trimA=std::make_unique<SA>(p.state,"trim",trim);trim.setTextValueSuffix(" dB");reset.onClick=[this]{auto& spec=vfx::specs()[size_t(selected)];for(int i=0;i<int(spec.controls.size());++i){auto* a=p.state.getParameter(FxProcessor::id(selected,i));a->beginChangeGesture();a->setValueNotifyingHost(a->convertTo0to1(spec.controls[size_t(i)].initial));a->endChangeGesture();}};setSize(1140,880);show(int(p.state.getRawParameterValue("engine")->load()));startTimerHz(30);
 }
 ~FxEditor() override{stopTimer();setLookAndFeel(nullptr);}
 void paint(juce::Graphics& g) override{g.fillAll(juce::Colour(0xff100b0c));g.setColour(juce::Colour(0xff8b1216));g.drawRect(getLocalBounds(),5);g.setColour(juce::Colour(0xfff4e6df));g.setFont(32.f);g.drawText("VANTA FX",28,17,300,45,juce::Justification::centredLeft);g.setFont(11.f);g.setColour(juce::Colour(0xffa98d87));g.drawText("NINE SPACES. ONE EFFECT.   /   DEVELOPMENT BUILD",28,60,700,18,juce::Justification::centredLeft);g.setFont(24.f);g.setColour(juce::Colour(0xfff4e6df));g.drawText(vfx::specs()[size_t(std::max(0,selected))].name,28,126,700,35,juce::Justification::centredLeft);
 juce::Rectangle<float> area(28,177,835,160);g.setColour(juce::Colour(0xff1a1113));g.fillRoundedRectangle(area,5);g.setColour(juce::Colour(0xff552024));for(int x=28;x<863;x+=40)g.drawVerticalLine(x,177,337);juce::Path path;unsigned pos=p.write.load(std::memory_order_acquire);for(unsigned i=0;i<256;++i){float v=p.scope[(pos+i)%256].load(std::memory_order_relaxed);float x=28+float(i)*835/255,y=257-juce::jlimit(-1.f,1.f,v)*65;if(i==0)path.startNewSubPath(x,y);else path.lineTo(x,y);}g.setColour(juce::Colour(0xfff04438));g.strokePath(path,juce::PathStrokeType(1.8f));g.setFont(10.f);g.drawText("LIVE OUTPUT / WAVEFORM",40,184,300,18,juce::Justification::centredLeft);
 g.setColour(juce::Colour(0xffa98d87));g.setFont(12.f);g.drawText("ALGORITHM",900,180,190,20,juce::Justification::centredLeft);g.drawText("CHARACTER",900,265,190,20,juce::Justification::centredLeft);g.drawText("DRY / WET",900,365,190,20,juce::Justification::centredLeft);g.drawText("OUTPUT TRIM",900,465,190,20,juce::Justification::centredLeft);g.drawText("STEREO IN  →  ACTIVE ENGINE  →  STEREO OUT",28,840,800,22,juce::Justification::centredLeft);
 }
 void resized() override{for(int i=0;i<9;++i)tabs[size_t(i)].setBounds(28+i*120,91,115,30);bypass.setBounds(980,28,125,34);mode.setBounds(900,210,190,32);color.setBounds(900,295,190,32);mix.setBounds(890,395,210,55);trim.setBounds(890,495,210,55);reset.setBounds(900,590,190,32);for(int i=0;i<int(knobs.size());++i){int x=28+(i%4)*211,y=355+(i/4)*118;knobs[size_t(i)]->label.setBounds(x,y,200,20);knobs[size_t(i)]->slider.setBounds(x,y+20,200,94);}}
};
juce::AudioProcessorEditor* FxProcessor::createEditor(){return new FxEditor(*this);}juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new FxProcessor;}
