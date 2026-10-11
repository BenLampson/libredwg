# 2026-10-11 上游同步与核心分支核验

## 同步结果

- 核心分支：`ben/stream-page-cache-optimization`。
- 同步前 HEAD：`bf54bb4fc030be8687ffa4218e847053374b23e7`。
- 原上游基线与 merge-base：
  `7eb90a9f933623729f82781cb1d68de2e50593f3`。
- 最新 `upstream/master`：
  `a3d51f13f7afc143c2adb87d4b0e858e24cdac95`。
- 获取前后的新增主线提交为 7；合并前左右差异为 `121 7`。
- 合并提交：`e838b3a730e9238c9de1ee9cfbeb1a2ea5b6c9e7`。
  使用 `git merge --no-ff`，无冲突。
- 合并共修改 11 个文件，新增 62 行、删除 21 行。
- `git merge-base --is-ancestor upstream/master HEAD` 退出 0。

本轮仅吸收已经进入官方 `master` 的提交，没有 cherry-pick 开放 PR。
同步开始时工作树干净。此前的 `bf54bb4f` 删除派生仓库继承的 CI 自动化，
这些删除和相关文档修改完整保留，没有恢复上游工作流。

## 逐提交 Stream 调用路径审计

以下七项均属于 `SYNC.md` 的第 3 类：与 DWG Stream 读取无关。

| 提交 | 修复 | 调用路径与 Stream 结论 |
| --- | --- | --- |
| `5abbe5dc` | DXF 字符串按实际写入长度结束 | `out_dxf.c` 的静态 `cquote` 由 `dxf_fixup_string` 调用，属于 DXF 输出路径 |
| `aafb376e` | DXF 空值仍输出组码 | `dxf_write_chunked` 同样只在 DXF 输出路径使用；这是防御性加固 |
| `34f02f54` | ODA 转换调用的 stdin 重定向 | 修改 roundtrip 和测试 shell 脚本，不修改解码器 |
| `921c61ff` | ACAD EED 句柄通过 APPID 表查找 | `in_dxf.c` 的 `add_eed` 是 DXF 导入辅助函数，不是 DWG EED 解码器 |
| `93b89d33` | 保留各应用独立的 EED 分组 | 修改同一 DXF 导入函数的 slot 初始化及前一分组查找逻辑 |
| `53978ef6` | 非法 JSON SAB 数据立即返回错误 | `json_acis_data` 经 JSON 对象导入调用，不用于 DWG 二进制对象读取 |
| `a3d51f13` | TABLESTYLE 名称按来源版本使用 ASCII 或 UTF-16 | `dwg2.spec` 中 `upconvert_TABLESTYLE` 的调用位于 `#ifdef IS_ENCODER` 内，属于版本转换编码路径 |

Stream 解码回调在 `src/stream/stream_callbacks.c` 中调用
`dwg_decode_add_object`。上述转换入口不在该读取调用链中。本次不需要额外
Stream 补丁，也未修改 `src/stream`、版本支持、R2004 解压上限或无回退策略。

## 构建与回归

| 检查 | 结果 |
| --- | --- |
| CMake `stream_test`、`dwgprobe`、`dwg2dxf`、`dxf2dwg`、`dwgwrite` | 构建通过，exit 0 |
| CMake `stream_test.exe` | 通过，exit 0；正常 parity 为 `decode_errors=0`、`full=0` |
| Autotools 静态 `libredwg.la` | 严格构建通过，exit 0 |
| Autotools `stream_test.exe` | 通过，exit 0 |
| Autotools 完整 `make check -j4` | 通过，exit 0；programs 3/3、examples 2/2、unit tests 255/255 |
| TABLESTYLE 定向回归 | R2004 来源的 ASCII 和 R2007 来源的 UTF-16 默认、标题名称均正确 |
| JSON SAB 定向回归 | 合法两元素数组通过；短头和三元素数组均返回 `DWG_ERR_INVALIDTYPE`（0x8），未设置 `acis_data` |
| DXF MTEXT 复现样本 | `example_2018.dwg` 转 DXF 后，目标字符串准确以 `^J10` 结束，没有额外尾部字节 |
| `git diff --check` | 通过 |

Autotools 使用现有 `.build-autotools-codex`，先通过原配置的
`config.status --recheck` 和 `config.status` 更新构建文件，再对源码构建
传入 `-g -O2 -std=gnu99 -Werror -Werror=declaration-after-statement`，保留
项目 `WARN_CFLAGS`。没有执行 `make clean`。

转换边界回归使用本地临时 C harness，直接链接本次严格构建的静态库，并带有
合法 JSON 与旧版 ASCII 正向对照。该 harness、生成的 DXF 和 JSON、日志均留在
忽略的构建目录中，不提交派生生成物。本轮未执行 ASan；没有把历史结果记作
本次验证。CMake 仍有既存 MinGW format warnings，严格 Autotools 构建通过。

### 项目现有真实文件

均使用 `dwgprobe -d --` 的严格 Stream file-map 入口，无阻塞式回退。
没有下载新语料，没有对大型 R2004 放开阻塞式解压安全上限。

| 文件 | 版本 | 对象数及 decoded 数 | 解码错误 | decode mode | RSS MiB | 状态 |
| --- | --- | ---: | ---: | --- | ---: | --- |
| `qqq.dwg` | R2000 | 152346 | 0 | r13-object-map | 111 | ok |
| `A36.1-上海长宁古北路直营店项目总平系统图2026.1.19.dwg` | R2004 | 478534 | 0 | r2004-object-map | 150 | 既存 0x40 |
| `A44.3-餐厅平面系统图-2025-厦门市-088-福建厦门海沧旅游码头加盟全季项目.dwg` | R2007 | 174505 | 0 | r2007-object-map | 20 | ok |
| `B01.37-未知品牌-酒店大堂参考.dwg` | R2010 | 492855 | 0 | r2004-object-map | 98 | ok |
| `A35.1-海宁奥特莱斯建国璞隐酒店-平面图1120.dwg` | R2013 | 247813 | 0 | r2004-object-map | 45 | ok |
| `北京湾里建国璞隐项目（直营）平面方案.dwg` | R2018 | 174161 | 0 | r2004-object-map | 43 | ok |
| `3F.00.dwg` | R2004 | 1769224 | 0 | r2004-object-map | 367 | 既存 0x40 |

七个进程均 exit 0，`decoded == objects`、`decode_errors=0`，输入模式为
`file-map`。大型 R2004 的 1769224 个对象符合 `SYNC.md` 验收要求。
本地汇总脚本最初的 Windows PowerShell 编码及列索引问题已纠正，最终检查
全部通过；这些脚本问题不是 DWG 解码失败。

日志和临时回归工具保存在本地 `.build-autotools-codex/sync-20261011/`，
不提交私有 DWG、绝对来源路径、内容哈希或构建产物。

## 单一核心分支

- `git branch` 确认本地只有 `ben/stream-page-cache-optimization`。
- `git ls-remote --heads origin` 确认 GitHub 只有同名分支。
- `gh repo view` 确认该分支已是派生仓库默认分支。
- `gh pr list --state open` 确认派生仓库没有开放 PR。
- 本地只有当前工作树，聊天没有附加其他活动工作树。

因此没有多余分支或工作树需要删除，没有创建临时开发分支。保留
`upstream/master` 远端跟踪引用用于后续同步，它不是派生仓库的第二条分支。
没有清理本地未跟踪文件、历史日志、语料或构建目录。

## 尚未进入主线的相关工作

[PR #1426](https://github.com/LibreDWG/libredwg/pull/1426) 针对
`dwg_decode_eed` 的逐项分配内存膨胀，仍不在本次官方 `master` 基线中。
该共享对象解码路径与 Stream 有关，待进入主线后另行同步并验证。
其他开放 PR 同样未纳入本次合并。
