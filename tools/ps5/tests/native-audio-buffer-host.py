#!/usr/bin/env python3
"""Run the actual PS5 fill callback against the actual SampleBuffer."""
from pathlib import Path
import tempfile,subprocess
r=Path(__file__).resolve().parents[3]
s=(r/"external/libcross2d/source/platforms/sdl2/sdl2_audio.cpp").read_text();s=s[s.index("void SDL2Audio::fillBufferedAudio"):];s=s[:s.rindex("#endif")]
pre=r"""
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include "audio_buffer.h"
using Uint8=uint8_t;
class SDL2Audio {
public:
 c2d::SampleBuffer b{7680}; bool m_priming=true; bool locked=false;
 void lock(){assert(!locked);locked=true;}void unlock(){assert(locked);locked=false;}
 int getSampleBufferQueued(){assert(locked);return b.space_filled();}
 int getSamples(){return 800;}int getChannels(){return 2;}
 c2d::SampleBuffer*getSampleBuffer(){assert(locked);return &b;}
 void fillBufferedAudio(Uint8*,int);
};
"""
post=r"""
int main(){
 SDL2Audio a;int16_t data[1600],out[1536];for(int i=0;i<1600;i++)data[i]=100+i;
 assert(a.b.push(data,1600));a.fillBufferedAudio((Uint8*)out,sizeof(out));
 for(auto x:out)assert(x==0);assert(a.b.space_filled()==1600);
 assert(a.b.push(data,1600));a.fillBufferedAudio((Uint8*)out,sizeof(out));
 for(int i=0;i<1536;i++)assert(out[i]==100+i);
 a.fillBufferedAudio((Uint8*)out,sizeof(out));assert(a.b.space_filled()==128);
 a.fillBufferedAudio((Uint8*)out,sizeof(out));
 for(int i=0;i<128;i++)assert(out[i]==data[1472+i]);
 for(int i=128;i<1536;i++)assert(out[i]==0);
 assert(a.m_priming&&a.b.space_filled()==0);
 assert(a.b.push(data,1600));a.fillBufferedAudio((Uint8*)out,sizeof(out));
 for(auto x:out)assert(x==0);assert(a.b.space_filled()==1600);
 assert(a.b.push(data,1600));a.fillBufferedAudio((Uint8*)out,sizeof(out));
 for(int i=0;i<1536;i++)assert(out[i]==data[i]);
 assert(!a.m_priming&&!a.locked);
 puts("PASS two-frame priming; sample order; short callback preserves tail; rebuffer recovery");
}
"""
with tempfile.TemporaryDirectory() as tmp:
 exe=str(Path(tmp)/"test")
 subprocess.run(['clang++-18','-std=c++17','-fsanitize=address,undefined','-I'+str(r/'external/libcross2d/include/cross2d/skeleton'),'-x','c++','-o',exe,'-'],input=pre+s+post,text=True,check=True)
 subprocess.run([exe],check=True)
