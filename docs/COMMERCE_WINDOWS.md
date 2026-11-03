# Inventory, cart, vending, trade and player actions

The inventory remains on Alt+E and now uses a larger 360x300 Qt panel with
usable/equipment/other tabs, item counts, equipment marking and a scrollable
grid. Double-click uses/equips an item; right-click opens its description.
The Cart button appears when server cart state is present. Both bags support
quantity prompts for stack transfers. Drags onto legacy equipment, shortcuts,
storage and the ground still use UIWindowMgr's existing drop path.

Vending opens on the server's 012d response to MC_VENDING. It opens the cart
alongside the draft. Drag cart items into the draft, edit quantity/price and
enter a shop title, then Open shop. Drag a draft listing outside the window or
press Remove to remove it. Drafts do not alter cart quantities. The 0136 reply
confirms opening; 0137 sales update remaining stock. Close shop sends 012e.
Legacy skill failure 0110 (MC_VENDING 41) releases the pending-open state.

Trade invitations have explicit Accept/Decline. Drag inventory items into the
local offer and optionally offer Zeny. Added items appear after 00ea acceptance.
Lock offer sends 00eb; the final Confirm trade button is enabled only after
both 00ec lock notifications, and sends 00ef once. Cancel sends 00ed. Inventory
and actual character Zeny are never modified optimistically. The displayed
Zeny offer is editable request state because rAthena does not acknowledge it
to the sender. Normal inventory/status replies update actual possessions.
Ref's cart quantity selection and two-sided trade/money/confirmation flow are
preserved with Qt styling (Ref/UIItemWnd.cpp, Ref/dhxj/UI/UICart.cpp and
Ref/dhxj/UI/UIPalyerTrade.cpp).

The player context menu now supports party/guild invites, trade requests,
whisper targeting, friend requests, mail composition, whisper blocking and
equipment inspection. Guild invites honor master/invite permissions. Incoming
guild/friend requests require explicit replies. Equipment inspection respects
the server/player's sharing policy. Legacy mail supports recipient, subject
and body without attachments. A known vendor adds Browse shop to the menu;
0133 stock can be purchased with an explicit quantity and Buy action.
Vendor boards also appear as clickable bubbles above remote players, following
the projected actor position and disappearing when the server removes them.
Inventory/cart cells use a compact empty grid with high-contrast quantity badges.

## Protocol

Layouts were checked against the local rAthena clif.cpp, packets_struct.hpp,
clif_packetdb.hpp and trade.cpp, plus Ref/RunningServer/packet_db.txt. This
implementation targets the client's existing pre-Renewal/2008 packet family.

- Cart snapshots: 0123/01ef/02e9 normal, 0122/0297/02d2 equipment; additions
  0124/01c5, removals 0125, capacity/weight 0121, off 012b and failures 012c.
- Cart transfers: 0126/0127, index.W and amount.L.
- Vending: 012d, 01b2 (85-byte header plus 8-byte rows), 0136, 0137 and 012e.
  Buying uses 0130/0133/0134/0135; boards are tracked via 0131/0132.
- Trade: 00e4/e5/e6/e7/e8/e9/ea/eb/ec/ed/ee/ef/f0/f1 and 01f4/01f5.
- Social/mail: 0168/0169/016a/016b, 0202/0207/0208/0209, 00cf/00d1,
  02d6/02d7, 0246/0248/0249. Requests use byte-bounded UTF-8 fields.

All parsing checks minimum lengths and complete array records before replacing
snapshots. Transactions gate equipped/rental items, quantities, prices, row
limits, concurrent pending additions, and lock/commit state. The server remains
authoritative about trade restrictions, weight, funds and transfer results.
Character login resets state; map changes close transactions and discard cart
snapshots before the server refresh.

The native input bridge routes pointer, wheel, Unicode/IME and clipboard input
to visible `nativeInteractive` Qt panels. Internal item dragging resolves drops
against scene panels, avoiding the native drag service in the offscreen Quick
window. Existing chat and guild input share the same bridge.

## Validation

`python tools/tests/check_commerce.py` compiles the production commerce source
with a captured connection. It checks requests, acknowledgements, malformed
arrays, permissions, field limits, pending/locked states and connection failure.
Nine isolated Vulkan fixtures render every new window, including icons.
Five native mouse-input fixtures use a temporary 127.0.0.1 mock server to
verify the actual cart-transfer, trade-add/lock/commit and context-trade bytes,
plus cart/vending drag-in and drag-out removal, shop-bubble browsing and purchases.
The vendor fixture also verifies that OPT3 buffs do not trigger OPTION hiding,
and character selection replaces a previous character's movement speed. No live game account is used.
Loopback testing requires local network access outside the restricted sandbox.
Existing guild-window and self-hover checks cover the shared input bridge.

## Multiplayer regression fixes

Guild-position packet 01eb has a fixed 10-byte size. Treating its payload as
packet headers corrupts subsequent sitting, movement and trade notifications.
The native vendor fixture now frames a partial/concatenated position, sit and
trade-acceptance stream, then checks stand-up delivery as well.

Trade failures produce chat feedback, including distance and busy results.
Pending trade windows use 380x170; accepted trades expand to 660x420.
Guild/friend invitations match the party invitation chrome and layout.
Party HP bars use the player's zoom metrics, square black border, white empty
track, green fill and red low-HP threshold, below GUI windows. The server does
not supply other party members' SP, so these show only the matching HP row.
Carts retain projected world depth on both sides of the owner instead of
forcing front-of-owner vertices to z=0. The fixture checks their queued depth.

Pending trades now check the normal two-cell distance before requesting or
accepting, cancel when the peer moves out of range/leaves view, and expire
after 30 seconds without a reply. Closing the window clears local waiting
state immediately after sending cancellation. This also handles rAthena's
acceptance-distance failure branch, which notifies only the accepting player
before clearing both server-side partners; the requesting client receives no
failure packet in that branch. No rAthena source or running server was changed.

Guild packet coverage now includes 01eb minimap updates/removal, 017f guild
chat, 0176/0182 incremental membership, 0171/0172/0173 alliance invitations and
responses, 0181 opposition results, 0184/0185 relationship changes, and
015e/015f disband results/reasons, in addition to the existing guild-window,
notice, rank, emblem, invite, departure and online-status handlers. Native
fixtures audit framing for all 32 supported incoming guild notifications.
