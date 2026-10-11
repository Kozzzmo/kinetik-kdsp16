#include "../src/Engine.hpp"
#include <cstdio>
using namespace kn;
int main(){
  const int sr=44100; Engine* e=new Engine(); e->bank.initDemo(); e->init(sr);
  // single kick on every quarter, no swing; everything else muted
  for(int t=1;t<kTracks;t++) e->bank.mute[t]=1;
  e->bank.patterns[0].swing=50; e->trackParam[0][1]=10;
  double bpm=128; int64_t frame=0; std::vector<float> L(1024),R(1024); std::vector<double> on;
  int sizes[]={441,512,97,1024,333};
  bool prevStep=false;
  for(int i=0;i<3000;i++){
    int n=sizes[i%5]; Transport tp; tp.hostValid=true; tp.hostPlaying=true; tp.bpm=bpm;
    tp.beat=frame*bpm/(60.0*sr);
    if(i==1500){ frame=0; tp.beat=0; } // loop jump back to start
    e->process(L.data(),R.data(),n,tp,nullptr,0);
    for(int k=0;k<n;k++){ bool hit=std::fabs(L[k])>0.02f; if(hit&&!prevStep){ on.push_back((frame+k)*bpm/(60.0*sr)); } prevStep=std::fabs(L[k])>0.0005f; }
    frame+=n;
  }
  double maxErr=0; int cnt=0; for(double b:on){ double err=std::fabs(b-std::round(b)); if(err<0.2){maxErr=std::max(maxErr,err);cnt++;} }
  printf("onsets %zu, on-beat %d, max error %.5f beats (%.2f ms)\n",on.size(),cnt,maxErr,maxErr*60000/bpm);
  for(size_t i=0;i<8&&i<on.size();i++) printf("%.3f ",on[i]); printf("\n");
}
