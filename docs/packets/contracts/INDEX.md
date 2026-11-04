# PK01 P0 applicability contract index

These are **source-backed applicability records only** for hash-pinned rAthena snapshots and named configurations. They do not certify client semantics, reachability beyond the cited call paths, UI/lifecycle, fixtures, live behavior, or implementation completeness. Independent review is deferred to PK48 and is not a prerequisite to implementation; each feature package still establishes bounded contracts and evidence in its own steps.

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
| PK44.CLAN.0988-20131223+ | Map S2C 0x0988 clan online count; inactive in selected 20080910 source config | PK44; review PK48 | Source-config applicability only |
| PK44.CLAN.0989-20131223+ | Map S2C 0x0989 clan leave acknowledgement; inactive in selected 20080910 source config | PK44; review PK48 | Source-config applicability only |
| PK44.CLAN.098E-20131223+ | Map S2C 0x098E clan chat; inactive in selected 20080910 source config | PK44; review PK48 | Source-config applicability only |

Independent reviewer: `/root/pk01_p0_review` (gpt-6-astra, high reasoning). Review summary and limits: [PK01.03 applicability approval](../evidence/PK01.03-applicability-approval.md). Server source file hashes, no-Git-metadata limitation, and effective registry evidence: [PK01.02 evidence](../evidence/PK01.02.md).
