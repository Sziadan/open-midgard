#pragma once

// Snapshot previews (cards/remote items) must not bind to a same-index local row.
enum class ItemInfoSource { Snapshot, Inventory, Cart, Storage };

// Primary message24: selecting the same key toggles closed; nonzero val2
// forces an update (used by selected-item fallback callers).
enum class ItemInfoAction { Select, ForceUpdate };

// Ref WID12 and WID82 share content but own independent snapshots/rectangles.
enum class ItemDetailKind { Main, SlottedCard };
