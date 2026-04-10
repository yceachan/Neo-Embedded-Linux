---
name: design-demo
description: Design and deliver a self-contained, end-to-end-debugged observation demo (C/C++/Rust language-agnostic). Strict src/test/output layering, agent owns the full build→run→observe→conclude chain, one-click test.sh, and three required 中文 docs (README, Design-*, Conclude). Use when the user says "design a demo", "做一个 demo", "验证 X 现象", or invokes /dd. Never deliver until the chain is green AND Conclude.md is written from REAL captured output.
---

# Design-Demo Skill

A skill for building **observation-grade demos** — small projects whose purpose is to *prove a phenomenon*, not to ship a product. The Agent owns the full lifecycle:

> design → write → build → run → **observe** → **conclude** → document

**Never hand off until the chain is green and the conclusion is grounded in real captured output.**

The canonical reference layout is **archived alongside this skill** at `./ref-03-Advanced-IO/` (a frozen snapshot of the original `prj/03-Advanced-IO` demo: `README.md`, `Design-Drv.md`, `Design-Drv-TimeSeq.md`, `Design-app-c.md`, `src/`, `test/`, `Makefile`, `Kbuild`). **When in doubt, open it and mirror its shape — layout, doc structure, source-walk style, test splitting.** It is the source of truth for "what a good demo looks like" in this skill, independent of whatever lives under `prj/` today.

## When this skill triggers

- User says "design a demo for X" / "做一个 demo 验证 X" / "写个最小例子证明 Y"
- User invokes `/dd`
- User asks the agent to *show* a kernel/library/syscall/protocol behavior with code

## Non-negotiable rules

### 1. Project layout (mirror `./ref-03-Advanced-IO/`)

```
<demo-root>/
├── README.md             # 中文; what / how to deploy / how to test / Ref block
├── Design-<topic>.md     # 中文; architecture + key API walk-through
├── Design-<topic>-*.md   # split into multiple files when large (e.g. -TimeSeq, -app-c)
├── Conclude.md           # 中文; observed phenomena + analysis + conclusions table
├── src/                  # human-written source only (.c/.cpp/.rs/.h)
├── test/                 # user-space test programs + run_all.sh (or test.sh at root)
├── Makefile / Kbuild / CMakeLists.txt / Cargo.toml
├── output/               # ALL artefacts: .ko, binaries, .o, logs, dumps
│   └── (flat or bin/ obj/ log/ — match what the build system naturally emits)
├── test.sh               # one-click: clean → build → deploy → run → tee logs → assert
└── .gitignore            # ignore output/* except output/.gitkeep
```

**Hard rules**:
- **No binary, `.o`, `.ko`, dump, or log file may live outside `output/`.** Configure the build system to redirect there (`-o output/bin/...`, `OBJDIR=output/obj`, Cargo `target-dir = "output/target"`, Kbuild `M=$(PWD) ... O=output`, etc.).
- `src/` contains only human-written source. Build recipes live at the root (Makefile/Kbuild/CMakeLists.txt).
- `test/` holds user-space test programs (one per phenomenon when feasible — see how `./ref-03-Advanced-IO/test/` splits `test_block.c`, `test_nonblock.c`, `test_poll.c`, `test_fasync.c`, `test_aio.c`).
- `test.sh` is the **single entry point** for the user. It must: clean → build → (deploy if cross) → run → tee output to `output/log/` → exit non-zero on failure.

### 2. Language-agnostic posture

Pick the build system the user already uses in this repo if discoverable; otherwise default per language:

- **C (kernel module)** → Kbuild + root `Makefile`, artefacts under `output/`. Cross-compile via the active SDK env (e.g. `source ~/imx/imx-env.sh`).
- **C (userspace)** → plain `Makefile`, `gcc -o output/bin/...`.
- **C++** → `CMakeLists.txt`, `cmake -B output/cmake -S . && cmake --build output/cmake`.
- **Rust** → `Cargo.toml` with `[build] target-dir = "output/target"`, final binary copied/symlinked into `output/bin/`.

### 3. Full-chain ownership

Before showing the user anything, the Agent MUST itself complete:

1. **Clarify the phenomenon** — one sentence. What exactly is this demo proving? Do not start coding without it.
2. **Design (中文)** — produce `Design-<topic>.md` first; get architectural clarity *before* coding. Split into multiple `Design-*.md` files if the topic has clearly separable axes (driver layering vs. time sequence vs. app-side design — see `prj/03`).
3. **Implement** — write `src/` and `test/` and the build recipe.
4. **Build** — run `test.sh` and watch it succeed. Fix all warnings the user would reasonably ask about.
5. **Run** — execute the demo (cross to EVB via `ssh imx` if needed), capture stdout/stderr to `output/log/run.log`.
6. **Observe** — read the captured log. Extract the *phenomenon* the demo is supposed to prove. If you can't see it, the demo is not done.
7. **Conclude (中文)** — write `Conclude.md` using the **real captured output**, not hypothetical numbers. This is the highest-value document — see §4 for the conclusion prompt.
8. **README** — only after the above is real.

If any step fails, fix it and re-run. **Do not deliver a demo whose `test.sh` you have not personally seen pass.** Do not paste hypothetical output into Conclude.md.

### 4. Required documents (all 中文 with YAML frontmatter)

All docs follow the user's global convention: YAML header first (no H1 before YAML), 2 blank lines, then `# H1 Title`, then a `> [!note] **Ref:**` block citing sources of truth (notes under `note/`, source files under `src/`, upstream wiki URLs).

#### `README.md`
- YAML frontmatter (`title`, `tags`, `desc`, `update`)
- `> [!note] **Ref:**` block linking to:
  - relevant `note/**/*.md` knowledge base entries
  - this project's own `Design-*.md` files
- **目标 / 现象** — what phenomenon is being proved, in one paragraph
- **项目结构** — `tree -L 2` style listing
- **部署** — prerequisites, env (`source ~/imx/imx-env.sh`), how to push to EVB
- **一键测试** — literal `./test.sh` invocation + expected exit code + where to read the log
- **清理**

#### `Design-<topic>.md` (and siblings) — *the architectural backbone of the demo*

> Design-\*.md 的目的是让一个**没读过源码的人**只看这份文档，就能在脑子里把整个 demo 跑一遍：知道有哪些组件、谁调用谁、关键函数长什么样、为什么这么写。**没有源码片段的 Design 文档是不合格的。**

必须包含的小节（缺一不可）：

1. **YAML frontmatter + Ref block** —— 引用本项目 `src/**` 下的源文件（相对路径），以及 `note/**` 中对应的理论笔记。

2. **整体架构 (架构图)** —— mermaid `graph TD`，覆盖**所有**关键组件 + 数据流向 + 用户态/内核态边界。中文或含特殊字符的节点必须加引号：`Node["GPIO库"] -- register --> Sysfs["/sys/class/gpio"]`。架构图下面跟一段散文，解释每个组件存在的理由。

3. **运行时时序 (时序图)** —— mermaid `sequenceDiagram` + `autonumber`，至少画出一条**完整的代表性调用链**（例如 "open → read 阻塞 → 工作队列唤醒 → 数据返回"）。多种范式各画一张子图。`rect rgb(...)` 背景三通道必须 > 200。

4. **关键 API 走读 (带源码片段) ★** —— 这一节是 Design.md 的灵魂，**新 demo 经常省略，禁止省略**。对每一个承载关键机制的函数，按下面的模板写：

   ````markdown
   ### `xxx_read()` —— 阻塞读实现

   **位置**：`src/adv_io_fops.c:L78-L102`
   **作用一句话**：当环形缓冲区为空时让进程睡在等待队列上，被工作队列唤醒后再拷贝数据。

   ```c
   // src/adv_io_fops.c:78
   static ssize_t adv_io_read(struct file *filp, char __user *buf,
                              size_t cnt, loff_t *off)
   {
       struct adv_io_dev *dev = filp->private_data;

       if (kfifo_is_empty(&dev->fifo)) {
           if (filp->f_flags & O_NONBLOCK)
               return -EAGAIN;
           if (wait_event_interruptible(dev->r_wq,
                                        !kfifo_is_empty(&dev->fifo)))
               return -ERESTARTSYS;
       }
       ...
   }
   ```

   **为什么这么写**：
   - `wait_event_interruptible` 而不是 `_uninterruptible`：允许 Ctrl-C 打断，避免 D 状态进程；
   - 先判 `O_NONBLOCK` 再睡：把非阻塞分支前置，让 §4 的 nonblock 测试拿到 `-EAGAIN`；
   - 唤醒方在 `producer_work()`（见下一小节）。
   ````

   规则：
   - **必须粘贴真实的源码片段**（`Read` 工具读出来再贴），不要凭印象写伪代码；
   - **必须给出 `file:line` 范围**，并和当前 `src/` 实际行号一致；
   - 解释聚焦 **为什么** 而不是 **是什么**：选型权衡、易错点、和理论笔记的对应；
   - 每个非平凡的 fops / ISR / 工作函数 / 关键 helper 都要走读一遍，不要只挑一两个。

5. **源码巡读 (Source-walk)** —— 按"用户调用入口 → 内核响应路径 → 数据流回路"的叙事顺序，把 `src/` 当成一篇文章读完，串联第 4 节里所有片段，让读者建立完整心智模型。

6. **拆分原则**：单文件超过 ~400 行或承载多个独立轴时，必须按轴拆分为多个 `Design-<topic>-*.md`，照搬 `./ref-03-Advanced-IO/` 的做法：
   - `Design-Drv.md` —— 驱动分层与 fops 选型
   - `Design-Drv-TimeSeq.md` —— producer/consumer 时序与唤醒路径
   - `Design-app-c.md` —— 用户态测试程序设计要点

   每个文件内部仍然要有自己的架构图 / 时序图 / 关键 API 走读三件套。

**反模式（新 demo 出现过，禁止重蹈）**：
- ❌ 只有架构图和文字描述，没有任何源码片段 → Design.md 退化成 README
- ❌ 贴了源码但没标 `file:line`，读者无法回到源头
- ❌ 关键 API 只列函数名不展开 → 等于没写
- ❌ 时序图缺 `autonumber`，或只画 happy path 不画异常分支
- ❌ 用英文写 Design.md 然后翻译（参见 §5 中文优先）

#### `Conclude.md` *(this file is the point of the whole demo)*

> The conclusion document is what the user actually keeps. It must turn raw log lines into **knowledge that links back to the broader note system**. Treat writing it like writing a lab report, not release notes.

Required structure:

1. **YAML frontmatter + Ref block** linking back to the `note/**` entries this demo validates.
2. **现象速览** — a one-paragraph TL;DR: "我们想验证 X，跑完后观测到 Y，因此结论是 Z。"
3. **实测输出** — the **literal** captured output from `output/log/run.log`, fenced as ```text. Do not paraphrase, do not invent numbers, do not omit timestamps. If the log is long, quote the load-bearing lines and reference the full file by path.
4. **逐现象分析 (Phenomenon-by-phenomenon)** — for each observable behavior the demo set out to prove, write a subsection containing:
   - **预期 (Expected)**: what the theory/note says should happen, with a back-reference to the specific `note/**` file and section
   - **实测 (Observed)**: the exact log lines that demonstrate it, quoted
   - **机理 (Why)**: the kernel/library/protocol mechanism that produced it, in your own words, citing `src/` lines (`file.c:NN`)
   - **若不成立会怎样 (Counterfactual)**: briefly — what would the log look like if the mechanism were broken? This is what makes the observation falsifiable.
5. **结论表 (Conclusion table)** — a markdown table tying each observation to a deeper concept the demo proves and the note it lives in:
   | # | 观测现象 | 验证的概念 | 对应笔记 |
6. **延伸 / 待办** — questions the demo opened but did not answer; candidate follow-up demos.

**Anti-patterns for Conclude.md** (do not do these):
- Paraphrasing the log instead of quoting it
- "运行成功" with no analysis
- A wall of text with no structure
- English-first drafting then translating — write 中文 from the start
- Conclusions that don't reference either `note/**` or `src/**`

### 5. Convention compliance (project + global)

This user's `~/.claude/CLAUDE.md` and `imx/CLAUDE.md` are binding:

- **Markdown**: YAML header first, no H1 above YAML, 2 blank lines after YAML close, then H1.
- **Mermaid**: quote any node name with special chars or 中文: `GpioLib["GPIO库"] -- register --> Sysfs["/sys/class/gpio"]`. `sequenceDiagram` MUST include `autonumber`. `rect rgb(...)` backgrounds must be high-brightness (R,G,B > 200).
- **中文优先**: 所有最终文档输出使用中文，保留专业英文术语。**Do NOT draft in English first and translate.** Write 中文 from the first keystroke.
- **Ref block**: every doc starts with a `> [!note] **Ref:**` block citing local notes / source files / upstream wikis.
- **Note naming** (if writing into `note/`): `NN-topic.md`, two-digit serial + short English topic; Chinese title goes in YAML.
- **EVB workflow** (kernel demos): `source ~/imx/imx-env.sh`, `ssh imx` for the board, `prj/mount/` is mounted at EVB `/mnt/`. Drop the `.ko` and test binaries into the project's `output/`, then copy under `prj/mount/` for the EVB to see them.

### 6. Delivery checklist

Before saying "done":

- [ ] `tree` matches §1; no binaries / `.ko` / `.o` / logs outside `output/`
- [ ] `./test.sh` was run by **you**, exit 0, log captured under `output/log/`
- [ ] Each `Design-*.md` has 架构图 + 时序图(autonumber) + **关键 API 走读（带真实源码片段 + file:line）** + 源码巡读
- [ ] `Conclude.md` quotes **real** captured output, has 逐现象分析, has 结论表 linking to `note/**`
- [ ] `README.md` has Ref block, 部署 steps, one-click test instructions — verified
- [ ] All docs are 中文-first with YAML headers; mermaid quoting + autonumber + high-brightness rects all correct
- [ ] `.gitignore` ignores `output/*` except `output/.gitkeep`

If any box is empty, you are not done.

## Execution flow when invoked

1. **Phenomenon** — extract one-sentence "what are we proving?" from the user. Do not proceed until explicit.
2. **Survey** — look at `./ref-03-Advanced-IO/` (or the closest sibling demo) to mirror its layout/build/test conventions for this repo.
3. **Language + build** — pick per §2; ask only if ambiguous.
4. **Design first** — draft `Design-<topic>.md` in 中文. For non-trivial demos, split into multiple `Design-*.md` files. Show the user the design and pause for alignment if scope is uncertain; otherwise proceed.
5. **Scaffold** the layout from §1.
6. **Implement** `src/` + `test/` + build recipe + `test.sh`.
7. **Run `test.sh`**. Iterate until green. **Read the log line by line.**
8. **Write `Conclude.md`** in 中文 from real output, following the structure in §4. This is the highest-leverage step — invest in it.
9. **Write `README.md`**.
10. **Final smoke test** — run `test.sh` one more time end-to-end.
11. Hand the user a short summary: where the demo lives, what to run, and which section of `Conclude.md` to read first.
