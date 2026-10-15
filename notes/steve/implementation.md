# Steve for the SM64DS v0.9.1 PC recomp

Base: `bf42a2418` (`Bump the shipped game exe version resources to 0.9.1`).
`main` remains at `b1f63064a`; `port` starts at the release commit. This project
targets the Windows x86 PC recomp of the DS game, not a DS cartridge build.
No claim of Nintendo DS hardware compatibility follows from this implementation.

## Repository findings

The main branch's port README describes early smoke runners. The release branch
contains the playable game, native renderer, launcher, mod loader, rollback and
savestate infrastructure. Work must use the release tree rather than combine
main's declarations with release's host bridges.

`src/` is ROM-matching code. Original mod implementations belong in `port/steve/`
and narrowly scoped opt-in host seams. Do not enlarge `Player` or change its
recovered ABI. In particular, 32-bit pointers and `/vmg /vmm` are required by
the host build; a standalone mechanics core must not depend on those layouts.

The version-1 DLL API (`port/mods_api/sm64ds_mod.h`) has file filters, actor
factory replacement, level-loaded callbacks, and frame callbacks. Its frame
callback runs after actors update and submit their geometry
(`port_actor_frame_end` in `port/hal/actor_registry.cpp`). It does not provide a
pre-physics callback, collision provider, input ownership, world draw pass, or
state serialization. Plugins are disabled in multiplayer; their allocations
are outside F8/F9 snapshots. The full mod therefore requires opt-in source
integration or an expanded API, rather than a frame-callback-only DLL.

## Implementation boundaries

1. **World and timing.** One sparse world per save slot, level and area; 16-cube
   sections with compact state IDs, bounded resident data and a dirty mesh queue.
   Run at 20 Hz using simulation elapsed time, not presentation frames. Derive
   the DS game's actual simulation cadence from the frame spine; do not copy the
   video's N64 30-Hz assumption. Pause supplies zero elapsed time. Interpolate
   visual poses without writing interpolated positions into collision state.
2. **Player bridge.** Keep a native Player as the interaction proxy. Integrate
   before native actor behavior and after native collision responses so coins,
   stars, doors, damage, platforms, cutscenes and camera tracking remain valid.
   Centralize 20.12 coordinate conversion and a measured blocks-to-game-unit
   scale. Existing evidence is in `port/hal/player_fields.h`; never scatter raw
   offsets or infer scale from screenshots. Suspend Minecraft input during
   scripted states using the existing no-control field. Convert health damage
   exactly once and preserve star/door prerequisites.
3. **Steve rendering.** Procedural six-box humanoid with independently rotated
   limbs and head, atlas UVs, held items, first/third-person poses and wing gear.
   The native body draw seam is `port/hal/player_bridges.cpp`; substitute Steve
   while retaining the actor's lifecycle and native shadow. Obtain a dedicated
   render context; restore texture, material and matrix state after the draw.
4. **Voxel rendering.** `port/ntr/include/ntr/gx.h` exposes the DS geometry
   command stream and decoded polygon state. Emit meshes through this path so
   both software and graphics-card rendering receive the same world. The video
   uses N64 display lists; this port needs DS GX commands. Mesh only exposed
   faces, split transparent water from opaque terrain, add atlas textures and
   vertex lighting, and rebuild neighboring sections at boundaries. Budget mesh
   work across frames; never allocate one native actor per block.
5. **Placement and collision.** Compare voxel-grid traversal and native KCL
   raycasts; use the nearer result. Add a voxel AABB collision query for Steve
   and a shared block collision provider for native actors. Native geometry is
   KCL with a spatial index, not Minecraft voxels. Account for slopes, ceilings,
   one-way surfaces and moving platforms. The port's mesh-collider registry is
   finite, so registering a collider per block will not scale.
6. **Mining terrain.** Initially mine placed voxels only. Full level mining
   needs six-plane triangle clipping, triangulation, render and KCL updates,
   spatial-index rebuilds, exposed interior faces and explicit material maps.
   Editing only rendered triangles leaves invisible solid terrain. Exclude
   scripted boss platforms and transition geometry until corresponding state
   machines can recover from deletion.
7. **Inventory and crafting.** Separate item IDs from state IDs; stack limits,
   durability, hotbar, held items, drops, 2x2 inventory and 3x3 workbench. Match
   shaped recipes with offset/mirroring, and shapeless recipes by multisets.
   Consume ingredients atomically only when the output fits. Use an explicitly
   selected Minecraft reference version and locally supplied assets; no version
   is implied by the video. Export models, states, shape, sounds and recipes into
   versioned, validated files rather than hand-writing thousands of variants.
8. **Redstone and TNT.** Basic dust is insufficient for a bomber. Implement
   directional dust connections, strong/weak power, neighbor notifications,
   repeaters, comparators, torches, observers, pistons, sticky pistons, slime/honey
   movement groups and update ordering. A moving group must be validated then
   committed atomically, respecting push limits and block-entity state. TNT is a
   moving entity with a fuse, gravity, bounce, chain reactions and explosion
   resistance. Test an actual observer/piston bomber fixture, not merely dust
   igniting a stationary TNT block.
9. **Mobs.** Share living-entity movement and collision; implement zombie pursuit,
   skeleton arrows/aiming/strafe, creeper fuse/cancellation, spider climbing and
   passive behavior independently. Add capped spawning, drops, damage and
   terrain-aware navigation. Reusing native enemy visuals is not implementing
   Minecraft mobs. Verify behavior against the selected Minecraft version.
10. **Swimming, oxygen, flight and boats.** Resolve native water volumes and
    placed water consistently. Oxygen is 300 ticks, then a heart per second,
    with the video's coin-air concession. Wing power maps to elytra flight with
    its own timer and boost policy; DS Mario's power flower/wing mechanics must
    be checked, rather than assuming N64 cap actors. Boats need buoyancy, seated
    input, dismount safety, passenger rendering and collision.
11. **Persistence.** Keep voxel edits outside the cartridge's 8192-byte save
    image. Write a versioned sidecar atomically keyed by slot/level/area. Add
    dedicated mod snapshot/restore hooks; an STL heap cannot be captured by
    copying its pointer-containing object into `.dsstate`. Clear or restore
    modifications for ending cutscenes explicitly. Disable incompatible debug
    savestate operations until the mod's restoration path is implemented.
12. **Multiplayer.** v0.9.1's plugin prohibition is deliberate. A later network
    protocol needs authoritative edits, inventories, entities, ticks, initial
    world sync and rollback-safe state. Do not just remove the prohibition.

## Current evidence

`port/steve/core.*` is an independent original prototype: bounded sections,
negative coordinate handling, empty-section reclamation, exposed cube-face
meshing, a 20-Hz clock with bounded catch-up, same-height dust attenuation,
stationary primed TNT and chain reactions, oxygen/coin air restoration, and
atomic shaped/mirrored/shapeless recipe transactions for 2x2 and 3x3 grids.
`core_test.cpp` uses explicit checks even in optimized builds.

Validation: optimized x86 MSVC 19.44 and Visual Studio 2026 core builds passed.
Run `port\steve\build-test.cmd` to rebuild, exercise scenarios and measure size.
The original v0.9.1 CMake project configures with the Visual Studio Community
toolchain and `CMAKE_TRY_COMPILE_CONFIGURATION=Release`. Its initial default
debug compiler check failed with C1902 (PDB manager mismatch); Release passed.
The baseline `tools/port_refcheck.py` cannot parse `cmake_parse_arguments` in
the release CMake file and reports two analysis failures. No existing source
references or matched source files were changed in this work.

The stock `walk_window` target compiled and linked successfully (9,914 build
steps). A hidden castle-grounds run completed 30 frames with exit code 0 and
zero double-walk frames. It needed unsandboxed execution: the sandbox's launch
failed before game startup with an Illegal System DLL Relocation error; the
same executable and test settings succeeded outside that sandbox. This is
baseline validation only, with the Steve core still separate.

## Native integration and verification

The opt-in host frontend registers player/frame/render/health hooks without
editing recovered `src/` code or changing `Player`'s ABI. It loads 1.18 item
texture layers, 1,968 textures, 20,342 block states and 38,410 official collision
boxes. Inventory icons resolve item models; placed blocks resolve block-state
models. The native crafting-table test displayed three oak doors from six
planks, with the item sprite in the output slot.

The backend is the SHA1-verified official Minecraft 1.18 server, with an
original Java instrumentation adapter. Block notifications are captured at
method entry and resolved after the complete server tick, including updates
without the usual client-notification flag. This fixed missing piston,
observer and explosion deltas. Unsent block changes survive snapshot coalescing;
air cells persist and are replayed when an arena is revisited.

A tested ten-block observer/slime/sticky-piston engine moves using actual
vanilla redstone. Temporary construction stops wait for chunk ticking and
scheduled observer pulses. The survival builder consumes six slime blocks,
two sticky pistons and two observers. Its bomber consumes one TNT, primes it
on the moving engine and releases a falling vanilla entity. A live test observed
40 destroyed ground cells and moving-piston snapshots in the original finite
fixture. The sustained bomber now uses eight slime blocks, two sticky pistons,
two observers, TNT, a coral fan, a wall, detector rail and minecart. Its arming
piston pushes the cart onto the rail before launch. A moving-TNT compatibility
hook suppresses placement priming and duplicate neighbor notifications for two
ticks after piston finalization; subsequent coral updates use real vanilla TNT
priming. The final sustained regression recorded 30 drops and 264 destroyed cells.
This deliberately adapted duplication behavior is not claimed as stock vanilla.

Walking and jumping use Nintendo's player. Placed blocks use exported vanilla
VoxelShapes. Native ground/walls/slopes stream into Minecraft through bounded
triangle/voxel tests; original terrain edits carve stage triangles and suppress
removed KCL hits. A native terrain-hole test moved Steve from Y=254 to Y=200.
The slab test landed at Y=250 and the fence test at Y=350. Ray contacts now
filter individually within the KCL walk, preserving hits behind a hole. A
generated host-only sphere walk checks the nearest point on each triangle;
ROM source stays unchanged. Measured fractional floor tops are stored in the
STC3 cache and override both Minecraft collision-shape overloads only while
the original imported block remains. Old STC1/STC2 caches remain readable.
The grid still approximates slopes within one column.

Special block meshes now use the entity atlases for chests, beds, shulkers,
signs, banners, skulls, dragon heads, bells and conduits. Chest half joins and
latches, rotated bed UVs, banner dye tint and wall attachment offsets are baked.
Shield/trident preview meshes use private renderer IDs, excluded from the world
block catalog and collision registry. Minecarts have world geometry. Asset
rebakes replace completed files atomically; the native asset-loading regression
checks the real registry alongside private item models. Live block-entity snapshots now drive chest/shulker lids, banner cloth, bell
strikes, powered dragon jaws and active conduit cage/wind/eye rendering. Signs
support dye/glow, four Unicode BMP text lines and component colors, bold, italic,
underline, strikethrough and obfuscation. Ordered banner patterns use the official
entity textures and diffuse dye colors (distinct from sign text colors).
The full snapshot budget is 512 nearby block entities and 400,000 text-builder
characters; normal block updates retain the existing coalescing behavior.
Block-state capability caching avoids reflective lookups for ordinary terrain.
Generic container slot coordinates and 128 native slot storage support double
chests as well as ender chests and shulkers; switching menus closes the prior
vanilla menu so opener counts and carried stacks remain correct.
The asset pack includes 222 Unicode font pages with a 128 MiB decoded texture
budget. Glyphs outside the available BMP pages fall back to a question mark.

Boat, placed-water swimming and Elytra positions are authoritative in Minecraft
and applied to the native player. Live-server swimming tested forward motion,
ascent, descent and oxygen. Native runs showed swimming and oxygen depletion,
boat passenger movement, and sustained Elytra flight. Flight steering uses its
own yaw/pitch so the third-person camera's downward pitch cannot force a dive.
Flight preserves the previous chest item. Bow release creates a vanilla arrow,
consumes ammo and wears the bow.

The shared hosted health seat forwards Nintendo damage/healing once, while
retaining its original arithmetic when Steve is disabled. Nintendo's swimming
health clocks are suppressed so Minecraft owns drowning. Oxygen uses the
server's breathing/respiration logic and respects water-breathing effects.
Coin collection refills air. Saved game mode reconciles restored abilities,
fixing survival mining with leftover creative abilities. Native damage and
healing, actual death/respawn, inventory preservation and per-level arena
isolation have live tests.

The game build passes dsstate, facecycle, arm9-global bounds and KERNEL32-only
static import checks. Socket/input libraries load after DS reservations. Native
180-frame runs exit successfully, and a Steve-disabled run preserves the
baseline spawn position. Reports and screenshots are in `build/steve/`; the
launch and control guide is `port/steve/README.md`.

## Workspace budget

Initial main checkout: about 308 MiB, including 242 MiB of Git data. There was
one checkout and only one local branch. The other names were remote-tracking
refs sharing a packed object store. Removed local remote-tracking refs are
recorded in `pruned-refs.txt`; no remote branches were deleted. Origin has no
remote branch named exactly port, so origin and upstream fetches are narrowed
to main until port is published to avoid bringing extra refs back. Release history is preserved
by local port. Do not expire reflogs or prune recent objects during active work.

Copy only cartridge/extracted assets from `C:/SahilAppProjects/64DS-DX`; do not
copy its build tree, Git directory, caches or toolchains. Build one game target
instead of every smoke/hires executable. Count `.git`, extracted data and ignored
build outputs toward the strict 1,073,741,824-byte (1 GiB) workspace limit,
reduced from the initial 5 GB requirement during implementation.

Terrain imports are now write-once on the server: existing cell edits, original
terrain IDs and non-air Minecraft blocks reject late imports. This closes the
race between asynchronous native mesh discovery and placement/mining. A live
regression checks both sequential requests and a placement/import pair queued
within one tick. In-game conduit verification exposed this case in its frame.
The shutdown CLI keeps IPC alive until stop is consumed; the launcher waits for
an owned Java process to release the world before starting another instance.
