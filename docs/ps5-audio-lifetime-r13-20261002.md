# PS5 audio buffer lifetime fix r13 — 2026-10-02

## Console evidence

R12 touchpad input confirmed by user; GAME_INPUT reaches the core. Two immediate post-load SIGSEGVs have runtime RIP 0x19c93c4, null vtable access +0x158. Matching r12 LLVM ELF (subtract 0x400000) identifies C2DObject::onUpdate, with Renderer::onUpdate / UiMain::onUpdate in the stack. A later menu-opening SIGBUS has runtime RIP 0x1b39e4a in ps5_set_vertex_buffers / pipe_reference_described; incoming resource pointer resembles paired16-bit samples (0xfbc1fbc1fbbbfbbb). These are failure sites, not proof of independent UI/driver bugs.

## Confirmed source defect

PFBAUiEmu::load frees the driver-init pBurnSoundOut, leaves the pointer dangling, creates the host audio object, and allocates a replacement only if audio->isAvailable(). With unavailable host audio, BurnDrvFrame's sound emulation can write into a freed allocation reused for UI/driver objects. Silence can zero a vtable; duplicated stereo samples can produce the patterned invalid pointer observed. This is a strong explanation consistent with both logs, not yet a console-confirmed attribution of every crash. Host audio availability was not logged in r12.

## Change

Clear pBurnSoundOut immediately after free. Always allocate a new zeroed mix buffer using the Audio object's sample count and rate, independent of host device availability. The core therefore retains normal sound-chip execution and a valid output buffer even when no host sound is available. On temporary buffer allocation failure, abort before DrvInit; on final buffer failure, stop the initialized driver and show an error. Existing stop() frees and clears the owned buffer. Log host audio availability and valid mix-buffer ownership.

No GPU/SDL/core emulation changes in this revision. R12 input mapping and r11 allocator/rendering retained. Native marker audio-lifetime-r13-20261002.

## Verification

Test tools/ps5/tests/native-audio-lifetime-host.py extracts the actual handoff code into a host harness with platform boundaries. Old r12 code under ASan reproduces heap-use-after-free on first simulated stereo frame with unavailable audio. New code passes available/unavailable device writes and allocation-failure teardown with ASan/UBSan. Tests do not establish hardware sound availability or console crash resolution.

Native build and SELF checks passed. Packaged ELF verified against LLVM mappings/sections, and LOAD bytes compared to unsigned ELF. Non-eboot/non-build-identity title resources match r12. ZIP CRC and embedded eboot verified during packaging. Console test pending.

## Baseline / artifacts / rollback

Git baseline 9482d136666450743a0c6e19fb96000085b6a273 plus existing dirty r12 work. No commit/push. Baseline eboot SHA256 148dbeddaea07bace5cab2c21600089263e4a35f240761ff2af36c53ebbc42f3.

Evidence: /home/humor/ps5dev/logs/audio-lifetime-r13-20261002/ (source backups, baseline archive, incremental patch, original ASan failure, passing test harness, matching ELF files, build logs, result.json).

Package: /home/humor/ps5dev/build/native-pfbneo/dist/pfbneo-ps5-audio-lifetime-r13-20261002.zip

New eboot SHA256: 32205f3a86f86ce45506db41e207bf0560aa4a9549a62fa2df6e08cf195c8447.

Binary rollback: retained input-abort-r12 package, which contains the confirmed lifetime defect. Source rollback: restore only files from baseline-r12, preserve unrelated work, restage baseline libpfbneo_ps5_native.a, rebuild native title. Tests are standalone additions.

## Console test

Merge PPSA99998 into /data/homebrew/PPSA99998/, preserve user files. Start88game,1942,1944 separately; coin with touchpad, Start with Options. Repeatedly open menu with Options+touchpad and resume; then switch games. Return complete boot log and report sound availability. If audio device unavailable is logged, this patch only guarantees valid mixing memory; host audio initialization requires a separate investigation.
