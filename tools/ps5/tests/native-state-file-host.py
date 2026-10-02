#!/usr/bin/env python3
"""Check the actual frontend save wrapper preserves an old slot on failure."""
import subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[3]
s=(root/"src/cores/pfbneo/sources/pfbneo_ui_menu_state.cpp").read_text()
s=s[s.index("bool PFBAUIStateMenu::saveStateCore"):].replace("PFBAUIStateMenu::", "")
pre=r"""
#include <cstdio>
#include <cerrno>
#include <string>
#include <fstream>
#include <cassert>
#include <sys/stat.h>
static void stateMark(const char*){}
static int fail;
int BurnStateSave(char *path,int) {
 if(fail==2) return 0; // no state data: must not commit a stale temporary
 FILE *f=fopen(path,"wb"); assert(f); fputs(fail?"partial":"new state",f); fclose(f);
 return fail?1:0;
}
std::string read(const char *p){std::ifstream f(p);return std::string((std::istreambuf_iterator<char>(f)),{});}
"""
post=r"""
int main(){
 {std::ofstream f("slot.state");f<<"old state";}
 fail=1;assert(!saveStateCore("slot.state") && read("slot.state")=="old state");
 {std::ofstream f("slot.state.tmp");f<<"stale state";}
 fail=2;assert(!saveStateCore("slot.state") && read("slot.state")=="old state");
 fail=0;assert(saveStateCore("slot.state") && read("slot.state")=="new state");
 assert(mkdir("destination",0700)==0);
 assert(!saveStateCore("destination")); // rename fails; temp must be cleaned
 assert(fopen("destination.tmp","rb")==nullptr);
 puts("PASS failed/empty save retains old slot; success replaces; rename failure cleans temp");
}
"""
with tempfile.TemporaryDirectory() as tmp:
 exe=str(Path(tmp)/"test")
 subprocess.run(["clang++-18","-std=c++17","-fsanitize=address,undefined","-x","c++","-o",exe,"-"],input=pre+s+post,text=True,check=True)
 subprocess.run([exe],cwd=tmp,check=True)
