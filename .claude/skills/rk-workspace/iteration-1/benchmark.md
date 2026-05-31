# `/rk` Skill — Iteration 1 Benchmark

## Headline

| Configuration | Pass rate | Mean duration | Mean tokens | Distilled files created |
|---------------|-----------|---------------|-------------|--------------------------|
| **with_skill** | **100%** (15/15 applicable assertions) | **243 s** | **55k** | **2** |
| without_skill | 100% (12/12 applicable) | 291 s | 65k | 0 |
| **Δ (skill − baseline)** | 0 | **−48 s (−16.5%)** | **−10k (−15.7%)** | **+2** |


## Per-eval breakdown

| Eval | with_skill (s / tokens) | baseline (s / tokens) | Δ duration | Δ tokens | Notes |
|------|------------------------|----------------------|------------|----------|-------|
| 1 chip-matrix-lookup | **97 / 31k** | 208 / 51k | **−53%** | **−39%** | Fast path: skill avoided any PDF; baseline opened 2 datasheets. |
| 2 gmac-rgmii-delayline | 331 / 59k | **284 / 45k** | +17% | +31% | Skill paid the distillation cost; baseline grep'd kernel source. See "trade-off" below. |
| 3 rk356x-usb-gadget-uac | **300 / 74k** | 380 / 98k | **−21%** | **−25%** | Multi-PDF synthesis: skill's resources-map pointed straight at 3 right PDFs. |


## Trade-off worth understanding

`eval-2 with_skill` looks inefficient on this one-shot benchmark (+17% time, +31% tokens). That's by design — the skill paid the cost of (a) reading 2 PDFs and (b) writing a `distilled/gmac-rgmii-delayline.md` file. On a *re-run* of the same query (cache hit), the skill should answer in seconds with near-zero tokens. The baseline has no such state and would re-do the kernel-source grep every time. **This benchmark captures the first-time cost; the skill's value compounds.**


## Pass-rate saturation

Both conditions hit 100% on functionally-verifiable assertions. This means:

- The SDK doc base is sufficient for competent agents to find correct answers without the skill.
- Pass-rate assertions in iteration 1 don't discriminate. To get useful signal in iteration 2:
  - Add **anti-trigger** assertions (e.g. test that a "iMX6ULL GPIO" prompt does NOT invoke `/rk`).
  - Add a **cache-reuse** eval: re-run eval-2 after iteration-1 populated `distilled/gmac-rgmii-delayline.md` — only `with_skill` should benefit; baseline should regress to the same 284s/45k cost.


## Side-effect artifacts (skill only)

```
/home/pi/imx/.claude/skills/rk/references/distilled/
├── INDEX.md (updated with 2 entries)
├── gmac-rgmii-delayline.md  ← created by eval-2 with_skill
└── usb-gadget-uac-rk356x.md ← created by eval-3 with_skill
```

These are persistent knowledge artifacts. Future invocations of the skill on the same topics should consult these directly (Step 2 of the workflow) instead of re-reading PDFs.
