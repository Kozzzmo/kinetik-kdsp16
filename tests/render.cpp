#include "../src/Engine.hpp"
#include <cstdio>
#include <string>
#include <chrono>
using namespace kn;
static void writeWav(const char* path, const std::vector<float>& L, const std::vector<float>& R, int sr){
  FILE* f=fopen(path,"wb"); uint32_t n=L.size(); uint32_t data=n*4;
  auto w32=[&](uint32_t v){fwrite(&v,4,1,f);}; auto w16=[&](uint16_t v){fwrite(&v,2,1,f);};
  fwrite("RIFF",1,4,f); w32(36+data); fwrite("WAVEfmt ",1,8,f); w32(16); w16(1); w16(2); w32(sr); w32(sr*4); w16(4); w16(16);
  fwrite("data",1,4,f); w32(data);
  for(uint32_t i=0;i<n;i++){ int16_t a=(int16_t)std::max(-32767.f,std::min(32767.f,L[i]*32767)); int16_t b=(int16_t)std::max(-32767.f,std::min(32767.f,R[i]*32767)); fwrite(&a,2,1,f); fwrite(&b,2,1,f);} fclose(f);}
int main(int argc,char**argv){
  const int sr=48000; std::string mode=argc>1?argv[1]:"machines";
  if(mode=="machines"){
    for(int m=1;m<kMachines;m++){
      Engine* e=new Engine(); e->init(sr);
      for(int t=1;t<kTracks;t++) e->bank.mute[t]=1;
      e->bank.machine[0]=m; for(int p=0;p<8;p++) e->trackParam[0][p]=kMachineDefs[m].def[p];
      std::vector<float> L(sr*2),R(sr*2); Transport tp; NoteEvent ne{0,0,1.f};
      for(int b=0;b<sr*2;b+=256){ e->process(&L[b],&R[b],256,tp,b==0?&ne:nullptr,b==0?1:0);}
      float pk=0; bool bad=false; for(int i=0;i<sr*2;i++){ if(!std::isfinite(L[i])) bad=true; pk=std::max(pk,std::fabs(L[i]));}
      int last=0; for(int i=0;i<sr*2;i++) if(std::fabs(L[i])>0.001f) last=i;
      printf("%-8s peak %.3f len %.2fs %s\n",kMachineDefs[m].name,pk,last/(float)sr,bad?"NAN!":"");
      std::string fn=std::string("out/m_")+kMachineDefs[m].name+".wav"; writeWav(fn.c_str(),L,R,sr); delete e;
    }
  } else if(mode=="stress"){
    // all tracks, random params & locks, every machine: check stability and CPU
    Engine* e=new Engine(); e->init(sr); e->global[gPLAY]=1; uint32_t s=1;
    auto rnd=[&](){ s^=s<<13; s^=s>>17; s^=s<<5; return s; };
    for(int pi=0;pi<1;pi++) for(int t=0;t<kTracks;t++) for(int st=0;st<16;st++){ if(rnd()%3==0){ e->bank.patterns[0].flags[t][st]=1|(rnd()%2?2:0)|(rnd()%4==0?4:0); for(int k=0;k<kLockable;k++) if(rnd()%5==0) e->bank.patterns[0].locks[t][st][k]=rnd()%128; } }
    for(int k=0;k<20;k++) e->global[gECHO_TIME+k]=rnd()%128;
    e->global[gECHO_FB]=127; e->global[gREV_SIZE]=127;
    int secs=60; std::vector<float> L(512),R(512); float pk=0; bool bad=false;
    auto t0=std::chrono::steady_clock::now();
    for(int b=0;b<sr*secs;b+=512){
      if(b%(sr*2)==0) for(int t=0;t<kTracks;t++){ e->bank.machine[t]=rnd()%kMachines; for(int p=0;p<kLockable;p++) e->trackParam[t][p]=rnd()%128; e->trackParam[t][tpVOL]=90; }
      e->process(L.data(),R.data(),512,Transport(),nullptr,0);
      for(int i=0;i<512;i++){ if(!std::isfinite(L[i])||!std::isfinite(R[i])) bad=true; pk=std::max(pk,std::fabs(L[i])); }
    }
    double el=std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count();
    printf("stress: peak %.3f %s  cpu %.1f%% of realtime\n",pk,bad?"NAN!":"ok",el/secs*100);
  } else {
    Engine* e=new Engine(); e->bank.initDemo(); e->init(sr); e->global[gPLAY]=1; e->trackParam[1][tpREV]=40; e->trackParam[4][tpDEL]=50;
    int secs=8; std::vector<float> L(sr*secs),R(sr*secs); Transport tp;
    for(int b=0;b<sr*secs;b+=512){ e->process(&L[b],&R[b],512,tp,nullptr,0);}
    float pk=0; for(int i=0;i<sr*secs;i++) pk=std::max(pk,std::fabs(L[i]));
    printf("pattern peak %.3f\n",pk); writeWav("out/pattern.wav",L,R,sr);
  }
}
