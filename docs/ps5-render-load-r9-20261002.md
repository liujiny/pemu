# PS5 rendering / common game-load diagnostics r9 — 2026-10-02

## Evidence

User report: every game crashes on launch; menu input remains delayed.
Steady windows: 9 frames / 5.355377..5.405434 seconds; ~1.67 FPS.
Frame average 594831..600410 us; max 600607..600610 us.
Input poll 47..49 us; max poll gap 602485..610986 us.
Input dispatch 0..22 us outside loading.
Swap average 28290..28919 us.
Loading window: 6 frames / 18885627 us; frame max 15882610 us;
dispatch max 15814359 us. Time within loading callbacks can include nested renders.
Fault signal 11, address 0; runtime RIP 0x19cc976; LLVM image RIP 0x15cc976 (bias 0x400000).
Resolved to std::__1::__tree_balance_after_insert in Font.cpp.
Instruction movq (%rax),%rdx, with RAX=0: invalid tree ancestor access.
Font.cpp symbol COMDAT can be shared by other std::map instantiations; without caller stack this does NOT establish the specific map owner or originating corruption.
This is not evidence that low frame rate directly causes the crash. Need load-stage evidence and finer timing.

## Changes and limits

This is a diagnostic package, not a confirmed performance or crash fix. R8 buffered pad and button-edge logic remains. Render appearance, core emulation and GPU runtime are unchanged.

The existing bounded timing hook adds update, clear, draw traversal, glDrawArrays wall time and pre-draw GL setup. `submit` means the CPU wall time of glDrawArrays, not a GPU timestamp; it may include driver work and synchronization. `submits` and `*_total_us` are per reporting window; update/clear/draw are per completed stage averages. Nested load/progress renders mean these are not additive independent timing categories. Stages use outermost nesting per category. First timing window now begins with the actual frontend loop, excluding pre-loop input initialization.

New GAME_LOAD checkpoints bracket configuration, temporary audio, driver initialization, game audio, video creation/attachment, cheat menu and UI transition. There is no return-address stack in the existing crash dump: the exact caller/owner of the shared tree function remains unknown. The user's report that all games crash prioritizes common frontend loading and runtime state over a single ROM driver.

## Baseline, tests, rollback

Baseline Git remains 9482d136666450743a0c6e19fb96000085b6a273 plus existing dirty r8 work. R8 eboot SHA256 ecf89993847ad3c991a6983e83957ad0b1d29e6f0922e07d1ab2d0f266d4793f. Baseline sources and libraries saved in evidence/baseline-r8, exact incremental source-r9.patch saved alongside.

Host tests with actual extracted source functions and SDL/platform boundary doubles pass ASan/UBSan, including nested stage totals, exclusion of startup polls, errno preservation and 12-report stop. Application/core, cross2d and UI archives rebuilt in pemu-launch-r7 and all three staged. Native title builds and FSELF integrity pass. Extracted final FSELF passes ELF mapping validation, and all LOAD bytes match the converted unsigned ELF. ZIP CRC/hash checked; 64 title files; other title resources byte-identical to r8. These tests do not establish console speed or crash resolution.

Restore pfbneo-ps5-input-r8-20261002.zip to roll back on console. Source rollback should restore only files from baseline-r8; preserve unrelated existing modifications. No commits or pushes performed.

## Test

Install merged PPSA99998 at /data/homebrew/PPSA99998, preserving config/ROM/save data. Spend ~15 seconds in menu, move selection a few times, then start any game while timing collection is still active. Provide the final render-load-r9-20261002 log session, including INPUT_PERF, RENDER_PERF, GAME_LOAD and fault records.

Prioritize the measured dominant stage next: draw-call duration/count and setup vs UI update or clear. Loading checkpoints identify the next crash boundary. Do not infer that frame slowness causes map corruption or that a graphics API replacement fixes it.

## Artifacts

- zip: `/home/humor/ps5dev/build/native-pfbneo/dist/pfbneo-ps5-render-load-r9-20261002.zip`
- zip_sha256: `80f6c180eee76fa8d49b9ee718cd5e65479dc2ded6a3a5542e016d4a9c684e3b`
- eboot_sha256: `cf0235f763e57dc0d72824cf86b3b051c14894ead6c4225df944969d2ee546ab`
- eboot_bytes: `103097699`

Evidence: `/home/humor/ps5dev/logs/render-load-r9-20261002/`.
