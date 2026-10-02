# PS5 native pFBNeo stable source

Accepted on PS5 firmware 4.03 through ShadowMount, title PPSA99998, 2026-10-02.
The user confirmed the r21 build works after testing r20 save/load successfully.
No numerical gain is inferred from the short r21 acceptance message.

## Included fixes

- Native title loader/ELF relocation, CRT initialization and native libc filesystem imports.
- Readable boot/crash reports and normal system-service Quit.
- Owned FBNeo audio buffers, native SDL audio output and brief underrun buffering.
- Correct default framebuffer color channels, deferred draw submission and 60 Hz presentation.
- CPU-writeback direct-memory core arena; separate temporary buffers for large Cave save/load.
- Portable Salvia fixed-alpha blending and a PS5 CV1000 blitter worker, with save/reset/draw/exit barriers.
- Routine input/render/CV1000 performance logging disabled in r21.

The PS5 renderer remains OpenGL. Xbox PowerPC SH3 DRC code is not included.
The worker is CV1000-only, controlled by Thread Blitter; it preserves one ordered
command stream and falls back to synchronous execution if thread creation fails.

## Source layout and restoration

This branch stores submodule changes as patches, as the original PS5 pipeline did.
The gitlinks stay pinned to their existing upstream/fork commits. Every modified
FBNeo/libcross2d source file, including new headers, is in these complete patches:

- patches/ps5/fbneo-ps5-native-video-owner.patch
- patches/ps5/libcross2d-ps5-opengl.patch

After cloning with submodules, apply them once (the helper is idempotent):

```bash
git submodule update --init --recursive
bash tools/ps5/apply-source-patches.sh
```

Main-repository frontend, CRT, allocator and filesystem changes are ordinary source
files. Compiler/platform dependencies remain external; no SDK, ROM, logs or binaries
are committed. Read the per-iteration reports for hashes, tests and rollback details.

## External dependency patches used for the tested build

PS5 OpenGL base: 7f9bfabdddb187a11e4401058eba8c9e55194d0a.
Native boilerplate base: 722f2227a8bb6fa2229120546995b6562552c752.
SDL base: 8c56053f13ca13a0c050de613706ff69eb615836.
Payload SDK: public v0.42. Use the installed LLVM 18/21 tools described in the workflow.

Apply ps5-opengl-capability-audit.patch and ps5-present-completion-poll.patch to
ps5-opengl before building. The native runtime must also be built with:

```bash
make -C "$PS5_OPENGL_SOURCE/tests/ps5" -f native-app.mk runtime \
  PS5_GPU_PRESENT_BATCH=1 PS5_PAYLOAD_SDK="$PS5_PAYLOAD_SDK"
```

Stage the resulting build/core33-native-runtime/libps5_opengl_core33.a in the native
link inputs; preserve the rest of the SDK archives. A thin archive must be materialized
before moving it. This step is required for the tested presentation behavior.

The ps5-opengl SDL builder extracts a pinned SDL archive and ignores dirty cache files.
Therefore, patch its extracted build/native-sdl2/SDL source with sdl-buffered-input.patch,
sdl-back-button.patch and sdl-native-audio-lifecycle.patch. Patch the generated native
CMakeLists.txt with sdl-native-audio-enable.patch. Reconfigure, build and install that
SDL build before staging libSDL2.a. These patches are committed; old unpatched SDK
archives will not reproduce input/audio fixes merely by rebuilding pEMU.

The native-build-pfbneo.sh wrapper applies the boilerplate build, loader and PLT patches
and copies the current CRT/heap/filesystem sources into the isolated staging directory.
Use tools/ps5/build-local-native-package.sh with the existing dependency prefixes and
PEMU_BUILD_ROOT to stage/link PPSA99998. The historical GitHub workflow is not itself
an assertion that a freshly generated dependency SDK has all local runtime overrides.

## Verification and tested artifact

Cross-compilation, SELF/ELF mapping and LOAD byte checks passed. Host tests cover large
state buffers, safe slot replacement, real SDL worker lifecycle/ordering, input mapping,
color conversion and audio buffer ownership. Hardware FPS is not claimed from host tests.

r21 eboot SHA256:
`db53d246017c486268d3f7c23c30ce2dced30d1c102311bb15d27a381204b23d`

r21 test ZIP SHA256:
`f7c72ac7a2a2c4d51cd72922159a8d3bd535daee00e649742330db67c22d5600`

The package remains local at
`/home/humor/ps5dev/build/native-pfbneo/dist/pfbneo-ps5-cv-thread-r21-20261002.zip`.
The preceding r20 package is retained locally as the console rollback build.

## Source rollback

The preceding tested r20 source is commit `2a2f2ef`. Reverting the next r21
commit removes the PS5 blitter worker and restores the diagnostic r20 hooks.
The Git history is independent of the local binary packages listed above.
