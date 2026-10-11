#include "../src/Engine.hpp"
#include <cstdio>
#include <string>
using namespace kn;
// helper: run internal play, record trig times of tracks via LED rising edges at fine resolution
static std::vector<std::pair<int,double>> run(Engine* e, double seconds, int block=64){
  std::vector<std::pair<int,double>> hits; std::vector<float> L(block),R(block); uint32_t prev=0; long frame=0;
  // detect triggers by wrapping: use voice LEDs (80 ms) -> too coarse; instead check track outputs? use outLeds edges with short blocks
  for(long f=0; f<seconds*48000; f+=block){ e->process(L.data(),R.data(),block,Transport(),nullptr,0); uint32_t l=e->outLeds; for(int t=0;t<16;t++) if((l>>t&1)&&!(prev>>t&1)) hits.push_back({t,frame/48000.0}); prev=l; frame+=block; }
  return hits;
}
static Engine* fresh(){ Engine* e=new Engine(); e->init(48000); e->global[gPLAY]=1; Pattern& p=e->bank.patterns[0]; p.clear(); p.swing=50;
  for(int t=0;t<16;t++){ e->bank.machine[t]=mGND_SN; for(int i=0;i<8;i++) e->trackParam[t][i]=kMachineDefs[mGND_SN].def[i]; e->trackParam[t][1]=2; } return e; }
int main(){
  const double step=0.125; // 120 bpm 16ths
  { // micro timing: track0 step0 micro +6 (= 1/4 step late)
    Engine* e=fresh(); Pattern& p=e->bank.patterns[0]; p.flags[0][0]=sfTrig; p.micro[0][0]=6; p.flags[1][4]=sfTrig; p.micro[1][4]=-6;
    auto h=run(e,2.1); printf("micro: "); for(auto& x:h) printf("t%d@%.4f(step %.3f) ",x.first+1,x.second,x.second/step); printf("\n"); }
  { // ratchet: 4 hits in step 0 -> LED edges unreliable at 31ms; count voice retriggers via a counter of rising envelope instead
    Engine* e=fresh(); Pattern& p=e->bank.patterns[0]; p.flags[0][0]=sfTrig; p.ratch[0][0]=4; e->trackParam[0][1]=0;
    std::vector<float> L(48000),R(48000); for(int b=0;b+64<=48000;b+=64) e->process(&L[b],&R[b],64,Transport(),nullptr,0);
    // count onsets: amplitude jumps
    float pk=0; for(int i=0;i<24000;i++) pk=std::max(pk,std::fabs(L[i])); printf("ratchet peak %.3f\n",pk);
    int on=(int)e->trigCount[0];
    printf("ratchet x4 in first 0.5 s (4 steps): onsets=%d (expect 4 for the step 0 + 4 for step... only step 0 has trig -> 4)\n",on); }
  { // conditions: 1:2 on track0 step0 (16-step loop) -> plays every other loop; 50%
    Engine* e=fresh(); Pattern& p=e->bank.patterns[0]; p.flags[0][0]=sfTrig; p.cond[0][0]=13; // 1:2
    p.flags[1][0]=sfTrig; p.cond[1][0]=3; // 50%
    p.flags[2][0]=sfTrig; p.cond[2][0]=11; // 1ST
    p.flags[3][0]=sfTrig; p.cond[3][0]=7; // FILL
    auto h=run(e,16.1); int c[4]={0,0,0,0}; for(auto&x:h) if(x.first<4) c[x.first]++;
    printf("conditions over 8 loops: 1:2=%d (expect 4)  50%%=%d (~4)  1ST=%d (expect 1)  FILL(off)=%d (expect 0)\n",c[0],c[1],c[2],c[3]);
    e->fill=true; auto h2=run(e,4.1); int f=0; for(auto&x:h2) if(x.first==3) f++; printf("FILL on: %d hits in 2 loops (expect 2)\n",f); }
  { // choke: track0 long sound, track1 same group triggers at step 4 -> track0 silenced
    Engine* e=fresh(); e->trackParam[0][1]=127; e->trackParam[0][0]=40; e->bank.kit[0][kfChoke]=1; e->bank.kit[1][kfChoke]=1; e->bank.mute[1]=0;
    e->trackParam[1][tpVOL]=0; Pattern& p=e->bank.patterns[0]; p.flags[0][0]=sfTrig; p.flags[1][4]=sfTrig;
    std::vector<float> L(48000),R(48000); for(int b=0;b+64<=48000;b+=64) e->process(&L[b],&R[b],64,Transport(),nullptr,0);
    auto rms=[&](int a,int b2){double s=0; for(int i=a;i<b2;i++) s+=L[i]*L[i]; return std::sqrt(s/(b2-a));};
    printf("choke: rms before %.3f, after choke %.4f\n", rms(12000,24000), rms(26000,40000)); }
  { // separate outputs: track0 -> OUT 2
    Engine* e=fresh(); e->bank.kit[0][kfOut]=1; Pattern& p=e->bank.patterns[0]; p.flags[0][0]=sfTrig; e->trackParam[0][1]=60;
    std::vector<float> o[8]; float* outs[8]; for(int i=0;i<8;i++){o[i].assign(24000,0); outs[i]=o[i].data();}
    for(int b=0;b+64<=24000;b+=64){ float* ob[8]; for(int i=0;i<8;i++) ob[i]=outs[i]+b; e->process(ob,64,Transport(),nullptr,0);} 
    for(int i=0;i<8;i++){ float pk=0; for(float x:o[i]) pk=std::max(pk,std::fabs(x)); printf("out%d %.3f ",i,pk);} printf(" (expect only out2/out3 non-zero)\n"); }
  { // LFO cross-track: track0 LFO1 -> track1 VOL (dest 17), square, full depth; track1 constant sine
    Engine* e=fresh(); e->trackParam[1][1]=127; e->trackParam[1][6]=127; // sustained? GND-SN decay max -> hold
    Pattern& p=e->bank.patterns[0]; p.flags[1][0]=sfTrig; p.trackLen[1]=64; p.length=64;
    e->trackParam[0][tpLFOS]=70; e->trackParam[0][tpLFOD]=127; e->trackParam[0][tpLFOSH]=50; e->trackParam[0][tpLFODST]=lfoDestValue(tpVOL); e->trackParam[0][tpLFOTRK]=16;
    printf("lfo trk index %d dest %d\n", lfoTrackIndex(16), lfoDestIndex(lfoDestValue(tpVOL)));
    std::vector<float> L(96000),R(96000); for(int b=0;b+64<=96000;b+=64) e->process(&L[b],&R[b],64,Transport(),nullptr,0);
    for(int w=0;w<8;w++){ double s=0; for(int i=w*12000;i<(w+1)*12000;i++) s+=L[i]*L[i]; printf("%.3f ",std::sqrt(s/12000)); } printf(" (rms per 0.25s: should alternate)\n"); }
  { // MIDI melodic: semis +12 -> double frequency (zero crossings)
    for(int semis: {0,12}){ Engine* e=fresh(); e->global[gPLAY]=0; e->trackParam[0][1]=80; NoteEvent ne{0,0,1.f,semis};
      std::vector<float> L(9600),R(9600); for(int b=0;b+64<=9600;b+=64) e->process(&L[b],&R[b],64,Transport(),b==0?&ne:nullptr,b==0?1:0);
      int zc=0; for(int i=1000;i<9600;i++) if((L[i-1]<0)!=(L[i]<0)) zc++; printf("semis %d: zc rate %.0f Hz\n",semis,zc*48000.0/(2*8600)); } }
  { // filter env: FENV 127 with low FLTW: brighter at attack
    Engine* e=fresh(); e->global[gPLAY]=0; e->bank.machine[0]=mGND_NS; for(int i=0;i<8;i++) e->trackParam[0][i]=kMachineDefs[mGND_NS].def[i]; e->trackParam[0][2]=127;
    e->trackParam[0][tpFLTW]=30; e->trackParam[0][tpFENV]=127; e->trackParam[0][tpFDEC]=50; NoteEvent ne{0,0,1.f,0};
    std::vector<float> L(48000),R(48000); for(int b=0;b+64<=48000;b+=64) e->process(&L[b],&R[b],64,Transport(),b==0?&ne:nullptr,b==0?1:0);
    auto zcr=[&](int a,int b2){int z=0; for(int i=a+1;i<b2;i++) if((L[i-1]<0)!=(L[i]<0)) z++; return z*48000.0/(2*(b2-a));};
    printf("filter env: brightness (zcr) at start %.0f Hz, later %.0f Hz\n", zcr(200,2000), zcr(30000,40000)); }
}
