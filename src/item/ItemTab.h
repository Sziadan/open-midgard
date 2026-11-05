#pragma once

namespace ro::item {

// Primary UIItemWnd/UIItemStoreWnd::Sort with the initialized All selectors
// {3,3,4}. Wire category (normal/equipment records) is a separate decision.
constexpr int ItemTabForType(int type)
{
    switch (type) {
    case 0: case 1: case 2: case 18:
        return 0;
    case 4: case 5: case 8: case 9:
    case 11: case 12: case 13: case 14: case 15:
        return 1;
    case 3: case 6: case 7: case 10: case 16: case 17:
        return 2;
    default:
        return -1;
    }
}

constexpr bool ItemTypeBelongsToTab(int type, int tab)
{
    return tab >= 0 && tab < 3 && ItemTabForType(type) == tab;
}

} // namespace ro::item
