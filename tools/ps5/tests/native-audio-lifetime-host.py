"""Compile the actual load handoff against host audio/UI boundaries.
No hardware audio claim. ASan checks core writes with host device unavailable.
"""
from pathlib import Path
import argparse, subprocess
parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--out', type=Path, required=True)
parser.add_argument('--expect-uaf', action='store_true')
a = parser.parse_args(); a.out.mkdir(parents=True, exist_ok=True)
s = a.source.read_text()
fragment = s[s.index('    delete (aud);', s.index('LOAD_TRACE("driver and cheats ok")')):s.index('    audio_sync = !bForce60Hz;')]
code = r"""
#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cstdio>
using INT16 = int16_t;
static bool device_available, allocation_fails;
static bool stopped;
static INT16 *pBurnSoundOut;
static int nBurnSoundRate, nBurnSoundLen, nFramesEmulated, nFramesRendered, nCurrentFrame, nBurnFPS=6000;
static void *test_calloc(size_t n,size_t s) {return allocation_fails ? nullptr : calloc(n,s);}
struct Audio {
 bool isAvailable() {return device_available;}
 int getSampleRate() {return 48000;}
 int getSamples() {return 800;}
 int getSamplesSize() {return 3200;}
 static int toSamples(int rate,float fps) {return int(rate/fps);}
};
static Audio *audio;
static void addAudio(int,int) {audio=new Audio;}
static void stop() {stopped=true;free(pBurnSoundOut);pBurnSoundOut=nullptr;delete audio;audio=nullptr;}
enum class Visibility {Hidden};
struct UI {void setVisibility(Visibility) {} void show(const char*,const char*,const char*) {}};
struct Main {UI ui;UI *getUiProgressBox(){return &ui;} UI *getUiMessageBox(){return &ui;}};
static Main main_ui, *pMain=&main_ui;
#define LOAD_TRACE(x) ((void)("GAME_LOAD " x))
#define calloc test_calloc
static int handoff() {
 auto *aud=new Audio;int audio_freq=48000;
 pBurnSoundOut=(INT16*)malloc(3200);
 memset(pBurnSoundOut,0x55,3200);
""" + fragment + r"""
 return 0;
}
#undef calloc
int main(int argc,char **argv) {
 device_available=argc>1 && argv[1][0]=='1';
 allocation_fails=argc>1 && argv[1][0]=='2';
 int result=handoff();
 if(allocation_fails) {assert(result==-1 && stopped && !pBurnSoundOut);return 0;}
 assert(result==0 && pBurnSoundOut);
 // Simulate the sound chips' first frame; 16-bit stereo, including silence.
 for(int i=0;i<1600;++i) pBurnSoundOut[i]=(INT16)(i/2-1000);
 stop();assert(!pBurnSoundOut);return 0;
}
"""
c = a.out/'audio-handoff.cpp'; c.write_text(code)
exe=a.out/'audio-handoff'
subprocess.run(['clang++-18','-std=c++17','-fsanitize=address,undefined','-g',str(c),'-o',str(exe)],check=True)
if a.expect_uaf:
 p=subprocess.run([str(exe),'0'],capture_output=True,text=True)
 (a.out/'asan-original.log').write_text(p.stdout+p.stderr)
 assert p.returncode and 'heap-use-after-free' in p.stderr
 print('CONFIRMED original handoff heap-use-after-free when audio device unavailable')
else:
 for mode in ['0','1','2']:subprocess.run([str(exe),mode],check=True)
 print('PASS actual handoff: unavailable/available device core writes; allocation failure stops with null pointer')
