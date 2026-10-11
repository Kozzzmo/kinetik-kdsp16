#include "../src/Engine.hpp"
#include "../src/Presets.hpp"
#include <cstdio>
using namespace kn;
int main(int argc,char**argv){
  const int sr=48000; int i=atoi(argv[1]);
  std::vector<float> v(kTotalParams,0.f); Bank* bank=new Bank(); buildPreset(i,v.data(),*bank);
  for(int solo=0;solo<kTracks;solo++){
    if(bank->machine[solo]==0) continue;
    Engine* e=new Engine(); e->init(sr);
    for(int t=0;t<kTracks;t++) for(int p=0;p<kTrackParams;p++) e->trackParam[t][p]=v[t*kTrackParams+p];
    for(int g=0;g<gOUT_STEP;g++) e->global[g]=v[kGlobalBase+g];
    e->bank=*bank; for(int t=0;t<kTracks;t++) e->bank.mute[t]= t==solo?0:1; e->global[gPLAY]=1; e->global[gDRIVE]=0; e->global[gREV_LEV]=0; e->global[gECHO_LEV]=0;
    std::vector<float> L(480),R(480); float pk=0;
    for(int b=0;b<sr*6;b+=480){ e->process(L.data(),R.data(),480,Transport(),nullptr,0); for(float x:L) pk=std::max(pk,std::fabs(x)); }
    printf("t%02d %-8s peak %.2f\n",solo+1,kMachineDefs[bank->machine[solo]].name,pk); delete e;
  }
}
