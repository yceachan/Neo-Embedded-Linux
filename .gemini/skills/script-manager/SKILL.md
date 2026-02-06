---
name: script-manager
description: Automates the registration and indexing of high-value Python scripts in the project's script library.
license: Proprietary. LICENSE.txt has complete terms

---

# Script Manager Skill

## 1. Trigger Conditions (When to use)

*   **Active**: When the Agent saves, creates, or moves a "permanent" or "high-value" script into `.gemini/scripts/`.
*   **Passive**: When the user explicitly asks to "index scripts" or "register this script".
*   **Negative**: Do *not* trigger for temporary, throwaway, or one-off debugging scripts unless explicitly requested.

## 2. Core Workflow
1.  **Identify**: Confirm the script is located in (or being written to) `.gemini/scripts/`.
2.  **Validate**: Ensure the script has a docstring describing its purpose.
3.  **Register**: Update `.gemini/scripts/GEMINI.md`.
    *   If the file doesn't exist, create it.
    *   Add a row to the "Registered Scripts" table.

## 3. Index Format (`.gemini/scripts/GEMINI.md`)

The index file must contain a table with the following columns:

| Script Name | Function | Usage Example | Tags |
| :--- | :--- | :--- | :--- |
| `example_script.py` | Brief description of what it does. | `python .gemini/scripts/example.py --arg` | `pdf`, `extraction` |

## 4. Instructions for Agent
*   **Consistency**: Always check existing tags and try to reuse them (e.g., `pdf`, `parsing`, `validation`).
*   **Brevity**: Keep descriptions under 15 words.
*   **Verification**: After updating the index, verify the entry is correct.
