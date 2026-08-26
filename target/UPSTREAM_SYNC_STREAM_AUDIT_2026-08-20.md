# 2026-08-20—26 上游 0.14.8585 与 Stream 影响审计

## 同步基线与合并事实

- 实际开发分支：`ben/stream-page-cache-optimization`，同步前 `HEAD` 为
  `4dab1b58d2d305da7e980f830177c53e205e30df`。
- 正式 merge-base 为
  `a8ce24891c1a82cd51a8d0f3f97095a2a5d8aa31`，即上次已审计的标签
  `0.14.8566`。
- 2026-08-20 首次获取后的 `upstream/master` 为
  `318a874db260fdb6ee9208ea8ab445772caab72e`，标签为 `0.14.8583`；当时
  `HEAD...upstream/master` 的左右提交数为 `103 19`。
- 任务延续到 2026-08-26 后再次执行 `git fetch upstream --prune --tags`，
  `upstream/master` 前进到
  `2e629cb7124b788033661c9e8ef8f19a8878b3be`，标签为 `0.14.8585`。
  因此本次共正式吸收 21 个上游提交，版本点包括 `0.14.8578`、
  `0.14.8580`、`0.14.8583`、`0.14.8584` 和 `0.14.8585`。
- 合并前的虚拟三方合并没有发现文本冲突；实际使用
  `git merge --no-ff upstream/master` 合并也没有文本冲突。merge commit 为
  `37ccb4782e362ab24f2375112d5b626c6228620e`，两个 parent 分别是同步前
  `HEAD` 和 `318a874d`。
- merge commit 的 first-parent 差异为 32 个文件、496 additions、
  803 deletions；`git diff --check 37ccb478^1 37ccb478` 没有输出。
- 0.14.8583 的共享实现集成修正以签名提交
  `a87fc4634f480492898c5092220a4e465efd92cc` 保存。随后 0.14.8585 通过
  第二个无冲突签名 merge commit
  `8f7dad4bdd9bb30e70d3ff38b9edb3c05eed304d` 合入，其两个 parent 为
  `a87fc463` 和 `2e629cb7`，first-parent 差异为 2 个文件、
  1348 additions、1329 deletions，`git diff --check` 通过。
- 两侧同时修改了 8 个文件：`examples/dwgadd.c`、`include/dwg.h`、
  `src/decode.c`、`src/dwg.c`、`src/dwg_api.c`、`src/encode.c`、
  `src/in_dxf.c` 和 `src/out_json.c`。实际合并保留了两侧内容；最终语义复核、
  构建和测试均通过。
- 同步前已有未提交用户修改 `src/stream/stream_callbacks.c`。本次 21 个上游
  提交没有修改任何 `src/stream/*` 文件，尤其没有触碰该文件；合并后它仍以
  用户修改状态保留。本次没有清理、覆盖或提交 `.build*`、本地归档、私有 DWG
  或其他用户产物。

## 新增上游提交逐项分类

| Commit | 内容 | Stream 分类与依据 |
| --- | --- | --- |
| `433b79c1` | R12 DXF layout block 名改用 `$MODEL_SPACE` | 仅 DXF 输入命名，与 DWG Stream 读取无关 |
| `54f1529b` | pre-R13 源转换到 R13+ 时仍写 handle stream | DWG 编码路径，与 DWG Stream 读取无关 |
| `4d72ce03` | 各工具接受帮助中已声明的 `r12` 版本字符串 | CLI/版本字符串解析，不改变 DWG Stream 解码 |
| `89e34918` | 接受 MTEXT 的 DXF group 50 | DXF 输入，与 DWG Stream 读取无关 |
| `2e87b847` | 恢复 DXF group 420 的 true-color method 和 RGB-present 标志 | DXF 输入颜色转换，与 DWG Stream 读取无关 |
| `9263d72d` | CMC RGB 降级后可往返时保留 palette index | 颜色转换/编码辅助，不是 DWG Stream 独立读取逻辑 |
| `a0ad6e80` | 只有 fit points 的 SPLINE 归为 scenario 2 | DXF 输入，与 DWG Stream 读取无关 |
| `8db64b26` | SEQEND 后处理不再把相对 handle 重新包装成绝对 handle | DWG 编码路径，与 DWG Stream 读取无关 |
| `38b40ba8` | 无结尾换行的 DXF 不再丢最后一个 group pair | `dwg_read_dxf` 输入路径，与 DWG Stream 读取无关 |
| `56a05f98` | 把 pre-R13 sentinel 搜索从实际约 `+-200` 扩大到文档所述 `+-1000` | **共享实现自动覆盖**：R1-R11 Stream 的 table 读取直接调用共享 `decode_preR13_sentinel()`；无需在 `src/stream/*` 复制搜索逻辑。精确正负边界已由本地集成修正和专项测试锁定 |
| `22df4f0b` | 通过 `make regen-dynapi` 更新对象元数据输入 | dynapi/对象元数据维护，不新增 Stream 独立页面、section 或 object-map 逻辑 |
| `5c1c94ae` | 增加旧版 REPEAT/ENDREP 构造 API | 构造/编码 API，与 DWG Stream 读取无关 |
| `0cacde7f` | 增加 REPEAT/ENDREP add API 测试 | 仅构造 API 测试，与 DWG Stream 读取无关 |
| `46df9439` | `dwgadd` 增加 repeat/endrep 命令、示例和文档 | CLI/示例/编码路径，与 DWG Stream 读取无关 |
| `2d8f2773` | `dwgadd` bare command 接受 CRLF | CLI 文本输入，与 DWG Stream 读取无关 |
| `dbaa9b36` | 限制 `dwg_add_PDFUNDERLAY` 的 `sscanf` 字段宽度 | 构造 API 输入安全修正，与 DWG Stream 读取无关 |
| `b6ef0d0d` | DXF/JSON 输出把 EED 1070/1071 作为有符号值 | 输出路径，与 DWG Stream 读取无关 |
| `4b7d6893` | 修正 ASSOCPERSSUBENTMANAGER 对象布局、spec、dynapi 和测试 | **共享实现自动覆盖**：阻塞式和 Stream 的 decoded object 最终都进入共享 `dwg2.spec` 对象解码器；无需新增 Stream 对象布局。上游配套 DXF 字段和单测问题已在本地集成提交修正 |
| `318a874d` | 增加 ASSOCPERSSUBENTMANAGER 的 DXF 输入 | DXF 输入路径，与 DWG Stream 读取无关；但字段名需随 `4b7d6893` 的新结构修正 |
| `7580bc78` | `CHECK_DICTIONARY_HDR` 已带 `ACAD_` 前缀时不再构造 `ACAD_ACAD_*` 回退名 | DXF 导入后的字典回退日志修正；上游说明输出 DWG byte-identical，与严格 Stream 读取无关 |
| `2e629cb7` | 从 `objects.in` 重新生成 gperf 名称表 | **共享实现自动覆盖**：补入 `_3DFACE`、`_3DSOLID` 别名并修正 `_3DLINE` 映射；Stream 直接使用同一对象名称表，无独立副本 |

分类汇总：3 项由共享实现自动覆盖，0 项需要补写 Stream 独立实现，18 项与
DWG Stream 读取无关。本轮没有改动 Stream 的版本分发、页面缓存、section、
object map、handle map、对象窗口或无回退策略。

## 共享调用路径与合并语义

### pre-R13 sentinel

R1-R11 Stream 在 `src/stream/stream_read_r1_to_r11.c` 的 table 解码中直接调用
`src/decode.c` 的 `decode_preR13_sentinel()` 检查 begin/end sentinel。因此
`56a05f98` 的搜索恢复行为天然覆盖阻塞式和严格 Stream 两条路径，不应在 Stream
读取器中建立第二套 sentinel 搜索。

上游补丁把 `window` 设为 1000，并从 `dat->byte - window` 搜索长度
`2 * window`。由于调用点已读取 16-byte sentinel，且 `memmem` 的候选起点还受
needle 长度约束，本地集成改为真正覆盖 sentinel 起点左右各 1000 bytes，
并用正负边界测试锁定。此项不改变“共享实现自动覆盖”的
分类。

### ASSOCPERSSUBENTMANAGER

`4b7d6893` 把对象结构收敛为 `unknown_bl1`、`unknown_bl2`、`num_steps`、
`steps`、`num_subents`、`subents`、`unknown_bl3` 和 `unknown_b4`，并同步共享
`dwg2.spec`。Stream 对象字节解码最终调用同一个 spec，因此没有独立 Stream
结构需要补齐。

后续 `318a874d` 的 DXF handler 仍使用 `numassocsteps` / `numassocsubents`，没有
与结构字段 `unknown_bl1` / `unknown_bl2` 对齐；同时上游对象单测检查
`unknown_b1`，而实际结构字段是 `unknown_b4`。这两处属于本轮上游集成质量修正，
不代表 Stream 有独立实现；现已修复并增加/调整断言。

## 本地集成修正

1. **sentinel 真正 `+-1000` 边界**

   - `src/decode.c` 以读取前的 sentinel 起点为窗口中心，候选起点精确覆盖
     `-1000..+1000`，搜索范围包含完整 16-byte needle；使用减法式边界检查并
     在文件头尾裁剪，避免 `size_t` 加法溢出。
   - `test/unit-testing/decode_test.c` 的 synthetic 回归覆盖 `-300`、`-608`、
     精确 `-1000`、精确 `+1000` 命中，`-1001` / `+1001` 拒绝，以及
     15-byte short buffer 和越界 offset。最终 `decode_test.exe` 共 26 个断言，
     exit 0。

2. **ASSOCPERSSUBENTMANAGER 字段与测试**

   - `src/in_dxf.c` 的 handler 把前两个 group 90 写入结构实际存在的
     `unknown_bl1` / `unknown_bl2`。
   - `test/unit-testing/assocperssubentmanager.c` 改为检查真实字段
     `unknown_b4`；`test/unit-testing/add_test.c` 新增内存 AC1024 DXF，断言
     `unknown_bl1=17`、`unknown_bl2=23`、`unknown_b4=1`。`add_test.exe`
     exit 0，完整测试中的 `dxf_test.exe` 也通过。

3. **格式化和最终差异**

   - 仅对本次修改的 5 个 C 文件运行仓库 `build-aux/clang-format.sh`。
   - 集成提交和两个 merge 的 `git diff --check` 均通过。
   - 最终 tracked 工作树只有原有用户修改 `src/stream/stream_callbacks.c`；构建
     没有留下待提交生成文件。上游 `src/objects.c` 是 `2e629cb7` 自身提交的
     gperf 生成结果，不是本地构建产物。

## 验证结果

| 验证项 | 结果 |
| --- | --- |
| CMake 构建 `stream_test`、`dwgprobe` | 通过，exit 0；MinGW 仍输出既有 `%z` format warning，不是新增诊断 |
| CMake `stream_test.exe`（严格无 full fallback） | 通过，exit 0；所有报告 `decode_errors=0`、`full=0` |
| sentinel 正/负 1000-byte 边界专项回归 | 通过，`decode_test.exe` 26 tests，exit 0 |
| ASSOCPERSSUBENTMANAGER DXF/单元测试 | 通过，`add_test.exe`、`dxf_test.exe` exit 0 |
| Autotools 严格 `libredwg.la` 构建 | 通过，静态 UCRT 构建 exit 0 |
| Autotools `stream_test.exe` | 通过，exit 0；严格路径无 full fallback |
| Autotools 相关对象/decoder 单测 | `decode_test.exe`、`add_test.exe` 均 exit 0 |
| canonical `make -C .build-autotools-static-codex -j4 check` | 通过，programs 3/3、examples 2/2、unit tests 255/255，整体 exit 0 |
| 仓库 DWG/refs sweep | 通过，`files=123 refs=1`，`decode_errors=0`、`full=0`，exit 0 |
| R2000、R2004、R2007、R2010、R2013、R2018 项目文件 | 6/6 通过，均 exit 0、`decoded==objects`、`decode_errors=0`、`file-map`、无回退 |
| 大型 R2004 `3F.00.dwg` | 1,769,224 objects 全部 decoded，`decode_errors=0`、`r2004-object-map`、`file-map`、无回退、exit 0；保留既有 `warn 0x40` |

真实文件严格 Stream 结果如下；`full/fallback` 均为无：

| 样本 | 版本 | Objects | Entities | Decode mode | Exit |
| --- | --- | ---: | ---: | --- | ---: |
| `qqq.dwg` | R2000 | 152,346 | 102,779 | `r13-object-map` | 0 |
| `A36.1-上海长宁古北路直营店项目总平系统图2026.1.19.dwg` | R2004 | 478,534 | 312,149 | `r2004-object-map` | 0 |
| `A44.3-餐厅平面系统图-2025-厦门市-088-福建厦门海沧旅游码头加盟全季项目.dwg` | R2007 | 174,505 | 134,457 | `r2007-object-map` | 0 |
| `B01.37-未知品牌-酒店大堂参考.dwg` | R2010 | 492,855 | 473,695 | `r2004-object-map` | 0 |
| `A35.1-海宁奥特莱斯建国璞隐酒店-平面图1120.dwg` | R2013 | 247,813 | 223,912 | `r2004-object-map` | 0 |
| `北京湾里建国璞隐项目（直营）平面方案.dwg` | R2018 | 174,161 | 154,222 | `r2004-object-map` | 0 |
| `3F.00.dwg` | R2004 | 1,769,224 | 1,081,856 | `r2004-object-map` | 0 |

真实文件验证只读取项目所有者已提供的现有资料，不下载、不复制、不修改或提交
额外 DWG。大型 `3F.00.dwg` 只运行 Stream 验收，不进行无限制阻塞式解码。

## 尚未进入 master 的相关 PR

2026-08-26 再次复核 GitHub 状态，以下 PR 均仍为 OPEN、未进入本次
`upstream/master`，因此没有 cherry-pick：

- [PR #1358](https://github.com/LibreDWG/libredwg/pull/1358)，
  `decode: r2004 sections whose declared size exceeds the page estimate
  (fixes #1294)`：修改阻塞式 R2004 section 分配和逐页边界。Stream 有独立
  R2004 对象页、窗口与解压上限；未来进入 master 时仍需逐项审计。
- [PR #1360](https://github.com/LibreDWG/libredwg/pull/1360)，
  `decode: resync the object map when a modular char fails to parse
  (fixes the data loss in #1355)`：修改阻塞式 handle/object map 的失败重同步。
  Stream 有独立 object-map 迭代路径，未来合入时必须确认同等 resync、页边界和
  错误传播。
- [PR #1368](https://github.com/LibreDWG/libredwg/pull/1368)，
  `r11: give pre-R13 drawings their paper space BLOCK_HEADER
  (fixes #1337)`：修改 pre-R13 文档构造、实体所有权和 DXF block name。
  R1-R11 Stream 独立组装 BLOCK_HEADER 所有权与实体列表，未来合入时必须审计
  paper-space `entmode` 分流和 owner 语义。
- [PR #1364](https://github.com/LibreDWG/libredwg/pull/1364)，
  `decode_r11: a missing table sentinel must not reject the drawing
  (fixes #767)`：处理 sentinel 真正缺失而不是仅发生位移的情况。它与已进入
  master 的 `56a05f98` 搜索窗口修正互补；未来合入时必须同时验证阻塞式和严格
  R1-R11 Stream 的空表、地址、CRC 与错误传播。当前分支已有 `dcc34af5` 对
  bounds-valid 表缺失 sentinel 的容错，未来合入时应做语义比较而不是重复套用。

按 `SYNC.md` 继续等待这些改动进入 `upstream/master`，不绕过正式同步基线
单独 cherry-pick。

## 已知边界与完成检查表

- [x] 工作树、分支和远端基线已确认，已有用户修改未被覆盖。
- [x] 已获取并合并 `upstream/master`，保留可追踪 merge commit。
- [x] 21 个新增上游提交已逐项完成 Stream 调用路径分类。
- [x] 虚拟与实际合并均无文本冲突。
- [x] sentinel 精确搜索边界修正和专项回归完成并回填。
- [x] ASSOCPERSSUBENTMANAGER 字段/测试修正完成并回填。
- [x] CMake 必验目标通过并回填。
- [x] Autotools 严格构建和测试通过并回填。
- [x] 项目真实现代 DWG 完成严格无回退验证并回填。
- [x] 最终差异、格式、生成物和用户修改保留情况复核完成。
- [x] 集成修正和两个 merge commit 均为有效签名；2026-08-26 首次推送
  `4dab1b58..8f7dad4b` 成功，推送检查点本地/远端为 `0 0`。本审计记录作为
  后续独立文档提交推送。

上游这 21 个提交没有改变 Stream 支持版本、严格无回退政策或能力优先级，因而
无需更新 `README`、`target/README.md` 或 `target/STREAM_BLOCKING_PARITY.md`。

## ChangeLog 与最终结论

按照仓库 `AGENTS.md` / `HACKING` 约定，日常提交不直接编辑顶层
`ChangeLog`；发布 ChangeLog 由 GNU 风格提交日志生成。本文件记录可追踪 merge、
逐项 Stream 分类、后续集成修正和最终验证证据，生成文件不作为本地补丁提交。

最终结论：上游 `0.14.8566..0.14.8585` 的 21 个提交已经通过签名 merge commit
`37ccb478` 和 `8f7dad4b` 进入当前 Stream 优化分支，且没有覆盖现有用户修改。
三个共享实现影响均自动覆盖严格 Stream，Stream 独立补丁为 0；sentinel 精确窗口
和 ASSOCPERSSUBENTMANAGER 集成问题已在签名提交 `a87fc463` 修正。CMake、静态
Autotools、canonical `make check`、123 文件 sweep、六代真实项目样本和 176 万
对象大型样本均通过。本轮同步可以判定完成。
