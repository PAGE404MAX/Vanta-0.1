#include <JuceHeader.h>
#include "Engine.h"
class FxEditor;
class FxProcessor : public juce::AudioProcessor {
public:
 juce::AudioProcessorValueTreeState state;
 vfx::Engine engine; std::array<std::atomic<float>,2048> scope{},scopeR{};std::atomic<unsigned> write{0};
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
 FxProcessor():AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),state(*this,nullptr,"VantaFX",layout()){for(auto& x:scope)x.store(0);for(auto& x:scopeR)x.store(0);}
 const juce::String getName() const override{return "VANTA FX";}
 void prepareToPlay(double sr,int) override{engine.prepare(sr);}
 void releaseResources() override{}
 bool isBusesLayoutSupported(const BusesLayout& b) const override{return b.getMainInputChannelSet()==juce::AudioChannelSet::stereo()&&b.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();}
 void processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&) override{juce::ScopedNoDenormals dn;int e=juce::jlimit(0,8,int(state.getRawParameterValue("engine")->load()));std::array<float,16> vals{};for(int p=0;p<int(vfx::specs()[size_t(e)].controls.size());++p)vals[size_t(p)]=state.getRawParameterValue(id(e,p))->load();int mode=int(state.getRawParameterValue(id(e,100))->load()),color=int(state.getRawParameterValue(id(e,101))->load());float mix=state.getRawParameterValue("mix")->load(),trim=state.getRawParameterValue("trim")->load();bool bypass=state.getRawParameterValue("bypass")->load()>.5f;
  auto* l=b.getWritePointer(0);auto* r=b.getWritePointer(1);unsigned pos=write.load(std::memory_order_relaxed);for(int i=0;i<b.getNumSamples();++i){engine.tick(l[i],r[i],e,vals.data(),mode,color,mix,trim,bypass);scope[pos%2048].store(l[i],std::memory_order_relaxed);scopeR[(pos++)%2048].store(r[i],std::memory_order_relaxed);}write.store(pos,std::memory_order_release);
 }
 bool acceptsMidi() const override{return false;}bool producesMidi() const override{return false;}bool isMidiEffect() const override{return false;}double getTailLengthSeconds() const override{return 60;}
 int getNumPrograms() override{return 1;}int getCurrentProgram() override{return 0;}void setCurrentProgram(int) override{}const juce::String getProgramName(int) override{return "Init";}void changeProgramName(int,const juce::String&) override{}
 bool hasEditor() const override{return true;}juce::AudioProcessorEditor* createEditor() override;
 void getStateInformation(juce::MemoryBlock& b) override{auto xml=state.copyState().createXml();copyXmlToBinary(*xml,b);}
 void setStateInformation(const void* d,int n) override{auto xml=getXmlFromBinary(d,n);if(xml&&xml->hasTagName("VantaFX"))state.replaceState(juce::ValueTree::fromXml(*xml));}
};
class FxLook final : public juce::LookAndFeel_V4 {
public:
 FxLook(){setColour(juce::Slider::textBoxTextColourId,juce::Colour(0xfff4e6df));setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xff100b0c));setColour(juce::ComboBox::outlineColourId,juce::Colour(0xff552024));setColour(juce::ComboBox::textColourId,juce::Colour(0xfff4e6df));setColour(juce::PopupMenu::backgroundColourId,juce::Colour(0xff1a1113));setColour(juce::PopupMenu::highlightedBackgroundColourId,juce::Colour(0xff8b1216));}
 void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float start,float end,juce::Slider& slider) override {
  auto box=juce::Rectangle<float>(float(x),float(y),float(w),float(h)).reduced(2);const float radius=juce::jmin(box.getWidth(),box.getHeight())*.5f,cx=box.getCentreX(),cy=box.getCentreY();
  g.setOpacity(slider.isEnabled()?1.f:.35f);juce::Path rail,arc;rail.addCentredArc(cx,cy,radius,radius,0,start,end,true);arc.addCentredArc(cx,cy,radius,radius,0,start,start+pos*(end-start),true);g.setColour(juce::Colour(0xff422329));g.strokePath(rail,juce::PathStrokeType(4.f));g.setColour(juce::Colour(0xfff04438));g.strokePath(arc,juce::PathStrokeType(4.f));
  juce::ColourGradient face(juce::Colour(0xff352328),cx-radius*.4f,cy-radius,juce::Colour(0xff100b0c),cx+radius,cy+radius,false);g.setGradientFill(face);g.fillEllipse(cx-radius+5,cy-radius+5,2*radius-10,2*radius-10);g.setColour(juce::Colour(0xff552b30));g.drawEllipse(cx-radius+5,cy-radius+5,2*radius-10,2*radius-10,1.f);
  const float a=start+pos*(end-start);g.setColour(juce::Colour(0xfff4e6df));g.drawLine(cx+std::sin(a)*(radius-20),cy-std::cos(a)*(radius-20),cx+std::sin(a)*(radius-9),cy-std::cos(a)*(radius-9),3);g.setOpacity(1);
 }
 juce::Font getTextButtonFont(juce::TextButton&,int) override {return juce::Font(11.f);}
 void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour&,bool over,bool down) override {g.setColour(juce::Colour(b.getToggleState()?0xfff04438:down?0xff552024:over?0xff361215:0xff211416));g.fillRoundedRectangle(b.getLocalBounds().toFloat().reduced(.5f),4);g.setColour(juce::Colour(b.getToggleState()?0xfff04438:0xff552024));g.drawRoundedRectangle(b.getLocalBounds().toFloat().reduced(.5f),4,1);}
};
class FxEditor : public juce::AudioProcessorEditor, private juce::Timer {
 FxProcessor& p;FxLook look;
 std::array<juce::TextButton,9> tabs;std::array<juce::TextButton,3> views;
 juce::TextButton bypass{"BYPASS"},reset{"RESET ENGINE"},previous{"<"},next{">"},pause{"PAUSE"};juce::ComboBox mode,color,preset;juce::Slider mix,trim;
 using SA=juce::AudioProcessorValueTreeState::SliderAttachment;using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
 std::unique_ptr<SA> mixA,trimA;std::unique_ptr<BA> bypassA;std::unique_ptr<CA> modeA,colorA;
 struct Knob{juce::Slider slider;juce::Label label;std::unique_ptr<SA> attachment;};std::vector<std::unique_ptr<Knob>> knobs;int selected=-1,view=0;bool frozen=false;
 std::array<float,2048> left{},right{};std::array<float,4096> spectrum{};juce::dsp::FFT fft{11};
 float scale() const{return getWidth()/1180.f;}
 void place(juce::Component& c,int x,int y,int w,int h){const auto s=scale();c.setBounds(juce::roundToInt(x*s),juce::roundToInt(y*s),juce::roundToInt(w*s),juce::roundToInt(h*s));}
 void setParameter(const juce::String& id,float value){auto* a=p.state.getParameter(id);a->beginChangeGesture();a->setValueNotifyingHost(a->convertTo0to1(value));a->endChangeGesture();}
 void setEngine(int e){setParameter("engine",float(e));show(e);}
 void configureKnob(juce::Slider& s){s.setSliderStyle(juce::Slider::RotaryVerticalDrag);s.setRotaryParameters(juce::MathConstants<float>::pi*1.25f,juce::MathConstants<float>::pi*2.75f,true);s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,130,22);s.setMouseDragSensitivity(200);s.setVelocityModeParameters(1.0,1,0.0,true,juce::ModifierKeys::shiftModifier);}
 void applyPreset(int index){const auto& spec=vfx::specs()[size_t(selected)];for(int i=0;i<int(spec.controls.size());++i){const auto& c=spec.controls[size_t(i)];float v=c.initial;if(index==1&&juce::String(c.name)=="Width")v=juce::jmin(c.max,150.f);if(index==1&&juce::String(c.name)=="Feedback")v=juce::jmin(c.max,65.f);if(index==2&&juce::String(c.name)=="High cut")v=juce::jlimit(c.min,c.max,3500.f);setParameter(FxProcessor::id(selected,i),v);}setParameter(FxProcessor::id(selected,100),0);setParameter(FxProcessor::id(selected,101),float(index==2?1:0));preset.setSelectedId(index+1,juce::dontSendNotification);}
 void show(int e){selected=juce::jlimit(0,8,e);knobs.clear();modeA.reset();colorA.reset();mode.clear();color.clear();auto& s=vfx::specs()[size_t(selected)];for(int k=0;k<int(s.modes.size());++k)mode.addItem(s.modes[size_t(k)],k+1);for(int k=0;k<int(s.colors.size());++k)color.addItem(s.colors[size_t(k)],k+1);modeA=std::make_unique<CA>(p.state,FxProcessor::id(selected,100),mode);colorA=std::make_unique<CA>(p.state,FxProcessor::id(selected,101),color);
  for(int k=0;k<int(s.controls.size());++k){auto n=std::make_unique<Knob>();auto& c=s.controls[size_t(k)];n->label.setText(juce::String(c.name).toUpperCase(),juce::dontSendNotification);n->label.setJustificationType(juce::Justification::centred);n->label.setFont(11.f);n->label.setColour(juce::Label::textColourId,juce::Colour(0xffa98d87));configureKnob(n->slider);n->slider.setTextValueSuffix(c.unit);n->slider.setComponentID("parameter-"+FxProcessor::id(selected,k));n->slider.setDoubleClickReturnValue(true,c.initial);addAndMakeVisible(n->slider);addAndMakeVisible(n->label);n->attachment=std::make_unique<SA>(p.state,FxProcessor::id(selected,k),n->slider);knobs.push_back(std::move(n));}
  for(int i=0;i<9;++i)tabs[size_t(i)].setToggleState(i==selected,juce::dontSendNotification);preset.setSelectedId(0,juce::dontSendNotification);resized();repaint();
 }
 void timerCallback() override{int e=juce::jlimit(0,8,int(p.state.getRawParameterValue("engine")->load()));if(e!=selected)show(e);if(!frozen){const auto pos=p.write.load(std::memory_order_acquire);for(unsigned i=0;i<2048;++i){left[i]=p.scope[(pos+i)%2048].load(std::memory_order_relaxed);right[i]=p.scopeR[(pos+i)%2048].load(std::memory_order_relaxed);}spectrum.fill(0);for(size_t i=0;i<2048;++i)spectrum[i]=(left[i]+right[i])*.5f*(.5f-.5f*std::cos(juce::MathConstants<float>::twoPi*float(i)/2047));fft.performFrequencyOnlyForwardTransform(spectrum.data());}repaint();}
 void drawVisual(juce::Graphics& g){juce::Rectangle<float> a(28,210,840,205);g.setColour(juce::Colour(0xff140e10));g.fillRoundedRectangle(a,6);g.setColour(juce::Colour(0xff552024));g.drawRoundedRectangle(a,6,1);for(int x=28;x<868;x+=40)g.drawVerticalLine(x,211,414);for(int y=250;y<415;y+=32)g.drawHorizontalLine(y,29,867);
  juce::Graphics::ScopedSaveState clip(g);g.reduceClipRegion(a.reduced(2).toNearestInt());
  if(view==0){for(int ch=1;ch>=0;--ch){juce::Path path;const auto& data=ch==0?left:right;for(size_t i=0;i<2048;i+=2){float x=28+float(i)*840/2047,y=315-juce::jlimit(-1.f,1.f,data[i])*75;if(i==0)path.startNewSubPath(x,y);else path.lineTo(x,y);}g.setColour(juce::Colour(ch==0?0xfff04438:0xffb38962));g.strokePath(path,juce::PathStrokeType(1.4f));}}
  else if(view==1){for(int b=0;b<72;++b){double sr=p.getSampleRate()>0?p.getSampleRate():44100;const float f=20.f*std::pow(float(sr*.5/20),float(b)/71);int bin=juce::jlimit(1,1023,int(f*2048/sr));float mag=spectrum[size_t(bin)]/512.f;float db=juce::Decibels::gainToDecibels(mag,-90.f);float height=juce::jlimit(0.f,1.f,(db+90)/90)*150;g.setColour(juce::Colour(0xfff04438));g.fillRect(40+b*11.3f,395-height,7.f,height);}}
  else{g.setColour(juce::Colour(0xfff04438));for(size_t i=0;i<2048;i+=2){float mid=(left[i]+right[i])*.5f,side=(left[i]-right[i])*.5f;float x=448+juce::jlimit(-1.f,1.f,side)*185,y=315-juce::jlimit(-1.f,1.f,mid)*80;g.fillEllipse(x,y,2.f,2.f);}}
  g.setColour(juce::Colour(0xffa98d87));g.setFont(10.f);g.drawText(frozen?"DISPLAY PAUSED":"LIVE OUTPUT / "+juce::String(view==0?"STEREO WAVEFORM":view==1?"SPECTRUM":"STEREO SPACE"),40,221,400,18,juce::Justification::centredLeft);
 }
public:
 FxEditor(FxProcessor& proc):AudioProcessorEditor(proc),p(proc){setLookAndFeel(&look);for(int e=0;e<9;++e){auto& b=tabs[size_t(e)];b.setButtonText(vfx::specs()[size_t(e)].name);b.setComponentID("tab-"+juce::String(e));b.onClick=[this,e]{setEngine(e);};addAndMakeVisible(b);}const char* labels[]={"WAVEFORM","SPECTRUM","SPACE"};for(int i=0;i<3;++i){views[size_t(i)].setButtonText(labels[i]);views[size_t(i)].onClick=[this,i]{view=i;for(int n=0;n<3;++n)views[size_t(n)].setToggleState(n==i,juce::dontSendNotification);repaint();};addAndMakeVisible(views[size_t(i)]);}views[0].setToggleState(true,juce::dontSendNotification);
  for(auto* c:std::array<juce::Component*,10>{&bypass,&reset,&mode,&color,&mix,&trim,&previous,&next,&preset,&pause})addAndMakeVisible(c);
  mode.setComponentID("algorithm");color.setComponentID("character");mix.setComponentID("global-mix");trim.setComponentID("global-trim");bypass.setComponentID("bypass");bypass.setClickingTogglesState(true);bypassA=std::make_unique<BA>(p.state,"bypass",bypass);
  configureKnob(mix);configureKnob(trim);mix.textFromValueFunction=[](double v){return juce::String(v*100,1)+" %";};mix.valueFromTextFunction=[](const juce::String& t){return t.getDoubleValue()/100;};mix.setDoubleClickReturnValue(true,.5);trim.setDoubleClickReturnValue(true,0);mixA=std::make_unique<SA>(p.state,"mix",mix);trimA=std::make_unique<SA>(p.state,"trim",trim);mix.textFromValueFunction=[](double v){return juce::String(v*100,1)+" %";};mix.valueFromTextFunction=[](const juce::String& t){return t.getDoubleValue()/100;};mix.updateText();trim.setTextValueSuffix(" dB");
  preset.addItem("INIT",1);preset.addItem("WIDE",2);preset.addItem("DARK",3);preset.setTextWhenNothingSelected("CURRENT PATCH");preset.onChange=[this]{if(preset.getSelectedId()>0)applyPreset(preset.getSelectedId()-1);};previous.onClick=[this]{applyPreset((juce::jmax(1,preset.getSelectedId())+1)%3);};next.onClick=[this]{applyPreset(juce::jmax(1,preset.getSelectedId())%3);};reset.onClick=[this]{applyPreset(0);};pause.onClick=[this]{frozen=!frozen;pause.setButtonText(frozen?"RESUME":"PAUSE");};
  setResizable(true,true);setResizeLimits(826,700,1770,1500);getConstrainer()->setFixedAspectRatio(1180./1000.);setSize(1180,1000);show(int(p.state.getRawParameterValue("engine")->load()));startTimerHz(30);
 }
 ~FxEditor() override{stopTimer();setLookAndFeel(nullptr);}
 void paint(juce::Graphics& g) override{g.addTransform(juce::AffineTransform::scale(scale()));g.fillAll(juce::Colour(0xff100b0c));g.setColour(juce::Colour(0xff8b1216));g.drawRoundedRectangle(juce::Rectangle<float>(4,4,1172,992),10,7);g.setColour(juce::Colour(0xff552024));g.drawHorizontalLine(94,10,1170);g.drawHorizontalLine(149,10,1170);g.drawHorizontalLine(951,10,1170);
  auto text=[&](const juce::String& t,int x,int y,int w,int h,float size,juce::Colour colour){g.setFont(size);g.setColour(colour);g.drawText(t,x,y,w,h,juce::Justification::centredLeft);};const juce::Colour cream(0xfff4e6df),muted(0xffa98d87),red(0xfff04438);
  text("VANTA",28,24,175,40,34,cream);text("FX",205,24,100,40,34,red);text("NINE SPACES. ONE EFFECT.",28,66,400,16,10,muted);text("PRESET",660,41,60,16,10,muted);
  text(juce::String(vfx::specs()[size_t(juce::jmax(0,selected))].name).toUpperCase(),28,166,700,33,25,cream);text(juce::String(selected+1).paddedLeft('0',2),808,162,60,40,35,juce::Colour(0xff552024));drawVisual(g);
  for(int i=0;i<int(knobs.size());++i){int x=28+(i%4)*213,y=464+(i/4)*117;g.setColour(juce::Colour(0xff1a1113));g.fillRoundedRectangle(float(x),float(y),201,109,6);g.setColour(juce::Colour(0xff392023));g.drawRoundedRectangle(float(x),float(y),201,109,6,1);}
  g.setColour(juce::Colour(0xff1a1012));g.fillRoundedRectangle(889,210,263,722,6);g.setColour(juce::Colour(0xff552024));g.drawRoundedRectangle(889,210,263,722,6,1);
  text("ENGINE SETTINGS",908,228,220,20,11,red);text("Algorithm",908,269,210,20,12,muted);text("Character",908,351,210,20,12,muted);text("GLOBAL OUTPUT",908,451,220,22,11,red);text("DRY / WET MIX",908,495,220,20,11,muted);text("OUTPUT TRIM",908,634,220,20,11,muted);text("One active engine at a time.",908,832,220,20,12,muted);text("Each tab retains its settings.",908,854,220,20,12,muted);text("STEREO IN   >   "+juce::String(vfx::specs()[size_t(juce::jmax(0,selected))].name).toUpperCase()+"   >   STEREO OUT",28,962,820,22,11,muted);text("VANTA FX / DEVELOPMENT",908,962,230,22,10,red);
 }
 void resized() override{for(int i=0;i<9;++i)place(tabs[size_t(i)],28+i*125,108,119,28);place(previous,724,30,28,35);place(preset,760,30,183,35);place(next,951,30,28,35);place(bypass,1000,30,150,35);for(int i=0;i<3;++i)place(views[size_t(i)],28+i*113,425,105,26);place(pause,761,425,107,26);place(mode,908,297,225,34);place(color,908,379,225,34);place(mix,915,520,211,110);place(trim,915,659,211,110);place(reset,908,783,225,32);for(int i=0;i<int(knobs.size());++i){int x=28+(i%4)*213,y=464+(i/4)*117;place(knobs[size_t(i)]->label,x+6,y+7,189,17);place(knobs[size_t(i)]->slider,x+25,y+26,151,79);}}
};
juce::AudioProcessorEditor* FxProcessor::createEditor(){return new FxEditor(*this);}juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new FxProcessor;}
