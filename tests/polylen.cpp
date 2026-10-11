#include "../src/Engine.hpp"
#include <cstdio>
using namespace kn;
// track 1 length 3 with a trig on step 0, track 2 length 4 trig on step 0: check trigger steps
int main(){
  Engine* e=new Engine(); e->init(48000); e->global[gPLAY]=1;
  Pattern& p=e->bank.patterns[0]; p.clear(); p.length=16;
  for(int t=0;t<16;t++) e->bank.mute[t]=1; e->bank.mute[0]=0; e->bank.mute[2]=0;
  e->bank.machine[0]=mGND_SN; e->bank.machine[2]=mTRX_CL;
  p.trackLen[0]=3; p.flags[0][0]=sfTrig; p.trackLen[2]=4; p.flags[2][0]=sfTrig;
  for(int i=0;i<8;i++){ e->trackParam[0][i]=kMachineDefs[mGND_SN].def[i]; e->trackParam[2][i]=kMachineDefs[mTRX_CL].def[i]; }
  e->trackParam[0][1]=5; // short
  std::vector<float> L(240),R(240); uint32_t prevLeds=0; int frame=0; double stepSamples=0.125*48000;
  for(int b=0;b<48000*3/240;b++){ e->process(L.data(),R.data(),240,Transport(),nullptr,0);
    uint32_t l=e->outLeds; for(int t:{0,2}) if((l>>t&1)&&!(prevLeds>>t&1)) printf("t%d@%ld ",t+1,std::lround(frame/stepSamples)); prevLeds=l; frame+=240; }
  printf("\n");
}
