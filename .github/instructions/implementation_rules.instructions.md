---
description: "Use when implementing features, fixes, or refactors in this workspace. Prefer complete production-ready implementations over placeholders, mocks, stubs, or quick throwaway versions."
---

# Implementation Rules

- Deliver full implementations that solve the actual requested problem.
- Do not leave placeholder logic, temporary mockups, fake data paths, or intentionally incomplete behavior unless the user explicitly asks for a scaffold.
- Prefer production-ready code paths, real integrations, and real control flow over demonstration-only shortcuts.
- If a complete implementation is blocked by missing information or an external dependency, state the blocker clearly and ask for the minimum clarification needed instead of filling the gap with a placeholder.