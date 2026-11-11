# Companion commands and skill 244

The selected `CGameMode::SendMsg` cases in `Ref/GameMode2.cpp` send `022D` for message 198 with a zero type field and one-byte command, `0231` for message 199 with a 24-byte name field, and `029F` for message 200 with a one-byte action. The effective server registry assigns sizes 5, 26, and 3 respectively. Rename accepts up to 23 name bytes and terminates the field. The port zero-fills the rest of a short name field.

The selected `Zc_Use_Skill` at `0x58CB50` resets the local homunculus when the source GID of skill 244 matches the account ID. The registered `011A` handler now clears the session alive flag before effect delivery. The homunculus GID and status fields remain available for subsequent packets. Window closing and target reset depend on the corresponding companion UI and target state being implemented.

The replay captures all three sends through the production connection, including command byte truncation and short and long rename fields. Receive replay covers a foreign source targeting the local player and a local source with a zero result. All 164 application checks and 473 standalone checks passed. The skill tree window files and OldRO executable matched baseline.

The companion status windows, their confirmed action callbacks, and live verification remain open.

[Server registry assignments](PK01.02-server-registry-assignments.txt)
