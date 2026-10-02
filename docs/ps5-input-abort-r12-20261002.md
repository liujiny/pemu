# PS5 input / abort diagnostics r12 — 2026-10-02

## Evidence

R11 console starts successfully with 256MiB heap; measured allocation failures=0. 88game enters gameplay, but user reports no effective controls. Menu draw cost improved from ~570ms/frame to ~50–123ms/frame depending on draw count. Gameplay measures ~33.34ms/frame, largely presentation wait (~27ms). These are distinct from the two SIGABRT failures after GAME_LOAD load returned for 1942/1944. The logs alone do not establish the abort caller or a rendering cause.

## Changes

- Actual native SDL driver: touchpad press also emits button5; enable Back mapping to button5. Existing touchpad/button4 remains. pEMU defaults map Back to coin and Options to Start. Options+Back opens menu. No change to existing custom configuration. This fixes a confirmed missing coin/menu input, not proof of all input symptoms resolved.
- Log up to32 player-one mapped state transitions from core input polling, preserving errno. No state mutation. This verifies what reaches the core if gameplay still ignores inputs.
- Install crash handlers before querying main-thread stack bounds with pthread attributes. On crash, PS5 4.03 context RSP word31 is used only if aligned and within captured bounds, with up to128 words. Unknown/other-thread stacks skip. No allocation/stdio/unwinder in handler. Signal/default termination retained. Stack words require offline symbolication and are not guaranteed frames.
- Keep r11 allocator and rendering unchanged. Abort cause remains unconfirmed; this build gathers caller evidence.

## Baseline and reproduction

Git 9482d136666450743a0c6e19fb96000085b6a273 plus extensive dirty work. Baseline eboot e789289cadd229ba6e59c7b6b7060ba4c39f581194d744e7962a5f13d709582e. No commit/push.

SDL source: /home/humor/ps5dev/ps5-opengl/build/native-sdl2/SDL/src/joystick/ps5/SDL_ps5joystick.c. Apply patches/ps5/sdl-back-button.patch after the existing buffered-input patch. Rebuild SDL2-static, stage cmake/sdl/libSDL2.a (not installed SDK archive), rebuild pfbneo_ps5_native_core and stage its archive, run tools/ps5/native-build-pfbneo.sh with the existing archive list. Stage copy and all changed source baselines are in the evidence folder. native-crash-report.c and native-input-probe.c are staged by native-build-pfbneo.sh.

## Validation

Actual SDL producer/consumer boundary tests passed under ASan/UBSan, including touchpad press/release emitting both physical indices4 and5. Core input logger tests: transition-only, correct hexadecimal output,32-record limit, errno preservation. Actual crash report boundary tests: invalid/unaligned RSP skipped, end-of-stack truncation,128-word cap, original signal retained. These use platform doubles, not console hardware.

Native rebuild and SELF integrity passed. Extracted packaged ELF verified against LLVM:5 LOAD mappings,11 preserved source sections. All packaged LOAD bytes/mappings equal unsigned converted ELF. Non-eboot/non-identity title files equal r11. ZIP CRC and embedded eboot verified. Console validation pending, including availability of added stack query imports on4.03.

## Test and rollback

See README-TEST.txt in the package. First use touchpad coin then Options Start in88game; test movement/actions. Separately reproduce1942/1944 and return full log with game name. Restore r11 package for binary rollback. Source rollback uses baseline-r11 copies and external SDL source/archive; preserve unrelated work.

## Artifacts

- zip: `/home/humor/ps5dev/build/native-pfbneo/dist/pfbneo-ps5-input-abort-r12-20261002.zip`
- zip_sha256: `d01caab421f55b9dd470dd1a344a52aea22bc59c066377faf5fc945c871302e0`
- eboot_sha256: `148dbeddaea07bace5cab2c21600089263e4a35f240761ff2af36c53ebbc42f3`
- eboot_bytes: `103103011`
- console_test: `pending`

Evidence: `/home/humor/ps5dev/logs/input-abort-r12-20261002`.
