# PS5 启动第五轮：使用直接导入的文件系统接口

## 实机证据

用户提供两次相同的 r4 日志：已通过 `_start`、BSS、运行库、构造函数和 main 包装入口。
安装崩溃记录器后，所有 `dlsym(RTLD_DEFAULT, ...)` 查询都记录 unavailable。
随后本工程的保护分支主动退出并记录 `main returned`；没有 BOOT_FAULT，也未调用原前端 main。

这证明 r4 的动态查询路线在当前 PS5 4.03 native title 中失败，不能据此认定这些 API 不存在。
对实际转换后的 ELF 审查确认，dlsym 来自 libkernel，不是 libc.prx 的空占位函数。

参考版本 `PS5_RetroArch@59a35aec4ee0c35d713a049e0b840726d6aa2695`：

- [build-title.sh](https://github.com/mihawk-99/PS5_RetroArch/blob/59a35aec4ee0c35d713a049e0b840726d6aa2695/tools/build-title.sh#L139)
  记录 native 动态模块加载/解析的限制，并采用链接时导入。
- [frontend_ps5.cpp](https://github.com/mihawk-99/PS5_RetroArch/blob/59a35aec4ee0c35d713a049e0b840726d6aa2695/src/frontend_ps5.cpp#L44)
  和 [permissions_ps5.cpp](https://github.com/mihawk-99/PS5_RetroArch/blob/59a35aec4ee0c35d713a049e0b840726d6aa2695/src/permissions_ps5.cpp#L61)
  直接调用 POSIX chmod。
- [PS5_PayloadSDK directory.c](https://github.com/mihawk-99/PS5_PayloadSDK/blob/fa69d00fe974a47a20009d32c9780c259a05a08f/platform/src/directory.c)
  直接调用 getdents 与 lstat。

没有采用 payload CRT 的固定模块句柄，也没有尝试 umask。
参考前端使用绝对路径，未提供 chdir 的实机验证；本工程的 chdir 调用仍需此轮日志确认。

## 修改

1. 启动兼容层取消 dlsym，直接引用头文件声明的 chmod、chdir、lstat，以及 getdents。
2. 保持 r4 最早日志只调用 sceKernelOpen/Write/Close。
   权限修改仍在进入 main 包装入口、安装崩溃记录器之后执行，不再导入 sceKernelFchmod。
3. chmod 使用 SDK 的 mode_t（目标平台 16 位）；保留按路径修复 0000/0600 日志为 0644 的实现。
   权限失败记录错误后继续，不能阻止前端初始化。
4. 记录四个导入函数的地址、chmod 失败时的 errno、chdir 的返回值和失败 errno。
   记录不用 stdio 或堆分配，并保留原 errno，避免日志改变错误处理。
5. getdents/lstat 转发保持 POSIX 返回值，不再经过 r4 的 SCE 错误码转换。
6. 保留原目录/getcwd 兼容实现及 data_romfs 资源布局。

## 验证

- PS5 native 编译成功；FSELF 完整性检查通过。
- 从包内 eboot 提取后，5 个 LOAD、11 个保留源节与 LLVM 校验通过；DT_PLTGOT 一致。
  所有 LOAD 内容与签名前 ELF 一致；不封装的非映射 NOTE 与前几版一样被填零。
- 与 r4 比较，仅新增 chmod/chdir/getdents/lstat 四个静态函数导入。
  启动兼容层目标文件没有 dlsym、dlopen 或 sceKernelDlsym 依赖。
- 独立复核转换后符号：四个函数均为 libkernel，library id 8 / module id 9（`#I#J`）。

| 函数 | 转换后的导入名称 |
|---|---|
| chmod | z0dtnPxYgtg#I#J |
| chdir | 6mMQ1MSPW-Q#I#J |
| getdents | 2G6i6hMIUUY#I#J |
| lstat | DRGXpDDh8Ng#I#J |

- 反汇编复核：boot_trace 仅调用原来三个日志接口；main 包装入口先装崩溃记录器，再执行权限与 chdir。
- 主机 ASan/UBSan 测试通过：POSIX 参数/返回值/errno、权限失败可继续、记录错误时保存 errno。
  主机测试不能验证 PS5 内核 ABI 或固件导出。
- 与 r4 比较，title 的参数、libc、资源均相同，只替换 eboot 并更新构建说明。
- 模拟核心、GPU/OpenGL/Vulkan、SDL backend 未修改或重编译。本轮无 Git 提交或推送。

## 产物及限制

- 新包：`/home/humor/ps5dev/build/native-pfbneo/dist/pfbneo-ps5-launch-r5-20261002.zip`
- eboot 大小：`103108899` 字节。
- eboot SHA256：`e418a58d3a1536c815a2e5e8f2a685b84d96f3923d150413b028920077803289`
- Git 基线：`9482d136666450743a0c6e19fb96000085b6a273`，含原先未提交改动。
- r4 eboot SHA256：`2cea37ef11b824b2eb54a52f293c8d7b214fc2ca0742afa1d2fb3a783f64f5b9`
- 基线、构建记录、实机输入、校验和测试输出：`/home/humor/ps5dev/logs/launcher-static-api-r5-20261002/`

r5 实机（PS5 4.03）已确认直接导入不再阻断启动，chmod 执行成功，日志记录 `log permissions 0644 ok`。
但 `chdir("/app0")` 返回 -1、errno=1（EPERM），包装入口随即主动返回，停留在启动背景。
此轮没有 BOOT_FAULT，也未进入原前端 main。下一轮使用前端绝对路径，移除切换 cwd 的要求。
r3“数据已损坏”的具体原因也未被证明；已确认当时首次写日志之前调用权限接口，使无日志无法判断故障层次。
本轮保留已验证的最早日志顺序，并按参考工程修正 API 绑定方式。

## 实机操作

退出应用，把 r5 的 PPSA99998 内容合并覆盖到 `/data/homebrew/PPSA99998/`，保留个人 ROM、配置和存档。
确认 eboot.bin 为 103108899 字节，再从 ShadowMount 启动。
日志仍在该目录的 pemu_boot.log；提供最后一次 `launch-r5-20261002` 之后的完整内容即可。
重点阶段为 `log permissions 0644 ok`、`chdir /app0 ok`、`frontend main body` 和 `io create ok`。
