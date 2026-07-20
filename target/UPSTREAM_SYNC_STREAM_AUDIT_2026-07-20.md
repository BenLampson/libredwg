# 2026-07-20 上游修复与 Stream 影响审计

## 同步基线

- 本分支此前已合入上游 `master` 的 `f2804663`（0.14.8447）。
- 2026-07-20 再次获取上游后，`upstream/master` 仍停在 `f2804663`，因此没有新的
  主线提交可合并。
- 上游维护者在 `smoke/ghsa-c8cq-pagemap-oob` 提交了 `c6e07aee`：修复
  R2004 Section Page Map 头在小文件或截断文件上的越界读取。本分支已同步该提交。

## 阻塞式与 Stream 覆盖关系

修复位于共享函数 `decode_R2004_header`。阻塞式 R2004 读取和独立 Stream R2004
入口都会先调用该函数，因此同一个边界检查同时保护两条通路，不需要在 Stream
目录复制第二份实现。

回归测试会生成一个有效 R2004 文件，再截断到 Section Page Map 头只剩 0x80
字节的位置，并验证：

- 阻塞式读取返回 `DWG_ERR_VALUEOUTOFBOUNDS`；
- Stream 使用 `DWG_STREAM_F_NO_FULL_FALLBACK` 时同样返回该错误；
- Stream 不回退到完整阻塞式读取，也不产生任何对象。

## 尚未进入上游 master 的修复

上游同时存在 PR #1311 至 #1323，当前全部仍为 OPEN，不能记作已同步的上游主线：

- #1311、#1314、#1315：DXF 输出；
- #1312、#1313、#1317、#1318、#1321、#1322：DXF 导入；
- #1316、#1320：以 DXF 导入和 DWG 生成期间的 handle/index 行为为主；
- #1319：R2000 DWG 编码；
- #1323：`dwg_page_x_min` 公共 API 返回了错误的 header 字段。

这些 PR 当前均不修改独立 DWG Stream 文件。`dwg.spec` 或共享 `dwg.c` 的改动若
以后进入 `master`，Stream 解码对象时会经过同一共享对象层，但仍须在下一次同步
时按调用路径重新审计，不能仅凭合并成功认定 Stream 已覆盖。

## 验证结果

- CMake `stream_test`：退出码 0，包含新增的截断 Section Page Map 双通路回归。
- Autotools 严格构建与 `stream_test`：退出码 0。
- `3F.00.dwg`：Stream 使用 `r2004-object-map` 和 `file-map`，解出
  1,769,224/1,769,224 个对象，解码错误 0，`full=0`；耗时 34.8 秒，峰值 RSS
  367 MiB。文件原有的非致命结构警告仍为 0x40。
