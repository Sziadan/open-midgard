# Legacy map property row updates

The selected client receiver at `0x5B2F30` and `Ref/GameModePacket.cpp:13696` read a signed row from offset 4 of the variable `02E7` frame. The selected executable indexes the destination as `row * 15 + offset` and advances the decimal conversion pointer by one byte after each value. The [map property contract](../contracts/PK19.MAP-PROPERTIES.md) records the wire layout and table ownership.

The receiver now bounds decimal reads to the frame and rejects any write span outside the 450-entry table before applying values. It preserves writes that cross a row boundary within that table. It updates the table without selecting another map type or recalculating world flags.

The production packet replay checks adjacent decimal suffixes, a trailing byte outside the frame, a valid cross-row update, rejected negative rows and overflow spans, and the subsequent `01D6` siege decision. The selected local rAthena MAIN 20080910 registry has a variable `02E7` entry at `src/map/clif_packetdb.hpp:957`. A source search for the opcode and packet names found that registration and no emitter. The replay uses synthetic frames.

All 164 application checks and 473 standalone checks passed with deployment disabled. The player skill tree files and installed OldRO executable matched the pre-change baseline. Live map entry and map transitions remain open.
