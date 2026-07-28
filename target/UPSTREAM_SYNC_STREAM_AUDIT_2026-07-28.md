# 2026-07-28 上游 0.14.8544 与 Stream 影响审计

## 同步基线

- 同步前分支：`codex/libredwg-c-stream`，`HEAD` 为 `afa0c010`。
- 正式 merge-base：`f280466307098ff7d279883eedc1898daff3cc7e`
  （上游标签 0.14.8447）。
- 获取前本地记录的 `upstream/master` 为 `073f9e94`；本次获取后为
  `d39cb4aa3ff98395e996cbf9769a7dcd862ffdc6`（上游标签 0.14.8544）。
- 相对正式 merge-base，上游主线共有 38 个尚未通过 merge 纳入本分支的提交；
  其中 31 个是本次获取相对 `073f9e94` 新增的提交。
- 使用 `git merge --no-ff upstream/master` 合并，merge commit 为
  `8b9cd0a1`。

同步开始前只存在以下用户未跟踪产物，本次没有覆盖、清理或提交它们：

- `target/libredwg-stream-win64-af803838.zip`
- `target/libredwg-stream-win64-af803838/`
- `target/libredwg-stream-win64-afa0c010.zip`
- `target/libredwg-stream-win64-afa0c010/`

## 共享实现自动覆盖

- `16fd2ce0` 修复 R2004 Section Page Map 头越界。该补丁与本分支已同步的
  `10869cca` 等价；阻塞式和 Stream R2004 入口都会调用
  `decode_R2004_header`。本分支已有截断文件双通路回归，严格 Stream 入口使用
  `DWG_STREAM_F_NO_FULL_FALLBACK`。
- `0d50991f` 为 R2007 `read_data_section` 的 `page->offset` 检查增加上游单元
  测试并将内部类型移到 `decode.h`。Stream R2007 对象/handle 路径直接调用同一
  `read_data_section`，单页路径还保留本分支在 `read_data_section_page` 和
  Stream section-map 入口中的同等检查。
- `98e86b12` 为共享的 R2004 压缩 section 大小乘法溢出保护增加测试。
  使用缓冲 section 的阻塞式与 Stream 元数据路径自动覆盖；大型对象页 Stream
  路径继续使用独立的页范围检查和可配置解压上限。
- `a7a7fc10` 为共享 `decode_3dsolid` 的 `block_size * 8` 溢出保护增加测试。
  Stream 对象回调使用同一 spec 对象解码器，因此自动覆盖。
- `899f90a4` 保留重复 handle-ref 情况下的有序索引，并在重新解码前重置索引。
  Stream 对象解码同样经过 `dwg_add_handleref`、`ordered_ref_add` 和共享对象
  所有权层，因此自动覆盖。
- `5feb4edc` 修复 AC1006 3DLINE elevation/二维坐标解析。R1-R11 Stream 最终
  调用共享的 pre-R13 entity/spec 解码器，因此无需复制修复。
- `bef934ba` 的 HATCH spline 条件以及本批其他 `dwg.spec` 对象层防御，只要发生
  DWG 对象解码，阻塞式和 Stream 都使用同一生成解码逻辑。

## Stream 独立实现检查与合并处理

本批没有出现只修复阻塞式页面、section、object map 或 handle map，而 Stream
仍保留同类漏洞的新增主线提交，因此没有新增第二份 Stream 安全逻辑。

合并 `0d50991f` 时，上游将 `r2007_page`、`r2007_section_page` 和
`r2007_section` 类型及 `read_data_section` 声明移入 `decode.h`。为保持单一
定义并通过 Autotools `-Werror=redundant-decls`，本分支从
`src/stream/stream_r2007_internal.h` 删除了旧的重复类型和函数声明；其余
Stream 专用共享入口保持不变。

`src/encode.c` 冲突保留本分支对 R10 以前 APPID control 大小的兼容行为，同时
吸收上游 `10232aa4`、`c34d1efb`、`410802f3` 和 `48e89030` 的编码修复。该冲突
不影响 DWG Stream 读取路径。

## 与 DWG Stream 读取无关

其余提交按实际入口归类如下：

- CI 和发布元数据：`cd9c9541`、`b86b7e9d`。
- DXF 导入、字段/所有权修复、性能和替换前释放：
  `3d4305a9`、`a0c308d1`、`964a6744`、`a510185f`、`21ecccf2`、
  `43bdd91f`、`5fdaf154`、`94c3fc28`、`176f368a`、`83ef6bed`、
  `1829a892`、`aaa91fe1`、`4b3d1c6b`、`ed121c68`、`d39cb4aa`。
- DXF 输出和 dynapi 映射：`2fa9c1fd`、`073f9e94`、`64c1e225`、
  `af364d4c`。
- DWG/API 构造或编码：`cc0f29c5`、`24562e9b`、`869f4c6b`、
  `10232aa4`、`c34d1efb`、`7dbfc0e9`、`ac644ec1`、`410802f3`、
  `48e89030`。
- 公共 API 返回值：`aabd345b` 修复 `dwg_page_x_min`，不参与读取器分发或
  页面解码。

这些提交随上游 merge 保留，但不要求在独立 DWG Stream 文件中复制实现。

## 验证结果

- CMake 增量构建 `stream_test` 和 `dwgprobe`：成功。
- CMake `stream_test`：退出码 0；默认真实语料覆盖 R2000、R2004、R2007、
  R2010、R2013 和 R2018，所有输出均为 `full=0` 且解码错误为 0。
- Autotools 严格构建 `libredwg.la` 和 `stream_test.exe`：成功，包括 C99
  和 `-Werror` 检查。
- Autotools `stream_test.exe`：退出码 0。
- 大型 `3F.00.dwg`：退出码 0，`decode_mode=r2004-object-map`，
  `input_mode=file-map`，解码 1,769,224/1,769,224 个对象，解码错误 0，
  峰值 RSS 366 MiB；保留原有非致命结构警告 0x40，没有阻塞式回退。

CMake 的 Windows 编译器仍会打印已有的 `%zu`/`%hhx` 日志格式分析告警；
Autotools 规范严格构建没有对应告警或错误。

## 尚未进入 master 的相关 PR

2026-07-28 检查开放 PR 后，以下改动仍未进入 `upstream/master`，因此本次没有
同步：

- PR #1339 至 #1343：均只修改 `src/in_dxf.c`，处理 DXF 重建时的释放或数组
  扩容，不修改 DWG Stream 读取路径。
- PR #1328：3DLINE API、示例和 add-test 改进，不修改共享或独立 DWG 解码器。
- PR #1324：bits 单元测试改进，不修改读取实现。

PR #1312、#1314、#1319、#1321 的页面仍为 OPEN，但本次
`upstream/master` 已包含相应主题的后续主线提交；不能把 PR 页面状态误记为
尚未同步的 Stream 补丁。当前开放 PR 中没有必须绕过 `master` 单独
cherry-pick 的 Stream 安全修复。

## 结论

本次同步未改变支持版本、严格无回退策略、能力结论或开发优先级，因此无需修改
根目录 `README`、`target/README.md` 或
`target/STREAM_BLOCKING_PARITY.md`。上游读取安全修复均已由共享调用路径或本
分支既有 Stream 专用检查覆盖。
