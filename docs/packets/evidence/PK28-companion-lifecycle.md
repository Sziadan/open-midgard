# Companion actor lifecycle and hunger

The selected `Zc_Notify_Newentry` path in `Ref/GameModePacket.cpp` creates an NPC or companion actor and marks the matching homunculus or mercenary GID alive. An entry that updates an existing actor leaves the flag alone. The dedicated `007C` NPC entry lacks the companion flag branch. Actor message 28 at `0x55D530` clears the owned homunculus alive flag. Ordinary actor removal leaves it intact. The existing status packets still supply companion job fields.

The `022E` property handler warns at hunger 1, 5, or 10 when the level differs from the last warned threshold. The selected executable pushes message 1018 and COLORREF `0xFF` at `0x5AF1AD` and `0x5AF1A8`. An intervening non-warning level leaves the previous warning level intact.

The registered replay covers companion creation, the NPC entry exclusion, ordinary removal, death, an update to an existing companion, and hunger warning color and deduplication. All 164 application checks and 473 standalone checks passed. The skill tree source and catalog match the baseline and the OldRO executable hash is unchanged.

Status windows, summon and dismissal, skill 244, and live companion verification remain open.

[Companion status evidence](PK28-companion-status.md), [feeding result evidence](PK28-homun-feed.md).
