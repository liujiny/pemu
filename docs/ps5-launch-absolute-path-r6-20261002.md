# PS5 启动第六轮：前端使用绝对路径

## 实机依据

r5 在 PS5 4.03 上完成 `_start`、运行库、构造函数和 main 包装入口。
直接导入成功，`chmod` 修复日志权限后记录 `log permissions 0644 ok`。
随后 `chdir("/app0")` 返回 -1、errno=1（EPERM），本工程的错误分支主动返回。
日志最后为 `main returned`，没有 BOOT_FAULT；用户看到应用停留在启动背景。

因此本轮处理的是启动对 cwd 切换的依赖。该日志不能证明 `/app0` 不存在；
事实上同一进程已经通过该路径写入并修改日志权限。FTP 服务器中也不要求暴露 `/app0`。

## 修改

- native CRT 提供 `pemu_native_data_path()`，返回 `/app0/`。
- PFBAIo 构造器先通过弱引用获取路径，调用已有 `setDataPath()`，然后才创建目录、
  执行 `BurnPathsInit` 和 `BurnLibInit`。没有添加类成员或虚函数。
- `POSIXIo::getDataPath()` 已有非空路径直接返回逻辑，故此流程无需 `getcwd()`。
- native main 包装入口删除 `chdir` 步骤；删除对应桥接、地址输出及测试。
- 保留最早日志、日志权限修复、真实 getcwd/目录兼容层。
- helper 仅由 native CRT 提供；其他平台及未提供 helper 的 payload 保持原路径逻辑。

| 内容 | native 路径 |
|---|---|
| 总配置 | `/app0/config.cfg` |
| 单游戏配置 | `/app0/configs/` |
| 存档 | `/app0/saves/` |
| 默认街机 ROM | `/app0/arcade/` |
| 内置皮肤和字体 | `/app0/data_romfs/skins/default/` |

用户自行保存的相对 ROM 路径不在本轮自动迁移。

## 构建与检查

- 现有依赖文件和归档符号确认：PFBAIo 构造器仅由 `main.cpp.o` 提供。
  重编该成员并替换 native 应用归档，然后重编 CRT 和启动兼容层、重新链接打包。
- 逐成员哈希比较确认归档仅 `main.cpp.o` 变化。
  包中的参数、runtime module、皮肤等资源与 r5 逐文件一致。
- native 编译和 FSELF 完整性检查通过。
- 从实际 eboot 提取 ELF：5 个 LOAD、11 个源节与 LLVM 映射校验通过；
  5 个 LOAD 的内容与签名前 ELF 完全相同。
- 独立反汇编审查：helper 调用和 `m_data_path` 写入在首次 `getDataPath()` 前；
  helper 为本地符号且有正确的 RELATIVE 重定位。main 包装入口直接进入原 main。
- 与 r5 比较，仅移除 `chdir` 动态导入，没有新增导入；依赖模块列表相同。
- 主机 ASan/UBSan 检查通过：直接文件系统桥接的参数、返回值、errno；
  权限失败可继续；新建及已有日志的 0644 修复；最早写入、追加和短写处理。
- 真实 PFBAIo、POSIXIo、Utility、BurnPathsInit 主机测试通过：cwd 位于另一目录，
  getcwd 固定返回 EPERM 时调用次数为 0；10 个 szApp 路径、目录创建、hiscore 复制、
  资源查找和配置/存档写入均使用绝对根，cwd 未变化。
  无 helper 的独立构建也通过，保留原 `./` 回退行为。两种构建均启用 ASan/UBSan。
  复现：`rtk proxy python3 /home/humor/ps5dev/logs/launcher-absolute-path-r6-20261002/build-native-data-path-host.py`。

实机补充：r6 在 PS5 4.03 已进入原前端 main，完成 I/O 创建、绑定和配置创建，并生成数据目录。
随后创建皮肤按钮纹理时通过空 `glad_glGenTextures` 调用而崩溃，说明 UI 对象构造返回不等于图形初始化成功。
对应二进制与当前构建配置另确认 PS5 编译标记被后续默认列表覆盖，旧包请求 GL 4.3，而 G19 仅接受 3.3。
未修改或重编 GPU、SDL backend、模拟核心。

## 基线与产物

- Git 基线：`9482d136666450743a0c6e19fb96000085b6a273`，保留原先未提交修改。
- r5 eboot SHA256：`e418a58d3a1536c815a2e5e8f2a685b84d96f3923d150413b028920077803289`。
- r6 eboot SHA256：`cbaeef44fd3286179c066521da89c4c88edf2071a71ceb821eafa780b73b0093`。
- r6 eboot 大小：`103108915` 字节。
- 测试包：`/home/humor/ps5dev/build/native-pfbneo/dist/pfbneo-ps5-launch-r6-20261002.zip`。
- ZIP SHA256：`27ee278788ea27f7344234efd6d3cf3f178813b25505559307a9fa2f0efba460`。
- 证据目录：`/home/humor/ps5dev/logs/launcher-absolute-path-r6-20261002/`。
  保存 r5 原 eboot/ELF/归档/修改前源码、r5 实机日志、编译命令、检查结果。
- 无新增 Git 提交或推送。r5 ZIP 保留供回退对照，r5 仍有已知启动停留问题。

## 实机步骤

退出应用，把 ZIP 中 `PPSA99998` 的内容合并覆盖到 `/data/homebrew/PPSA99998/`，
保留个人 ROM、配置和存档，不要嵌套两层同名目录。
确认 eboot 大小为 103108915 字节，启动后检查该目录内的 `pemu_boot.log`。

本轮标记为 `BOOT_STAGE build launch-r6-20261002`。
重点观察 `frontend main body`、`io create begin`、`native data path /app0/`、
`io create ok` 以及后续 UI 阶段；若仍失败，提供最后一次 r6 标记后的完整日志。
