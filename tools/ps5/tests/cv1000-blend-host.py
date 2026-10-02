#!/usr/bin/env python3
"""Salvia differential fixture against pEMU's original EPIC12 draw routines."""
import subprocess,tempfile,sys
from pathlib import Path
r=Path(__file__).resolve().parents[3]
d=r/"external/cores/FBNeo/src/burn/devices"
s=(d/"epic12.cpp").read_text();s=s[s.index("static UINT8 epic12_device_colrtable"):s.index("static UINT8 *dips")].replace('#include "epic12.h"','')
pre='#include <cstdint>\n#include <cstdlib>\nusing UINT8=uint8_t;using UINT16=uint16_t;using UINT32=uint32_t;using UINT64=uint64_t;using INT32=int32_t;\n#include "rectangle.h"\nstatic UINT32 *m_bitmaps;static UINT64 epic12_device_blit_delay;\n#define EPIC12_BLIT_TEST\n'
test=(Path(__file__).parent/"cv1000-blend-fixture.cpp").read_text()
with tempfile.TemporaryDirectory() as tmp:
 exe=str(Path(tmp)/"blend")
 subprocess.run(['clang++-18','-std=c++17']+(['-O3','-DEPIC12_BENCH'] if '--bench' in sys.argv else ['-O2','-fsanitize=address,undefined'])+['-Wno-ignored-qualifiers','-I'+str(d),'-x','c++','-o',exe,'-'],input=pre+s+test,text=True,check=True)
 subprocess.run([exe],check=True)
