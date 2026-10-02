#!/usr/bin/env python3
"""Exercise the real dedicated state buffer with platform API doubles."""
from pathlib import Path
import tempfile,subprocess
r=Path(__file__).resolve().parents[3]
s=(r/"tools/ps5/native-app-heap.c").read_text();s=s[s.index("/* The FBNeo state codec is non-reentrant."):]
pre=r"""
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/mman.h>
#include <string.h>
static int mode, allocs, releases;
static void* mapped;static size_t mapped_bytes;
void pemu_boot_mark(const char*s){puts(s);}
char *pemu_heap_hex(char*out,size_t n){out+=sprintf(out,"%zx",n);return out;}
void *__wrap_malloc(size_t n){return malloc(n);}
void __wrap_free(void*p){assert(!p||p!=mapped);free(p);}
int64_t sceKernelGetDirectMemorySize(void){return 4LL*1024*1024*1024;}
int32_t sceKernelAllocateDirectMemory(int64_t lo,int64_t hi,size_t n,size_t a,int type,int64_t*p){
 assert(!lo&&hi==(4LL*1024*1024*1024)&&n%0x4000==0&&a==0x4000&&type==0);
 if(mode==1)return -1;*p=0x4000;++allocs;return 0;
}
int32_t sceKernelMapDirectMemory(void**p,size_t n,int prot,int flags,int64_t phy,size_t align){
 assert(prot==3&&!flags&&phy==0x4000&&align==0x4000);
 if(mode==2)return -1;
 mapped=mmap(NULL,n,3,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(mapped!=MAP_FAILED);mapped_bytes=n;*p=mapped;return 0;
}
int32_t sceKernelReleaseDirectMemory(int64_t phy,size_t n){assert(phy==0x4000&&n%0x4000==0);++releases;mapped=NULL;return 0;}
"""
post=r"""
int main(){
 for(int i=0;i<4;i++){
  unsigned char*p=pemu_native_state_malloc(0x90c2c64);assert(p&&mapped_bytes>=0x90c2c64);
  memset(p,0xa5,0x90c2c64);assert(p[0x90c2c63]==0xa5);
  assert(!pemu_native_state_malloc(0x90c2c64));assert(state_memory==p);
  pemu_native_state_free(p);assert(!state_memory&&allocs==releases);
 }
 for(mode=1;mode<=2;mode++){assert(!pemu_native_state_malloc(0x90c2c64));assert(!state_memory&&allocs==releases);}
 mode=0;assert(!pemu_native_state_malloc(SIZE_MAX));
 void*p=pemu_native_state_malloc(16);assert(p);pemu_native_state_free(p);pemu_native_state_free(NULL);
 puts("PASS repeated145MiB allocation/use/release; ownership; overflow; allocation and mapping failures");
}
"""
with tempfile.TemporaryDirectory() as tmp:
 exe=str(Path(tmp)/"test")
 subprocess.run(['clang-18','-D_GNU_SOURCE','-std=c11','-fsanitize=address,undefined','-x','c','-o',exe,'-'],input=pre+s+post,text=True,check=True)
 subprocess.run([exe],check=True)
