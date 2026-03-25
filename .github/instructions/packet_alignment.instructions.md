---
description: "Use when working on packets, opcode mappings, packet serialization, packet parsing, or protocol behavior in this workspace. Check PACKET_VERSION_ALIGNMENT.md first and keep packet work aligned to packet_ver 23, using Ref/eAthena_src_2011 and RunningServer when the Ref client differs."
---

# Packet Alignment Rules

- Always review `PACKET_VERSION_ALIGNMENT.md` before making packet-related decisions in this workspace.
- The rebuilt client targets `packet_ver 23` for packet behavior and client-send packet alignment.
- Do not assume the decompiled client under `Ref` uses the same packet version as this rebuilt client.
- When packet version, opcode mapping, payload layout, or server expectations are in question, prefer `Ref/eAthena_src_2011` and `Ref/RunningServer` over raw client behavior from `Ref`.
- Use `Ref` for broader client behavior and implementation context, but treat packet-version-specific details as potentially different unless they match the packet alignment document and server references.
- If the packet alignment document and a reference source disagree, call out the mismatch explicitly and follow the source that matches the `packet_ver 23` target.