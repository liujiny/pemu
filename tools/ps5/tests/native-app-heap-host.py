#!/usr/bin/env python3
"""Actual allocator wrappers: frontend isolation, lazy core pool and failures."""
import subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[3]
s=(root/"tools/ps5/native-app-heap.c").read_text()
pre=r"""
#define _GNU_SOURCE
#include <assert.h>
#include <string.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
static int mode, attempts, releases, maps, fail_alloc, core_frees;
static void *front, *core;
static size_t front_used, core_used;
void pemu_boot_mark(const char *s) { puts(s); }
"""
post=r"""
void *__real_malloc(size_t n){return malloc(n);}
void *__real_calloc(size_t n,size_t m){return calloc(n,m);}
void *__real_realloc(void *p,size_t n){return realloc(p,n);}
void __real_free(void *p){assert(!ps5_heap_owns(p)&&!core_heap_owns(p));free(p);}
int __real_posix_memalign(void **p,size_t a,size_t n){return posix_memalign(p,a,n);}
size_t __real_malloc_usable_size(const void *p){(void)p;return 16;}
int64_t sceKernelGetDirectMemorySize(void){return 2LL*1024*1024*1024;}
int32_t sceKernelAllocateDirectMemory(int64_t lo,int64_t hi,size_t n,size_t a,int type,int64_t *p){
 ++attempts;assert(lo==0&&hi>=(int64_t)n&&n==256u*1024*1024&&a==0x4000&&type==0);
 if(mode==1)return -1;*p=0x4000;return 0;
}
int32_t sceKernelMapDirectMemory(void **p,size_t n,int prot,int flags,int64_t phy,size_t a){
 ++maps;assert(prot==3&&!flags&&phy==0x4000&&a==0x4000);
 if(mode==2)return -1;
 core=mmap(NULL,n,3,MAP_PRIVATE|MAP_ANON,-1,0);assert(core!=MAP_FAILED);*p=core;return 0;
}
int32_t sceKernelReleaseDirectMemory(int64_t p,size_t n){assert(p==0x4000&&n==256u*1024*1024);++releases;return 0;}
void *sceLibcMspaceCreate(const char *name,void *base,size_t n,unsigned flags){
 assert(n==256u*1024*1024&&!flags);
 if(!strcmp(name,"PS5-OpenGL")){front=base;assert(!attempts);return base;}
 assert(!strcmp(name,"pEMU-Core")&&base==core);
 return mode==3?NULL:base;
}
static void *alloc(void *space,size_t n,size_t a){
 if(fail_alloc||n>256u*1024*1024)return NULL;
 assert(space==front||space==core);size_t *used=space==front?&front_used:&core_used;
 *used=(*used+sizeof(size_t)+a-1)&~(a-1);
 assert(n<=256u*1024*1024-*used);
 void *p=(char*)space+*used;*used+=n?n:1;memcpy((char*)p-sizeof(n),&n,sizeof(n));return p;
}
void *sceLibcMspaceMalloc(void *s,size_t n){return alloc(s,n,16);}
void *sceLibcMspaceCalloc(void *s,size_t n,size_t m){if(m&&n>SIZE_MAX/m)return NULL;void*p=alloc(s,n*m,16);if(p)memset(p,0,n*m);return p;}
size_t sceLibcMspaceMallocUsableSize(const void*p){assert(ps5_heap_owns(p)||core_heap_owns(p));size_t n;memcpy(&n,(char*)p-sizeof(n),sizeof(n));return n;}
void sceLibcMspaceFree(void*s,void*p){assert((s==front&&ps5_heap_owns(p))||(s==core&&core_heap_owns(p)));if(s==core)++core_frees;}
void *sceLibcMspaceRealloc(void*s,void*p,size_t n){if(!n||fail_alloc)return NULL;size_t old=sceLibcMspaceMallocUsableSize(p);void*q=alloc(s,n,16);if(q)memcpy(q,p,old<n?old:n);return q;}
int sceLibcMspacePosixMemalign(void*s,void**p,size_t a,size_t n){void*q=alloc(s,n,a);if(!q)return ENOMEM;*p=q;return 0;}
int main(int argc,char**argv){
 assert(argc==2);mode=atoi(argv[1]);
 void *ui=__wrap_malloc(32);assert(ui&&ps5_heap_owns(ui)&&!attempts);
 void *small=pemu_native_core_malloc(64);assert(ps5_heap_owns(small)&&!attempts);
 void *large=pemu_native_core_malloc(160u*1024*1024);assert(large&&attempts==1);
 assert(core_heap_owns(large)==(mode==0));
 if(mode){assert(ps5_heap_owns(large));assert(releases==(mode>=2));}
 void *texture=__wrap_malloc(2u*1024*1024);assert(ps5_heap_owns(texture));
 assert(!core_heap_owns(texture));
 memset(large,0x5a,32);fail_alloc=1;
 assert(!__wrap_realloc(large,170u*1024*1024));
 assert(((unsigned char*)large)[0]==0x5a);fail_alloc=0;
 void *q=__wrap_realloc(large,64);assert(q&&((unsigned char*)q)[0]==0x5a);
 assert(core_heap_owns(q)==(mode==0));__wrap_free(q);__wrap_free(large);
 assert(__wrap_malloc_usable_size(texture)==2u*1024*1024);
 if(mode==0){void*state=pemu_native_core_malloc(32u*1024*1024);assert(core_heap_owns(state));__wrap_free(state);assert(core_frees==3);}
 __wrap_free(texture);__wrap_free(small);__wrap_free(ui);
 void*foreign=__real_malloc(16);foreign=__wrap_realloc(foreign,32);__wrap_free(foreign);
 if(mode!=3&&core)assert(munmap(core,256u*1024*1024)==0);
 assert(munmap(front,256u*1024*1024)==0);
 puts("PASS main heap isolation; lazy core pool; ownership; OOM preserves allocation; cleanup/fallback");
}
"""
with tempfile.TemporaryDirectory() as tmp:
 exe=str(Path(tmp)/"heap-test")
 subprocess.run(["clang-18","-std=c11","-fsanitize=address,undefined","-g","-x","c","-o",exe,"-"],input=pre+s+post,text=True,check=True)
 for mode in range(4):subprocess.run([exe,str(mode)],check=True)
