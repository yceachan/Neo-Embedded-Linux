---
description: Activate the design-demo skill to design, build, debug and document a self-contained observation demo (C/C++/Rust)
---

Invoke the **design-demo** skill (`.claude/skills/design-demo/SKILL.md`) and follow its rules **strictly and without shortcuts**:

- Project layout `src/ build/ output/` — every binary, object, dump and log MUST live under `output/`.
- You own the full chain: design → write → build → run → observe → document. Do NOT deliver until `test.sh` has been personally run by you and passes.
- Required deliverables: `README.md`, `Design*.md` (architecture + sequence diagrams + source walk-through), `Consult.md` (real captured output + analysis).
- Provide `test.sh` as the single one-click entry point.
- Respect repo CLAUDE.md conventions (YAML headers, mermaid quoting, 中文输出).

User request follows:

$ARGUMENTS
