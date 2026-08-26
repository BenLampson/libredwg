# 2026-08-26 上游与 Stream 完整同步审计

## 结论

- 重新执行 `git fetch upstream --prune --tags` 后，正式基线
  `upstream/master` 仍为
  `2e629cb7124b788033661c9e8ef8f19a8878b3be`（`0.14.8585`）。它同时也是
  当前分支的 merge-base；本轮结论为：**主线无更新**。
- 同步检查开始时，开发分支 `ben/stream-page-cache-optimization` 的 `HEAD` 为
  `2d9f92c0bf8a9bcb27cd92f077cf0c3885935921`；
  `HEAD...upstream/master` 左右提交数为 `108 0`。因为上游新增提交数为 0，
  本轮没有制造空 merge，也没有从开放 PR 绕过正式基线 cherry-pick。
- `2d9f92c0` 已让 R2007 和 R2004-family Stream 在 decoded-object callback 前
  解码 FileDepList，并在临时 callback parent 的生命周期内只读借用完整元数据。
  本轮继续核对阻塞式与严格无回退 Stream，修正了 R2007+ FileDepList 尾部数值
  字段的实际顺序，补齐全部字段一致性与三代精确值回归。
- 本轮还修复了 decoded-object callback 临时 `Dwg_Data` 借用宿主 ACIS SAB
  handle queue 的所有权错误。临时解码现在使用自己的空队列，并释放解码期间
  创建的全部 non-global handle refs 及队列，不会 `realloc`、悬空或破坏宿主队列。
- CMake、Autotools、默认 Stream 回归、123 文件 refs sweep，以及 R2000、
  R2004、R2007、R2010、R2013、R2018 和大型 R2004 共 7 个真实文件均通过严格
  无 full fallback 验证。
- `origin` 只有 `ben/stream-page-cache-optimization` 一个远端分支；远端 `HEAD`
  也指向该分支，没有多余的有效主分支。

**本轮代码提交：`db4e97a22fd95bbb389613b4901cd2c05f7130a4`**

## 同步事实

| 项目 | 结果 |
| --- | --- |
| 开发分支 | `ben/stream-page-cache-optimization` |
| 同步检查开始 `HEAD` | `2d9f92c0bf8a9bcb27cd92f077cf0c3885935921` |
| `upstream/master` | `2e629cb7124b788033661c9e8ef8f19a8878b3be` |
| merge-base | `2e629cb7124b788033661c9e8ef8f19a8878b3be` |
| `HEAD...upstream/master` | `108 0` |
| 上游新增提交 | 0，主线无更新 |
| 合并动作 | 无；按 `SYNC.md` 不创建空 merge |
| `origin` heads | 仅 `refs/heads/ben/stream-page-cache-optimization` |
| `origin` 默认分支 | `ben/stream-page-cache-optimization` |

2026-08-20—26 的上一次审计已经记录从 `0.14.8566` 到 `0.14.8585` 的 21 个
上游提交、两个 merge commit 和本地集成修正。本轮上游 commit 区间为空，因此
没有需要重新分类的上游补丁；工作重点是对已经合入的共享解码与 Stream 独立
callback 路径做更深的字段和所有权审计。

## Stream 审计与修复

### FileDepList callback 可见性

提交 `2d9f92c0` 已经完成以下 Stream 独立路径同步：

- R2007 Stream 在对象 callback 前读取 FileDepList section；
- R2004、R2006、R2010、R2013、R2018 和 R2022 共用的 R2004-family Stream
  路径在对象 callback 前读取 FileDepList section；
- 临时 callback parent 只读借用宿主 `filedeplist`，其有效期明确限制在 callback
  内；对象释放不会重复释放宿主元数据；
- 测试入口强制使用 `DWG_STREAM_F_NO_FULL_FALLBACK`，没有用
  `dwg_read_file` 替代 Stream 解码结果。

本轮把测试从“首个 feature 与 filename 可见”提升为完整结构一致性：

- feature/file 数量完全一致；
- 比较每一个 feature；
- 比较每个 file 的 `filename`、`filepath`、`fingerprint`、`version`；
- 比较每个 file 的 `feature_index`、`timestamp`、`filesize`、
  `affects_graphics`、`refcount`；
- 对 R2007+ TU 字符串先转为 UTF-8 再比较，对旧版本保留窄字符串比较规则；
- callback 所见结果必须与同一 fixture 的阻塞式基线逐字段一致。

### FileDepList 尾字段布局

完整字段检查发现原共享 `src/filedeplist.spec` 把所有版本都按 R2004 的尾字段
顺序读取。实际 R2007+ 文件把 `feature_index` 放在其他四个数值字段之后。现按
版本分流：

| 版本 | 尾字段顺序 |
| --- | --- |
| R2004-family 且早于 R2007 | `feature_index`、`timestamp`、`filesize`、`affects_graphics`、`refcount` |
| R2007 及以后 | `timestamp`、`filesize`、`affects_graphics`、`refcount`、`feature_index` |

这是**共享实现修正**：阻塞式读取器与 Stream section reader 都包含同一个
`filedeplist.spec`，因此两条路径同步获得相同字段语义，不在 Stream 中维护第二套
parser。R2004 保持
[ODA 官方 DWG 规范](https://static.opendesign.com/files/guestdownloads/OpenDesign_Specification_for_.dwg_files.pdf)
所述布局，没有因现代文件修正而改变。

R2007、R2010、R2013 三代 `Line.dwg` fixture 新增精确字段断言。首个文件
`arial.ttf` 的期望值均为 `feature_index=0`、`timestamp=995532864`、
`filesize=772192`、`affects_graphics=1`、`refcount=2`；同时仍要求全部 feature、
全部 file 及其字符串/数值字段与阻塞式结果一致。

原有 `gh44-error.dwg` 长 XRef 路径仍作为 callback 可见性 smoke test 保留；该
fixture 不用于声明全部数值字段的语义正确性。

### 临时 ACIS SAB handle queue 所有权

`dwg_stream_emit_decoded_object_ex()` 使用临时 `Dwg_Data` 解码单个对象。旧逻辑把
宿主的 `num_acis_sab_hdl` / `acis_sab_hdl` 浅拷贝进临时 parent；R2013+ 含
`has_ds_data` 的 3D solid 解码会通过 `PUSH_HV` 扩展该队列。这会让临时解码对
借用的宿主指针执行 `realloc`，可能导致宿主指针失效、重复释放，并泄漏临时创建
的 `dwg_add_handleref_free()` non-global refs。

本轮的 **Stream 独立实现修正** 为：

- 临时 parent 的 ACIS SAB handle queue 始终从 `0` / `NULL` 开始，不借用宿主；
- callback 结束时逐个释放临时 non-global ref，再释放临时向量；
- 保留宿主 queue 的数量、地址及元素不变；
- 单元回归预置非空宿主 queue，要求 callback 只看到隔离的临时空 queue，完成后
  宿主计数、指针和 ref 完全未改变。

Stream 的 ACDS section 仍由 R2004-family 页面路径读取并直接为对应对象附加 SAB
数据；上述临时 queue 只属于共享对象 decoder 的中间关联状态，隔离和清理不会
移除 callback 对最终对象 ACDS 数据的访问。

## 验证结果

| 验证项 | 结果 |
| --- | --- |
| CMake 构建 `stream_test`、`dwgprobe` | 通过，exit 0；仅有既存 MinGW `%hhx` / `%zu` format warnings |
| CMake 默认 `stream_test.exe` | 通过，exit 0；`decode_errors=0`、`full=0` |
| Autotools 静态 `libredwg.la` | 通过，exit 0，满足 C99 严格构建 |
| Autotools `stream_test.exe` | 通过，exit 0 |
| canonical Autotools `make check` | 通过；programs 3/3、examples 2/2、unit tests 255/255 |
| 仓库 DWG + refs 完整 sweep | 123 files、91,206 objects；全部 decoded，`decode_errors=0`、`full=0`、exit 0 |
| FileDepList 三代精确值与完整 parity | R2007、R2010、R2013 均通过 |
| callback ACIS queue 隔离回归 | 通过；非空宿主 queue 保持不变 |

真实文件均通过 `dwgprobe -d --` 的严格 Stream 文件映射入口，
`decoded == objects`、`decode_errors=0`、无 full fallback：

| 样本 | 版本 | Objects | Entities | Decode mode | Input mode | Exit |
| --- | --- | ---: | ---: | --- | --- | ---: |
| `qqq.dwg` | R2000 | 152,346 | 102,779 | `r13-object-map` | `file-map` | 0 |
| `A36.1-上海长宁古北路直营店项目总平系统图2026.1.19.dwg` | R2004 | 478,534 | 312,149 | `r2004-object-map` | `file-map` | 0 |
| `A44.3-餐厅平面系统图-2025-厦门市-088-福建厦门海沧旅游码头加盟全季项目.dwg` | R2007 | 174,505 | 134,457 | `r2007-object-map` | `file-map` | 0 |
| `B01.37-未知品牌-酒店大堂参考.dwg` | R2010 | 492,855 | 473,695 | `r2004-object-map` | `file-map` | 0 |
| `A35.1-海宁奥特莱斯建国璞隐酒店-平面图1120.dwg` | R2013 | 247,813 | 223,912 | `r2004-object-map` | `file-map` | 0 |
| `北京湾里建国璞隐项目（直营）平面方案.dwg` | R2018 | 174,161 | 154,222 | `r2004-object-map` | `file-map` | 0 |
| `3F.00.dwg` | R2004 | 1,769,224 | 1,081,856 | `r2004-object-map` | `file-map` | 0 |

大型 `3F.00.dwg` 保留文件已有的非致命结构警告 `0x40` 和既存 iconv 诊断；它们
不改变对象总数、解码错误数、模式或退出码。该文件只运行 Stream 验收，没有执行
可能消耗数十 GiB 内存的无限制阻塞式解码。全部真实文件均来自项目所有者已有
资料，没有下载、复制、修改或提交额外 DWG。

## 尚未进入 master 的相关 PR

2026-08-26 复核时，下列 PR 仍为 OPEN，均不属于 `upstream/master`，因此本轮
没有 cherry-pick：

- [PR #1358](https://github.com/LibreDWG/libredwg/pull/1358)：R2004 section
  声明 size 超过 page estimate 时的阻塞式分配/边界修正。Stream 有独立页面、
  窗口和解压上限，进入 master 后需逐项映射。
- [PR #1360](https://github.com/LibreDWG/libredwg/pull/1360)：modular char 解析失败
  后重同步阻塞式 object map。Stream 有独立 object-map 迭代路径，进入 master
  后需验证同等 resync、页边界和错误传播。
- [PR #1364](https://github.com/LibreDWG/libredwg/pull/1364)：pre-R13 table sentinel
  缺失时不拒绝整图。当前分支已有 bounds-valid 表缺失 sentinel 的容错；未来
  应做语义比较，不能重复套用。
- [PR #1368](https://github.com/LibreDWG/libredwg/pull/1368)：为 pre-R13 drawing
  补 paper-space `BLOCK_HEADER`。R1-R11 Stream 独立组装所有权和实体列表，
  进入 master 后需审计 `entmode` 分流和 owner 语义。
- [PR #1401](https://github.com/LibreDWG/libredwg/pull/1401)：codepage index
  边界检查。它尚未进入正式基线；合入 master 后需沿阻塞式与 Stream 共用的
  codepage/字符串转换调用路径确认覆盖范围。

## 已知边界

- 当前跟踪的 R2004 fixture 没有找到可用于精确断言的非空 FileDepList；R2004
  字段顺序保持原 ODA 规范实现，现代三代 fixture 则同时锁定共享 parser 与严格
  Stream callback。后续若加入非空 R2004 fixture，应补同样的精确字段回归。
- 当前没有跟踪的 R2013+ fixture 能稳定触发 3D solid `has_ds_data` 在临时 decoder
  中创建 ACIS queue；现有回归直接验证最危险的非空宿主 queue 借用边界，释放
  路径由实现和常规内存所有权检查覆盖。后续获得合适 fixture 时应增加真实 push
  与清理回归。
- 本轮没有改变支持版本、严格无回退策略或开发优先级，因此无需修改根目录
  `README`、`target/README.md` 或 `target/STREAM_BLOCKING_PARITY.md`。

## 完成检查表

- [x] 工作树、分支、远端和同步前 `HEAD` 已确认；没有覆盖或回滚并行修改。
- [x] 已获取 `upstream/master` 并记录 commit、merge-base 和 `108 0`。
- [x] 主线无更新；未创建空 merge，未 cherry-pick 开放 PR。
- [x] FileDepList 共享 parser 的版本字段布局已修正。
- [x] Stream callback 的完整 FileDepList parity 与三代精确值回归已补齐。
- [x] Stream 临时 ACIS SAB handle queue 的隔离、清理和宿主不变回归已补齐。
- [x] CMake、Autotools、默认 Stream、123 文件 refs sweep 已通过。
- [x] 六代现代真实样本和大型 R2004 样本均完成严格无回退验证。
- [x] `origin` 仅有一个分支，且远端 `HEAD` 指向该分支。
- [x] 代码提交 `db4e97a2` 已推送；代码推送检查点本地/远端为 `0 0`。

按照仓库 GNU 约定，本轮不直接编辑顶层 `ChangeLog`，也不提交构建目录、日志、
私有 DWG、下载文件或 Autotools 生成物。
