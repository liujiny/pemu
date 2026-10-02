#!/usr/bin/env python3
"""Exercise actual FBNeo statec.cpp with a 64 MiB mock scanned device."""
import subprocess, tempfile
from pathlib import Path
root = Path(__file__).resolve().parents[3]
source = (root / "external/cores/FBNeo/src/burner/statec.cpp").read_text().replace('#include "burnint.h"', '')
pre = r"""
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <vector>
using INT32=int32_t; using UINT32=uint32_t; using UINT8=uint8_t;
#define __cdecl
#define _T(x) x
#define bprintf(...) ((void)0)
#define ACB_EEPROM 1
#define ACB_FULLSCAN 2
#define ACB_READ 4
#define ACB_WRITE 8
#define ACB_NVRAM 16
struct BurnArea {void *Data; UINT32 nLen; char *szName;};
INT32 (*BurnAcb)(BurnArea*);
bool bWithEEPROM=false, fail_alloc=false, overflow=false;
int writes=0;
std::vector<UINT8> ram(64u*1024*1024);
void BurnAreaScan(int flags,void*) {
 if(flags&ACB_WRITE) ++writes;
 BurnArea a{ram.data(),overflow?UINT32_MAX:UINT32(ram.size()),(char*)"Cave RAM"};
 BurnAcb(&a);
}
void *state_malloc(size_t n) {return fail_alloc ? nullptr : std::malloc(n);}
#define malloc state_malloc
"""
post = r"""
#undef malloc
int main() {
 for(size_t i=0;i<ram.size();++i) ram[i]=UINT8(i*17+3);
 UINT8 *save=nullptr; INT32 size=0;
 assert(BurnStateCompress(&save,&size,1)==0 && save && size==INT32(ram.size()+20));
 memset(ram.data(),0,ram.size());
 assert(BurnStateDecompress(save,size,1)==0);
 for(size_t i=0;i<ram.size();++i) assert(ram[i]==UINT8(i*17+3));
 writes=0;
 assert(BurnStateDecompress(nullptr,0,1)!=0);
 for(int n=0;n<20;++n) assert(BurnStateDecompress(save,n,1)!=0);
 assert(BurnStateDecompress(save,size-1,1)!=0);
 UINT32 old, bad=UINT32_MAX; memcpy(&old,save+12,4); memcpy(save+12,&bad,4);
 assert(BurnStateDecompress(save,size,1)!=0 && writes==0);
 memcpy(save+12,&old,4);
 save[0]^=1; assert(BurnStateDecompress(save,size,1)!=0 && writes==0); save[0]^=1;
 free(save); save=(UINT8*)1; size=123; fail_alloc=true;
 assert(BurnStateCompress(&save,&size,1)!=0 && !save && size==0);
 fail_alloc=false; overflow=true;
 assert(BurnStateCompress(&save,&size,1)!=0 && !save && size==0);
 puts("PASS 64 MiB roundtrip; allocation failure; overflow; short/corrupt input rejected before write scan");
}
"""
with tempfile.TemporaryDirectory() as tmp:
    exe=str(Path(tmp)/"state-test")
    subprocess.run(["clang++-18","-std=c++17","-fsanitize=address,undefined","-g","-x","c++","-o",exe,"-"],input=pre+source+post,text=True,check=True)
    subprocess.run([exe],check=True)
