# Evidence: PKxx.nn

Contract IDs/approval, baseline commit and source/config hashes:
Owned files and stable commit subject:

## Source comparison

Record server emitter/parser/final-registration and Ref method branch checks with source locations. For each changed field/branch record expected and actual behavior. Explain intentional Qt adaptation and wire conflicts. List unreviewed branches honestly.

## Independent wire verification

Raw fixture origin and hash, effective guards and independent sizeof/offsetof/parser-offset output. Expected bytes must not be derived from the client serializer/struct under test. Record the production Connection/router/builder path exercised; a direct helper-only test is supplemental.

Cases: fragmentation at every meaningful boundary; concatenation with unrelated next packet; malformed min/max length/count/remainder; zero/extreme/sentinel values; string encoding; wrong stage/profile; response success/reject/cancel/duplicate; map/reconnect teardown. State which are applicable and why others are not. For snapshots assert unchanged state on malformed input. For requests assert every server-consumed offset and no unauthorized send. Never include real credentials/passwords.

## State and UI verification

Exact before/event/after assertions, generated reply bytes, pending-modal/input state and resource lifetime. Automated UI evidence or captured build-directory fixture output. Where human observation is required provide exact scenario and expected visual/interaction result. Pending live verification remains pending; screenshots alone cannot prove wire parity.

## Commands and outcomes

Exact configure/build/test commands with exit codes and current binary timestamp/hash. Confirm auto-deploy OFF. Do not cite stale tests after a failed build. Include artifact paths and concise failure output if blocked.

## Checkpoint

Updated state.json/STATUS/PROGRESS/HANDOFF, reviewed staged paths, stable commit subject, post-commit SHA reported to user. Next exact step and prerequisites. Any blocked gates or limitations, with source question and owner.
