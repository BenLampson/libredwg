# 2026-07-30 上游 0.14.8553 与 Stream 影响审计

## 同步基线

- 实际开发分支：`ben/stream-page-cache-optimization`，同步前 `HEAD` 为
  `e22b7428a3202869b3161a4eefad207288ac54c3`。
- 同步开始时 `SYNC.md` 仍将 `codex/libredwg-c-stream` 写作固定分支；本次没有
  切换到该旧分支，因为当前分支已经包含最新的 Stream 分页缓存优化。同步目标和
  验证规则仍完全按照该文档执行，并已把固定旧分支名改为读取实际当前分支，避免
  下次同步丢掉后续 Stream 能力。
- 正式 merge-base：
  `ed121c68a63b98f5b58bf470e25b116e4e2e5a52`。
- 获取后的 `upstream/master`：
  `d3a0a2dc1fdab5737bc6036db2d705300e6e59b6`
  。首次获取时描述为 `0.14.8547-2-gd3a0a2dc`；最终提交前复取时上游在同一
  commit 发布了标签 `0.14.8553`，没有增加代码提交。
- 同步前 `HEAD...upstream/master` 的左右提交数为 `97 7`；本次需吸收 7 个
  上游提交。
- 使用 `git merge --no-ff upstream/master` 合并，merge commit 为
  `4e93b3cf927023d39c686a1f02844859f2dc7604`，无文本冲突。
- 最终提交前再次 `git fetch upstream --prune --tags`，主线仍为上述 commit，
  此时 `HEAD...upstream/master` 为 `98 0`，不存在漏同步的新主线提交。
- 同步前工作树干净。本次没有清理、覆盖或提交已有 `.build*`、`target` 压缩包、
  私有 DWG 或其他本地产物。

## 新增上游提交逐项分类

| Commit | 内容 | Stream 分类与依据 |
| --- | --- | --- |
| `11865209` | 新增 `dwg_add_3DLINE` API，并调整 3DLINE 的公开/生成元数据 | 共享对象元数据自动覆盖 Stream；add API 属构造/写入路径 |
| `310845e0` | 为 `dwgadd` 增加 `3dline` 命令 | 工具与文档，不参与 DWG Stream 读取 |
| `e95cbd63` | 增加 `dwg_add_3DLINE` 测试 | add/DXF 测试，不是 Stream 独立实现 |
| `e402b6d1` | 让 `dwg_add_LINE` 委托 `dwg_add_3DLINE` | 构造/编码路径，不参与 Stream 读取 |
| `e0a152d7` | 修正 3DLINE stable 元数据、DXF R9 归属/handle，并重写 LINE 转型 | 元数据和对象解码的共享部分自动覆盖；其余为 add/DXF 路径 |
| `165e8793` | 修正 AC1009 二维 LINE 的 R11 elevation 标志 | 构造/编码路径；R11 Stream 仍使用共享 `dwg_decode_LINE` |
| `d3a0a2dc` | pre-R10 3DFACE 全零 Z 时不设置 elevation 标志 | 构造/编码路径；Stream 3DFACE 包装仍调用共享对象解码器 |

本批没有只修改阻塞式 page、section、object map、handle map 或版本分发而要求在
`src/stream/*` 复制的上游修复。

## 3DLINE 实际 Stream 调用路径

3DLINE 存在于 R2.4 至 R10（含 R10）的格式，走 R1-R11 Stream 专用入口：

1. `src/stream/stream_reader.c` 分发到
   `src/stream/stream_read_r1_to_r11.c`。
2. `stream_read_r1_to_r11.c` 的 raw type 21 分支调用
   `dwg_stream_decode_pre_r13__3DLINE`。
3. `src/decode.c` 中该包装函数直接返回
   `dwg_decode__3DLINE(dat, obj)`；阻塞式读取器的 raw type 21 分支也调用同一个
   `dwg_decode__3DLINE`。
4. 两条路径因此共用 `dwg_setup__3DLINE`、`dec_macros.h` 生成的对象解码器和
   `dwg.spec` 字段布局。
5. `stream_pre_r13_object_info` 从解码后的对象复制实际
   `type`、`fixedtype`、`name`、`dxfname` 和 `supertype`。

因此上游的 3DLINE 类型/稳定性元数据变化已经覆盖独立 Stream 读取器。
`dwg_stream_fixed_type_is_entity()` 没列 `_3DLINE` 不是遗漏：该辅助函数服务于
R13+ 对象路径，而 3DLINE 只走上述 pre-R13 分支。

## 合并后补齐的上游集成回归

合并本身没有覆盖本分支 Stream 文件，但完整构建和 API 调用审计发现了四项实际
功能回归，并以独立补充提交修复：

1. **公开 getter/setter 符号缺失。** 上游把 `_3DLINE` 从 stable-variable
   列表移到 fixed 类型后，保留了 `dwg_get__3DLINE` /
   `dwg_set__3DLINE` 的头文件声明，却没有在 fixed API 列表补回实现。
   `src/dwg_api.c` 现已恢复这两个导出符号，`3dline.c` 会直接调用它们，避免只
   编译声明却漏掉链接实现。
2. **前导下划线类型名无法通过公开 getter。** `dwg_get_OBJECT` 原来把
   `_3DLINE` 原样传入 dynapi，但 dynapi 的 canonical 名是 `3DLINE`。
   现在 getter/setter 宏统一去掉 C 标识符的一个前导下划线；同类
   `_3DFACE`、`_3DSOLID` 也一并得到正确行为。
3. **pre-R10 非零 Z LINE 被危险地“只改标签”。** 上游 `e0a152d7` 先分配
   `Dwg_Entity_LINE`，再把对象的 `fixedtype` 改成 `_3DLINE`；两种结构布局
   不同，编码和释放会把 LINE 内存当 `_3DLINE` 解释，可能产生错误坐标或越界。
   直接委托又会把物理 `_3DLINE` 指针伪装成公开返回类型
   `Dwg_Entity_LINE *`，调用者按 SDK 契约读取字段仍会错位。现在
   `dwg_add_LINE` 对该组合在分配前返回 NULL，要求调用显式且类型安全的
   `dwg_add_3DLINE`。R10/R11 的 `HAS_ELEVATION` 修复仍保留。
4. **`dwgadd 3dline` 错收 R11 beta。** 工具原条件 `version < R_11` 会把
   R11b1/R11b2 传给只接受到 R10 的 `dwg_add_3DLINE`，必然返回 NULL。条件现
   收紧为 `version <= R_10`；R11 beta 和正式 R11 使用 LINE。

对应回归覆盖包括：

- `_3DLINE` 的 start/end/thickness/extrusion 语义 hash，确保阻塞式和严格
  Stream 不只是对象数相同，而是字段相同。
- R2.4、R2.5、R9c1 先断言 `dwg_add_LINE(z != 0)` 安全拒绝且不残留对象，
  再通过 `dwg_add_3DLINE` 生成文件，断言物理分配、raw type 21、坐标和 opts
  位正确，并做 `DWG_STREAM_F_NO_FULL_FALLBACK` 回读。
- R10、R11 正式版二维 LINE 断言 `FLAG_R11_HAS_ELEVATION`，再做严格 Stream
  回读。
- `3dline.exe` 直接调用公开 getter/setter，锁定 SDK 链接和运行时行为。

没有修改任何 `src/stream/*` 文件；本次补丁是上游公共 API/写入集成修复和
Stream 交叉回归，不是复制一份 Stream 解码逻辑。

## 验证结果

- CMake `stream_test`、`dwgprobe` 增量构建成功。
- CMake `stream_test.exe`：退出码 0。
- 启用 `LIBREDWG_STREAM_TEST_REPOSITORY_SWEEP=1` 和
  `LIBREDWG_STREAM_TEST_REFS=1` 的严格仓库扫描：退出码 0，
  `files=123 refs=1`；所有已解码对象 `full=0`，最终语义覆盖包含 block、
  INSERT、dimension、poly3d、HATCH、WIPEOUT、TEXT/MTEXT 和所有权关系。
- Autotools 规范构建 `libredwg.la`、`stream_test.exe`、`3dline.exe` 和
  `add_test.exe` 成功，满足 C99 与 `-Werror`。
- Autotools `stream_test.exe`、`3dline.exe`（R9 fixture）和
  `add_test.exe` 均退出 0。
- Shared Autotools DLL 重链成功；`nm` 和 PE export table 均确认
  `dwg_add_3DLINE`、`dwg_get__3DLINE`、`dwg_set__3DLINE` 三个导出。
  当前头文件重链的动态 `3dline.exe`、`add_test.exe` 和
  `stream_test.exe` 均加载该 DLL 并退出 0。
- 完整 Autotools `make check` 退出 0：260 PASS，0 FAIL，0 SKIP。
  负向损坏输入和 `dwg_add_LINE` 安全拒绝用例会按预期打印内部 `ERROR:`，
  但测试本身均 PASS。
- `dwgadd` 的 `r10`、`r11b1`、`r11b2`、`r11` 3dline 小文件实测均退出
  0、产生非空 DWG，且没有 `Invalid entity 3DLINE`。
- `git diff --check` 通过。

Windows CMake/MinGW 仍会打印已有的 `%hh` 格式分析告警；Autotools 规范严格
构建没有对应错误。仓库格式化脚本使用本机 clang-format 22 时会对
`src/dwg_api.c` 产生数千行与本次无关的版本差异，因此该机械改排版已完整回退；
功能补丁保持周边现有样式，并由严格编译和 `git diff --check` 验证。

## 项目真实 DWG

项目私有语料只在原位置读取，没有复制、修改或提交。最终源码重链后，正常业务
语料 20/20 通过，覆盖 R2000、R2004、R2007、R2010、R2013 和 R2018：

- 总对象 6,347,881，实体 5,410,837，非实体 937,044；
- 每个文件均为 `decoded=objects`、`decode_errors=0` 和 `file-map`；
- 7 个代表文件对象数分别为 R2000 152,346、R2004 478,534、
  R2007 174,505、R2010 492,855、R2013 247,813，以及两个 R2018 文件
  114,441 / 174,161。

大型 R2004 `3F.00.dwg` 继续作为独立 Stream-only 回归，不执行无限制阻塞式
解码。验收结果为：

- `decode_mode=r2004-object-map`；
- `input_mode=file-map`；
- 对象数 1,769,224；
- `decode_error_objects=0`；
- 保留输入原有非致命警告 `0x40`；
- page-cache 和 low-memory 两种严格 harness 均使用
  `DWG_STREAM_F_NO_FULL_FALLBACK`，对象/实体/非实体统计逐字段相同；
- 两种模式均为 `full=0`、`file_map=1,769,224`、`heap=0`，没有调用
  `dwg_read_file` 回退。

中文路径/内容仍会打印已有的 `iconv errno 22` 日志噪声，不影响退出码、对象
统计或解码结果。

## 尚未进入 master 的相关 PR

2026-07-30 使用 GitHub 当前状态复核：

- PR #1328 已关闭但未以 PR merge 标记；它的 3DLINE 主题由本次
  `11865209` 至 `e0a152d7` 等主线提交吸收。
- PR #1339 至 #1343 仍为 OPEN，只涉及 `src/in_dxf.c` 的 DXF 重建释放/扩容。
- PR #1324 仍为 OPEN，只涉及 bits 等测试改进。
- PR #1312、#1314、#1319、#1321 页面仍为 OPEN，但此前已审计其主题与当前
  主线关系；没有必须绕过 `master` 单独 cherry-pick 的 Stream 修复。

本次默认只合并 `upstream/master`，没有手工同步任何未进入主线的 PR。

## 已知边界与后续项

- `gen-dynapi.pl` 现在会生成多个前导数字类型的下划线 alias，而 checked-in
  `objects.in` / `objects.c` 只显式加入 `_3DLINE`，且 alias 的 dxfname 为
  `_3DLINE`。本次不提交上游生成物，也不擅自改变其 alias 策略；公开
  `_3DLINE` getter 已通过 canonical 名修复并有链接/运行测试。生成器与
  checked-in 表的统一应由上游单独处理。
- 上游 `dwg_add_3DFACE` 的 pre-R10 mixed-Z `opts_r11` 第三/第四点位值疑似仍
  使用 `3/4` 而非声明和解码要求的 `4/8`。这是本批提交之前的既存问题，不混入
  本次同步补丁；应另立修复和 round-trip 测试。
- `165e8793` 和 `d3a0a2dc` 的版本条件不覆盖 R11 beta。现有 beta 用例保持
  原布局且通过；若上游确认 beta 也应使用正式版标志，应另行修正。

## ChangeLog 与结论

按照仓库 `AGENTS.md` / `HACKING` 约定，日常提交不直接编辑顶层
`ChangeLog`；发布 ChangeLog 由 GNU 风格提交日志生成。本文件记录完整同步审计，
补充提交使用带逐文件说明的 GNU 风格 commit message。

本次同步没有改变支持版本、严格无回退策略、能力结论或开发优先级，因此无需
改动根目录 `README`、`target/README.md` 或
`target/STREAM_BLOCKING_PARITY.md`。7 个上游提交已完整进入当前 Stream
优化分支；读取路径无需额外 `src/stream/*` 修复，但合并审计发现并补齐了公开
SDK 符号和 pre-R10 3DLINE 物理布局回归。
