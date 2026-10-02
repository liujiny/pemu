# PS5 启动第四轮：恢复最早日志，隔离新增 API

## 后续实机结果

PS5 4.03 两次启动均恢复 `_start`、运行库初始化、构造函数和 main 包装入口的日志。
随后 chmod、sceKernelChmod、chdir、getdents、sceKernelGetdents、lstat 全部动态查询返回空，
程序记录 `native filesystem APIs unavailable` 后主动返回，尚未进入原前端 main。
没有 BOOT_FAULT。r4 恢复了最早日志，但动态查询方案在这台机器上不可用，不能作为可用版本。
后续 r5 根据参考工程改用直接导入；本页原本的本地测试通过不代表动态查询实机可用。

## 问题和已确认范围

用户在 PS5 4.03 测试 r3 后，界面提示“数据已损坏”，没有错误码，也没有新的 pemu_boot.log。
r2 曾进入前端 main，在 I/O 构造期间记录到 SIGSEGV。因此 r3 是一次实机启动回退。

本地比较结果：

- r3 的 param.json、libc.prx 及 r2 已有的其它 title 文件哈希不变，只有 eboot 改变；另增加了 data_romfs。
- r2/r3 的依赖模块列表相同，LOAD 映射符合对齐约束，签名容器完整性校验通过。
- r3 新增静态函数导入：chdir、getdents、lstat、sceKernelFchmod。
- r3 在第一条日志写入前调用 sceKernelFchmod。这使得“无日志”不能单独证明 _start 没有执行。

以上不能证明某个新增接口就是“数据损坏”的原因。当前没有实机导出表、加载器错误或已部署文件的回读校验，
仍不能排除导入兼容、部署文件不完整或挂载状态问题。本轮按已知差异缩小故障范围，不更改模拟或渲染实现。

## 修改

1. 最早日志恢复为 Open → Write → Close，使用 r2 已执行过的三个 libkernel 导入。
   路径保持 /app0、/download0、/data 三个候选；创建模式仍为 0644。
   不读取 BSS 状态、不分配堆内存、不在早期或崩溃日志写入时解析或调用权限接口。
2. main 包装入口先安装原有崩溃记录器，再执行 native API 解析。
   使用 r2 已有的 dlsym 导入；每次解析前记录 API 名，随后记录 resolved/unavailable。
3. chdir、getdents、lstat 改为经过空指针检查的调用。关键接口缺失时记录并返回失败，避免调用空地址。
   getdents 可尝试 sceKernelGetdents，并把 SCE 错误码转换为 errno。
4. 权限修复移到最早日志之后：动态尝试 chmod / sceKernelChmod，按已知日志路径设置 0644。
   路径方式可以修复创建掩码造成的 0000 文件，而无需先重新打开它。
   权限接口缺失或返回失败会记录，不能阻止其它启动诊断。
5. 保留 r3 的真实 getcwd 目录遍历、/app0 工作目录设置与 data_romfs 布局修复。
   版本标记为 `BOOT_STAGE build launch-r4-20261002`。

## 验证

- PS5 native 编译成功，FSELF 完整性通过；从最终 FSELF 提取后，所有 LOAD 段与签名前文件完全一致。
  容器不封装的末尾非映射 NOTE 被填零，这与 r2 行为相同，因此不声称整个 ELF 文件逐字节相同。
- ELF 检查通过：5 个 LOAD 映射、11 个保留源节，DT_PLTGOT 与 LLVM 相同。
- 最终静态函数导入是 r2 的子集：无新增导入；不再静态导入 chdir/getdents/lstat/权限接口。
  删除的 r2 导入为 getcwd/opendir/readdir/closedir，由目录兼容层接管。
- 目录解析与 getcwd 主机回归通过（ASan、UBSan；ptrace 沙箱不支持 LeakSanitizer）。
- API 绑定测试通过：初始化前保护、全部接口缺失、可选权限接口缺失、SCE 别名及错误转换，解析前必有阶段记录。
- 使用真实 CRT 日志函数的主机测试通过：第一条记录先于权限 API 写出；0000 新文件和 0600 旧文件均可修复为 0644；
  三个目录回退、追加写与短写通过。
- 原 title 文件与 r3 相同，仅更换 eboot；包中额外提供静态构建说明、大小与 SHA256 清单以供部署核对。

这些结果不证明 PS5 上的 dlsym 默认搜索行为或具体 API 可用，也不代表“数据已损坏”已解决。
需要实机检查 r4 是否能恢复最早日志，若恢复再据日志继续定位。

## 产物

- 测试包：`/home/humor/ps5dev/build/native-pfbneo/dist/pfbneo-ps5-launch-r4-20261002.zip`
- eboot.bin 大小：`103108515` 字节。
- eboot SHA256：`2cea37ef11b824b2eb54a52f293c8d7b214fc2ca0742afa1d2fb3a783f64f5b9`
- 构建证据、基线 r3、主机测试和导入列表：`/home/humor/ps5dev/logs/launcher-recovery-r4-20261002/`
- Git 基线：`9482d136666450743a0c6e19fb96000085b6a273`，含之前未提交改动；本轮未提交或推送。
- 可回到保留的 r2 包核验已知行为。r3 包仅保留为回归证据。

## 实机步骤

1. 退出应用，将 r4 包中 PPSA99998 的内容合并覆盖到 `/data/homebrew/PPSA99998/`，保留自己的 ROM、配置和存档。
2. 确认传输全部完成，FTP 中 eboot.bin 的大小应为 103108515 字节。
   `pemu-build.txt` 是打包时生成的版本说明，可在无法启动时帮助核对覆盖的版本，不能代替 eboot 的哈希校验。
3. 在 ShadowMount 重新挂载这个 title 后启动。
4. 日志仍位于 `/data/homebrew/PPSA99998/pemu_boot.log`。记录最后一次 r4 标记后的内容。
   若仍为“数据已损坏”且没有日志，应回读已部署文件核对 SHA256，并用未修改的 r2 eboot 复测，进一步区分环境与新包。
