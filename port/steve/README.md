# Steve in SM64DS v0.9.2

Run the Windows PC port from the repository root:

```powershell
./port/steve/run.ps1
```

The launcher starts the verified official Minecraft **1.18** server, waits for
the IPC adapter, and launches `build/port/walk_window.exe` with Steve enabled.
It saves and stops Minecraft when the game exits. The existing EULA acceptance
is retained. Java 17 is installed at the path configured in the scripts.

To rebuild, run `port/steve/build-game.cmd` and `port/steve/server/build.ps1`.
Game assets are in `runtime/`; source assets remain pinned to the requested
Minecraft 1.18 asset repository. Generated packs and the server stay untracked.

## Controls

| Input | Action |
|---|---|
| WASD / existing game controls | Nintendo movement; steer boats or swim |
| Left mouse | Mine blocks; punch a targeted native boss, Goomba, Koopa or Minecraft mob |
| Right mouse | Place/use the held item, open a workbench, interact with a mob or enter a boat |
| Release right mouse | Release a charged bow or stop using an item |
| 1–9 | Select a hotbar slot |
| I | Inventory and 2×2 crafting; click slots to move stacks, right-click to split/place one |
| B | Block catalog; `[` and `]` change pages; click an item to receive a stack |
| M, then 1–9 | Spawn zombie, skeleton, creeper, spider, pig, cow, sheep, chicken or boat |
| V | Toggle creative/survival |
| F | Dismount; otherwise toggle Elytra flight while airborne |
| R while flying | Use a firework from the offhand |
| A/D while flying | Turn; W/S change the flight pitch |
| Space / Ctrl in placed water | Swim up / down |
| G | Assemble and launch the verified redstone flying engine |
| Shift+G | Assemble and arm the sustained TNT bomber |

Place a crafting table and right-click it for the actual vanilla 3×3 crafting
menu. Inventory icons use item-model texture layers; placed blocks use their
block-state model textures. Items without a flat sprite use a block preview.
Tall previews are fitted to their inventory slot, including shields and tridents.

The engine requires **6 slime blocks, 2 sticky pistons and 2 observers** in
survival. The bomber requires **8 slime blocks, 2 sticky pistons, 2 observers,
1 TNT, 1 dead brain coral fan, 1 cobblestone wall, 1 detector rail and 1 minecart**.
It needs a clear six by four by three block space, five blocks east and seven blocks above Steve's feet.
Construction stops hold it until chunk ticking and observer startup pulses
settle. The engine then travels east using vanilla redstone. TNT primes, falls
and explodes using the official server's TNT entities. A narrow adapter hook
preserves moving TNT through piston finalization and suppresses duplicate
placement notifications for two ticks. This compatibility change lets the
coral/rail module retain its TNT while dropping repeated primed copies; it is
not an unmodified-vanilla duplication claim. Arming and temporary stop removal
run in phases, with the minecart pushed onto its detector rail before launch.
Fresh inventories include the coral, wall, rail and minecart for this module.

## Integration and bounds

Minecraft handles block updates, redstone, TNT, mining, durability, mobs,
crafting, item use, boats, Elytra, health and inventory persistence. Nintendo
walking/jumping remains in the host engine. Boat, placed-water swimming and
flight positions come from Minecraft. Wing flight preserves the previous chest
item. Nintendo damage and healing are forwarded once; Minecraft oxygen owns
drowning. Coins refill air.

Each Nintendo level/area has a separate Minecraft arena. Static stage triangles
are imported incrementally using triangle/voxel intersection tests. Original
ground is preserved for walking; edited cells carve stage geometry and collision.
Imported floor columns carry their measured fractional top height into the
server's collision queries; mining or replacing them restores the new block's
normal shape. Ray carving continues to later contacts, and sphere carving
tests the actual triangle contact rather than disabling the entire query.
The actor-loop integration runs once per frame in both hosted loop paths.
Placed blocks use the official server's collision shapes, including slabs and
fences. Chunk meshes and moving-piston geometry follow the live IPC deltas. Delayed
terrain imports preserve placed blocks, original terrain IDs and mined holes.

The terrain grid uses 100 Nintendo units per block, with measured column tops;
slopes within a column remain an approximation for Minecraft entities. Chests,
beds, shulkers, signs, banners, skulls, dragon heads, bells and conduits now use
entity-atlas meshes; shields and tridents have inventory preview meshes and
minecarts render in the world. Live block-entity snapshots animate chest lids
(including double and ender chests), oriented shulker lids, banner cloth, bell
strikes, powered dragon jaws and active conduits. Signs render four lines of
NBT text with dye/glow, Unicode BMP glyphs, component colors and text formatting.
Banners composite their ordered NBT pattern layers using official dye colors.
Container menus use vanilla slot coordinates and support real item transfers.
Mob rigs remain simpler than Minecraft's client renderer.

Steve's punches also reach native Nintendo actors. Bosses receive a visible
launch, continue falling outside the camera, stop at walls and placed collision
shapes, and recover if they land back on their platform. Falling 600 units below
their last supported position, or landing more than 160 units below it, counts
as leaving the arena; the reward appears at that last supported position.
Bowser retains his two castle keys and final Grand Star; King Bob-omb, Whomp King,
Big/Chill Bully, Eyerok and Wiggler use their tracked stars. Goomboss, Chief Chilly
and the Luigi boss retain their character keys. Course Big Boos resolve their
star through their icon owner. Goomba and Koopa variants use Nintendo's normal
launch/death, coin and death-table code. Enclosed arenas still require an exit
or a mined opening for a fall; a punch does not delete a wall.

Verified native reward pickups: final Bowser, King Bob-omb, Whomp King,
Big Bully, Chill Bully, Goomboss, Chief Chilly and Wiggler. Goomba and Koopa
defeats also passed. King Boo and Eyerok remain unverified: their introductory
states did not reach an attackable boss in the automated arena tests. Their
reward adapters are present, but do not treat them as completed gameplay.

Underwater coins refill oxygen through IPC and release visible bubbles. The full
air meter stays visible underwater. Nintendo swimming retains its native controls;
Minecraft water uses W/A/S/D, Space to rise and Control to dive.

With the local server running, `port/steve/test-combat.ps1 -Level 40 -Actor 279`
tests a real arena with scripted positioning and aim, waits for native reward
collection, rejects actor faults, and writes evidence under `build/steve/`.
Use `-Level 6 -Actor 189` for King Bob-omb, or `-Level 6 -Actor 200 -Enemy`
for Goombas. These are automated fixtures, not a recording of manual play, and
use disposable Nintendo save files. They change their separate Minecraft QA
arenas; preserve the runtime world before running a test suite.
The adapter bounds cached cells to 65,536, active forced chunks to 128 and
synchronized entities to 128. It uses the overworld as the arena backend.
Visual snapshots cover up to 512 nearby block entities within 64 blocks, with
a bounded packet budget. Signs accept up to 96 codepoints per line. The pack
includes 222 Unicode BMP font pages; unavailable glyphs use a question mark.
Startup releases stale saved forceload tickets before applying that active budget.
Summoned mobs are enabled; natural spawning remains disabled by game rule.

The workspace cap is **1 GiB including Git, assets, server and build files**.
`check_budget.py` checks the complete tree. The launcher also monitors growth
and asks the owned server to save/stop at 950 MiB, leaving room for the save.

## Verification

Live-server tests are in `server/test_*.py`; reports are in `build/steve/`.
They cover block deltas, durability, eight mobs, TNT, a moving flying bomber,
material consumption, crafting, swimming and oxygen, boats, flight, equipment,
level isolation and death/respawn. They use the actual 1.18 server.

`test-native.ps1` runs isolated native scenes and verifies the game process:

```powershell
./port/steve/test-native.ps1 -Craft -Scene 917
./port/steve/test-native.ps1 -Health -Scene 920
./port/steve/test-native.ps1 -Flight -Scene 923
./port/steve/test-native.ps1 -Swim -Scene 924
./port/steve/test-native.ps1 -SwimCoin -Scene 1018
./port/steve/test-native.ps1 -Hole -Scene 925
./port/steve/test-native.ps1 -Shape slab -Scene 926
./port/steve/test-native.ps1 -Shape fence -Scene 927
./port/steve/test-native.ps1 -Stock
```

These tests require the server (`run.ps1 -ServerOnly`) and produce screenshots
and logs. Test scripts restore their environment variables afterward. Run each
test against an unused scene or clean its prior fixture first.

Block-entity verification (run tests one at a time; IPC supports one client):

```powershell
python port/steve/server/test_block_entities.py
python port/steve/server/test_block_animations.py
# Restart the server before this persistence check:
python port/steve/server/test_block_entities.py --persisted
./port/steve/test-native.ps1 -Animated -Scene 991
./port/steve/test-native.ps1 -Conduit -Scene 992
```

The `V` snapshot record carries cell, kind, animation progress, dye color,
glow/hunting flag, base64 UTF-8 text, ordered pattern/color pairs and per-glyph
component styles. These records are complete snapshots, so removal and scene
changes cannot retain stale NBT visuals. `L` records carry real menu slot
coordinates. The local `decorate` request accepts base64 SNBT only for sign
text/dye/glow and banner patterns; it does not expose a general server-command verb or inventory replacement.
