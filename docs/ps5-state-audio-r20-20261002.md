# PS5 temporary state allocation and audio buffering r20

Baseline Git9482d136666450743a0c6e19fb96000085b6a273 plus dirty r19, no commit/push. Baseline eboot67e9cadd846cb468f8ce4eaffa902fb8c898361818cee5b63713a4c17b14c46b.

Console r19: ddonpach normal/save+load success; ddpdfk mostly60FPS, slight bomb dips with audio interruption; 0x90c2c64 snapshot allocation fails, main heap live0xd8a7bd1, existing slot preserved. Quit works.

pEMU PS5 state-audio-r20-20261002
安装：PPSA99998 合并覆盖 /data/homebrew/PPSA99998/，保留 ROM/配置/存档。

修复ddpdfk存档OOM：约145MiB的单次快照使用独立、临时的type=0 CPU写回缓存
直接内存映射，不再与游戏的256MiB扩展池/界面的256MiB主堆竞争连续空间。
保存和读取均覆盖；完成后先解除映射，再释放物理内存。小于1MiB的快照使用主堆。
沿用FBNeo原存档布局、临时文件替换、保存失败保留旧槽位。
日志增加 STATE_BUFFER ready bytes=... / STATE_BUFFER released，失败时有对应标记。
FBNeo现有状态模块不支持并发读写；临时大缓冲只允许一个在用，重复申请拒绝。

音频：PS5回调在同一把锁内读取队列，先积累约两帧音频再播放；短缺时播放已有的
完整声道样本，只有不足尾部静音，然后重新预缓冲。不变调、不复制旧样本、不推进
额外模拟帧。增加约两帧启动/恢复缓冲延迟，针对短暂掉帧，不能消除持续供给不足。
不改变当前模拟CPU时钟、保护炸弹效果或渲染正确性。

已通过：交叉编译、SELF完整性、ELF映射及LOAD字节、ZIP CRC；ASan/UBSan测试
四次145MiB快照申请/访问/释放、OOM/映射失败/溢出、64MiB状态往返、旧文件保护；
真实音频回调+SampleBuffer测试预缓冲、样本顺序、部分供给、短缺后恢复。
平台API测试替身不能证明实机资源预算；ddpdfk实机存档恢复与音频效果仍待验证。

实机：ddpdfk进入关卡后保存空槽位，继续游玩，读取并检查关卡/位置/音效恢复；
连续保存/读取2至3次，再试保护炸弹音效。提供pemu_boot.log。
期望 STATE save ok / STATE load ok，且每次大快照都有 STATE_BUFFER released。
可覆盖r19包回退（r19的Cave大存档仍内存不足）。Quit及r19缓存/混合性能修复保持。

## Build / rollback

Build pfbneo_ps5_native_core and cross2d; stage both archives, native-build-pfbneo.sh with existing list. cross2dui build checked up to date and staged archive matches. Source and archive backups: /home/humor/ps5dev/logs/state-audio-r20-20261002/baseline-r19*. Restore these to revert; prior r19 ZIP retained. No SDK/ROM/build outputs committed.

```json
{
  "zip": "/home/humor/ps5dev/build/native-pfbneo/dist/pfbneo-ps5-state-audio-r20-20261002.zip",
  "zip_sha256": "aa8f8ad4e2379ff7186e0b17aed8a0d584ac67706764baa27af73587ff201802",
  "eboot_sha256": "199c78c89c32ec49d11d613e21960ff72e6d15145b03bd8bb6b5641938d4f4f7",
  "console": "pending"
}
```
