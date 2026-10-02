# Cave memory, state and exit r17

> R19 correction: the earlier “cached” label was wrong. Allocation type 3 is WC_GARLIC (write combining), not CPU write-back caching. R19 changes the core-only pool to type 0 (WB_ONION). Prior host mocks validated routing but did not validate memory-cache performance.

Baseline: 9482d136666450743a0c6e19fb96000085b6a273 plus dirty r16; no commit/push.
Baseline eboot b9bf9e97091af4377d300039b86d2191ba21a43b8cdc23e6ead0c4edd8e7ef85.
R16 console confirmed color/audio/input and approx60FPS gameplay/menu. Quit still fails
at native process exit begin. Cave loads show 152–160MiB allocation failure with 256MiB heap.

pEMU PS5 cave-state-exit-r17-20261002
安装：把 PPSA99998 中的文件合并覆盖 /data/homebrew/PPSA99998/，保留 ROM、配置和存档。
保留 r16 已实机确认的显示等待、颜色和声音修复，未切换 Vulkan。

内存：优先使用 512 MiB CPU-cached direct memory 的 mspace，失败回退原 256/128 MiB
匿名内存方案。启动日志应显示 HEAP ready capacity=512MiB source=direct cached。
不是重试此前失败的 512 MiB 匿名 mmap。主机验证无法确认 4.03 的实际直接内存可用预算。

存档：修复状态缓冲 malloc 失败后写空指针；长度溢出、截断和非法块长度检查；
读档文件长度与实际文件大小检查；保存报告写入/关闭错误。使用同目录 .tmp 文件，
成功后 rename 替换原槽位，失败保留已有存档。菜单增加 SAVE FAILED / LOAD FAILED，
日志增加 STATE save/load begin/ok/failed。保持 FBNeo 的原存档布局。
退出：应用清理完成后请求 sceSystemServiceLoadExec("exit", nullptr)。若接受则等待系统
结束进程；若拒绝，日志记录失败并走 _Exit 回退（该回退在 r16 仍有崩溃问题）。

已验证：交叉编译、SELF完整性、ELF映射/LOAD内容、ZIP CRC；ASan/UBSan 下
64 MiB状态往返、OOM、长度溢出、损坏/截断输入、保存失败保留旧文件、rename失败清理；
直接内存申请/映射/mspace失败回退及分配器归属测试。平台API使用测试替身。
未验证：实机 Cave 游戏完整运行及其设备状态恢复，直接内存性能，正常返回 PS5 桌面。

实机测试：
1. 启动之前加载失败的 Cave 游戏，观察是否成功并记录 ROM 名称。
2. 游玩中保存空槽位，继续一段时间后读取，确认关卡、位置、声音和操作恢复。
3. 返回主界面重新进入同一游戏再读取该槽位，确认可恢复。
4. 主界面 Quit，记录是否正常返回系统，提供 pemu_boot.log。
回退：覆盖保留的 present-exit-r16 包。

## Build and rollback

Build pfbneo_ps5_native_core and cross2dui in /home/humor/ps5dev/build/pemu-launch-r7; stage those two archives, run tools/ps5/native-build-pfbneo.sh with existing archive list. Keep r16 OpenGL runtime PS5_GPU_PRESENT_BATCH=1. All source backups and previous staged archives are in /home/humor/ps5dev/logs/cave-state-exit-r17-20261002/baseline-r16*; restore these and rebuild for source rollback. Prior r16 ZIP retained. This is not a new Git commit.

## Hashes

```json
{
  "zip": "/home/humor/ps5dev/build/native-pfbneo/dist/pfbneo-ps5-cave-state-exit-r17-20261002.zip",
  "zip_sha256": "5298fb23b74499500788263cc1525645a732926803f489882127cbc22f09c6a9",
  "eboot_sha256": "b1c1646d52c704f15c9024347a0d2433cccf0830704a20b5b8f4c62d8b504679",
  "console": "pending"
}
```
