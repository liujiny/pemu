# PS5 native title 启动修复（2026-10-02）

## 范围及基线

分支 `ps5-gpu-opengl`，基线提交 `9482d136666450743a0c6e19fb96000085b6a273`。
基线包含既有未提交修改，因此该 SHA 不能单独重现整份本机构建。
原始 title、源文件、ELF 和哈希保存在
`/home/humor/ps5dev/logs/launcher-fix-20261002/`。
本次仅修改 native 转换器、CRT 与构建校验，不改 FBNeo、SDL 或渲染代码。

## 参考结构对照

终端 clone 因 GitHub DNS 不可用失败，已读取公开仓库和原始文件；未取得
实机安装的 RetroArch 二进制。因此这里是源码对照，不是两份实机 title 的完整 diff。
用户已确认实机正常启动的 RetroArch title 为 `PPSA99169`，与当前源码参数一致。
参考仓库还包含旧的 `handoff/PPSA99002`，本次对照采用当前参数。

| 项目 | pEMU | 当前 RetroArch 源码 |
|---|---|---|
| titleId | PPSA99998 | PPSA99169 |
| contentId | UP9000-PPSA99998_00-PFBNEOPS5GPUTEST | UP9000-PPSA99169_00-RETROARCH0000001 |
| applicationCategoryType | 0 | 0 |
| contentBadgeType | 1 | 1 |
| gameIntent | permitted launchActivity | permitted launchActivity |
| attribute3 | 0 | 524352 |
| 模块 SDK / companion SDK | 0x02000009 / 0x08050001 | 相同构建常量 |
| 应用 FSELF magic | 0x1d3d154f | 相同构建常量 |

来源：[参数文件](https://github.com/mihawk-99/PS5_RetroArch/blob/main/sce_sys/param.json)、
[构建脚本](https://github.com/mihawk-99/PS5_RetroArch/blob/main/tools/build.sh)、
[仓库布局](https://github.com/mihawk-99/PS5_RetroArch)。
`attribute3` 的差异尚未确认与故障有关，本次没有移植其数值。
参考 title 的 runtime 模块字节、SELF authority 和各 segment 尚未做二进制比较。

## 已确认错误

转换器以 `.got` 的文件偏移作为整个 RELRO 区域的文件起点，
却以较早的 `.data.rel.ro` 地址作为虚拟起点。
旧版 PT_LOAD：文件偏移 `0x04366518`，虚拟地址 `0x03b04000`，对齐 `0x4000`。
偏移与地址不满足模对齐一致性，且 RELRO 数据映射错误；这足以解释加载器在入口前拒绝的可能性，
但 503 的实机原因仍需复测确认。

修复为从 GOT 文件偏移减去 GOT 到 RELRO 起点的地址距离，并校验所有 RELRO 节连续映射。
已在真实 pEMU ELF 上验证旧版失败、新版通过，保留的 11 个源节内容一致。
校验器同时检查程序入口、文件范围、process parameter 魔数及映射。
FSELF 签名完整性通过不能代替这类 ELF 映射校验。

## CRT

BSS 清零循环使用 volatile，避免编译器生成环境初始化之前的 libc memset。
启动探针直接使用 loader 绑定的 `sceKernelOpen/Write/Close`，不依赖应用 libc 初始化，
不在应用代码内发出原始 syscall 指令。
日志追加到 `/data/pemu_boot.log`，失败后尝试 `/app0/pemu_boot.log`。
阶段：`_start entered`、`bss ok`、`env ok`、`constructors begin`、
`constructors ok`、`main calling`、`main entered`、`main returned`。
`main entered` 位于包装入口，随后直接调用原有 pEMU main。
如果日志不存在，仍需排除文件权限和日志接口问题，不能单凭缺少文件证明未进入入口。

## 实机测试

1. 退出旧应用，备份 `/data/homebrew/PPSA99998`。
2. 解压测试 ZIP，将其中整个 `PPSA99998` 目录替换到 `/data/homebrew/PPSA99998`。
   避免形成 `PPSA99998/PPSA99998` 两层目录。
3. 备份并移走旧的 `/data/pemu_boot.log`（如果有），通过 ShadowMount 启动。
4. 记录是否仍为 503、是否进入主界面；提供新日志最后一次启动的阶段。
5. 无主日志时检查应用映射路径 `/app0/pemu_boot.log`；其实际存储位置取决于启动器挂载。

本地编译、ELF 映射、FSELF 校验可以确认；实机进入 main 尚未验证。
测试包哈希及文件清单见包内 `SHA256SUMS.txt` 和同目录外部 SHA256 文件。
回退时恢复证据目录中的 `baseline-title`；源码回退本次指定文件即可，勿覆盖原有未提交修改。

## 构建结果

重新构建成功；应用及 libc FSELF 完整性均 valid。
最终打包 eboot 提取后再次校验：5 个 LOAD 映射、11 个源节内容通过。
最终 CRT 反汇编中没有 syscall 指令和 memset 引用；libkernel 导入及 main 包装符号存在。
与基线 title 的逐文件哈希比较：35 个文件中只有 `eboot.bin` 变化，
param.json、libc.prx、其它资源保持原字节。ZIP CRC 自检通过。

eboot.bin SHA256：
`21e2ef4a1e6cb54ad4bf667afbe72e0ec27d27f713e8dd398e121f55678bb28d`。
