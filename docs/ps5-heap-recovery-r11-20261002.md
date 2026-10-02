# PS5 heap startup recovery r11 — 2026-10-02

## Evidence / scope

User r10 log, twice: constructors -> HEAP mmap failed; using libc fallback -> io create begin -> native data path /app0/ -> signal 11 at null, before UI. The enlarged 512 MiB anonymous mapping was rejected. This is an r10 regression. Exact mmap errno was not logged in r10; a specific kernel quota or limit is not established by those logs alone. CPU anonymous memory and direct GPU memory are distinct allocation paths in the local reference implementation; total PS5 RAM is not an anonymous mmap budget.

Baseline Git 9482d136666450743a0c6e19fb96000085b6a273 plus existing dirty r10 changes. Baseline eboot d21891752ded647fea28af6f2fb3d834ecab8dfc9a28b3d037831ab0ed36a308. Exact changed files preserved under baseline-r10; source-r11.patch retained. No commit/push.

## Change

Only app heap initialization and build identity change. Try 256 MiB, then 128 MiB (the size that reached the UI in r9). Log failed candidate bytes and errno. If mspace creation fails, unmap that candidate before the next attempt. Publish base, mspace, actual capacity only after successful initialization. Pointer ownership and stats use the actual published capacity. If both candidates fail, log and _Exit before entering frontend, rather than continuing on the libc private heap. Existing temporary/reentrant allocation routing during initialization is unchanged.

R10 triangle-list upload remains byte-for-byte unchanged. No core, GPU runtime, shader or SDL input changes. This is startup recovery, not proof that either budget satisfies every ROM. If hardware falls back to 128 MiB, prior game OOM may remain; use allocation telemetry to guide a separate direct-memory-backed application allocator rather than blindly increasing anonymous mapping size again.

## Validation

Allocator host tests use actual wrapper source with platform doubles and ASan/UBSan. Pass cases: 256 MiB initialization, first mapping rejected then 128 MiB success, both mappings rejected (controlled early exit), both mspace creations rejected (both candidates unmapped and controlled exit), actual-capacity ownership boundaries, foreign pointer routing, reentrancy, calloc/realloc/alignment/failure accounting. Host tests do not establish console memory availability.

Native build and eboot/libc integrity pass. Extracted FSELF ELF has five validated load mappings and 11 preserved source sections; all LOAD bytes equal converted unsigned ELF. ZIP CRC and embedded eboot hash validated. Other title resources match r10. Console startup pending.

## Test / rollback

Merge PPSA99998 into /data/homebrew/PPSA99998/, preserving user files. Check r11 identity and HEAP ready capacity. A failed 256 MiB attempt followed by ready 128 MiB is successful fallback. Spend ~15 seconds in menu, test movement, then start the same ROM and return full log.

The retained r9 package is the last console-confirmed menu baseline. R10 has a known pre-UI regression and is not the recommended fallback. Source rollback restores only baseline-r10 changed files; preserve unrelated work.

## Artifacts

- zip: `/home/humor/ps5dev/build/native-pfbneo/dist/pfbneo-ps5-heap-recovery-r11-20261002.zip`
- zip_sha256: `33968d2b213b500467f61bd149ad3af904e18bba4c4e22c2186af45b1164a560`
- eboot_sha256: `e789289cadd229ba6e59c7b6b7060ba4c39f581194d744e7962a5f13d709582e`
- eboot_bytes: `103102115`

Evidence: `/home/humor/ps5dev/logs/heap-recovery-r11-20261002/`.
