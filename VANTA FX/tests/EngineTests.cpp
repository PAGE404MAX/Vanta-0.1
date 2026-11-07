#include "Engine.h"
#include <iostream>
#include <cstdlib>
void check(bool b,const char* m){if(!b){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
void testDelay(){
 const int rate=48000;float v[16]{};const auto& spec=vfx::specs()[7];for(size_t i=0;i<spec.controls.size();++i)v[i]=spec.controls[i].initial;
 auto set=[&](const char* name,float value){for(size_t i=0;i<spec.controls.size();++i)if(std::strcmp(name,spec.controls[i].name)==0)v[i]=value;};
 set("Mod depth",0);set("Feedback",50);set("High cut",20000);set("Low cut",10);set("Diffusion",0);
 auto render=[&](bool bypass){vfx::Engine dsp;dsp.prepare(rate);float l=0,r=0;for(int n=0;n<rate;++n)dsp.tick(l=0,r=0,7,v,0,1,1,0,bypass);std::vector<float> out(size_t(rate)*2);for(int n=0;n<rate*2;++n){l=n==0?1.f:0.f;r=0;dsp.tick(l,r,7,v,0,1,1,0,bypass);out[size_t(n)]=l;check(std::isfinite(l)&&std::isfinite(r),"native delay finite");}return out;};
 auto out=render(false);auto energy=[&](const std::vector<float>& data,float a,float b){double e=0;for(int n=int(a*rate);n<int(b*rate);++n)e+=data[size_t(n)]*data[size_t(n)];return e;};
 const auto first=energy(out,.299f,.32f),second=energy(out,.599f,.62f);check(first>.01,"native delay produces a 300ms echo");check(second>first*.1&&second<first*.4,"50 percent feedback produces decaying repeats");check(energy(out,.01f,.29f)<1e-6,"wet signal has no early leak");
 set("Feedback",0);auto noFeedback=render(false);check(energy(noFeedback,.599f,.62f)<1e-7,"zero feedback removes second echo");
 auto dry=render(true);check(dry[0]>.99f,"settled bypass is unity dry");check(energy(dry,.299f,.32f)<1e-7,"bypass removes echoes");
 set("Delay",600);auto longer=render(false);check(energy(longer,.299f,.32f)<1e-7&&energy(longer,.599f,.62f)>.01,"delay knob changes repeat timing");
}
int main(){testDelay();for(double sr:{44100.,48000.,96000.})for(int e=0;e<9;++e){vfx::Engine dsp;dsp.prepare(sr);float values[16]{};auto& s=vfx::specs()[size_t(e)];for(size_t i=0;i<s.controls.size();++i)values[i]=s.controls[i].initial;double energy=0;for(int n=0;n<int(sr*2);++n){float l=n==0?1.f:0,r=0;dsp.tick(l,r,e,values,0,0,1,0,false);check(std::isfinite(l)&&std::isfinite(r),"finite output");if(n>200)energy+=l*l+r*r;}check(energy>1e-10,"nonzero effect tail");dsp.reset();for(int n=0;n<1000;++n){float l=0,r=0;dsp.tick(l,r,e,values,0,0,1,0,false);check(l==0&&r==0,"reset clears tail");}}std::cout<<"All nine engine smoke tests passed at 44.1/48/96 kHz\n";}
