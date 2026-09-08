# 2026-09-09 上游同步检查

## 同步基线与结果

- 工作分支：`ben/stream-page-cache-optimization`。
- 同步前 HEAD：`377cf6e631dd3c202ff7613c3daca586e2728b34`。
- 获取前后 `upstream/master` 与 merge-base 均为
  `7eb90a9f933623729f82781cb1d68de2e50593f3`（0.14.8594）。
- 执行 `git fetch upstream --prune --tags` 成功；另以
  `git ls-remote upstream refs/heads/master` 确认远端主线相同。
- `HEAD...upstream/master` 为 `119 0`，新增上游提交为 0。
- 执行 `git merge --no-ff upstream/master` 返回 `Already up to date.`。
  **主线无更新**，按 `SYNC.md` 不制造空 merge commit。

本次新增补丁集合为空，没有需要分类或移植的上游修复，也没有修改 Stream
源码、版本支持或无回退策略。此前修复的调用路径分类见
`target/UPSTREAM_SYNC_STREAM_AUDIT_2026-09-04.md`。

## 验证

- CMake 原缓存为 `BUILD_TESTING=OFF`，首次构建找不到 `stream_test`。
  重新配置 `BUILD_TESTING=ON` 后，构建 `stream_test`、`dwgprobe` 成功，
  `stream_test.exe` 退出 0，仓库 parity 输出 `decode_errors=0`、`full=0`。
- Autotools 在现有 `.build-autotools-codex` 中重新配置并完成库构建，
  编译选项为 `-g -O2 -std=gnu99 -Werror -Werror=declaration-after-statement`，
  同时保留项目 `WARN_CFLAGS`。`libredwg.la`、`stream_test.exe` 构建及
  `stream_test.exe` 执行全部退出 0。
- 首次 make 自动触发 configure 时，传入的全局 `-Werror` 导致 configure 的
  `char strdup(void)` 探测触发 GCC builtin declaration mismatch，并误报
  strdup 不存在。用既有 `config.status --recheck` 和 `config.status` 完成
  配置后，再仅对源码构建传入严格选项，验证通过；没有修改源码规避告警。
- 本次无源码变化，未额外重复完整 `make check` 或 ASan；不将上一轮完整
  测试结果记作本轮结果。`git diff --check` 通过。

真实文件使用 `dwgprobe -d --`；该工具设置
`DWG_STREAM_F_NO_FULL_FALLBACK`。以下均为项目已有文件，没有下载新语料。
全部退出 0，`decoded == objects`、`decode_errors=0`，入口均为 `file-map`。

| 样本 | 版本 | 对象数 | decode_mode | 状态 |
| --- | --- | ---: | --- | --- |
| `qqq.dwg` | R2000 | 152346 | r13-object-map | ok |
| `A36.1-上海长宁古北路直营店项目总平系统图2026.1.19.dwg` | R2004 | 478534 | r2004-object-map | 既存 0x40 |
| `A44.3-餐厅平面系统图-2025-厦门市-088-福建厦门海沧旅游码头加盟全季项目.dwg` | R2007 | 174505 | r2007-object-map | ok |
| `B01.37-未知品牌-酒店大堂参考.dwg` | R2010 | 492855 | r2004-object-map | ok |
| `A35.1-海宁奥特莱斯建国璞隐酒店-平面图1120.dwg` | R2013 | 247813 | r2004-object-map | ok |
| `北京湾里建国璞隐项目（直营）平面方案.dwg` | R2018 | 174161 | r2004-object-map | ok |
| `3F.00.dwg` | R2004 | 1769224 | r2004-object-map | 既存 0x40 |

大型 R2004 仅运行 Stream，RSS 为 368 MiB，没有放开阻塞式解压上限。
日志留在本地 `.build-autotools-codex/sync-20260909-*`，不提交。

## 未进入主线的相关 PR

2026-09-09 通过 `gh pr list --repo LibreDWG/libredwg --state open`
复核，读取相关 PR [#1358](https://github.com/LibreDWG/libredwg/pull/1358)、
[#1360](https://github.com/LibreDWG/libredwg/pull/1360)、
[#1364](https://github.com/LibreDWG/libredwg/pull/1364)、
[#1368](https://github.com/LibreDWG/libredwg/pull/1368) 仍开放。
它们不属于正式同步基线，本次没有 cherry-pick。

## 用户工作保护

开始时已有四个未暂存删除：`.github/workflows/cifuzz.yml`、
`codeql-analysis.yml`、`main.yml`、`oda.yml`。这些删除原样保留，不纳入本次
审计提交。因此“工作树干净”不适用于本次已有用户改动；不能为了满足该项而
恢复文件或提交用户删除。未清理任何未跟踪文件或构建目录。
