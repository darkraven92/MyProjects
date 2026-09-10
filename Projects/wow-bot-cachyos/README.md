# WoW 1.12.1 out-of-process bot skeleton for CachyOS

This is Deliverable 1: a read-only architecture foundation for the exact
WoW.exe whose SHA-256 is
`b4756d38ef207c02ed651f4952bd89a70b4857b73a33413339e1b285b28d2dc7`.
It reads a Wine process through Linux `process_vm_readv`, traverses the visible
object list, creates immutable world snapshots, and feeds them to a small FSM.

It does not contain anti-cheat bypasses, input injection, navigation, combat,
or questing. The included offsets were statically verified against that exact
client executable and must not be used with a different build or hash.

## Assumptions

- CachyOS or another x86-64 Linux distribution
- WoW 1.12.1 runs through Wine/Proton
- .NET 10 SDK and runtime
- Bot and Wine process run as the same Linux user
- Target client is x86; game pointers are read as `uint`

## Run

First test one snapshot without sending input:

```bash
dotnet run --project WowBot.CachyOS.csproj -- --pid 12345 --probe
```

Then run the continuous observe-only loop:

```bash
dotnet run --project WowBot.CachyOS.csproj -- --pid 12345
```

The observe loop now prints X/Y/Z and facing. On the affected CachyOS .NET
package state, prefix commands with:

```bash
env MSBuildEnableWorkloadResolver=false dotnet run --project WowBot.CachyOS.csproj -- --probe
```

Useful inspection commands:

```bash
pgrep -a -f 'WoW.exe|Wow.exe'
grep -i 'WoW.exe' /proc/12345/maps
cat /proc/sys/kernel/yama/ptrace_scope
```

If memory reads return `EPERM`, do not run the whole bot as root. First confirm
that both programs use the same Linux account and inspect the kernel's ptrace
policy. We can choose a narrowly scoped development setup in the next step.

## Object traversal

For this build the object manager uses a direct static pointer and Blizzard's
`TSExplicitList` visible-object list:

```text
WoW module base + 0x741414 = 0x00B41414
  -> ObjectManager pointer
  -> + FirstObjectFromObjectManager
  -> first object
  -> + NextObjectFromObject
  -> next object, until null/invalid/cycle/limit
```

Offsets in hexadecimal:

- Object Manager pointer RVA: `0x741414` (absolute VA `0x00B41414`)
- First visible object: `ObjectManager + 0xAC`
- Local player GUID: `ObjectManager + 0xC0`
- Next visible object: `Object + 0x3C`
- Object type: `Object + 0x14`
- Object GUID: `Object + 0x30`
- Position X/Y/Z: `Object + 0x9B8/+0x9BC/+0x9C0`
- Facing in radians: `Object + 0x9C4`

The visible list uses tagged end-sentinel pointers. `ObjectManager` rejects a
pointer whose low bit is set before trying to read it.

## Planned module boundaries

- Memory: Linux process attachment, primitive reads, validated pointer chains
- Objects: object-list traversal and immutable snapshots
- Runtime: fixed-rate main loop and behavior state transitions
- Input: Wayland/X11-specific driver behind `IInputDriver`
- Navigation: map transforms, path provider, steering, and stuck recovery
- Combat: target selection, pull policy, rotation, resource policy, and loot
- Questing: objective planner, NPC interactions, and grinding profiles
- Inventory: item cache, stat weights, equipment decisions, vendor and repair
- Diagnostics: structured logs, replayable snapshots, metrics, and emergency stop

The next milestone is to run `--probe` against the logged-in client and confirm
the object counts and local-player match before any movement is enabled.

## Dynamic verification

Verified on CachyOS under Wine with the target executable:

- Module base: `0x00400000`
- Object Manager: `0x08E7E408`
- Local GUID: `0x000000000001AA56`
- Local player object: `0x09E38008`
- Visible objects: 62
- Local GUID matched an object whose type was `Player`

The concrete heap addresses and GUID change between sessions. Their successful
relationship is what verifies the configured pointer chain.

## Bounded NavMesh movement test

After `--navmesh-test` returns a complete path, the same target can be tested
with explicit movement input:

```bash
env MSBuildEnableWorkloadResolver=false dotnet run --project WowBot.CachyOS.csproj -- \
  --navmesh-navigate-test \
  --navmesh-dir /home/ludvig/Games/WoW-NavData/mmaps \
  --map 1 \
  --target-x -600.1684 --target-y -4262.415 --target-z 38.956
```

This mode refuses partial paths, targets farther than 50 game units, paths with
more than 16 points, horizontal path segments longer than 35 units, and vertical
changes above 8 units. It has a 90-second total timeout and retains the existing
per-waypoint timeout, stuck detection, recovery limit, and key-release guard.
The first Detour point is skipped because it is the projection of the current
player position. The current map ID remains explicit until its memory offset is
verified against the exact client executable.

Turn steering is calibrated at runtime. The measured radians per second are
used both to size proportional turn pulses and to derive a practical deadband
from the shortest reliable 85 ms input pulse. This prevents repeated
left/right corrections when one minimum pulse rotates farther than the fixed
base tolerance. The adaptive tolerance is capped at 0.24 radians.

## Read-only nearby unit scan

The first perception diagnostic reads visible creature update fields without
sending input or writing process memory:

```bash
env MSBuildEnableWorkloadResolver=false dotnet run --project WowBot.CachyOS.csproj -- \
  --scan-units --radius 50
```

It lists validated creature entry IDs, GUIDs, level, current/maximum health,
health percentage, target GUID, position, and distance from the player. The
radius is limited to 5-100 game units. Invalid descriptor pointers or implausible
field values are skipped and counted. This diagnostic intentionally excludes
players and does not yet decide hostility or select a target.

Numeric CLI arguments use invariant formatting and reject decimal commas. Use
`-612.215`, not `-612,215`, so a coordinate cannot silently become a value one
thousand times larger.

## Read-only combat descriptor scan

After the unit layout has been verified, inspect the local player's combat
state and the raw combat fields of nearby creatures:

```bash
env MSBuildEnableWorkloadResolver=false dotnet run --project WowBot.CachyOS.csproj -- \
  --combat-scan --radius 50
```

This prints the player's class, level, health, active resource, combat state,
target GUID, faction template, unit flags, and dynamic flags. Nearby creatures
include faction templates, the reaction resolved from `FactionTemplate.dbc`,
and a conservative safety gate. Neutral units remain route-only targets and
unknown or friendly units are blocked. This mode sends no input and writes no
process memory. The DBC path defaults to `~/Games/WoW-NavData/dbc` and can be
overridden with `--dbc-dir /path/to/dbc`.

## Joana Horde quest route

The first normalized guide manifest covers the Orc Warrior Valley of Trials
route from levels 1-6. Validate and inspect it offline with:

```bash
env MSBuildEnableWorkloadResolver=false dotnet run --project WowBot.CachyOS.csproj -- \
  --inspect-quest-route routes/joana-horde-1-6.json
```

The manifest contains quest IDs and the guide's zone-map percentages. Those
percentages are metadata and are not passed to the navigation engine as world
coordinates. Each step records which resolver is still required before it can
be automated. The inspector does not attach to WoW and sends no input.

## Read-only quest log scan

Inspect all 20 Vanilla quest-log descriptor slots and correlate active quest
IDs with the Joana manifest:

```bash
env MSBuildEnableWorkloadResolver=false dotnet run --project WowBot.CachyOS.csproj -- \
  --quest-scan --quest-route routes/joana-horde-1-6.json
```

The output includes raw state flags, completion/failure bits, the four packed
objective counters, timer values, and each route step's observable status. The
manifest also contains strict creature allowlists for combat objectives. Quest
absence is reported as unknown because the visible quest log cannot distinguish
a quest that has never been accepted from one that was already turned in.
This diagnostic sends no input and writes no process memory.

For the verified 1.12.1.5875 client, the first quest-log slot is descriptor
field `0x00C6` (byte offset `0x0318`). Objective counters occupy bits 0-23 of
the second slot value and state flags occupy bits 24-31.

If a known active quest is missing from the scan, locate its exact field in the
player descriptor block without changing any configured offset:

```bash
env MSBuildEnableWorkloadResolver=false dotnet run --project WowBot.CachyOS.csproj -- \
  --quest-field-probe 4641
```

The probe scans aligned 32-bit descriptor values and prints every matching byte
offset plus the surrounding fields. It is a read-only build-specific diagnostic.

## Bounded Wayland movement test

The only input-enabled mode in this milestone is an explicit, one-shot test:

```bash
env MSBuildEnableWorkloadResolver=false dotnet run --project WowBot.CachyOS.csproj -- --move-test
```

It gives a five-second countdown, holds W for 750 milliseconds, releases W in a
`finally` block, waits for the game state to settle, then compares the starting
and ending coordinates. The duration is hard-limited to 100-2000 milliseconds:

```bash
env MSBuildEnableWorkloadResolver=false dotnet run --project WowBot.CachyOS.csproj -- --move-test --move-ms 500
```

This mode requires `ydotool`, its persistent `ydotoold` service, and a
focused WoW window. It does not write game memory.

On CachyOS/Arch, install and start the packaged user service with:

```bash
sudo pacman -S ydotool
systemctl --user enable --now ydotool.service
```

The bounded movement test was dynamically verified with a 500 ms W pulse. The
measured 2D displacement was 3.703 game units.

## Bounded waypoint navigation test

After the one-shot movement test succeeds, the next explicit mode tests the
first steering primitive:

```bash
env MSBuildEnableWorkloadResolver=false dotnet run --project WowBot.CachyOS.csproj -- --navigate-test
```

The default target is eight game units away at a relative bearing of +45
degrees. The test first measures which turn key produces a positive facing
change. It then alternates short turn and forward pulses, re-reading position
and facing after every action. It stops inside a 0.75-unit arrival radius,
after 30 seconds, or after repeated failed recovery attempts.

The distance is hard-limited to 2-20 units and the relative bearing to
-180..180 degrees:

```bash
env MSBuildEnableWorkloadResolver=false dotnet run --project WowBot.CachyOS.csproj -- \
  --navigate-test --distance 6 --bearing-deg 30
```

Use this only on open, level ground with the WoW window focused. The recovery
sequence is bounded to backward, turn, and jump pulses. Ctrl+C cancels the test
and releases every tracked key.

The steering test was dynamically verified in both directions. A +30 degree
target finished 0.649 units from the waypoint and a -60 degree target finished
0.415 units from the waypoint, both without recovery.

## Record a waypoint route

Recording is read-only. Start at the beginning of a short test route, run the
command, focus WoW, and walk the route manually. Ctrl+C saves the JSON file:

```bash
env MSBuildEnableWorkloadResolver=false dotnet run --project WowBot.CachyOS.csproj -- \
  --record-route routes/test-route.json --spacing 2
```

Spacing is limited to 0.5-10 game units. The recorder always stores the first
point and, when useful, the final point. A valid route contains 2-500 points.

## Follow a waypoint route

Return to within ten game units of the first recorded point and run:

```bash
env MSBuildEnableWorkloadResolver=false dotnet run --project WowBot.CachyOS.csproj -- \
  --follow-route routes/test-route.json
```

The follower validates every coordinate and refuses segments longer than 25
units. It follows the route once and stops. Each waypoint retains the bounded
30-second navigation timeout and two-recovery limit. This is waypoint steering,
not NavMesh pathfinding; recorded routes must therefore avoid obstacles, steep
terrain, drops, water, NPCs, and hostile mobs.

Forward pulses shrink automatically near a waypoint. The initial movement-speed
estimate is based on the dynamically measured client speed and is refined from
successful pulses. A waypoint also counts as reached when the measured movement
segment passes through its arrival circle, preventing oscillation across close
targets.

## Route control and movement FSM

Waypoint navigation reports explicit state transitions:

```text
Idle -> Calibrating -> Turning -> Moving -> Arrived
```

`Recovering`, `Stuck`, and `TimedOut` are explicit terminal/recovery paths.
Every turn pulse is verified against the read-back facing value. Turn pulses
start at a minimum of 85 ms and are progressively lengthened when Wine or the
game does not register a short pulse. Six consecutive failures abort the
waypoint as stuck.

Resume from the closest waypoint instead of requiring the beginning:

```bash
env MSBuildEnableWorkloadResolver=false dotnet run --project WowBot.CachyOS.csproj -- \
  --follow-route routes/test-route.json --nearest
```

Follow the stored points in reverse order:

```bash
env MSBuildEnableWorkloadResolver=false dotnet run --project WowBot.CachyOS.csproj -- \
  --follow-route routes/test-route.json --reverse
```

Run a closed route a bounded number of times:

```bash
env MSBuildEnableWorkloadResolver=false dotnet run --project WowBot.CachyOS.csproj -- \
  --follow-route routes/test-route.json --nearest --loops 2
```

Loop count is limited to 1-10. Multiple loops are refused when the last and
first waypoints are more than ten game units apart. `--nearest`, `--reverse`,
and `--loops` are valid only together with `--follow-route`.

## Read-only NavMesh query

The native bridge in `Native/` loads the VMaNGOS `.mmap` and `.mmtile` format
with the exact Detour sources bundled by the same VMaNGOS checkout. It converts
WoW coordinates `(X,Y,Z)` to Detour coordinates `(Y,Z,X)`, searches ground
polygons, and returns straight-path corners. It does not send input or write
game memory.

Build the native library after building the VMaNGOS extractors:

```bash
./Native/build-native.sh /home/ludvig/Programming/Projects/vmangos-core
```

Then rebuild the C# project so `libwowbot_nav.so` is copied beside the managed
executable:

```bash
env MSBuildEnableWorkloadResolver=false dotnet build WowBot.CachyOS.csproj
```

Query a target contained in one of the generated tiles:

```bash
env MSBuildEnableWorkloadResolver=false dotnet run \
  --project WowBot.CachyOS.csproj -- \
  --navmesh-test \
  --navmesh-dir /home/ludvig/Games/WoW-NavData/mmaps \
  --map 0 \
  --target-x -610.358 \
  --target-y -4252.906 \
  --target-z 38.956
```

The start position comes from the live Object Manager snapshot. The command
prints loaded tile count, every returned path point, whether the path is
partial, and total length. With only `0003933.mmtile` generated, both start and
target must be inside that tile. Generate adjacent tiles before attempting a
path that crosses a tile boundary.

## Silent bounded NPC target test

After a completed quest is verified in the quest log, the bot can perform one
guarded NPC approach and target test. The test:

1. requires the named quest to still be active and complete;
2. finds the NPC by Entry ID in the visible object list;
3. follows a complete NavMesh path to a point 3.25 units from the NPC;
4. cycles WoW's `Target Nearest Friend` binding with `Ctrl+Tab`; and
5. accepts the target only when the live target GUID equals the NPC GUID.

For Joana step 2 (`Your Place in the World`, quest 4641, Kaltunk Entry 10176):

```bash
env MSBuildEnableWorkloadResolver=false dotnet run --no-build \
  --project WowBot.CachyOS.csproj -- \
  --npc-target-test \
  --npc-entry 10176 \
  --required-quest 4641 \
  --navmesh-dir /home/ludvig/Games/WoW-NavData/mmaps \
  --map 1
```

The runner first asks XWayland to activate a visible `World of Warcraft`
window. If title lookup is unavailable it falls back to a deliberately timed
Alt+Tab sequence. Before navigation begins, a small left-turn pulse must cause
a measurable change in the live facing value; otherwise activation is retried
and the run stops after three failures. This mode never opens chat and never types text or Lua commands. It also sends no quest
interaction, Accept action, or process-memory write. It uses WoW's default
`Ctrl+Tab` binding for `Target Nearest Friend`.

The bounded NPC search accepts a visible questgiver up to 85 world units away.
The returned path must still be complete, no longer than 90 units in total,
have plausible height changes, and contain no more than 24 Detour points.

## Automatic bounded turn-in: Your Place in the World

This mode performs Joana step 2 without a user-created macro or key binding.
It verifies that quest 4641 is active and complete, locates Gornek by Entry ID
3143, requires his live `UNIT_NPC_FLAGS` to contain the questgiver bit,
navigates to him, selects and verifies his GUID, opens the NPC with bounded
right-clicks, and recognizes only these UI texts. A world-space OCR match for
`Gornek` is preferred; otherwise a bounded centre/lower-screen grid is used.
After a miss, the expected GUID is reacquired and verified before another
click is allowed:

For client build 5875, `UNIT_NPC_FLAGS` is descriptor field 147, byte offset
`0x24C` (588). This is distinct from `UNIT_FIELD_FLAGS` at byte offset `0xB8`.

- `Your Place in the World`
- `Continue`
- `Complete Quest`

Run the dedicated command:

```bash
env MSBuildEnableWorkloadResolver=false dotnet run --no-build \
  --project WowBot.CachyOS.csproj -- \
  --complete-your-place \
  --navmesh-dir /home/ludvig/Games/WoW-NavData/mmaps
```

Windowed mode is supported. The bot activates the visible WoW XWayland window
by title and uses Alt+Tab only as a fallback:
the active XWayland window geometry is read with `xdotool`, Spectacle captures
only that window, and all OCR and mouse positions are translated back to
desktop coordinates. The runner stops as soon as quest 4641 disappears from
the quest log and will not accept the following quest. It never opens chat,
types Lua, installs an addon, uses an in-game macro, or writes process memory.

This mode requires the packages already selected for the screen phase:
`spectacle`, `tesseract`, `tesseract-data-eng`, `xdotool`, and `ydotool`.
Dependency validation runs before any navigation or interaction input is sent.
