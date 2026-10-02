# PS5 present / exit r16

## Evidence

R15 console: color and sound confirmed working. Gameplay stable33.34ms/frame with ~26ms swap, core/update ~4.7ms. Menu has additional55–60ms draw cost (~85–90 submits/frame). Quit log reaches main returned then runtime exit begin, with no signal-handler output; exact failing callback not identified. The prior infinite catchReturnFromMain hook is no longer used.

## Changes

Application main already deletes skin/config/UI and returns. CRT now uses _Exit(status), bypassing libc atexit/static/loader callbacks which fail after application cleanup on this native setup. Kernel process teardown reclaims remaining allocations. This intentionally forgoes pending static destructor effects; application configuration persistence remains in main's existing cleanup. Confirm PS5 return-to-shell on hardware.

Existing runtime build did not set PS5_GPU_PRESENT_BATCH. Enable it when rebuilding native runtime, queuing flip after GPU frame draws instead of waiting for GPU completion before CPU flip submission. Keep marker/resource ordering. Change GPU-present finish wait from vblank-based checks to100us sleeps plus monotonic2s deadline; require matching marker and pending=0 before completion. Platform errors and timeout preserve non-completion state and caller fail-stop behavior. Idle/shutdown wait logic otherwise unchanged. Do not treat this as disabling vsync or GPU fences.

No menu batching change: its draw cost remains a separate next optimization. No prediction of60FPS before console test.

## Validation

Actual runtime_gpu_present_finish compiled with ASan/UBSan and API doubles: completion state becoming visible17.1ms after submission observed at17.1ms (avoids waiting to33.3ms), wrong buffer rejected, get-status and pending errors propagated,2s timeout leaves buffer ownership intact. No hardware timing claim.

Runtime and native title rebuild passed. ELF contains ps5_agc_gate2_batch_present and _Exit import. Packaged/unsigned LOAD bytes equal; mapped sections validated against LLVM. Other title assets match r15, ZIP CRC and embedded eboot verified.

## Baseline / reproduction / rollback

Git baseline9482d136666450743a0c6e19fb96000085b6a273 plus dirty r15. Baseline eboot822cc521cb4769706b95b25128124d5b2d6815f780e2905c2107573a7d6e4f27. No commits/pushes.

Apply patches/ps5/ps5-present-completion-poll.patch to external ps5-opengl. In its tests/ps5 directory build with native-app.mk PS5_GPU_PRESENT_BATCH=1 runtime and existing PS5_PAYLOAD_SDK. Stage rebuilt libps5_opengl_core33.a only (materialize thin archive if needed); installed SDK untouched. Rebuild native title with existing archive list. Runtime configuration recorded in evidence.

Evidence /home/humor/ps5dev/logs/present-exit-r16-20261002 contains old source/archive/config backups, state-machine test, builds, matching ELF and result hashes. Source rollback restores baseline-r15 native-app-crt.cpp, ps5_agc_native_runtime.c, runtime build flags and staged archive. Binary rollback retained r15 package; its known Quit issue returns.

pEMU PS5 present-exit-r16-20261002

安装：PPSA99998 合并覆盖 /data/homebrew/PPSA99998/，保留 ROM/配置/存档。
保留 r15 已经实机确认的声音和颜色修复。

Quit：main 完成应用对象清理后调用 _Exit，不再执行出错的 libc 退出回调。
显示：启用 PS5_GPU_PRESENT_BATCH，翻页随 GPU 绘制提交；匹配翻页标记并确认无 pending
后才允许复用缓冲。以100微秒休眠轮询完成状态，避免错过状态更新再多等一次垂直刷新。
2秒超时和平台错误均保留失败路径，不提前释放 GPU 资源。

测试：同一游戏进入游戏后游玩约15秒，记录实际FPS；呼出菜单、返回主界面观察反应；最后Quit。
提供完整 pemu_boot.log。退出新日志应为 native process exit begin。
目标是处理固定约30帧的显示等待。菜单原有55–60ms绘制成本未在本次解决，不能保证菜单60帧。
此包通过编译、等待状态机边界测试、FSELF/ELF/ZIP验证；实际FPS和退出行为待实机确认。
回退：覆盖之前的 av-r15 包（其 Quit 仍有已知崩溃）。

{
  "zip": "/home/humor/ps5dev/build/native-pfbneo/dist/pfbneo-ps5-present-exit-r16-20261002.zip",
  "zip_sha256": "51275c0ed370b45f13da16ac5eae5ce5249cfc289e180628f83dcbedc9d63bc3",
  "eboot_sha256": "b9bf9e97091af4377d300039b86d2191ba21a43b8cdc23e6ead0c4edd8e7ef85",
  "console": "pending"
}
