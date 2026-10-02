# Native heap isolation r18

> R19 correction: the earlier “cached” label was wrong. Allocation type 3 is WC_GARLIC (write combining), not CPU write-back caching. R19 changes the core-only pool to type 0 (WB_ONION). Prior host mocks validated routing but did not validate memory-cache performance.

Baseline: Git9482d136666450743a0c6e19fb96000085b6a273 plus dirty r17. No commit/push. Baseline eboot b1c1646d52c704f15c9024347a0d2433cccf0830704a20b5b8f4c62d8b504679.

pEMU PS5 heap-isolation-r18-20261002
覆盖 PPSA99998 到 /data/homebrew/PPSA99998/，保留 ROM/配置/存档。

撤回r17整体512MiB直接内存主堆。界面/SDL/OpenGL恢复r16的256MiB匿名主堆
（128MiB回退）。仅FBNeo BurnMalloc和存档缓冲的>=1MiB分配按需申请独立256MiB
cached direct memory池。普通malloc/calloc/posix_memalign始终使用原主堆。
free/realloc/usable_size识别所属池；扩展池无法满足时尝试主堆。
扩展池保留至进程结束，其内部游戏和存档分配照常释放复用。
HEAP live/peak只代表主堆；CORE_HEAP日志标明扩展池启用结果。
保留r17存档安全检查/临时文件保存和系统服务Quit方案，保留r16显示性能/颜色/声音。

实机检查：先确认菜单可进入，再加载Cave游戏，保存空槽位，继续游玩后读取，最后Quit。
提供新pemu_boot.log和ROM名称。启动应为 HEAP ready capacity=256MiB；大型核心分配
出现 CORE_HEAP ready capacity=256MiB direct cached。
r17停在ui create begin，具体内部原因尚未确定。r18隔离图形与核心扩展分配。
实机启动、Cave状态恢复与Quit仍待验证。

已通过：交叉编译、SELF完整性、ELF映射/LOAD内容一致、ZIP CRC；ASan/UBSan
主堆隔离、扩展池延迟初始化和分配归属、OOM保留旧指针、映射/mspace失败回退；
64MiB状态往返及损坏/OOM检测；保存失败保留旧存档。平台API使用测试替身。
稳定回退包：pfbneo-ps5-present-exit-r16-20261002.zip（已知Quit/Cave内存问题仍在）。

Build: pfbneo_ps5_native_core and cross2dui in pemu-launch-r7; stage libpfbneo_ps5_native.a then native-build-pfbneo.sh using existing archives. Evidence/source backups: /home/humor/ps5dev/logs/heap-isolation-r18-20261002 (baseline-r17). Linked symbol pemu_native_core_malloc is localized (t); its three weak references resolve to the actual implementation.

```json
{
  "zip": "/home/humor/ps5dev/build/native-pfbneo/dist/pfbneo-ps5-heap-isolation-r18-20261002.zip",
  "zip_sha256": "45054ec6d4bc79832c85efca9401e95b51b2930e0a0f55ba329d10b96d8f9470",
  "eboot_sha256": "090629b4304f991599a5043357ce8240b8ff43b1f5da80ca772fcec27ffa5ce9",
  "console": "pending"
}
```
