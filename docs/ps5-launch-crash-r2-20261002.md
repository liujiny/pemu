# PS5 启动崩溃第二轮：PLT 修复与 main 内部定位

## 实机证据

用户在 PS5 4.03 上测试第一轮包：报 CE-108255-1。
实际日志存在于 FTP 目录 `/data/homebrew/PPSA99998/pemu_boot.log`。
两次启动均依次经过 `_start entered`、`bss ok`、`env ok`、
`constructors begin`、`constructors ok`、`main calling`、`main entered`。

这证明启动器、加载器、CRT 已到达 main 包装入口，故障发生在之后。
`/app0` 是运行进程内部的挂载路径，不应在 FTP 根目录寻找它。
第一轮的“没有日志”判断已由实际文件纠正。

## 已修复的独立错误

转换器把 `DT_PLTGOT` 写成普通 `.got` 地址，丢失了 LLVM 给出的 `.got.plt` 地址。
第一轮文件中：

| 项目 | 地址 |
|---|---|
| LLVM 的 DT_PLTGOT / .got.plt | 0x043636c0 |
| 转换后错误的 DT_PLTGOT / .got | 0x04362518 |
| 被错误保留为 resolver 槽位的应用数据 | 构造函数数组的边界指针 |

该文件有 9 条 JUMP_SLOT 重定位，所以这不是不存在 PLT 的情况。
修复后原样保留 LLVM 的 DT_PLTGOT；若没有 PLT，就保留零地址。
校验器拒绝 resolver 的三个保留槽位与应用重定位重叠，并比较转换前后 DT_PLTGOT。
在真实第一轮 ELF 上回归检查失败，新包及从其 FSELF 提取的 ELF 通过。

FreeBSD 的 x86-64 loader 会向 PLTGOT 的第 1、2 槽写入解析器数据：
[上游实现](https://github.com/freebsd/freebsd-src/blob/main/libexec/rtld-elf/amd64/reloc.c)。
这是错误元数据可能破坏启动状态的依据，不能代替 PS5 实机崩溃地址证据。
现有日志显示构造函数已经完成，不能声称这个修复已证明解决 CE-108255-1。

## 本轮定位代码

- 日志优先写 `/app0/pemu_boot.log`，即已确认可见的实际 title 目录。
  写入失败才尝试 `/download0/pemu_boot.log` 和 `/data/pemu_boot.log`。
- 每次启动以 `BOOT_STAGE build launch-r2-20261002` 区分版本。
- 前端 main 标记 Game、I/O、界面、配置、皮肤、ROM 列表及主循环的启动进度。
- 进入原 main 前安装崩溃记录器。发生 SIGSEGV、SIGBUS、SIGILL、SIGFPE、SIGABRT 时，
  尽量写入 `BOOT_FAULT` 与 `BOOT_CONTEXT`，然后保持系统正常崩溃处理。
- 记录器不使用 stdio、堆分配或解引用崩溃时的栈指针；保存原始 context 前 40 个字，
  避免把旧 SDK 的 FreeBSD 布局误当作 PS5 寄存器布局。
  仍需实机验证信号是否可被记录器捕获。

参考 SDK 的 context 布局说明：
[PS5_PayloadSDK](https://github.com/mihawk-99/PS5_PayloadSDK/blob/fa69d00fe974a47a20009d32c9780c259a05a08f/include/freebsd/sys/_ucontext.h)。
参考 RetroArch 的启动文件、转换器和 runtime 哈希已通过 GitHub API 读取。
其 `libc.prx` 哈希与本项目一致；本轮没有更换 runtime。
参考源码：[CRT](https://github.com/mihawk-99/PS5_RetroArch/blob/59a35aec4ee0c35d713a049e0b840726d6aa2695/tooling/native/app_crt.cpp)、
[转换器](https://github.com/mihawk-99/PS5_RetroArch/blob/59a35aec4ee0c35d713a049e0b840726d6aa2695/tooling/native/sce_module_writer.cpp)。

## 构建和验证

- native 构建成功，应用和 libc FSELF 完整性通过。
- 最终 eboot 解包后，5 个 LOAD 映射与 11 个源节内容通过；DT_PLTGOT 与 LLVM 一致。
- CRT 无初始化前 memset 或应用内原始 syscall。
- 主机测试实际触发 SIGILL：信号记录一次，40 个 context 字写出，进程仍按 SIGILL 退出。
  这验证记录逻辑，不代表验证了 PS5 的信号 ABI。
- 前端只重编译 `src/cores/main.cpp` 并替换暂存档案中的对应成员；模拟核心和渲染库复用。
- title 中只更换 eboot.bin，参数、libc 和资源逐文件哈希保持一致。

新 eboot SHA256：
`f588010bbb4706a7978814bd0a05cc94410870ee303a61b2c6a348cdc884a3c3`

基线 Git SHA：`9482d136666450743a0c6e19fb96000085b6a273`，包含先前未提交修改。
第一轮 eboot SHA256：
`21e2ef4a1e6cb54ad4bf667afbe72e0ec27d27f713e8dd398e121f55678bb28d`

证据、前端实际编译参数及回退备份：
`/home/humor/ps5dev/logs/launcher-crash-20261002/`。
第一轮压缩包保留。没有推送 Git，也没有修改 GPU、SDL backend 或 FBNeo 模拟实现。

## 实机测试

1. 退出应用，备份实际 title 目录中的旧 `eboot.bin` 和 `pemu_boot.log`。
2. 可仅用本包 `PPSA99998/eboot.bin` 覆盖旧文件，其余文件哈希未变。
   也可以使用完整 title 目录，避免多嵌套一层 PPSA99998。
3. 经 ShadowMount 启动。如果依旧崩溃，请保留最新 r2 标记之后的全部日志，
   特别是最后的 BOOT_STAGE、BOOT_FAULT、BOOT_CONTEXT。
4. 日志位置为 FTP 的 `/data/homebrew/PPSA99998/pemu_boot.log`。

本轮包是待实机验证的修复及诊断包，尚不能确认已进入 pEMU 主界面。
