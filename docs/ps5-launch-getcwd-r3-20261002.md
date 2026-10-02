# PS5 启动第三轮：目录兼容与日志读取权限

## 后续实机结果：启动回退

用户测试 r3 后报告“数据已损坏”，没有错误码，也没有生成新的日志。
因此 r3 不能视为实机可用版本。以下编译、容器和主机测试结果仅代表本地验证。
后续 r4 将新增 API 改为在 main 入口后解析，并恢复不依赖权限接口的最早日志路径；
当前仍未确认“数据已损坏”的具体原因。

## r2 实机定位

环境：PS5 4.03，ShadowMount，PPSA99998。r2 日志已进入前端 main，最后一条阶段为
`BOOT_STAGE io create begin`，随后 SIGSEGV。这说明前面的 title 加载、CRT、构造函数和 main 入口已经通过。

I/O 构造器的第一组操作调用 `POSIXIo::getDataPath()`，其中调用 `getcwd(buf, 1024)`。
对应 r2 ELF 的反汇编也指向该调用；上下文中 RDI 是栈缓冲区，RSI 为 `0x400`，与实参一致。
故障地址 `0x100020a986` 在应用 ELF 代码映射之外。当前没有系统模块加载表，不能把该地址直接符号化为某个系统函数。

参考 RetroArch 明确记录：native title 使用的 libc 的 `getcwd` 会调用仅 `libkernel_sys`
导出的 `__getcwd`，导致未解析调用；native title 的目录遍历也需要 `getdents` 兼容实现。
本地 SDK 的 `libkernel` 导出与该说明一致。
因此本轮针对有代码、实参和参考工程支持的 `getcwd` 路径修复，尚不声称已在实机验证解决所有启动崩溃。

参考固定版本：

- [RetroArch 包装入口](https://github.com/mihawk-99/PS5_RetroArch/blob/59a35aec4ee0c35d713a049e0b840726d6aa2695/src/platform_wraps.c)
- [PS5_PayloadSDK 目录实现](https://github.com/mihawk-99/PS5_PayloadSDK/blob/fa69d00fe974a47a20009d32c9780c259a05a08f/platform/src/directory.c)

## 修改

1. `tools/ps5/native-directory.c`：保留参考实现的作者和 GPL 声明，仅移植目录流和 getcwd。
   通过 native 链接的 `--wrap` 接管 getcwd、opendir、fdopendir、readdir、closedir、rewinddir 和 dirfd。
   getcwd 按父目录的设备号与 inode 查找真实路径，不伪造固定返回值，也不修改进程工作目录。
   目录记录先检查边界；读取被 EINTR 中断时重试，成功后恢复 errno；失败时保留有意义的错误。
   路径上限 1024 字节，与参考实现一致。
2. CRT 在调用前端 main 前显式 `chdir("/app0")`，增加成功/失败阶段记录。
   该目录是 title 的进程内挂载点，对应用户已经确认的 FTP 安装目录。
3. 日志创建权限改为 `0644`。每次成功打开日志后，使用 `sceKernelFchmod(fd, 0644)`
   修复已有的 `0600` 文件及可能被创建权限掩码限制的新文件，然后追加日志。
   仍使用 libkernel 接口，可在 libc 初始化前写入；不依赖 stdio 或堆分配。
4. 修正资源布局：`POSIXIo::getRomFsPath()` 查找的是工作目录下的 `data_romfs/`。
   原包只有 `assets/`；现在另外打包 `data_romfs/`，包含皮肤、字体、图标和 hiscore 资源。
   构建前检查默认皮肤和字体存在。
5. 本轮标记：`BOOT_STAGE build launch-r3-20261002`。

## 验证与界限

- PS5 native 编译成功；eboot 和 libc 的 FSELF 完整性检查通过。
- 从最终 eboot FSELF 提取后校验：5 个 LOAD 映射、11 个保留源节、DT_PLTGOT 与 LLVM 一致。
- 最终 ELF 不再导入 getcwd、opendir、readdir 等被替代的目录函数；新增调用由 native libkernel 导出，未增加 libkernel_sys 依赖。
- 主机目录测试：根目录、独立挂载点、符号链接、重命名、已删除目录、缓冲区尺寸、fd 所有权、255 字符文件名、EINTR 和六类损坏目录记录通过。
  使用 ASan 和 UBSan；沙箱处于 ptrace 下，LeakSanitizer 无法运行，故禁用泄漏检测。
- 主机运行实际 CRT 日志写入函数：三个候选日志目录、新建时 umask=0777、已有 0600 文件、追加写及短写均通过；最终权限均为 0644。
- 资源逐文件核对：28 个 data_romfs 文件与原始构建资源一致。r2 包的既有文件仅 eboot.bin 改变。
- 未更换 libc；模拟核心、SDL backend、GPU/OpenGL/Vulkan 和渲染库均沿用原构建。
- 主机测试不能验证 PS5 的内核 ABI、挂载权限或后续界面初始化，仍需 PS5 4.03 实机启动确认。

## 产物和回退

新 eboot SHA256：
`3bafc6e16f6a72abff7d6397fd28d5865bdf4697e9e48909de02bb62f221765e`

测试包：
`/home/humor/ps5dev/build/native-pfbneo/dist/pfbneo-ps5-launch-r3-20261002.zip`

基线 Git：`9482d136666450743a0c6e19fb96000085b6a273`，工作区含此前未提交修改。
基线 r2 eboot SHA256：
`f588010bbb4706a7978814bd0a05cc94410870ee303a61b2c6a348cdc884a3c3`

回退使用保留的 `pfbneo-ps5-launch-r2-20261002.zip`；基线源文件、ELF、主机测试输出及本轮构建记录在：
`/home/humor/ps5dev/logs/launcher-getcwd-r3-20261002/`。
没有提交或推送 Git。

## 实机操作

1. 退出 pEMU，备份旧 eboot 和日志。
2. 将包内 `PPSA99998` 的内容合并覆盖到 `/data/homebrew/PPSA99998/`。
   本轮必须复制新增的 `data_romfs`，不能只替换 eboot。保留个人 ROM、存档和配置。
3. 通过 ShadowMount 启动一次。已有日志会继续追加，并尝试自动修正为 0644。
4. FTP 日志仍为 `/data/homebrew/PPSA99998/pemu_boot.log`。
   若继续崩溃，提供最后一次 `launch-r3-20261002` 后的所有日志。
