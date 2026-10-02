# PS5 启动第七轮：恢复已有的平台构建配置

## 范围

本轮继续处理 PPSA99998 的启动失败。后续图形移植目标按用户最新说明为 Vulkan；
当前 `ps5-gpu-opengl` 分支的测试包仍链接已有 OpenGL/SDL 实现。
本轮修正构建参数和前端启动检查，不修改渲染器、SDL backend、GPU 实现或模拟核心源码。

## r6 实机证据

PS5 4.03 日志已到原前端 main，完成 I/O 创建、绑定和配置创建。
用户确认 title 目录中出现新目录，符合 BurnPathsInit 正常创建 configs、saves、arcade 等目录的行为。
随后创建皮肤时 SIGSEGV，执行地址为零。

与 r6 原始 LLVM ELF 对照：

- `Skin::Skin` 创建按钮纹理 → `GLTexture` → `GLTexture::createTexture`。
- ELF `0x160b36e` 间接调用 `glad_glGenTextures`；该指针位于 `0xba27a38`。
- 日志 RDI=1，RSI−RBX=`0x268`，对应 `glGenTextures(1, &m_texID)`。
- RDX=`0x8058`、RCX=`0x1907`、R8=`0x8363` 与之前设置像素格式的指令一致。
- 日志 `_start=0x400300`，ELF `_start=0x300`，加载偏移为 `0x400000`。
  指针变量运行时地址为 `0xbe27a38`，也与上下文一致。

故障为未初始化的 GLAD 函数指针调用。`ui create ok` 原本只表示构造函数返回；
SDL 初始化、窗口或上下文失败时，该构造函数会直接返回，但旧 main 仍继续创建皮肤。

## 确定的构建错误

libcross2d 的 CMake 在 PS5 分支追加：

```text
-DC2D_PS5_MESA_GL -D__PS5__ -DNO_KEYBOARD
```

随后 COMMON STUFF 又使用 `set(C2D_CFLAGS ...)` 覆盖整个列表，链接选项列表同样被覆盖。
因此 CMakeCache 中开关为 ON，也未让现有 PS5 分支编入旧产物。

实际 r6 二进制请求 OpenGL **4.3**；链接的 G19 SDL 在创建上下文时严格要求 **3.3**，
这个组合必然失败。当前 SDL 的 proc lookup 直接调用本地 eglGetProcAddress，
并非 r4 的动态解析失败重现。此构建的 SDL_SetMainReady 为直接返回，也不是当前根因。

## 修改

1. 两处默认 CMake 列表初始化改为 `list(APPEND ...)`，保留先前的平台选项。
   让已有 1920×1080 / GL 3.3 初始化分支实际生效。
2. native title UI 构造期间把 SDL 日志接入现有 boot log；随后恢复原回调。
3. 构造后先记录 SDL error，再记录 available、window/context、GLAD 基础入口状态。
   只读状态，不通过这些入口调用 GL。
4. 若初始化不完整，在配置/皮肤创建前记录错误并返回；不析构可能继续调用空 GL 入口的半初始化对象。
5. 增加 `skin create begin`；CRT 构建标记更新为 `launch-r7-20261002`。

## 一致构建

新构建目录：`/home/humor/ps5dev/build/pemu-launch-r7`。
旧目录包含 `/mnt/e/Projects/pemu-ps5-port` cache，因此保留它并在独立目录重新配置。

cross2d 的 PUBLIC 编译选项会传给 cross2dui 和 pfbneo native archive。
`NO_KEYBOARD` 影响前端配置枚举编号，必须一致重编三个目标，不能只替换 renderer 对象。
SDL、OpenGL/GPU 库仍使用已有预编译归档。

首次构建发现独立目录缺少生成的 `neo_sprite_func_table.h`；
执行现有 `pfbneo.deps` 生成目标后继续构建，不修改模拟核心源码。

复现配置参数保存在证据目录 `configure-command.json`；顺序为：

```text
cmake 配置新目录（见 configure-command.json）
cmake --build /home/humor/ps5dev/build/pemu-launch-r7 --target pfbneo.deps -j 2
cmake --build /home/humor/ps5dev/build/pemu-launch-r7 --target pfbneo_ps5_native_core -j 6
```

后续打包需使用这个新构建目录的归档。保留 r6 ZIP、ELF、归档和修改前源码供对照。
Git 基线仍为 `9482d136666450743a0c6e19fb96000085b6a273` 加现有未提交改动，无新增提交或推送。

证据目录：`/home/humor/ps5dev/logs/launcher-ui-init-r7-20261002/`。
本机检查不能替代 PS5 实机启动验证。

## 构建结果与产物

- native 编译、FSELF 完整性检查通过。
- 从最终 eboot 提取 ELF 后，5 个 LOAD、11 个源节与 LLVM 校验通过；所有 LOAD 内容与签名前一致。
- 独立反汇编核查确认已编入 1920×1080 / GL 3.3，main 的失败检查位于配置和皮肤创建之前。
- 299 个 cross2d source 文件哈希与 r6 完全一致；修改只在构建配置及前端入口。
- 最终 ELF 与 r6 均有 383 个未定义符号名称，没有新增或删除；包内 runtime、参数和资源均与 r6 一致。
- ZIP 内容和 CRC 检查通过；无日志、ROM 或编译归档进入测试包。

测试包：`/home/humor/ps5dev/build/native-pfbneo/dist/pfbneo-ps5-launch-r7-20261002.zip`。

eboot 大小：`103092851` 字节。

eboot SHA256：`160f4432ce3c348c4cc6fd5b93fb00f5cd298a6611bab371a96626733a2da9b3`。

退出应用后，把 ZIP 内 PPSA99998 内容合并覆盖到 `/data/homebrew/PPSA99998/`，保留个人文件与已生成的数据目录。
测试后提供 `pemu_boot.log` 最后一次 `launch-r7-20261002` 之后的完整记录。
重点检查 SDL error/driver/available/window/context/GL entrypoints，以及 `skin create ok` 和 `main loop begin`。
若出现 `frontend stopped: renderer unavailable`，表示仍有初始化失败，应继续依据前面的 SDL 错误定位。
