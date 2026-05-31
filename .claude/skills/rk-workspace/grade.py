#!/usr/bin/env python3
"""
Grade iteration-N runs against per-eval assertions.

For each eval dir (eval-N-name/), read eval_metadata.json and grade each
condition (with_skill, without_skill) by checking assertions against
outputs/answer.md, outputs/trace.md, and skill-side side-effects.

Writes grading.json into each <condition>/ dir with fields:
  { "assertions": [ {"text", "passed", "evidence"}, ... ],
    "passed": int, "total": int, "pass_rate": float }
"""
import json
import re
from glob import glob
from pathlib import Path


WORKSPACE = Path(__file__).parent / "iteration-1"


def check_assertion(assertion: dict, answer: str, trace: str) -> tuple[bool, str]:
    pat = assertion.get("pattern") or assertion.get("pattern_path_glob") or ""
    aid = assertion["id"]
    verifier = assertion.get("verifier", "regex_in_answer")

    # File-existence assertions (for distilled side effects)
    if verifier == "file_exists_glob" or "pattern_path_glob" in assertion:
        matches = glob(pat)
        if matches:
            return True, f"file(s) found: {[Path(m).name for m in matches]}"
        return False, f"no file matches glob {pat}"

    rx = re.compile(pat, re.IGNORECASE)

    # Verifiers that check trace OR answer
    if verifier in ("regex_in_trace_or_answer", "manual_or_grep_trace", "manual_count_trace_pdfs") \
       or aid in ("skill_fast_path", "skill_cited_pdf", "cites_usb_uac_pdf", "multi_pdf_synthesis"):
        for src_name, src in [("answer", answer), ("trace", trace)]:
            m = rx.search(src)
            if m:
                return True, f"match in {src_name}: {m.group(0)!r}"
        return False, "no match in answer or trace"

    # Default: search answer.md
    m = rx.search(answer)
    if m:
        return True, f"match in answer: {m.group(0)!r}"
    return False, "no match in answer"


def grade_condition(eval_dir: Path, cond: str, assertions: list) -> dict:
    out_dir = eval_dir / cond / "outputs"
    answer = (out_dir / "answer.md").read_text() if (out_dir / "answer.md").exists() else ""
    trace = (out_dir / "trace.md").read_text() if (out_dir / "trace.md").exists() else ""

    results = []
    for a in assertions:
        skill_only = "[with_skill" in a.get("text", "")
        if skill_only and cond == "without_skill":
            # Skip with_skill-only assertions for baseline (mark as N/A → passed)
            results.append({
                "text": a["text"],
                "passed": True,
                "evidence": "N/A for baseline (skill-only assertion)",
                "n_a": True,
            })
            continue
        passed, evidence = check_assertion(a, answer, trace)
        results.append({"text": a["text"], "passed": passed, "evidence": evidence})

    # Compute pass rate excluding N/A
    actual = [r for r in results if not r.get("n_a")]
    passed_count = sum(1 for r in actual if r["passed"])
    total = len(actual)
    return {
        "assertions": results,
        "passed": passed_count,
        "total": total,
        "pass_rate": passed_count / total if total else 0.0,
    }


def main():
    for eval_dir in sorted(WORKSPACE.glob("eval-*")):
        meta_path = eval_dir / "eval_metadata.json"
        if not meta_path.exists():
            continue
        meta = json.loads(meta_path.read_text())
        assertions = meta["assertions"]
        print(f"\n=== {eval_dir.name} ===")
        for cond in ("with_skill", "without_skill"):
            res = grade_condition(eval_dir, cond, assertions)
            (eval_dir / cond / "grading.json").write_text(json.dumps(res, indent=2, ensure_ascii=False))
            print(f"  {cond}: {res['passed']}/{res['total']} = {res['pass_rate']:.0%}")
            for r in res["assertions"]:
                tag = "N/A" if r.get("n_a") else ("✓" if r["passed"] else "✗")
                print(f"    [{tag}] {r['text']}")
                if not r.get("n_a") and not r["passed"]:
                    print(f"        ↳ {r['evidence']}")


if __name__ == "__main__":
    main()
