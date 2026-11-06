#include "Engine.h"
#include <iostream>
#include <cstdlib>
void check(bool b,const char* m){if(!b){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
int main(){for(double sr:{44100.,48000.,96000.})for(int e=0;e<9;++e){vfx::Engine dsp;dsp.prepare(sr);float values[16]{};auto& s=vfx::specs()[size_t(e)];for(size_t i=0;i<s.controls.size();++i)values[i]=s.controls[i].initial;double energy=0;for(int n=0;n<int(sr*2);++n){float l=n==0?1.f:0,r=0;dsp.tick(l,r,e,values,0,0,1,0,false);check(std::isfinite(l)&&std::isfinite(r),"finite output");if(n>200)energy+=l*l+r*r;}check(energy>1e-10,"nonzero effect tail");dsp.reset();for(int n=0;n<1000;++n){float l=0,r=0;dsp.tick(l,r,e,values,0,0,1,0,false);check(l==0&&r==0,"reset clears tail");}}std::cout<<"All nine engine smoke tests passed at 44.1/48/96 kHz\n";}
