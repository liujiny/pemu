#!/usr/bin/env python3
"""Exercise actual PS5 worker with host SDL threads, including init failures."""
from pathlib import Path
import subprocess, tempfile, shlex
root = Path(__file__).resolve().parents[3]
source = r'''#include <SDL.h>
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cerrno>
static int fail_stage;
static SDL_mutex *make_mutex() { return fail_stage == 1 ? nullptr : SDL_CreateMutex(); }
static SDL_cond *make_cond() { return fail_stage == 2 ? nullptr : SDL_CreateCond(); }
static SDL_Thread *make_thread(SDL_ThreadFunction f, const char *n, void *p) {
    return fail_stage == 3 ? nullptr : SDL_CreateThread(f,n,p);
}
#define SDL_CreateMutex make_mutex
#define SDL_CreateCond make_cond
#define SDL_CreateThread make_thread
#include "epic12_ps5_thread.h"
#undef SDL_CreateMutex
#undef SDL_CreateCond
#undef SDL_CreateThread
extern "C" void pemu_boot_mark(const char *) {}
static std::atomic<int> count{0}, entered{0};
static SDL_threadID callback_id;
static bool slow;
static std::atomic<bool> gate{false}, release{false};
static void callback() {
    entered = 1;
    if (gate) while (!release.load()) SDL_Delay(0);
    if (slow) SDL_Delay(3);
    callback_id = SDL_ThreadID();
    ++count;
}
int main() {
    assert(SDL_Init(0) == 0);
    const auto main_id = SDL_ThreadID();
    for (int failure=1; failure<=3; ++failure) {
        fail_stage=failure; thready.init(callback);
        assert(!thready.thread && !thready.mutex && !thready.condition);
        int before=count; thready.notify(); thready.notify_wait();
        assert(count==before+1 && callback_id==main_id); thready.exit();
    }
    fail_stage=0;
    for (int pass=0; pass<3; ++pass) {
        count=0; slow=false; thready.init(callback); assert(thready.thread);
        for (int i=0;i<180;++i) thready.notify();
        assert(count==180 && callback_id==main_id);
        for (int i=0;i<2000;++i) thready.notify();
        thready.notify_wait(); assert(count==2180 && callback_id!=main_id);
        // Prove main and worker can overlap, independent of host scheduling.
        entered=0; gate=true; release=false; thready.notify();
        while (!entered.load()) SDL_Delay(0);
        assert(count==2180); release=true; thready.notify_wait();
        assert(count==2181 && callback_id!=main_id); gate=false; count=2180;
        // Completion barriers wait for the delayed callback.
        slow=true; entered=0; thready.notify();
        while (!entered.load()) SDL_Delay(0);
        thready.set_threading(0); assert(count==2181);
        thready.notify(); assert(count==2182 && callback_id==main_id);
        thready.set_threading(1); thready.notify(); thready.scan();
        assert(count==2183 && callback_id!=main_id);
        thready.notify(); thready.reset(); assert(count==2184);
        slow=false; for(int i=0;i<180;++i) thready.notify();
        slow=true; thready.notify(); thready.exit();
        assert(count==2365 && !thready.thread && !thready.mutex && !thready.condition);
        thready.exit();
    }
    SDL_Quit(); puts("PASS real SDL worker ordering; 6000 jobs; synchronous warmup/fallback; disable, scan, reset, teardown; reinit; all init failures");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src=Path(tmp)/'test.cpp'; src.write_text(source); exe=Path(tmp)/'test'
    flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','sdl2'],text=True))
    subprocess.run(['clang++-18','-std=c++17','-fsanitize=address,undefined','-g','-I'+str(root/'external/cores/FBNeo/src/burn/devices'),str(src),'-o',str(exe)]+flags,check=True)
    subprocess.run([str(exe)],check=True,timeout=40)
