#include "../src/Engine.hpp"
#include "../src/Presets.hpp"
#include <cstdio>
#include <string>
using namespace kn;
static void writeWav(const char* path, const std::vector<float>& L, const std::vector<float>& R, int sr){
  FILE* f=fopen(path,"wb"); uint32_t n=L.size(); uint32_t data=n*4;
  auto w32=[&](uint32_t v){fwrite(&v,4,1,f);}; auto w16=[&](uint16_t v){fwrite(&v,2,1,f);};
  fwrite("RIFF",1,4,f); w32(36+data); fwrite("WAVEfmt ",1,8,f); w32(16); w16(1); w16(2); w32(sr); w32(sr*4); w16(4); w16(16);
  fwrite("data",1,4,f); w32(data);
  for(uint32_t i=0;i<n;i++){ int16_t a=(int16_t)std::max(-32767.f,std::min(32767.f,L[i]*32767)); int16_t b=(int16_t)std::max(-32767.f,std::min(32767.f,R[i]*32767)); fwrite(&a,2,1,f); fwrite(&b,2,1,f);} fclose(f);}
static void apply(Engine* e, const float* v){ for(int t=0;t<kTracks;t++) for(int p=0;p<kTrackParams;p++) e->trackParam[t][p]=v[t*kTrackParams+p]; for(int g=0;g<gOUT_STEP;g++) e->global[g]=v[kGlobalBase+g]; }
int main(int argc,char**argv){
  const int sr=48000;
  int which = argc>1? atoi(argv[1]) : -1;
  for(int i=0;i<kNumPresets;i++){
    if(which>=0 && i!=which) continue;
    std::vector<float> v(kTotalParams,0.f); Bank* bank=new Bank(); buildPreset(i,v.data(),*bank);
    for(int pat=0; pat<2; pat++){
      Engine* e=new Engine(); e->init(sr); apply(e,v.data()); e->bank=*bank; e->global[gPATTERN]=pat; e->global[gPLAY]=1; if(getenv("NODRIVE")) e->global[gDRIVE]=0;
      double bpm=v[kGlobalBase+gTEMPO]; int secs=(int)std::ceil(4*bank->patterns[pat].length/4.0*60/bpm*0.25*4)+1; if(secs<6)secs=6;
      std::vector<float> L(sr*secs),R(sr*secs); bool bad=false; float pk=0; double rms=0;
      for(int b=0;b<sr*secs;b+=480){ e->process(&L[b],&R[b],480,Transport(),nullptr,0); }
      for(int k=0;k<sr*secs;k++){ if(!std::isfinite(L[k])) bad=true; pk=std::max(pk,std::fabs(L[k])); rms+=L[k]*L[k]; }
      rms=std::sqrt(rms/(sr*secs));
      printf("%-20s A%02d bpm %3.0f len %2d  peak %.2f rms %.3f %s\n",kPresets[i].name,pat+1,bpm,bank->patterns[pat].length,pk,rms,bad?"NAN!":"");
      if(pat==0){ std::string fn="out/preset_"+std::to_string(i)+".wav"; writeWav(fn.c_str(),L,R,sr);} delete e;
    }
    delete bank;
  }
  // randomizer smoke test
  RandGen r(42); float worst=0; bool bad=false;
  for(int it=0; it<40; it++){
    std::vector<float> v(kTotalParams,0.f); Bank* bank=new Bank(); buildPreset(0,v.data(),*bank);
    randomizeKit(v.data(), *bank, r); randomizePattern(bank->patterns[0], *bank, r);
    Engine* e=new Engine(); e->init(sr); apply(e,v.data()); e->bank=*bank; e->global[gPLAY]=1;
    std::vector<float> L(4800),R(4800);
    for(int b=0;b<sr*4;b+=4800){ e->process(L.data(),R.data(),4800,Transport(),nullptr,0); for(float x:L){ if(!std::isfinite(x)) bad=true; worst=std::max(worst,std::fabs(x)); } }
    delete e; delete bank;
  }
  printf("random kits x40: worst peak %.2f %s\n", worst, bad?"NAN!":"ok");
}
