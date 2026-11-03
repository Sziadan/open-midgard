# Guild window

Alt+G opens/closes a draggable Qt guild window in the existing RoFrame/RoTheme
style. It registers with UIWindowMgr so mouse input over it cannot target the
world. Closing, moving and saved placement follow other gameplay windows.

Ref/UIGuildWnd.cpp and Ref/dhxj/UI/UIGuild.cpp divide the interface into basic
information, members, positions, skills and expulsions, with related guilds and
the notice alongside the information view. The Qt version keeps these views in
six tabs: Overview, Members, Positions, Skills, Relations and Expulsions.

Opening/refreshing sends CZ_REQ_GUILD_MENUINTERFACE (014d), then 014f requests
0–4 when the player belongs to a guild. Notice data is pushed by the server;
rAthena does not implement Ref's request type 6, so it is not sent. The receive
router and packet-size table cover 014c/014e/0150/0154/0156/0160/0162/0163/0166/
016d/016f/0174/01b6. Legacy member records are 104 bytes and position updates
are 40 bytes. Guild EXP stays unsigned. Malformed arrays leave valid state intact.

Guild masters can edit the notice, position names/invite-expel rights/EXP tax,
assign member positions and spend available guild skill points. Other members
can read these views and request guild departure through a confirmation dialog.
Edits are sent as 016e/0161/0155/0112/0159 requests. Server replies update the
display; sending a request does not fabricate a successful change. Leave/expel
notifications also remove the corresponding member and clear local membership
when appropriate.

Protocol layouts and permissions were checked against the local rAthena
src/map/clif.cpp, clif_packetdb.hpp, packets_struct.hpp and pre-re/skill_db.yml.
The client currently uses the pre-Renewal packet profile; modern roster and
expulsion packet variants are outside this implementation.

Run `python tools/tests/check_guild_window.py` after the MinGW Qt build. It
compiles production packet-parser and action-body tests with a captured socket,
then renders every tab, a regular-member view and the no-guild view using native
Vulkan and isolated fixture packets. It verifies Alt+G, actual receive framing,
permissions, bounded text fields, and nonblank/error-free QML rendering without
logging in or changing a real guild.

The native input bridge forwards pointer events for the guild panel, maintains
capture during dragging, and routes focused guild editors through the existing
Unicode/IME path. The fixture exercises real mouse messages for all tabs, title
dragging, scrolling, closing, and Unicode text input, plus Escape and Alt+G from
an editor. The title itself is simply `Guild`.

On Overview, a guild master can click the emblem box to select a 24 x 24 BMP
or PNG using the Windows file picker. PNG alpha below 128 becomes the legacy
magenta transparency key. Images are encoded as a 24-bit BMP (1782 bytes),
zlib-compressed and uploaded in CZ_REGISTER_GUILD_EMBLEM_IMG (0153, length,
compressed BMP), matching Ref's UIGuildInfoManageWnd and rAthena's
clif_parse_GuildChangeEmblem. Selection cancellation sends nothing. Invalid
dimensions, unreadable files and send failures are reported in chat. The server
still enforces its WoE and transparency restrictions. The displayed emblem
updates through the existing server version notification and download cache.
Packet tests decompress the actual upload, check BMP dimensions, orientation,
color and transparency, and verify that only masters can invoke the picker.

The picker runs on an independent worker thread with no native owner; it must
not block or disable the game window. The main loop continues processing socket
traffic and heartbeats while it is open. Its result is delivered back on the
game thread and rechecks character, guild, master status and connection before
sending. Duplicate picker requests are ignored. The asynchronous regression
test holds the picker open until eight main-loop heartbeats have run, checks
callback thread affinity and cancellation, and rejects duplicate requests.

Sending only reports a request. Success requires a valid 0152 reply with a new
version and matching compressed data; invalid replies cannot confirm an upload.
Disconnects or 15 seconds without confirmation report an unconfirmed update.
Native packet fixtures verify the confirmation and corrupt-reply paths.

The socket send path offers the whole contiguous queue to send(), rather than
splitting packets into 1024-byte scratch-buffer writes. The regression runner
`python tools/tests/check_send_queue.py` executes the production flush code with
the reported 1349-byte emblem, short writes, would-block retries, buffer growth,
ordering and disconnects. This also fixes artificial splits in other large
outgoing requests.

The inspected rAthena clif_parse additionally disconnects on a partial TCP read.
TCP does not preserve application packet boundaries, even with a single send().
`tools/patches/rathena-wait-for-complete-packet.patch` restores waiting for the
rest of a valid packet; existing invalid opcode/length checks remain in place.
It is supplied separately because changing this reference source does not
update the user's running map server. Apply it to the running server's source,
rebuild map-server and restart it for reliable handling of network fragmentation.

Emblem display and upload share a narrow magenta key (red/blue >= 240 and
green < 16). This removes almost-magenta background pixels introduced by image
editors, while retaining the pink artwork. Display applies the key to existing
server emblems too, so no re-upload is needed. New uploads normalize these
pixels to exact FF00FF for other clients. Passing a BMP path to
`check_guild_window.py` additionally tests that supplied image.

The hover emblem sits to the left of both player/party and guild/rank rows.
Its square size follows their combined text height. Native rendering checks
verify its top/bottom alignment with the two rows and spacing from the text.

Castle flag NPCs (job 722) use their server-supplied owner, independently of the
local player's guild. rAthena packs the owner into the high/low head accessory
words and the emblem version into the bottom accessory (shield for legacy
007c). Ownership removal via 01b4 clears the flag. Visible owned flags request
0151 downloads through the same throttled guild/version cache used by hover UI.
Their decoded emblems share GPU textures per guild/version and replace mesh 1
in the animated GR2 flag, as in Ref/GameActor3d.cpp RenderGuildMark. Unowned or
not-yet-downloaded flags omit that mesh rather than displaying its placeholder.

Rendering a GR2 actor refreshes the hit cache after its projected bounds become
available, including flags that finish loading while the pointer is stationary.
`python tools/tests/check_guild_flags.py` checks native Vulkan rendering,
pre-render hit-cache recovery, legacy ownership fields, emblem isolation,
ownership removal, guardian picking, and the production 0090 NPC click route.

Flag spawns may report emblem version 0 even for an owned castle. Treat this
as unspecified and request the guild's current emblem. PrepareGr2FlagEmblem
resolves the version from the server's 0152 reply before texture lookup, and
shares the download throttle with the guild UI. Native fixtures reproduce a
version-zero flag and test missing, matching and newer advertised versions.

Job 111 (HIDDEN_NPC), including prtg_cas01's treasure-room exit lever at 15,208,
is deliberately invisible in Ref and the server script. It now supplies a
projected cell-sized hit area to the picker without requiring a sprite or
submitting a render/shadow entry. Explicit server concealment still prevents
selection. The native fixture and captured-send test verify invisible lever
selection and the normal 0090 NPC conversation request.

Flag emblems retain Ref's texture footprint: GetEmblemTexture requests 24x24,
C3dDevice::AdjustTextureSize allocates 32x32, and the 24x24 pixels are uploaded
at (0,0). RenderGuildMark uses the authored UVs without adjusting for the unused
space. The native renderer therefore uploads a 32x32 transparent canvas with
the emblem in its upper-left 24x24 region. Filling the whole mesh with 24x24
incorrectly enlarged the image by 4/3 on both axes. Regression checks verify
the transparent right/bottom padding, key transparency, full 32x32 GPU upload,
and native flag rendering. Hover and guild-window emblem sizing is independent.
