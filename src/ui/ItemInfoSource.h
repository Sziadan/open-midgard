#pragma once

// Snapshot previews (cards/remote items) must not bind to a same-index local row.
enum class ItemInfoSource { Snapshot, Inventory, Cart, Storage };
