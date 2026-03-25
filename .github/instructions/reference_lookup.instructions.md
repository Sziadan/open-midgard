---
description: "Use when looking up reference implementations, packet logic, server behavior, or GRF asset data in this workspace. Check the Ref sources first, including the original client decompilation, eAthena_src_2011, RunningServer, and GRF-Content."
---

# Reference Lookup Rules

- When additional implementation detail is needed, check the `Ref` folder before guessing.
- Use `Ref` as the decompiled original client reference when recreating client behavior.
- Use `Ref/eAthena_src_2011` and `Ref/RunningServer` when verifying server-side packet logic, protocol behavior, or emulator expectations.
- Use `Ref/GRF-Content` when verifying unpacked GRF asset names, paths, or data layout.
- Prefer conclusions grounded in these reference sources over unsupported assumptions.
- If the reference sources disagree, call out the mismatch explicitly and explain which source is being followed.