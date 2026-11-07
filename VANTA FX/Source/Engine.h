#pragma once
#include "Specs.h"
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstring>
namespace vfx {
// Independent, bounded delay network. Storage is allocated only in prepare().
class Engine {
 struct Line { std::vector<float> data; size_t pos=0; float read(float delay) const { const float d=std::clamp(delay,1.0f,float(data.size()-2));float p=float(pos)-d;if(p<0)p+=float(data.size());auto i=size_t(p);float f=p-float(i);return data[i]*(1-f)+data[(i+1)%data.size()]*f; } void push(float x){data[pos]=x;pos=(pos+1)%data.size();} };
 std::array<Line,8> lines; std::array<float,8> low{},high{},dc{};double sr=44100,phase=0;float mix=.5f,trim=1;
 float pitchPhase=0; std::array<Line,2> pitch;
 std::array<Line,4> diffuser;float delayTime=.3f,delayFeedback=.7f;int lastEngine=-1;
 static float safe(float x){return std::isfinite(x)?std::clamp(x,-4.f,4.f):0.f;}
public:
 void prepare(double rate){sr=rate;for(auto& l:lines){l.data.assign(size_t(sr*3)+8,0);l.pos=0;}for(auto& l:pitch){l.data.assign(size_t(sr*.15)+8,0);l.pos=0;}for(auto& l:diffuser){l.data.assign(size_t(sr*.15)+8,0);l.pos=0;}reset();}
 void reset(){for(auto& l:lines){std::fill(l.data.begin(),l.data.end(),0);l.pos=0;}for(auto& l:pitch){std::fill(l.data.begin(),l.data.end(),0);l.pos=0;}for(auto& l:diffuser){std::fill(l.data.begin(),l.data.end(),0);l.pos=0;}low.fill(0);high.fill(0);dc.fill(0);phase=0;pitchPhase=0;lastEngine=-1;}
 void tick(float& l,float& r,int engine,const float* values,int mode,int color,float wet,float db,bool bypass){
  if(lines[0].data.empty())return;
  engine=std::clamp(engine,0,8);const auto& spec=specs()[size_t(engine)];
  auto get=[&](const char* name,float fallback){for(size_t i=0;i<spec.controls.size();++i)if(std::strcmp(name,spec.controls[i].name)==0)return values[i];return fallback;};
  const float inL=std::isfinite(l)?l:0,inR=std::isfinite(r)?r:0;
  if(engine==7){
   // Native counterpart of the auditioner's stereo delay: independent dry path,
   // interpolated repeats, filtered feedback, wet mid/side width and output trim.
   const float smooth=1-std::exp(-1.f/float(sr*.025));
   if(lastEngine!=7){delayTime=get("Delay",300)*.001f;delayFeedback=get("Feedback",70)*.01f;}lastEngine=7;
   delayTime+=smooth*(get("Delay",300)*.001f-delayTime);delayFeedback+=smooth*(get("Feedback",70)*.01f-delayFeedback);
   phase+=6.283185307179586*get("Mod rate",.5f)/sr;if(phase>6.283185307179586)phase-=6.283185307179586;
   const float age=get("Age",50)*.01f,spread=get("Spread",0)*.001f;
   const float cutoff=std::min(get("High cut",20000),float(sr*.45))*(mode==1?1-age*.65f:mode==2?.55f-age*.4f:1);
   const float lpCoeff=1-std::exp(-6.2831853f*std::max(100.f,cutoff)/float(sr));
   const float hpCoeff=1-std::exp(-6.2831853f*get("Low cut",10)/float(sr));
   const float diffusion=get("Diffusion",0)*.01f,diffSize=get("Diff size",100)*.01f;
   const float drive=std::pow(10.f,get("Drive",0)/20.f)*(mode==1?1+age*2:mode==2?1+age*3:1);
   float outputs[2]{};
   for(int ch=0;ch<2;++ch){
    float modulation=std::sin(float(phase)+float(ch)*1.5707963f)*get("Mod depth",50)*.00002f;
    if(mode!=0)modulation+=std::sin(float(phase)*.31f+ch)*age*.0005f;
    float tap=lines[size_t(ch)].read(std::clamp(delayTime+(ch==0?-spread*.5f:spread*.5f)+modulation,.001f,2.9f)*float(sr));
    low[size_t(ch)]+=lpCoeff*(tap-low[size_t(ch)]);high[size_t(ch)]+=hpCoeff*(low[size_t(ch)]-high[size_t(ch)]);tap=low[size_t(ch)]-high[size_t(ch)];
    float diffuse=tap;
    for(int stage=0;stage<2;++stage){auto& ap=diffuser[size_t(ch*2+stage)];float delayed=ap.read(float(sr)*(.003f+diffSize*(.011f+stage*.007f+ch*.0017f)));float y=delayed-.5f*diffuse;ap.push(safe(diffuse+.5f*y));diffuse=y;}
    tap=tap*(1-diffusion)+diffuse*diffusion;
    if(mode==3){auto& grain=pitch[size_t(ch)];grain.push(tap);float pp=pitchPhase,qq=std::fmod(pp+.5f,1.f);tap=grain.read(1+pp*float(sr*.06))*std::pow(std::sin(pp*3.14159265f),2)+grain.read(1+qq*float(sr*.06))*std::pow(std::sin(qq*3.14159265f),2);}
    float feedback=tap;if(drive>1.0001f)feedback=std::tanh(feedback*drive)/std::sqrt(drive);
    // Character varies saturation/damping without changing the dry source.
    if(color==0&&mode!=0)feedback=std::tanh(feedback*(1+age))/std::sqrt(1+age);
    if(color==2)feedback*=.95f;
    lines[size_t(ch)].push(safe((ch==0?inL:inR)+feedback*std::clamp(delayFeedback,0.f,.95f)));outputs[ch]=tap;
   }
   pitchPhase-=float(1/(sr*.06));pitchPhase-=std::floor(pitchPhase);
   float mid=(outputs[0]+outputs[1])*.5f,side=(outputs[0]-outputs[1])*.5f*get("Width",100)*.01f;
   mix+=smooth*((bypass?0.f:std::clamp(wet,0.f,1.f))-mix);trim+=smooth*(std::pow(10.f,std::clamp(db,-24.f,6.f)/20)-trim);
   l=safe((inL*(1-mix)+(mid+side)*mix)*trim);r=safe((inR*(1-mix)+(mid-side)*mix)*trim);return;
  }
  lastEngine=engine;

  const float rate=get("Mod rate",get("Rate",get("Slow rate",.5f)));
  phase+=6.283185307179586*rate/sr;if(phase>6.283185307179586)phase-=6.283185307179586;
  const bool delay=engine==1||engine==2||engine==3||engine==7;
  float decay=get("Decay",3),size=get("Size",50)*.01f;
  float base=delay?get("Delay",10)*.001f:.022f+size*.035f;
  if(engine==2)base=(get("Manual",0)+2)*.001f;
  if(engine==6)base=get("Delay",300)*.001f*(.2f+get("Warp",0)*.008f);
  if(mode>0&&engine==1)base=60.f/120.f/(mode==1?1:mode==2?2:mode==3?1.3333f:4);
  float fb=delay?get("Feedback",30)*.01f:std::pow(.001f,(base*2)/std::max(.1f,decay));
  if(engine==6)fb=get("Feedback",50)*.01f;
  fb=std::clamp(fb,-.95f,.97f);
  float cutoff=get("High cut",get("High freq",10000));if(color==1)cutoff=std::min(cutoff,4500.f);
  float a=1-std::exp(-6.2831853f*std::clamp(cutoff,100.f,float(sr*.45))/float(sr));
  float hp=1-std::exp(-6.2831853f*get("Low cut",10)/float(sr));
  float dep=get("Mod depth",get("Depth",get("Slow depth",30)))*.00004f;
  std::array<float,8> taps{};float sum=0;
  for(size_t k=0;k<8;++k){float mod=std::sin(float(phase)+float(k)*.8f)*dep;float time=base*(delay?1.f:1.f+float(k)*.173f+mode*.07f)+mod;if(engine==2)time=base+get("Depth",10)*.001f*(.5f+.5f*std::sin(float(phase)+k*.5f));taps[k]=lines[k].read(std::max(1.f,time*float(sr)));low[k]+=a*(taps[k]-low[k]);high[k]+=hp*(low[k]-high[k]);taps[k]=low[k]-high[k];sum+=taps[k];}
  float outL=0,outR=0;
  for(size_t k=0;k<8;++k){float input=(k%2==0?inL:inR);float feedback=delay?taps[k]:taps[k]-sum*.25f;
   if(engine==0){const size_t ch=k%2;float shift=get("Shift",12);float window=float(sr*.06);float p=std::fmod(pitchPhase+float(k%2)*.5f,1.f);float q=std::fmod(p+.5f,1.f);feedback=pitch[ch].read(1+p*window)*std::pow(std::sin(p*3.14159265f),2)+pitch[ch].read(1+q*window)*std::pow(std::sin(q*3.14159265f),2);(void)shift;}
   if(engine==1)feedback*=std::cos(float(phase)+float(k)*.2f); // replaced by quadrature shifter below in next DSP iteration
   if(engine==7){float drive=1+get("Drive",0)*.15f;feedback=std::tanh(feedback*drive)/std::tanh(drive);}
   lines[k].push(safe(input*.25f+feedback*fb));if(k%2==0)outL+=taps[k]*.5f;else outR+=taps[k]*.5f;
  }
  pitch[0].push(safe(sum*.25f));pitch[1].push(safe((taps[1]+taps[3]+taps[5]+taps[7])*.5f));pitchPhase+=float((1-std::pow(2.,get("Shift",12)/12.))/ (sr*.06));pitchPhase-=std::floor(pitchPhase);
  float width=get("Width",100)*.01f;float mid=(outL+outR)*.5f,side=(outL-outR)*.5f*width;outL=mid+side;outR=mid-side;
  const float smooth=1-std::exp(-1.f/float(sr*.01));mix+=smooth*((bypass?0.f:std::clamp(wet,0.f,1.f))-mix);trim+=smooth*(std::pow(10.f,std::clamp(db,-24.f,6.f)/20)-trim);
  l=safe((inL*(1-mix)+outL*mix)*trim);r=safe((inR*(1-mix)+outR*mix)*trim);
 }
};
}
