#include <JuceHeader.h>
#include <atomic>
#include <array>
#include <cmath>

class Sound final : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};
class Voice final : public juce::SynthesiserVoice
{
public:
    Voice(std::atomic<float>* a, std::atomic<float>* r, std::atomic<float>* shape)
        : attack(a), release(r), waveform(shape) {}
    bool canPlaySound(juce::SynthesiserSound* s) override { return dynamic_cast<Sound*>(s) != nullptr; }
    void startNote(int note, float velocity, juce::SynthesiserSound*, int) override
    {
        phase = 0.0; amplitude = velocity * 0.12f;
        increment = juce::MathConstants<double>::twoPi * juce::MidiMessage::getMidiNoteInHertz(note) / getSampleRate();
        juce::ADSR::Parameters p;
        p.attack = attack->load(); p.decay = 0.1f; p.sustain = 0.8f; p.release = release->load();
        envelope.setSampleRate(getSampleRate()); envelope.setParameters(p); envelope.noteOn();
    }
    void stopNote(float, bool tail) override { if (tail) envelope.noteOff(); else { envelope.reset(); clearCurrentNote(); } }
    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}
    void renderNextBlock(juce::AudioBuffer<float>& out, int start, int count) override
    {
        if (!isVoiceActive()) return;
        const auto shape = waveform->load();
        for (int i = 0; i < count; ++i)
        {
            const auto sample = static_cast<float>(shape < 0.5f ? std::sin(phase) : (phase / juce::MathConstants<double>::pi - 1.0));
            const auto value = sample * amplitude * envelope.getNextSample();
            for (int ch = 0; ch < out.getNumChannels(); ++ch) out.addSample(ch, start + i, value);
            phase += increment;
            if (phase >= juce::MathConstants<double>::twoPi) phase -= juce::MathConstants<double>::twoPi;
        }
        if (!envelope.isActive()) clearCurrentNote();
    }
private:
    std::atomic<float>* attack; std::atomic<float>* release; std::atomic<float>* waveform;
    juce::ADSR envelope; double phase = 0, increment = 0; float amplitude = 0;
};
class Vanta final : public juce::AudioProcessor
{
public:
    Vanta() : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
              state(*this, nullptr, "VANTA", makeLayout())
    {
        synth.addSound(new Sound());
        for (int i = 0; i < 16; ++i)
            synth.addVoice(new Voice(state.getRawParameterValue("attack"), state.getRawParameterValue("release"), state.getRawParameterValue("wave")));
    }
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout p;
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"level",1}, "Level", juce::NormalisableRange<float>(0.f,1.f), 0.7f));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"attack",1}, "Attack", juce::NormalisableRange<float>(0.001f,2.f,0.001f,0.4f), 0.02f));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"release",1}, "Release", juce::NormalisableRange<float>(0.01f,4.f,0.001f,0.4f), 0.35f));
        p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"wave",1}, "Waveform", juce::StringArray{"Sine", "Saw"}, 0));
        return p;
    }
    const juce::String getName() const override { return "Vanta Beta 0.3"; }
    void prepareToPlay(double rate, int) override { synth.setCurrentPlaybackSampleRate(rate); }
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() || l.getMainOutputChannelSet() == juce::AudioChannelSet::mono(); }
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    { buffer.clear(); synth.renderNextBlock(buffer, midi, 0, buffer.getNumSamples()); buffer.applyGain(state.getRawParameterValue("level")->load()); }
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    double getTailLengthSeconds() const override { return 4.0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& dest) override { if (auto xml = state.copyState().createXml()) copyXmlToBinary(*xml, dest); }
    void setStateInformation(const void* data, int size) override { if (auto xml = getXmlFromBinary(data,size)) if (xml->hasTagName(state.state.getType())) state.replaceState(juce::ValueTree::fromXml(*xml)); }
public:
    juce::AudioProcessorValueTreeState state;
private:
    juce::Synthesiser synth;
};

class VantaLook final : public juce::LookAndFeel_V4
{
public:
    VantaLook() { setColour(juce::Slider::textBoxTextColourId, juce::Colours::white); setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack); setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff30090c)); setColour(juce::ComboBox::textColourId, juce::Colours::white); setColour(juce::ComboBox::outlineColourId, juce::Colour(0xffb82029)); setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff19090a)); setColour(juce::PopupMenu::textColourId, juce::Colours::white); }
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float position, float start, float end, juce::Slider&) override
    {
        const auto size = (float) juce::jmin(width, height) - 14.0f;
        auto r = juce::Rectangle<float>((float)x + ((float)width-size)/2.0f, (float)y + ((float)height-size)/2.0f, size, size);
        const auto centre = r.getCentre(); const auto radius = size*0.46f;
        juce::Path track; track.addCentredArc(centre.x,centre.y,radius,radius,0,start,end,true);
        g.setColour(juce::Colour(0xff64141a)); g.strokePath(track,juce::PathStrokeType(4.0f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        juce::Path progress; progress.addCentredArc(centre.x,centre.y,radius,radius,0,start,start+position*(end-start),true);
        g.setColour(juce::Colour(0xffff3039)); g.strokePath(progress,juce::PathStrokeType(4.0f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        g.setColour(juce::Colour(0xff090909)); g.fillEllipse(r.reduced(size*0.15f));
        g.setColour(juce::Colour(0xffb21f28)); g.drawEllipse(r.reduced(size*0.15f),1.5f);
        const auto angle=start+position*(end-start); const auto inner=radius*0.49f, outer=radius*0.72f;
        g.setColour(juce::Colours::white); g.drawLine(centre.x+std::sin(angle)*inner,centre.y-std::cos(angle)*inner,centre.x+std::sin(angle)*outer,centre.y-std::cos(angle)*outer,2.0f);
    }
};
class VantaEditor final : public juce::AudioProcessorEditor
{
public:
    explicit VantaEditor(Vanta& owner) : AudioProcessorEditor(owner), processor(owner),
        levelAttachment(owner.state,"level",level), attackAttachment(owner.state,"attack",attack), releaseAttachment(owner.state,"release",release)
    {
        setLookAndFeel(&look); setSize(900,560);
        configureKnob(level); configureKnob(attack); configureKnob(release);
        for (auto* l : { &levelLabel, &attackLabel, &releaseLabel, &waveLabel }) { addAndMakeVisible(l); l->setJustificationType(juce::Justification::centred); l->setColour(juce::Label::textColourId,juce::Colours::white); l->setFont(juce::FontOptions(11.0f,juce::Font::bold)); }
        levelLabel.setText("LEVEL",juce::dontSendNotification); attackLabel.setText("ATTACK",juce::dontSendNotification); releaseLabel.setText("RELEASE",juce::dontSendNotification); waveLabel.setText("WAVEFORM",juce::dontSendNotification);
        wave.addItem("SINE",1); wave.addItem("SAW",2); addAndMakeVisible(wave); waveAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(owner.state,"wave",wave);
        auto makeButton=[this](juce::TextButton& button, const juce::String& text) { button.setButtonText(text); button.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff35090d)); button.setColour(juce::TextButton::textColourOffId,juce::Colours::white); addAndMakeVisible(button); };
        const char* names[]={"MAIN","OSC","EFFECTS","MODULATION","SETTINGS"};
        for(int i=0;i<5;++i) { makeButton(tabs[(size_t)i],names[i]); tabs[(size_t)i].onClick=[this,i]{ browser=false; page=i; refresh(); }; }
        makeButton(browserButton,"<> "); browserButton.setTooltip("Open preset browser"); browserButton.onClick=[this]{browser=!browser; refresh();};
        makeButton(previous,"^"); previous.onClick=[this]{ changePreset(-1); }; makeButton(next,"v"); next.onClick=[this]{ changePreset(1); };
        const char* folderNames[]={"Sfx","Pads","Leads","Synth"};
        for(int i=0;i<4;++i){ makeButton(folders[(size_t)i],folderNames[i]); folders[(size_t)i].onClick=[this,i]{ folder=i; refresh(); }; }
        for(int i=0;i<4;++i){ makeButton(presetButtons[(size_t)i],presetNames[i]); presetButtons[(size_t)i].onClick=[this,i]{currentPreset=i; browser=false; refresh();}; }
        for(int i=0;i<4;++i){ makeButton(partials[(size_t)i],"PARTIAL "+juce::String(i+1)); partials[(size_t)i].onClick=[this,i]{partial=i; refresh();}; }
        makeButton(reset,"RESET UI SETTINGS"); reset.onClick=[this]{quality.setSelectedId(1); midi.setSelectedId(1); scale.setSelectedId(1);};
        for(auto* c : {&module,&quality,&midi,&scale}) addAndMakeVisible(*c);
        module.addItemList(juce::StringArray{"01  OSCILLATORS","02  PITCH","03  FILTER","04  AMP / EQ","05  LFO 1","06  LFO 2"},1); module.setSelectedId(1); module.onChange=[this]{refresh();};
        quality.addItemList(juce::StringArray{"Standard","High","Draft"},1); quality.setSelectedId(1);
        midi.addItemList(juce::StringArray{"Normal","Soft","Hard","Fixed"},1); midi.setSelectedId(1);
        scale.addItemList(juce::StringArray{"100%","75%","125%","150%"},1); scale.setSelectedId(1);
        for(int i=0;i<9;++i) { configureKnob(demoKnobs[(size_t)i]); demoKnobs[(size_t)i].setRange(0,100,1); demoKnobs[(size_t)i].setValue(50); addAndMakeVisible(demoLabels[(size_t)i]); demoLabels[(size_t)i].setJustificationType(juce::Justification::centred); demoLabels[(size_t)i].setColour(juce::Label::textColourId,juce::Colours::white); demoLabels[(size_t)i].setFont(juce::FontOptions(10.0f,juce::Font::bold)); }
        configureKnob(poly); poly.setRange(1,16,1); poly.setValue(16); poly.setTextValueSuffix("v");
        polyLabel.setText("POLYPHONY",juce::dontSendNotification); polyLabel.setJustificationType(juce::Justification::centred); polyLabel.setColour(juce::Label::textColourId,juce::Colours::white); addAndMakeVisible(polyLabel);
        refresh();
    }
    ~VantaEditor() override { setLookAndFeel(nullptr); }
    void configureKnob(juce::Slider& s) { s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,70,19); s.setRotaryParameters(juce::MathConstants<float>::pi*1.25f,juce::MathConstants<float>::pi*2.75f,true); addAndMakeVisible(s); }
    void changePreset(int dir) { currentPreset=(currentPreset+dir+4)%4; repaint(); }
    void refresh()
    {
        const bool main=!browser && page==0, osc=!browser && page==1, fx=!browser && page==2, mod=!browser && page==3, settings=!browser && page==4;
        for(auto* c : std::array<juce::Component*,8>{&level,&attack,&release,&wave,&levelLabel,&attackLabel,&releaseLabel,&waveLabel}) c->setVisible(main);
        for(auto& b:tabs) b.setVisible(!browser);
        for(auto& b:folders) b.setVisible(browser);
        for(int i=0;i<4;++i){ bool inFolder=(folder==1&&i==1)||(folder==2&&i==2)||(folder==3&&(i==0||i==3)); presetButtons[(size_t)i].setVisible(browser&&inFolder); }
        for(auto& b:partials) b.setVisible(osc);
        module.setVisible(osc); poly.setVisible(osc); polyLabel.setVisible(osc);
        quality.setVisible(settings); midi.setVisible(settings); scale.setVisible(settings); reset.setVisible(settings);
        const int count=osc ? (module.getSelectedId()==1?9:3) : (fx||mod?9:0);
        for(int i=0;i<9;++i){demoKnobs[(size_t)i].setVisible(i<count);demoLabels[(size_t)i].setVisible(i<count);}
        const juce::StringArray oscLabels{"LEVEL","DETUNE","WAVE","LEVEL","DETUNE","WAVE","LEVEL","DETUNE","WAVE"};
        const juce::StringArray otherLabels{"AMOUNT","RATE","DEPTH","TIME","FEEDBACK","MIX","DRIVE","TONE","MIX"};
        const juce::StringArray fxLabels{"SIZE","DECAY","MIX","TIME","FEEDBACK","MIX","DRIVE","TONE","MIX"};
        const juce::StringArray modLabels{"ATTACK","DECAY","SUSTAIN","RATE","DEPTH","SHAPE","SOURCE","TARGET","AMOUNT"};
        for(int i=0;i<9;++i) demoLabels[(size_t)i].setText(osc&&module.getSelectedId()==1?oscLabels[i]:fx?fxLabels[i]:mod?modLabels[i]:otherLabels[i],juce::dontSendNotification);
        resized(); repaint();
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff87090e)); g.setColour(juce::Colour(0xffff3039)); g.fillRect(0,0,getWidth(),4);
        g.setColour(juce::Colours::white); g.setFont(juce::FontOptions(25.0f,juce::Font::bold)); g.drawText("VANTA",22,12,180,32,juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xffffb5b8)); g.setFont(juce::FontOptions(10.0f,juce::Font::bold)); g.drawText("BETA 0.3  /  SOFTWARE SYNTHESIZER",24,42,350,15,juce::Justification::centredLeft);
        panel(g,15,66,870,105); g.setColour(juce::Colour(0xffff333b)); g.setFont(juce::FontOptions(10.0f,juce::Font::bold)); g.drawText("PRESET   /   FACTORY COLLECTION",94,78,400,18,juce::Justification::centredLeft);
        g.setColour(juce::Colours::white); g.setFont(juce::FontOptions(22.0f,juce::Font::bold)); g.drawText(juce::String(95+currentPreset).paddedLeft('0',3)+"   "+presetNames[currentPreset],94,105,600,34,juce::Justification::centredLeft);
        g.setFont(juce::FontOptions(10.0f)); g.setColour(juce::Colour(0xffffa9ac)); g.drawText("DEMO PRESET NAMES ONLY",630,135,225,20,juce::Justification::centredRight);
        if(browser) { panel(g,15,185,870,315); g.setColour(juce::Colours::white); g.setFont(juce::FontOptions(14.0f,juce::Font::bold)); g.drawText("PRESET BROWSER",35,197,250,25,juce::Justification::centredLeft); g.drawText(juce::String(folder==0?"SFX":folder==1?"PADS":folder==2?"LEADS":"SYNTH")+" / PRESETS",255,197,400,25,juce::Justification::centredLeft); if(folder==0){g.setColour(juce::Colours::grey);g.setFont(juce::FontOptions(12.0f));g.drawText("No presets yet",255,240,250,20,juce::Justification::centredLeft);} }
        else { panel(g,15,225,870,270); g.setColour(juce::Colours::white); g.setFont(juce::FontOptions(13.0f,juce::Font::bold)); const char* titles[]={"PERFORMANCE & SOUND","OSC / PARTIAL EDITOR","EFFECTS RACK","MODULATION","SETTINGS"}; g.drawText(titles[page],32,232,350,22,juce::Justification::centredLeft);
            if(page==1){ g.setFont(juce::FontOptions(10.0f));g.setColour(juce::Colour(0xffff9399));g.drawText("UI DEMO ONLY — DOES NOT CHANGE THE SOUND",450,234,400,18,juce::Justification::centredRight);if(module.getSelectedId()==1) {g.setColour(juce::Colours::white);for(int i=0;i<3;++i)g.drawText("OSC "+juce::String(i+1),40+i*275,326,240,18,juce::Justification::centred);} }
            if(page==2||page==3){g.setFont(juce::FontOptions(11.0f,juce::Font::bold));for(int i=0;i<3;++i){const char* fx[]={"REVERB","DELAY","DISTORTION"};const char* mo[]={"ENVELOPE","LFO","ROUTING"};g.drawText(page==2?fx[i]:mo[i],40+i*275,275,240,20,juce::Justification::centred);} }
            if(page==4){g.setFont(juce::FontOptions(11.0f,juce::Font::bold));g.drawText("RENDER QUALITY",40,280,230,20,juce::Justification::centredLeft);g.drawText("MIDI VELOCITY",310,280,230,20,juce::Justification::centredLeft);g.drawText("UI SCALE",580,280,230,20,juce::Justification::centredLeft);g.setColour(juce::Colour(0xffffa9ac));g.setFont(juce::FontOptions(11.0f));g.drawText("Settings are placeholders and do not affect audio or the interface yet.",40,410,750,25,juce::Justification::centredLeft);}
        }
        g.setColour(juce::Colour(0xffffb1b4));g.setFont(juce::FontOptions(10.0f));g.drawText("BETA 0.3  /  UI TEST   ·   ONLY LEVEL, ATTACK, RELEASE & WAVEFORM AFFECT SOUND",22,510,850,26,juce::Justification::centredLeft);
    }
    void panel(juce::Graphics& g, float x,float y,float w,float h){g.setColour(juce::Colours::black);g.fillRoundedRectangle(x,y,w,h,7);g.setColour(juce::Colour(0xfff12b34));g.drawRoundedRectangle(x,y,w,h,7,1.5f);}
    void resized() override
    {
        previous.setBounds(26,76,48,25);browserButton.setBounds(26,105,48,25);next.setBounds(26,134,48,25);
        for(int i=0;i<5;++i)tabs[(size_t)i].setBounds(16+i*128,184,120,30);
        for(int i=0;i<4;++i)folders[(size_t)i].setBounds(32,230+i*43,180,34);
        int row=0;for(int i=0;i<4;++i)if(presetButtons[(size_t)i].isVisible())presetButtons[(size_t)i].setBounds(250,230+(row++)*44,390,36);
        level.setBounds(78,277,120,130);levelLabel.setBounds(78,411,120,20); attack.setBounds(230,277,120,130);attackLabel.setBounds(230,411,120,20); release.setBounds(382,277,120,130);releaseLabel.setBounds(382,411,120,20);waveLabel.setBounds(565,310,150,20);wave.setBounds(565,340,150,35);
        for(int i=0;i<4;++i)partials[(size_t)i].setBounds(30+i*104,267,98,28);
        module.setBounds(30,301,220,27);poly.setBounds(715,258,65,65);polyLabel.setBounds(785,276,88,20);
        for(int i=0;i<9;++i){ const int col=i/3, sub=i%3; const int x=32+col*275+sub*83; const int y=page==1?350:309;demoKnobs[(size_t)i].setBounds(x,y,76,89);demoLabels[(size_t)i].setBounds(x,y+88,76,18); }
        quality.setBounds(40,310,220,32);midi.setBounds(310,310,220,32);scale.setBounds(580,310,220,32);reset.setBounds(40,365,180,30);
    }
private:
    Vanta& processor; VantaLook look; int page=0,folder=1,partial=0,currentPreset=0; bool browser=false;
    const char* presetNames[4]={"CRIMSON BELL","RED HORIZON","NIGHT CIRCUIT","GLASS SIGNAL"};
    juce::Slider level,attack,release,poly; juce::ComboBox wave,module,quality,midi,scale; juce::Label levelLabel,attackLabel,releaseLabel,waveLabel,polyLabel;
    std::array<juce::TextButton,5> tabs; std::array<juce::TextButton,4> folders,presetButtons,partials; juce::TextButton browserButton,previous,next,reset;
    std::array<juce::Slider,9> demoKnobs; std::array<juce::Label,9> demoLabels;
    juce::AudioProcessorValueTreeState::SliderAttachment levelAttachment,attackAttachment,releaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttachment;
};
juce::AudioProcessorEditor* Vanta::createEditor() { return new VantaEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Vanta(); }
