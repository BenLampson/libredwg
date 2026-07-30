# LibreDWG 上游同步工作说明

本文档用于把 LibreDWG 官方仓库的更新同步到当前 C Stream 开发分支，并确认
上游针对阻塞式读取器的修复是否也覆盖独立 Stream 读取路径。以后执行上游同步
时，应完整走完本文档，而不能以“Git 合并成功”代替功能验证。

## 固定仓库与目标

- 官方上游：`upstream`，地址 `https://github.com/LibreDWG/libredwg.git`。
- 当前派生仓库：`origin`，地址 `https://github.com/BenLampson/libredwg.git`。
- 开发分支：执行同步时承载最新 C Stream 能力的当前工作分支；不得为了匹配旧
  分支名而切换到缺少后续 Stream 优化的分支。
- 正式同步基线：`upstream/master`。
- 当前开发与验证目标是 C 实现，不修改或依赖 `ProtocolVNext`。
- DWG Stream 不允许回退到 `dwg_read_file`；测试必须使用
  `DWG_STREAM_F_NO_FULL_FALLBACK` 或等价的严格入口。
- 优先保证 2000 年以后的正式 DWG 版本，同时不得破坏已有旧版本能力。

## 1. 同步前检查

先确认工作树、分支和远端。工作树存在未提交内容时，不得覆盖、清理或回滚这些
内容；应先判断它们是否属于本次工作。

```powershell
git status --short --branch
git remote -v
git branch --show-current
```

预期当前分支为实际 C Stream 开发分支，且同时存在 `origin` 和 `upstream`。
应在审计记录中写明分支名和同步前 `HEAD`。

## 2. 获取并列出上游变化

```powershell
git fetch upstream --prune --tags
$base = git merge-base HEAD upstream/master
git rev-list --left-right --count HEAD...upstream/master
git log --oneline --no-merges "$base..upstream/master"
git diff --stat "$base..upstream/master"
git diff --name-status "$base..upstream/master"
```

必须记录 merge-base、上游最新提交和新增提交数量。若 `upstream/master` 没有新增
提交，应明确写“主线无更新”，不能制造空合并。

上游仓库中的工作分支和开放 PR 不等于 `master`。默认只合并
`upstream/master`。只有明确的安全公告、上游维护者修复且确有立即同步必要时，
才可单独 cherry-pick；此时必须在审计文档中写明来源、未进入主线的原因和 commit。

## 3. 逐项审计 Stream 影响

对每个新增修复查看完整补丁和调用路径：

```powershell
git show --stat <commit>
git show <commit>
rg -n "<被修改的函数或字段>" src test
```

每项修复必须归入以下一种类型：

1. **共享实现自动覆盖**：阻塞式与 Stream 调用同一函数、spec 或对象解码器。
2. **Stream 独立实现需要同步**：上游只改了阻塞式路径，而 Stream 有自己的页面、
   section、object map、handle map 或版本分发实现。
3. **与 DWG Stream 读取无关**：例如仅涉及 DXF 导入/导出、DWG 编码、工具输出或
   构建元数据。

不能仅因改动位于 `decode.c`、`dwg.spec` 或合并没有冲突，就判断 Stream 已覆盖。
必须沿实际函数调用证明。

重点检查这些独立 Stream 文件：

| 范围 | 文件 |
| --- | --- |
| 版本策略与分发 | `src/stream/stream_version_policy.c`、`src/stream/stream_reader.c` |
| R1-R11 | `src/stream/stream_read_r1_to_r11.c` |
| R13-R2002 | `src/stream/stream_read_r13_to_r2002.c` |
| R2004-R2006、R2010-R2022 | `src/stream/stream_read_r2004_to_r2006_and_r2010_to_r2022.c` |
| R2007 | `src/stream/stream_read_r2007.c` |
| 对象与回调辅助 | `src/stream/stream_object_helpers.c`、`src/stream/stream_callbacks.c` |

尤其关注整数溢出、加减下溢、文件偏移、压缩/解压长度、页边界、对象长度、handle
数量、分配上限和错误码传播。阻塞式修复若对应 Stream 独立逻辑，必须同步实现
同等检查并新增 Stream 回归。

## 4. 合并与补齐

上游主线有新增提交时，保留可追踪的 merge commit：

```powershell
git merge --no-ff upstream/master
```

解决冲突时必须保留本分支的 C Stream 能力、可配置 R2004 解压上限和无回退策略，
同时吸收上游根因修复。不得用整文件覆盖的方式丢掉任何一侧改动。

合并完成后，根据第 3 节审计结果补齐 Stream 独立实现。优先把检查放在阻塞式与
Stream 都会经过的共享函数中；只有调用路径确实独立时才分别修改。修改 C 文件后
只格式化本次涉及的文件：

```powershell
build-aux/clang-format.sh <changed-c-file>
git diff --check
```

## 5. 必须执行的验证

### CMake Stream 回归

```powershell
$env:PATH="D:\Codes\libredwg\build-codex-stream-tdd;C:\msys64\ucrt64\bin;C:\msys64\usr\bin;$env:PATH"
cmake --build build-codex-stream-tdd --target stream_test dwgprobe -j 4
.\build-codex-stream-tdd\test\unit-testing\stream_test.exe
```

必须确认退出码为 0。新增的边界或安全修复应有阻塞式与无回退 Stream 的交叉
回归；不能只测试正常文件。

### Autotools 严格构建

Autotools 是本项目的规范构建路径：

```powershell
C:\msys64\usr\bin\bash.exe -lc 'cd /d/Codes/libredwg && PATH=/ucrt64/bin:/usr/bin:$PATH make -C .build-autotools-static-codex/src libredwg.la && PATH=/ucrt64/bin:/usr/bin:$PATH make -C .build-autotools-static-codex/test/unit-testing stream_test.exe && PATH=/ucrt64/bin:/usr/bin:$PATH ./.build-autotools-static-codex/test/unit-testing/stream_test.exe'
```

必须通过 C99 和严格告警构建，不能只依赖 CMake 结果。

### 项目真实文件回归

只使用项目所有者提供的现有资料，不得为同步验证擅自下载 AutoCAD、DWG 语料或
其他大文件。现代版本回归至少应覆盖项目已有的 R2000、R2004、R2007、R2010、
R2013 和 R2018 文件。

大型 R2004 `3F.00.dwg` 使用 Stream 验证：

```powershell
.\build-codex-stream-tdd\dwgprobe.exe -d -- "E:\项目额外大文件\CAD项目\CAD算量材料(1)\3F.00.dwg"
```

验收条件：

- `decode_mode` 为 `r2004-object-map`；
- `input_mode` 为 `file-map`；
- 对象数为 1,769,224，解码错误为 0；
- 不发生阻塞式回退；
- 文件原有非致命结构警告 0x40 可以保留。

不得在自动回归中对该文件无限制运行阻塞式解码。默认阻塞式会因安全上限得到
0 个对象；强制放开限制曾消耗数十 GiB 内存。

## 6. 更新同步记录

每次同步新建：

```text
target/UPSTREAM_SYNC_STREAM_AUDIT_YYYY-MM-DD.md
```

记录至少包括：

- 同步前后的上游 commit 和 merge-base；
- 新增修复列表；
- 每项修复属于共享覆盖、Stream 补丁或无关改动中的哪一类；
- 实际修改的 Stream 文件和原因；
- CMake、Autotools、真实文件测试结果；
- 尚未合入 `master`、因此没有同步的相关 PR；
- 已知警告、未覆盖项和后续目标。

若支持版本、无回退策略、能力结论或开发优先级发生变化，还要同步更新根目录
`README`、`target/README.md` 和 `target/STREAM_BLOCKING_PARITY.md`。不能只更新
其中一份，造成状态描述互相矛盾。

## 7. 提交与推送

提交前检查实际差异和生成文件：

```powershell
git status --short
git diff --check
git diff --stat
git diff
```

不要提交本地构建目录、日志、测试临时 DWG、下载文件或 Autotools 生成物。上游
合并 commit 与本分支的 Stream 补齐/测试 commit 应尽量分开，便于以后追踪来源。

```powershell
$branch = git branch --show-current
git add -- <本次修改文件>
git commit
git push origin $branch
git rev-list --left-right --count "HEAD...origin/$branch"
```

最终必须满足：工作树干净，本地与远端差异为 `0 0`。汇报时明确列出上游修复、
Stream 是否需要额外修改、测试证据、commit 和推送结果。

## 完成检查表

- [ ] 工作树和远端已确认，没有覆盖用户改动。
- [ ] 已获取 `upstream/master` 并记录同步基线。
- [ ] 每个新增修复都完成 Stream 调用路径分类。
- [ ] Stream 独立路径需要的同等修复和回归已经补齐。
- [ ] CMake `stream_test` 通过。
- [ ] Autotools 严格构建和 `stream_test` 通过。
- [ ] 项目真实现代 DWG 完成无回退验证。
- [ ] 新的 `target/UPSTREAM_SYNC_STREAM_AUDIT_YYYY-MM-DD.md` 已建立。
- [ ] 没有提交生成物、下载物或测试临时文件。
- [ ] commit 已推送，本地与远端差异为 `0 0`。
