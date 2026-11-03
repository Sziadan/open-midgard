# Resume packet implementation

Next action: **PK00.01 (S)**. Read ../PACKET_IMPLEMENTATION_PLAN.md and WORK_PACKAGES.md. Do not start protocol edits until the baseline is reproducible.

At planning time: branch main, HEAD9c158616, about80 dirty/untracked paths, including Commerce, GuildState and Qt guild/commerce implementation. The planning commit tracks documentation/audit tooling only and deliberately does not absorb runtime changes. First inspect `git status --short`, `git diff`, existing commits and all untracked dependencies. Preserve user work. Document an agreed baseline snapshot/commits with owned paths; do not reset/clean/stage all. If ownership is unclear, record that blocker and continue only source research that does not depend on the baseline.

Effective source baseline in audit: rAthena PACKETVER/PACKETVER_SAK_NUM20080910, source at C:/Users/sziad/source/repos/rathena-master; client default profile23. Reconfirm rather than assume. Current server runtime binary provenance is not established. Ref is ignored and read-only.

No packet feature is certified complete by this plan. After PK00, follow PK01→PK02→PK03, then framing repairs PK04–PK09. PK50 quests is appended numerically but scheduled by dependencies. PK48 depends on all feature/audit packages, so it cannot bypass quests or unresolved older/later families. Server-side trade correction in PK13 requires separate explicit authorization; client-only timeout is not the fix.

Validate plan: `python tools/check_packet_plan.py`. Refresh generator: `python tools/generate_packet_plan.py --refresh-coverage` preserves progress and adjudicated coverage. Audit regeneration first overwrites annotations; run render_packet_audit.py afterward and review diff. Work packages are broad specifications: create per-contract child IDs before assigning any broad implementation/review step. Complete approved L leaves only; unresolved protocol inference needs S.

Build: configure mingw-qt-x64 with -DRO_ENABLE_DEV_DEPLOY=OFF; confirm cache OFF; build build-mingw-qt-x64. Focused production replay commands must be established in PK03, not guessed. Do not run installed OldRO client, deploy, restart server or do live account transactions automatically.

After every completed numbered leaf: own evidence + state/STATUS + appended PROGRESS + replacement HANDOFF + separate reviewed commit + user update with SHA. Replace this handoff with exact next leaf and context, including dirty paths and unresolved tests.
