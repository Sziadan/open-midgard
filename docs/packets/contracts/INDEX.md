# PK01 P0 applicability contract index

These are **source-backed applicability approvals only** for the hash-pinned rAthena snapshot evaluated with `PACKETVER=20080910` and `PACKETVER_SAK_NUM=20080910`. They do not certify client semantics, reachability beyond the cited call paths, UI/lifecycle, fixtures, live behavior, or implementation completeness. Each feature package still requires its own `.01` strong contract gate.

| Permanent ID | Identity | Owner | Status |
|---|---|---|---|
| PK01.P01.HANDSHAKE | Character S2C raw four-byte account ID | PK04; review PK44/PK47 | Approved for applicability only |
| PK01.P02.FRAMING | Cross-stage receive framing state machine | PK05 | Approved for applicability boundary only |
| PK01.P03.01F2 | Map S2C 0x01F2, 20-byte guild status | PK06; review PK47 | Approved for applicability only |
| PK01.P04.01A2-20080910 | Map S2C 0x01A2, 35-byte pet status | PK07; lifecycle PK27; review PK47 | Approved for applicability only |
| PK47.P04.01A2-later | Map S2C 0x01A2 with guarded job field, 37 bytes | PK47 | Open historical variant |
| PK01.P05.0190 | Map C2S 0x0190, 19-byte ActionRequest | PK46; remediation dependency PK08; review PK47 | Approved for applicability only |
| PK01.P05.007E | Map C2S 0x007E, 102-byte UseSkillToPosMoreInfo | PK08; review PK46/PK47 | Approved for applicability only |
| PK01.P05.0437 | Map C2S 0x0437, 7-byte profile-23 ActionRequest | PK46; review PK47 | Approved for applicability only |

Independent reviewer: `/root/pk01_p0_review` (gpt-6-astra, high reasoning). Review summary and limits: [PK01.03 applicability approval](../evidence/PK01.03-applicability-approval.md). Server source file hashes, no-Git-metadata limitation, and effective registry evidence: [PK01.02 evidence](../evidence/PK01.02.md).
