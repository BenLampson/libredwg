# Stream 上游同步与 R2007 修复审计（2026-09-04）

## 结论

本轮始终以 `ben/stream-page-cache-optimization` 为产品分支，没有把旧 R2007
实验分支整头合并。已将 LibreDWG `upstream/master` 从 `2e629cb7` 更新到
`7eb90a9f`（0.14.8594），并把旧分支中经真实文件验证有意义的 R2007 高压缩
section 修复按当前 Stream 代码净移植。CMake Stream 回归、Autotools 严格构建、
完整 `make check`、六代现代真实文件、大型 R2004 和高压缩 R2007 实样均通过。

BenLampson fork 和本地仓库最终都只保留一个命名分支：
`ben/stream-page-cache-optimization`。旧 worktree 仅改为 detached，未删除目录、
构建产物或可恢复历史。

## 同步基线

| 项目 | Commit |
| --- | --- |
| 同步前 Stream HEAD | `99769aa551dd4eaccbf73f63a29d76e3f84c382e` |
| 同步前共同上游基线 / merge-base | `2e629cb7124b788033661c9e8ef8f19a8878b3be` |
| `git fetch upstream --prune --tags` 后的 `upstream/master` | `7eb90a9f933623729f82781cb1d68de2e50593f3` |
| 上游合并 commit | `53a273867eda7ce8c77146b9aaf334bbd571d858` |
| R2007 Stream 修复 commit | `0727e02a895d9db2d8ea60c8bb18edcb3fc11953` |

上游合并使用可追踪的 `--no-ff` merge。合并后
`git rev-list --left-right --count HEAD...upstream/master` 为 `114 0`：Stream 保留
自己的产品提交，同时没有遗漏 `upstream/master` 提交。

## 新增上游修复及 Stream 覆盖判断

| Commit | 内容 | 分类与 Stream 结论 |
| --- | --- | --- |
| `014c83b8` | `bit_copy_chain` 按实际追加长度扩容；EED 初始化改用公共 helper | 共享覆盖。修复大块追加超过固定 4096 字节时静默丢数据；Stream 所在库的公共编码层直接受益，无需 Stream 特有补丁。 |
| `7f46cfa4` | DXF 省略 MTEXT 72/73/44 时恢复合法默认值 | 非 Stream 读取路径，但属于同一库的 DXF 导入正确性修复，随官方基线保留。 |
| `138d0b96` | MTEXT `bg_fill_scale` 从错误的 `BL` 改为 `BD` | 共享覆盖。Stream callback 使用公共对象 decoder 和 `dwg.spec`，因此自动获得正确的双精度字段解码。 |
| `8a1ae764` | 同步 MTEXT dynapi 类型和生成文档/测试 | 共享覆盖。与上一项保持 API、结构体和动态字段描述一致，无独立 Stream 实现。 |
| `14032c99` | 拒绝越界 codepage table index | 共享覆盖且有安全意义。Stream 字符串和对象层使用公共 codepage helper，越界值不再导致数组外读取或野指针解引用。 |
| `7eb90a9f` | 缩短 codepage 边界回归 | 仅整理上一项测试，不改变运行时语义。 |

这六项均可由官方 merge 直接覆盖。没有发现需要复制到 `src/stream/` 的独立实现，
因此没有制造重复逻辑。

## 旧 R2007 分支审视与净移植

审视了 fork 上的三个旧远端分支：

- `ben/r2007-high-compression-guard`（`3af428fa`）确认原 `data_size > 10 *
  file_size` 是错误经验阈值。真实 18,625,504 字节 R2007 文件的 object section
  解压数据为 231,554,810 字节（12.43 倍），会被当前 Stream 错拒。
- `ben/r2007-nominal-page-size`（`51ce08c7`）正确修正前一提交中的二次误判：
  `max_size` 是名义页目标，writer 可为保持对象完整而生成略大的页，不能用
  `max_size * num_pages` 当 section 的硬容量。
- `ben/r2007-section-budget` 的后续三个提交只增加 `dwgprobe` 大对象、字典和页面
  诊断，没有改变解码正确性，不作为产品修复合入。

这些分支从旧 Stream `88eaf289` 分叉，落后当前 Stream 约 40 个产品提交。整头
合并会回退新版已有的 R2007 非压缩 Reed-Solomon 页识别、FileDepList、sentinel
边界和其他 Stream 能力，因此只移植最终有效语义：

- 用既有的 `0x2f000000`（约 790 MiB）绝对解压上限替代 10 倍文件大小阈值；
- section 名称长度必须可落在文件中且小于 48；
- 非空 section 必须有正的名义页尺寸、正页数，页数上限为 `0xf0000`；
- 实际页面 offset、size 和解压输出范围继续由后续 page-map/data-page 逻辑校验；
- 同一 validator 同时供阻塞式和新版 Stream 的 R2007 section-map 入口使用。

修改文件为 `src/decode_r2007.c`、`src/stream/stream_r2007_internal.h` 和
`test/unit-testing/decode_test.c`。C 文件已通过仓库 `build-aux/clang-format.sh`
定向格式化，`git diff --check` 通过。

## 验证结果

| 验证项 | 结果 |
| --- | --- |
| CMake 构建 `stream_test`、`dwgprobe` | 通过，exit 0；仅有既存 MinGW `%hhx` / `%zu` format warnings |
| CMake `stream_test.exe` | 通过，exit 0；仓库 parity 为 `decode_errors=0`、`full=0` |
| Autotools 静态 `libredwg.la` | 通过，满足规范构建路径和严格 C 编译 |
| Autotools `stream_test.exe` | 通过，exit 0 |
| Autotools `decode_test.exe` | 通过，新增 R2007 高压缩、绝对上限、名义页尺寸、零页和零页尺寸断言均通过 |
| Autotools `bits_test.exe` | 通过；新增 `out-of-range codepage rejected` 回归通过 |
| canonical Autotools `make check -j4` | 通过；programs 3/3、examples 2/2、unit tests 255/255 |

CMake 的非必需独立 `decode_test` 目标仍有既存 Windows 链接配置缺口：该目标自行
编译 `dwg.c`，但没有链接提供 `dwg_decode_stream` 的 Stream reader object。
本次修改没有引入该问题；同一 `decode_test` 在规范 Autotools 配置下已成功链接并
通过。为避免把无关构建系统修整混进解码修复，本轮未扩大改动范围。

真实文件均使用当前 CMake `dwgprobe -d --` 的严格 Stream file-map 入口；所有文件
`decoded == objects`、`decode_errors=0`、无 full fallback：

| 样本 | 版本 | Objects | Entities | Decode mode | 状态 |
| --- | --- | ---: | ---: | --- | --- |
| `qqq.dwg` | R2000 | 152,346 | 102,779 | `r13-object-map` | ok |
| `A36.1-上海长宁古北路直营店项目总平系统图2026.1.19.dwg` | R2004 | 478,534 | 312,149 | `r2004-object-map` | 既存非致命 `0x40` |
| `A44.3-餐厅平面系统图-2025-厦门市-088-福建厦门海沧旅游码头加盟全季项目.dwg` | R2007 | 174,505 | 134,457 | `r2007-object-map` | ok |
| `B01.37-未知品牌-酒店大堂参考.dwg` | R2010 | 492,855 | 473,695 | `r2004-object-map` | ok |
| `A35.1-海宁奥特莱斯建国璞隐酒店-平面图1120.dwg` | R2013 | 247,813 | 223,912 | `r2004-object-map` | ok |
| `北京湾里建国璞隐项目（直营）平面方案.dwg` | R2018 | 174,161 | 154,222 | `r2004-object-map` | ok |
| `3F.00.dwg` | R2004 | 1,769,224 | 1,081,856 | `r2004-object-map` | 既存非致命 `0x40` |
| `a70-c117107f.dwg` 高压缩实样 | R2007 | 460,871 | 254,384 | `r2007-object-map` | ok |

高压缩实样文件大小为 18,625,504 字节，解码后对象总字节 229,711,310，最大对象
44,812,645 字节；新版 Stream 使用 `file-map`，RSS 约 128 MiB，460,871 个对象
全部解码且错误为 0。修复前同一 Stream 会返回 `DWG_ERR_INVALIDDWG`，没有版本或
对象结果。这是本轮 R2007 修复有意义的直接证据。

大型 `3F.00.dwg` 的对象数仍为 1,769,224，RSS 约 370 MiB。其既存 iconv 诊断和
非致命 `0x40` 不改变对象总数、错误数、入口模式或退出码。全部真实文件来自项目
所有者已有资料，没有下载、复制、修改或提交额外 DWG。

## 尚未进入 master 的相关 PR

2026-09-04 通过 GitHub CLI 复核：

- [PR #1358](https://github.com/LibreDWG/libredwg/pull/1358)、
  [PR #1360](https://github.com/LibreDWG/libredwg/pull/1360)、
  [PR #1364](https://github.com/LibreDWG/libredwg/pull/1364) 和
  [PR #1368](https://github.com/LibreDWG/libredwg/pull/1368) 仍为 OPEN，不属于
  本轮 `upstream/master`，继续不做 cherry-pick。
- [PR #1401](https://github.com/LibreDWG/libredwg/pull/1401) 已于 2026-09-03
  合并，其 codepage 安全修复已包含在本轮官方基线中。

## 分支与用户工作保护

同步前的 `.gitignore`、`test/unit-testing/CMakeLists.txt` 和未跟踪
`test/unit-testing/stream_oracle.c` 使用包含 untracked 文件的 stash 原样保护；
同步和推送完成后恢复，不把它们混入本轮提交。`oracle-work/` 内容未删除或改动。

已删除本地命名分支 `ben/r2007-nominal-page-size`、
`ben/r2007-section-budget`，以及 fork 远端的
`ben/r2007-high-compression-guard`、`ben/r2007-nominal-page-size`、
`ben/r2007-section-budget`。两个关联 worktree 在确认干净后转为 detached，仍分别
保留在 `51ce08c7` 和 `5987a50b`，因此实验历史仍可恢复。`git ls-remote --heads
origin` 最终仅返回 `ben/stream-page-cache-optimization`。

本轮没有改变 Stream 支持版本、严格无回退策略或开发优先级，因此无需同步修改
根 `README`、`target/README.md` 或 `target/STREAM_BLOCKING_PARITY.md`。
