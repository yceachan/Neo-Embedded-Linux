---
name: design-demo
description: Design and deliver a self-contained, end-to-end-debugged observation demo. Strict project layout, agent owns the full design chain, and required 中文 docs (README, Arch, Impl-*, Phenomenon). Use when the user says "design a demo", "做一个 demo", "验证 X 现象", or invokes /dd. Never deliver until the chain is green AND Phenomenon.md is written from REAL captured output.
---

# Design-Demo Skill

A skill for building **observation-grade demos** — small projects whose purpose is to *prove a phenomenon*, not to ship a product. The Agent owns the full lifecycle:

> Arch → implement → build → run → **observe** → **conclude** → document

**Never hand off until the chain is green and the conclusion is grounded in real captured output.**

This skill exists for demos complex enough to need upfront design. A single-file API verification or a one-liner shell experiment does not need it — just write the code directly.

## When this skill triggers

- User says "design a demo for X" / "做一个 demo 验证 X" / "写个最小例子证明 Y" / "*show* Z with code"
- User invokes `/dd`
- The demo involves multiple components, timing dependencies, or design tradeoffs worth writing down

## Project layout

```
<demo-root>/
├── README.md              # 中文; how to Getting Start (written LAST)
├── Arch.md                # 中文; architecture + time-sequence design (written FIRST, before any src/)
├── Impl-*.md              # 中文; source walkthrough — one per major component (optional but typical)
├── Phenomenon.md          # 中文; real captured output → analysis → conclusions table
├── src/                   # source
├── [test/]                # test program source (C/Rust driver demos typically have this)
├── build-system           # Makefile, CMakeLists.txt, Cargo.toml, shell script, etc.
├── output/                # ALL binaries + logs
│   ├── .gitkeep
│   ├── bin/               # compiled binaries
│   └── log/               # captured stdout/stderr from actual runs
├── [deploy.sh]            # sync binaries to EVB (only for cross-compile demos)
├── test.sh                # one-click: clean → build → (deploy) → run → tee logs → assert
└── .gitignore             # ignore output/* except output/.gitkeep
```

**Hard rules**:

- No binaries under Git sight — `output/bin/` and `output/log/` are gitignored except `.gitkeep`.
- `test.sh` is the **full chain** for the user. Its contract: clean → build → (deploy if cross) → run → tee output to `output/log/` → exit non-zero on failure. The Agent writes it to fit the actual project — no rigid template, but the chain must be complete.
- `deploy.sh` is optional. Include it only when the demo targets an EVB (cross-compile + rsync/scp). For local demos, omit it; `test.sh` runs directly.

## Workflow — the Agent MUST complete each step before proceeding

The numbered order is intentional. The Agent must not reorder or skip steps. The biggest failure mode of this skill is **jumping to code before design is clear** — step 2 is the guardrail.

### Step 1 — Clarify the goal

Write exactly **one sentence** that captures what phenomenon this demo aims to prove. If you cannot state it in one sentence, the scope is not clear enough — narrow it.

### Step 2 — Arch.md (write FIRST, before any src/)

Arch.md exists to force architectural clarity before a single line of code is written. If you find yourself writing code and then "backfilling" Arch.md, you have already failed this step.

Arch.md must contain:

- **Architecture diagram** (Mermaid) — components, their relationships, data flow. This is not a code-structure diagram (no "src/foo.c → src/bar.c"); it shows runtime entities: processes, kernel threads, wait queues, buffers, buses, registers, etc.
- **Time-sequence diagram** (Mermaid sequenceDiagram) — required when the demo involves multiple concurrent actors (process + workqueue + interrupt, client + server, producer + consumer). Show who initiates what, who waits on whom, and the wake-up / callback paths.
- **Key design decisions** — a table listing each non-obvious choice and *why* it was made over the alternative. Examples: spinlock vs mutex, blocking vs polling, shared memory vs message passing, select vs epoll.

**Every diagram must be accompanied by prose.** Before or after each Mermaid block, explain in plain language: what the diagram depicts, which relationships are most important, and how it connects to the design decisions table. A diagram without interpretation is a puzzle for the reader — the Agent's job is to tell the reader what story the diagram is telling.

**Anti-patterns that make Arch.md worthless:**
- A box-and-arrow diagram that just mirrors the directory structure
- A sequence diagram drawn *after* the code was written, matching the implementation exactly with no design insight
- Design decisions that say "we used X because the code uses X" (circular)
- Skipping Arch.md entirely and writing src/ first
- Mermaid diagrams standing alone with no surrounding text — the reader should not have to decode arrows to understand the architecture

If you genuinely cannot think of a design tradeoff worth discussing, ask yourself whether this demo needs the design-demo skill at all.

### Step 3 — Implement

Write `src/` and `test/` and the build recipe. Follow the architecture laid out in Arch.md. If implementation reveals a flaw in the design, **update Arch.md first**, then fix the code — don't let them drift apart.

Source code conventions:
- Match the language and idiom of the surrounding repo (`prj/`).
- For C kernel modules: `src/` holds driver source, `test/` holds user-space test programs, `Kbuild` + `Makefile` at root.
- For Rust user-space: standard Cargo layout under `src/`, `Cargo.toml` at root.
- Warnings are errors in spirit — fix anything the user would reasonably ask about.

### Step 4 — Build

Run `test.sh` (or the build portion of it) and make it succeed. Fix all warnings. The build must be clean before proceeding.

### Step 5 — Run

Execute the demo on the target environment:
- **Local demo**: run directly, capture stdout/stderr to `output/log/`.
- **Cross-compile demo**: deploy to EVB (via `deploy.sh` or manual scp/rsync), then run on the target, capturing both the local deploy output and the remote session output to `output/log/`.

The key word is **capture**. Do not run interactively and summarize from memory. Pipe everything through `tee` into `output/log/`. These log files are the raw evidence for Phenomenon.md.

### Step 6 — Observe

Read the captured logs. Extract the *phenomenon* the demo was supposed to prove. If you cannot see it in the logs, the demo is not done — go back to Step 3.

Ask yourself:
- Can I point to a specific log line that demonstrates the claimed behavior?
- Is the timing / ordering consistent with the sequence diagram in Arch.md?
- Did anything unexpected happen that deserves mention?

### Step 7 — Phenomenon.md (write from REAL captured output)

Phenomenon.md is the **core deliverable** of this skill. It turns raw log lines into knowledge that links back to the broader note system. Treat it like a lab report, not release notes.

Phenomenon.md must contain:

- **`> Aimed:`** — the goal, repeated for context.
- **Observations table** — the heart of the document:

  | # | 观测点 | 预期行为 | 实际 log 片段（来自 output/log/） | 验证结论 |
  |---|--------|----------|----------------------------------|----------|
  | 1 | *what event are we watching* | *what should happen* | *pasted from real log* | *pass / fail / note* |

  Every row in the "实际 log 片段" column must be traceable to a file under `output/log/`. If you are tempted to type a hypothetical log line by hand — stop. That is the cardinal sin of this skill.

- **Key log analysis** — for the most important 2–4 log sequences, walk through them line by line: what each line means, how it maps to the Arch.md sequence diagram, and why it proves the phenomenon.
- **Link to note system** — at least one reference to a relevant file under `note/`. The demo does not exist in isolation; it is evidence for the theoretical framework laid out in the notes. Example: `note/SysCall/IO/04-IO范式总览.md`.
- **Unexpected findings** (if any) — things you observed that were not predicted by the design, or behavior that contradicted assumptions. Honesty here is more valuable than a clean story.

**Anti-patterns:**
- Hypothetical / idealized log output
- A conclusions table with empty "实际 log 片段" cells
- No links to the note system
- "Everything worked as expected" with no analysis

### Step 8 — Impl-*.md (source walkthrough)

Written after the demo runs and the phenomenon is confirmed. Impl-*.md explains *why* the code looks the way it does — not *what* the code says (the code already says that).

Each Impl-*.md covers one major component (driver, app, library, protocol layer). Every Impl-*.md MUST follow this three-section structure:

**Section 1 — 数据模型 (Data model)**
- Show the key struct(s) with each field's type, purpose, and lifecycle (created in → used in → destroyed in).
- Use a table or annotated struct definition — the reader should understand what state the component holds before reading any control flow.

**Section 2 — 源码执行流程 (Source execution flow)**
- Walk through the code in execution order — from entry function through initialization to runtime loop or event handlers.
- Use indented pseudocode or annotated call chains. Show the actual function names and file locations so the reader can follow along in the source.
- This is NOT a prose summary of "what the component does" — it is a guided tour of the code path, step by step.

**Section 3 — 关键实现细节 (Key implementation details)**
- Individually noteworthy decisions, tradeoffs, or pitfalls. Each point should answer "why this way and not another."
- Link back to specific design decisions in Arch.md where relevant. Example: "Because Arch.md chose spinlock for the ring buffer (producer runs in BH context, cannot sleep), `ring_push()` uses `spin_lock_irqsave` — see `src/adv_io_dev.c:42`."

**Anti-patterns:**
- Organizing by concept instead of by code path — the reader can't find where things happen
- Line-by-line code translation ("first we declare a variable, then we check if it's null...")
- Repeating what Arch.md already said without adding implementation-level detail
- Writing Impl-*.md for trivial code that is self-explanatory
- Omitting the data model section — the reader needs to see the struct before understanding the flow

### Step 9 — README.md (write LAST)

README is the entry point for someone who has never seen this demo. It must be self-contained for "getting started" but should not duplicate the deep content in Arch / Impl / Phenomenon — link to those instead.

README.md must contain:

- **What this demo proves** — 2-3 lines, linking to Phenomenon.md for details.
- **Dependencies** — toolchain, kernel version, libraries, hardware (if any).
- **Quick start** — copy-paste-able commands:

  ```bash
  cd <demo-root>
  ./test.sh          # one-shot: build + run + verify
  ```

- **Expected output** — a snippet of what success looks like (pasted from `output/log/`, not invented).
- **File index** — 2-3 lines listing the key files and what each does, so the reader knows where to look next.

## Delivery checklist

Before telling the user the demo is done, the Agent MUST verify every item:

```
[ ] Arch.md was written BEFORE src/ (check git log or file timestamps if unsure)
[ ] Arch.md contains: Aimed, architecture diagram (with prose), design decisions table
[ ] Arch.md contains: time-sequence diagram (if multi-actor), with prose explanation
[ ] Arch.md: no diagram stands alone without surrounding text
[ ] Build is clean — zero warnings the user would ask about
[ ] test.sh was run by the Agent and exited 0
[ ] output/log/ contains real captured stdout/stderr from the Agent's own run
[ ] Phenomenon.md observations table has real log snippets in every row
[ ] Every log snippet in Phenomenon.md can be found in output/log/
[ ] Phenomenon.md links to at least one file under note/
[ ] Impl-*.md follows the three-section structure: data model → execution flow → key details
[ ] Impl-*.md explains implementation rationale, not code syntax
[ ] README.md quick-start commands work when copy-pasted
[ ] README.md links to Phenomenon.md for the full analysis
[ ] .gitignore ignores output/* except output/.gitkeep
```

If any item fails, fix it and re-check. **Do not deliver a demo whose test.sh you have not personally seen pass.** Do not paste hypothetical output into Phenomenon.md.

## Document conventions

All docs follow the user's global conventions (`~/.claude/CLAUDE.md` and `imx/CLAUDE.md`):

- YAML frontmatter with `title`, `tags`, `desc`, `update` — no blank line before the opening `---`.
- 中文 for narrative, English for technical terms.
- Mermaid diagrams follow the Writing-Mermaid skill conventions: `""` quoting, `autonumber` on sequence diagrams, `rect rgb(>200,>200,>200)` for high-brightness backgrounds.
- References to external sources go in a `> [!note] > **Ref:**` block below the H1 title.
