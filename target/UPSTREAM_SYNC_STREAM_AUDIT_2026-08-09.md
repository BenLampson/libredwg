# 2026-08-09 上游 0.14.8566 与 Stream 影响审计

## 同步基线

- 实际开发分支：`ben/stream-page-cache-optimization`，同步前 `HEAD` 为
  `88eaf28986cc0419de3d78e89002a5e13e0fe235`。
- 正式 merge-base：
  `d3a0a2dc1fdab5737bc6036db2d705300e6e59b6`（上次已审计的上游
  0.14.8553）。
- 获取后的 `upstream/master` 为
  `a8ce24891c1a82cd51a8d0f3f97095a2a5d8aa31`，标签为 `0.14.8566`。
- 同步前 `HEAD...upstream/master` 的左右提交数为 `101 8`；本次需吸收 8 个
  上游提交。
- 使用 `git merge --no-ff upstream/master` 合并，merge commit 为
  `221514f7504948c7d41b2e639281afcbcc5117fd`，无文本冲突。合并后该 merge
  commit 相对 `upstream/master` 为 `102 0`。
- merge commit 的 first-parent 差异只包含 7 个预期上游文件，共
  95 additions、11 deletions；`git diff --check 221514f7^1 221514f7`
  通过，没有冲突标记或生成文件。
- 同步前工作树干净。本次没有清理、覆盖或提交已有 `.build*`、本地归档、私有
  DWG 或其他用户产物。

## 新增上游提交逐项分类

| Commit | 内容 | Stream 分类与依据 |
| --- | --- | --- |
| `330deeeb` | 修正 pre-R13 `dwg_add_POINT` 的类型、elevation 标志和旧版本 3D 限制 | 构造/编码 API，不直接参与 Stream 读取；合并审计发现失败残留和 elevation round-trip 语义需要本分支补齐 |
| `b9fb511a` | 修正 pre-R13 `dwg_add_ARC` 的版本限制与 elevation 表示 | 构造/编码 API；合并审计发现共享 decoder 需把 common elevation 回填到 `center.z` |
| `e405fcff` | 终止 `dxf_CMC` / `dxfb_CMC` 的 `book_name` 临时副本 | DXF/DXFB 输出，与 DWG Stream 读取无关 |
| `ed95ce71` | UTF-16 转 UTF-8 时把 surrogate half 替换为 U+FFFD | 共享 `bit_convert_TU*` 实现，阻塞式、Stream 对象 API 和输出调用者自动覆盖 |
| `3859b5af` | R2007 未压缩数据页也按页大小识别并执行 Reed-Solomon 解码 | 上游只修改整 section 的 `read_data_section()`；本分支 Stream 独立单页入口 `read_data_section_page()` 需要同步 |
| `153d8530` | `add_test` 版本字符串扫描指针改为 const-correct | 仅测试源码类型修正，不改变读取行为 |
| `0f6f14b5` | pre-R13 INSERT 的 `has_attribs` 从标志值 128 归一化为 0/1 | 共享 `dwg.spec` 对象 decoder；阻塞式和 R1-R11 Stream 都调用同一 `dwg_decode_INSERT`，自动覆盖 |
| `a8ce2489` | DXF ENTITIES section 不再输出 R11 `JUMP` 记录 | DXF 输出修正；`JUMP` 是读取位置标记而非实体数据，不要求修改 Stream 解码 |

分类汇总：2 项由共享实现自动覆盖，1 项需要补齐 Stream 独立单页路径，5 项与
DWG Stream 读取无关。POINT/ARC 虽属于构造/编码路径，但当前分支使用这些 API
生成严格 Stream 回归文件，因此仍完成了写入、阻塞式回读和 Stream 回读的交叉
审计。

## R2007 Stream 独立单页补丁

上游 `3859b5af` 修正的是阻塞式整 section 路径：当
`comp_size == uncomp_size`，但实际 `page->size` 等于 Reed-Solomon 编码后的页
大小时，`read_data_section()` 也调用 `read_data_page()`，先去除 RS parity，
再直接复制未压缩 payload。

当前分支为了低内存 R2007 对象读取，在 `src/decode_r2007.c` 额外导出了
`read_data_section_page()`。它被 `src/stream/stream_read_r2007.c` 的两条实际
路径调用：

1. `load_stream_data_page()` 为分页缓存装入单页；
2. `copy_data_section_window()` 在不使用缓存时直接装入单页。

merge 后该函数仍只以 `comp_size != uncomp_size` 判断是否进入
`read_data_page()`，因此未压缩但 RS 编码的对象页会被原样复制，payload 与
parity 交错，导致对象大量丢失或字段损坏。补充修改让单页入口复用上游新增的
`page_size_if_rs_coded()` 判据，并把错误日志从 “compressed page” 改为通用的
“page”。修复放在阻塞式和 Stream 共用的 `src/decode_r2007.c` 中，不复制第二套
RS 算法。

`test/unit-testing/decode_test.c` 增加直接单页回归：同一份 252-byte payload
分别构造为 512-byte、两 block 的 RS 页面，以及 256-byte 的 verbatim 页面；
前者必须去除 RS 交错，后者必须保持直接复制。该测试同时防止把所有未压缩页
误判为 RS 页。

## POINT/ARC 上游集成补齐

### POINT 失败不得残留对象

`330deeeb` 在 `dwg_add_POINT()` 中先执行 `API_ADD_ENTITY`，再检查
`version < R_2_4 && z != 0`。此时 `dwg_add_object()` 已增加
`dwg->num_objects`，pre-R10 `numentities` 已增加，且
`dwg_insert_entity()` 已更新 BLOCK_HEADER 的 `entities`、`num_owned` 和实体链；
随后返回 NULL 并不会回滚这些副作用。

补充修改改用 `API_ADD_PREP`，在分配前完成 3D point 和版本检查，成功后才执行
`API_ADD_ENTITY2`。严格 pre-R2.4 生成回归会在非法非零 Z 调用前后比较
`num_objects`、`numentities` 和 BLOCK_HEADER `num_owned`，并确认合法二维 POINT
仍能写出和无回退 Stream 回读。

### common elevation 必须回填实体字段

上游 add API 会把旧格式的非零 Z 存入 common entity 的
`FLAG_R11_HAS_ELEVATION` / `elevation_r11`，但合并后的 POINT 与 ARC spec decoder
没有把该值写回公开实体字段。结果是二进制中的 elevation 正确，阻塞式与 Stream
却会一致地把 `POINT.z` 或 `ARC.center.z` 留为 0；只做 blocking/Stream parity
仍无法发现这一类共同丢值。

`src/dwg.spec` 的共享 decoder 现按 `FLAG_R11_HAS_ELEVATION` 分别回填
`POINT.z` 和 `ARC.center.z`。R1-R11 Stream 的包装函数直接调用共享的
`dwg_decode_POINT` / `dwg_decode_ARC`，所以无需在 `src/stream/*` 再写一份修复。

交叉回归补充以下覆盖：

- pre-R2.4 非零 Z POINT 安全拒绝且没有对象计数或所有权残留；
- R2.4-R9c1 的 elevation 版本边界通过生成文件明确验证；add API 的原上界
  `R_9` 已补为 `R_9c1`，避免 R9c1 返回成功却不写 common elevation；
- R10、R11 非零 Z ARC 经阻塞式回读后仍保留 `center.z`；
- 同一文件再以 `DWG_STREAM_F_NO_FULL_FALLBACK` 读取，验证严格 Stream 语义；
- Stream semantic hash 新增 POINT 与 ARC 的坐标、半径、角度、厚度和 extrusion，
  避免以后只比较对象数量或 common 字段。

## 其他共享覆盖与合并语义复查

- `ed95ce71` 修改所有主要 `bit_convert_TU*` UTF-16 转换循环；Stream 解码出的
  TU 字段通过公开 API、dynapi 或输出层转换时使用同一实现，没有独立 Stream
  UTF-8 emitter 需要同步。
- `0f6f14b5` 位于共享 `dwg.spec`。R1-R11 Stream 的
  `dwg_stream_decode_pre_r13_INSERT()` 只是 `dwg_decode_INSERT()` 包装，因此
  `has_attribs` 的 0/1 归一化同时覆盖两条读取路径。
- `src/dwg.spec` 的 INSERT 修复与本分支 PROXY_OBJECT raw handle 修复位于不同
  spec 区域，merge 后两者都保留。
- `src/dwg_api.c` 的 POINT/ARC 改动没有覆盖本分支既有的 3DLINE getter/setter、
  pre-R10 LINE 安全拒绝或显式 `dwg_add_3DLINE` 分配逻辑。
- `e405fcff`、`153d8530` 和 `a8ce2489` 均完整吸收，没有发现与本地 Stream
  页面、section、object map、handle map 或版本分发的语义冲突。

## 验证结果

- merge commit first-parent 和最终补充差异的 `git diff --check` 均通过。
- CMake `stream_test`、`dwgprobe` 增量构建成功；`stream_test.exe` 退出 0。
  R1-R11 生成文件的 POINT/ARC elevation、POINT 安全拒绝、blocking/Stream
  semantic snapshot 均通过，严格 Stream 统计保持 `full=0`。
- 启用 `LIBREDWG_STREAM_TEST_REPOSITORY_SWEEP=1` 和
  `LIBREDWG_STREAM_TEST_REFS=1` 的完整仓库扫描退出 0：
  `files=123 refs=1`；所有已解码对象 `decode_errors=0`、`full=0`。
- CMake 的可选 `decode_test` target 在当前 Stream 专用构建目录中仍有既存链接
  配置缺口：该 target 直接编译 `src/dwg.c`，但没有链接提供
  `dwg_decode_stream` 的 Stream 模块，因而报 undefined reference。同步前后的
  `test/unit-testing/CMakeLists.txt` 无差异；本次新增测试已由下述规范 Autotools
  target 完整编译和运行，不把这一非强制 CMake target 误报为通过。
- Autotools 规范静态构建 `libredwg.la`、`decode_test.exe`、
  `stream_test.exe` 成功，实际告警集包含 C99、`-Werror`、
  `-Wdeclaration-after-statement` 和 `-Wimplicit-function-declaration`。
- Autotools `decode_test.exe` 退出 0；新增的 252-byte、两 block RS 交错页与
  256-byte verbatim 页对照均通过。Autotools `stream_test.exe` 退出 0。
- 完整 Autotools `make check` 退出 0：255 PASS，0 FAIL，0 SKIP。负向损坏
  输入按预期打印内部 `ERROR:`，但测试本身全部 PASS。
- Windows CMake/MinGW 仍打印仓库既有的 `%hh` 格式分析告警；规范 Autotools
  严格构建没有对应错误。仓库格式化 helper 已用于本次 R2007 实现和单测；
  `src/dwg_api.c` 保持周边既有样式，避免 clang-format 22 造成无关机械改排版。

## 项目真实 DWG

项目私有语料只在原位置读取，没有复制、修改或提交。最终源码重链后，20/20 张
正常业务图通过 `dwgprobe -d` 的严格无回退读取：总对象 6,347,881，实体
5,410,837，非实体 937,044；每个文件均为 `decoded=objects`、
`decode_errors=0` 和 `file-map`。

六个版本代表文件的对象数与模式为：

- R2000：152,346，`r13-object-map`；
- R2004：478,534，`r2004-object-map`；
- R2007：174,505，`r2007-object-map`；
- R2010：492,855，`r2004-object-map`；
- R2013：247,813，`r2004-object-map`；
- R2018：174,161，`r2004-object-map`。

大型 R2004 `3F.00.dwg` 继续单独执行 Stream-only 验收，没有无限制阻塞式
解码：

- 1,769,224/1,769,224 个对象全部解码，实体 1,081,856，非实体 687,368；
- `decode_errors=0`，`decode_mode=r2004-object-map`，
  `input_mode=file-map`，RSS 368 MiB；
- 保留输入原有非致命警告 `0x40`，没有阻塞式回退；
- 中文路径输出仍有既存 `iconv errno 22` 日志噪声，不影响退出码、对象统计或
  解码结果。

## 尚未进入 master 的相关 PR

2026-08-09 通过 GitHub 当前状态复核，以下 PR 均为 OPEN、未合并，本次没有
cherry-pick：

- PR #1358，`decode: r2004 sections whose declared size exceeds the page
  estimate (fixes #1294)`：修改阻塞式 R2004 section 缓冲大小和逐页边界。当前
  Stream 有独立的 R2004 对象页读取、解压上限与窗口逻辑；若进入 master，必须
  对照 declared size、最后一页实际大小和低内存上限逐项补齐并回归。
- PR #1360，`decode: resync the object map when a modular char fails to parse
  (fixes the data loss in #1355)`：修改阻塞式 R2004 handle/object map 的失败
  重同步。Stream 有独立 object map 迭代路径，未来合入时必须确认同等 resync、
  页边界和错误传播，不能只依赖共享解码器。
- PR #1368，`r11: give pre-R13 drawings their paper space BLOCK_HEADER
  (fixes #1337)`：修改 pre-R13 文档构造、实体所有权和 DXF block name。
  R1-R11 Stream 独立组装 BLOCK_HEADER 所有权与实体列表，未来合入时必须审计
  paper-space `entmode` 分流和严格 Stream 回调中的 owner 语义。

这些改动均尚未进入正式同步基线。按 `SYNC.md` 继续等待
`upstream/master`，不绕过主线单独 cherry-pick；下一次合入时重新沿实际 Stream
调用路径审计。

## 已知边界与后续项

- 上游 `3859b5af` 明确指出仓库既有 DWG 不含未压缩 RS data page，因此其原有
  `make check` 没有走新分支。本次合成单页测试用于锁定 Stream 独立路径；真实
  R2007 项目文件已通过 174,505 个对象的严格回归，但不能代替合成 RS 分支覆盖。
- 上游 surrogate 修复没有增加 lone high/low surrogate 的直接单元测试，现有
  `bit_convert_TU` 数据主要为 BMP。该测试缺口不阻塞 Stream 同步，但后续应为
  `bit_convert_TU`、bounded variant 和 `bit_TU_to_utf8_len` 增加 U+FFFD 断言。
- CMC 长 `book_name` 和 R11 JUMP DXF 过滤也没有随上游提交增加针对性测试；两项
  属输出路径，不是本次严格 DWG Stream 验收阻塞项。
- POINT 的 R9c1 elevation 上界已由生成文件的 blocking/严格 Stream round-trip
  锁定；add API 现覆盖完整 R2.4-R9c1 范围，不再返回成功却静默丢失 Z。
- POINT/ARC 的新增 semantic hash 锁定本次 elevation 相关公开字段，但旧格式
  common `thickness_r11` 尚未进入 common semantic hash；本次不把结论扩大为完整
  POINT/ARC 所有语义字段的 parity。
- 上游 `page_size_if_rs_coded()` 以对齐后页大小推断 RS 布局。理论上 raw payload
  481-496、737-752、993-1000 bytes 的 32-byte 对齐尺寸可能与 RS 页尺寸相等，
  存在误判边界；本次修改让 blocking/Stream 判据保持一致，不另造分叉，后续应
  结合 checksum、parity 或额外页元数据消歧。

## ChangeLog 与阶段性结论

按照仓库 `AGENTS.md` / `HACKING` 约定，日常提交不直接编辑顶层
`ChangeLog`；发布 ChangeLog 由 GNU 风格提交日志生成。本文件记录 merge 与后续
Stream/API 集成补丁，生成文件不进入补丁。

上游 0.14.8566 的 8 个提交已经通过可追踪 merge commit 完整进入当前 Stream
优化分支。合并本身无文本冲突，但调用路径审计发现 R2007 独立单页读取仍缺少
RS 判据，并发现 POINT 失败残留及 POINT/ARC common elevation 未回填公开字段；
这些根因已在共享实现或构造 API 中补齐，并由 blocking/严格 Stream 交叉回归
锁定。CMake 必验目标、Autotools 规范构建、完整 255 项测试、123 文件仓库扫描、
20 张正常项目图和大型 `3F.00.dwg` 均通过；支持版本、严格无回退政策和现有能力
结论没有改变，因此无需同步修改 README 或能力/版本政策文档。
