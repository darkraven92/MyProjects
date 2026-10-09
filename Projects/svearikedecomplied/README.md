# Svea Rike — C / WebAssembly reconstruction

Target: a **1:1 browser port** of the supplied game, using its original assets,
rules, interface, text, sound, animation, and timing. No replacement game rules
or redesigned interface. The original files in `SveaRike/` are the reference.

**Status: menu → family selection → welcome → starting province now runs in a
browser, with working tax, recruitment and farm/mining/military-upgrade controls.
The crossbow contest is separately playable through all fifteen shots, with
original artwork and sounds. Linné's six-pair memory game is also playable with
its original images, time limits and four sounds. The hourglass advances a five-year turn through
original event screens, crossbow rewards and settlement. Troop dismissal and
person hiring work; culture can develop the estate and science enables mining
upgrades. Starvation can resume after reducing the army's upkeep. Neighbor,
diplomacy and trade panels now support upgrades, relation spending and buy/sell orders.
Quick battles now resolve casualties, territory and compensation and resume the
turn. Manual battles allow troop deployment, animated movement, attacks,
counterattacks and both sides' artillery/troop turns. Manual outcomes and retreat
now apply losses and return to the campaign result screen.
The eight remaining minigames also pause at unfinished gameplay.**
There is no emulator, Lingo VM, or bundled Director
runtime. Game behavior is translated into C and compiled to WebAssembly.
This is a Macromedia Director 5 game. Its game logic is compiled Lingo in the
`.DIR` and `.CST` files; translating the Windows projector executable alone
would not recover that logic as C.

The first pass recovered 497 Lingo scripts and their disassembly using
[ProjectorRays](https://github.com/ProjectorRays/ProjectorRays). The project now
has a C11 resource reader, reproducible inventory/text extraction, and the first
translated game routine: `fixrandomPrice`. A second C module renders the original
menu/setup artwork and handles buttons, credits, and family selection. The new
game module initializes province, resource, family and seeded random values
from the original scripts and data tables.
Recovered scripts are evidence for translation, not proof of behavioral parity.

## Build and inspect

Requires a C compiler, CMake, and Python 3.8+; Ninja is optional.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/svea-inspect SveaRike/SVEA.DIR
python3 tools/analyze.py
```

The analyzer validates all 42 standalone Director archives through the C reader,
hashes all 54 supplied files, and writes:

- `analysis/inventory.json`: file hashes, sizes, resource counts, and failures.
- `analysis/resources/<original filename>/map.json`: indexed resource offsets.
- `analysis/resources/<original filename>/*.bin`: selected unmodified resource payloads.
- Matching `.txt` / `.json` files: decoded text fields and Lingo name tables.

Original files are read only. Generated outputs live in ignored `analysis/` and
`build/` directories. The 18 menu/setup, 497 strategy, 42 crossbow and 37 Linné `BITD`
graphics are packed. All 155 event bitmaps are decoded; the 144 referenced event
screens are streamed individually. Seven original PCM sounds are decoded for the two minigames.
Windows CP1252 is used for text-field previews,
while menu cast names use MacRoman. The raw bytes remain the
reference until font-specific encoding behavior has been checked.

## Recover the scripts again

The initial pass used ProjectorRays commit
`6f9bcebf626b43719abe2affcbbcb041d154d666` (MPL-2.0), built separately from the C
runtime. Its documented build dependencies are a C++17 compiler, Boost, mpg123,
and zlib development files, plus `make` and `xxd`.

```sh
git clone https://github.com/ProjectorRays/ProjectorRays.git /tmp/svea-projectorrays
git -C /tmp/svea-projectorrays checkout 6f9bcebf626b43719abe2affcbbcb041d154d666
make -C /tmp/svea-projectorrays -j4
python3 tools/analyze.py --projectorrays /tmp/svea-projectorrays/projectorrays
python3 tools/index_scripts.py
```

If that checkout already exists, reuse it. Decompiler outputs are under
`analysis/decompiled/<movie or cast>/casts/`: `.ls` is recovered Lingo and
`.lasm` is annotated disassembly. Rebuilt `.dir` / `.cst` files also live under
`analysis/decompiled/`; they do not replace the originals. The decompiler may
emit legacy-encoded strings. Do not bulk-convert the reference files in place.

`analysis/script-index.json` indexes script paths, hashes, and handler line numbers.

## Run the browser preview

Requires Clang with the wasm32 target and `wasm-ld`:

```sh
bash tools/build_wasm.sh
python3 -m http.server 8000 --bind 127.0.0.1
```

Open **http://127.0.0.1:8000/web/**. Run the native CMake build first: the WebAssembly
build uses its C asset decoder to prepare the original artwork.

The title screen, pressed-button artwork, credits, five family choices, and setup
confirmation work. Confirmation opens the original welcome advisor; its OK button
reveals the selected family's starting province, buildings and ownership map.
Click the town (around x=327, y=260) to open the city panel. The tax arrows and
slider work, and its original income/unrest banners update. OK returns to the
province. Tax changes preserve resources and time until the turn settles.
Click the regiment (around x=349, y=379) to recruit infantry/cavalry or upgrade
the regiment. These actions deduct the original silver/metal costs. Artillery
requires a smithy, so it is blocked in the current starting state. Requirements
and capacity limits are enforced; failed purchases do not change resources.
The preview status below the canvas reports troop/resource values and errors
while original dynamic text rendering is pending.
The middle icon (x=393, y=372) opens troop dismissal: right arrows select 1000
troops, left arrows undo, and OK commits the selection without a refund.
The same panel opens when upkeep exceeds food at settlement; selecting too few
troops repeats the warning without awarding income again.
The right icon (x=489, y=372) opens the original commander panel. Choose an
available commander in the right-hand list, then ANSTÄLL to pay the original
rank-based price. Names and selection details appear below the canvas while
field rendering and list scrolling remain unfinished. Troop transfer and other
military pages are still pending.
Click the farm (x=452, y=190) or mine (x=301, y=143) for their original panels,
production banners and upgrade controls. Farms require the next city level;
mines require employed science ranks (owned universities each contribute five).
Those prerequisites are not met in a new game. Growth runs at settlement.
Click the estate (x=479, y=317), then its culture/science icon (x=495, y=280)
to hire people who have arrived through events. Select an available person under
LEDIGA, then ANSTÄLL. Culture ranks develop the estate at settlement; employed
scientist ranks count immediately toward mining requirements. The original
estate family shield and level pictures update from game state. Original list
glyphs, wrapping and scrolling remain pending, with names reported below the canvas.
The original starting year, resources and bonuses are initialized in C.
`src/fields.c` now produces 72 dynamic field values, with 111 original placements
across twelve strategy frames. Person lists preserve original order and line endings;
selected rank/price labels retain the original Swedish wording. The browser's
person-name decoding now uses the Windows-1252 encoding of those source rows.
Dynamic text fields remain blank on the canvas until original font rendering is
recovered. Original Arial and MS Sans Serif glyph files have not been found among
the supplied assets; installed replacement fonts do not establish parity. Other
strategy controls, eight minigames, intro playback, save loading, general game audio, and
actual game exit are not implemented. The page identifies itself as a preview.

Click a foreign shield on the lower-left map, then **DIPLOMATI** for diplomatic
upgrades and relation spending. **FÖRBÄTTRA** reserves 100 silver per click to
improve relations; **FÖRKLARA KRIG** reserves spending in the opposite direction.
Reversing a reservation refunds the silver. Each country allows up to 1000 in
either direction; the relation effect is calculated at turn end. A war presents
the original negotiation, quick-battle and manual-battle choices. Manual battle
opens troop deployment and movement; its outcome returns to the campaign result
screen, where OK resumes the turn.

Choose **HANDEL** from the neighbor panel (or diplomacy) for trade. Left arrows
buy crops/metal; right arrows sell. Hold an arrow to repeat, or drag its marker.
Both goods share each country's capacity (10/25/50/100/250 at levels 1–5).
Orders reserve silver or goods immediately and deliver at settlement. Upgrades
cost silver and metal; later levels require an estate upgrade or owned harbor.
Original numeric glyphs and general button sounds are still pending.
For the tested `?seed=1` Eka route, upgrade Denmark once, sell ten crops, buy
five metal, return with OK and end the turn. It settles in 1528 with 945 silver,
114 crops and 90 metal. This verifies a trade route, not the full campaign.

For a quick-battle route, open **http://127.0.0.1:8000/web/?seed=3** and select
**Tre Rosor**. Open Denmark's diplomacy and click **FÖRKLARA KRIG** four times
(400 silver), return to the province and end the turn. Acknowledge the event,
then choose **SNABBSTRID**, Skåne and the Nyland regiment. Confirm the battle
and result screens. Victory acquires Skåne; settlement in 1528 leaves 688 silver,
123 crops and 111 metal, with 1480 infantry and 1220 cavalry in Nyland.
War lists, figures and messages currently appear below the canvas; their original
field rendering and war sounds remain pending. Negotiation and no-troop
surrender rules are implemented, with model tests for their distinct outcomes.

For manual troop deployment, follow the same seed-3 route and choose **STRID**,
then Skåne and Nyland. Original battle images load automatically. Drag the six
groups from the upper-left staging panel onto the marked starting squares.
Occupied squares and drops outside the first three rows are rejected. When all
groups are placed, the enemy forms up according to the original rules. Select a
group and click an adjacent empty square to move it. Each group has one action;
infantry and cavalry use their original movement images and different durations.
Ending this route's player turn fires the Danish cannon, moves the enemy army,
then returns control for the next round. Firefox tests compare rendered scenes
with native C. Native fixtures without artillery additionally verify player
attacks, counters, death, forced advances, enemy turns and an enemy victory.
Swedish artillery also works: after deployment, wait for the officer to reach a
loaded cannon, then move over an enemy group and click its original red sight.
Each ready cannon fires once; the officer leaves before the troop turn starts.
Guns reload according to their technology level. Use the bottom-left retreat
control (around x=40, y=440) during troop selection or cannon aiming to withdraw.
Retreat retains artillery and counts existing infantry/cavalry losses, including
initial division remainders. Defeat without retreat loses half the Swedish guns.
The result screen applies casualties, territory or compensation, then OK resumes
the campaign. Battle sounds, original numeric glyphs and runtime parity remain pending.

The tested Swedish-artillery route continues the seed-4 Eka game: hire Johannes
Magnus after turn one, hire Willem Boy after turn four, then upgrade the mine to
level two. Recruit 1000 infantry and 1000 artillery, spend 400 silver declaring
war on Denmark, and end the turn. Win the naturally occurring Bagge crossbow
contest, acknowledge the reward and choose **STRID**, a target province and your
regiment. This reaches the cannon sequence in 1548 through normal gameplay;
after the first shot, retreat and confirm the result to finish the fifth turn.
The tested settlement is 1548 with 574 silver, 219 crops, 135 metal, 996 infantry
and 1000 artillery. This does not establish full campaign playability.

Open **http://127.0.0.1:8000/web/?minigame=armborst** for the crossbow contest.
Click **SPELA**, then hold the left mouse button and drag to aim; release to shoot.
The original wind changes before every shot. Three rounds of five shots produce
the original 0–10 result tier. The score and remaining bolts appear below the
canvas while original field glyphs are pending. Animation cadence and matte
silhouettes still need original-native comparison. The standalone link ends at
the contest's result. When triggered by a strategy event, it now returns to the
original reward dialog, applies the earned reward once, and resumes settlement.

Open **http://127.0.0.1:8000/web/?minigame=linne** for Linné's memory game.
Click **SPELA**, study the six plants for five seconds, then click matching
flowers and name labels within thirty seconds. Correct pairs move into the book.
The result follows the original attempt-count table, including its unusual
positive reward for some unsuccessful timeout results. The dynamic numeric field
is still pending; score, attempts and time appear below the canvas. A historical
native C fixture verifies the 1737 event, earned reward and employment of Linné;
it does not establish a complete campaign route to that year.

For a reproducible integrated route, open **http://127.0.0.1:8000/web/?seed=8**,
start the default Eka family, dismiss the welcome, and click the hourglass at
bottom right. Jakob Bagge's original event screen appears in 1524; OK leads to
the crossbow contest. After fifteen shots and the reward dialog, the first turn
settles in 1528. Browser tests cover this whole route, including the actual
earned score, resources and return to working province controls. This verifies
one playable route, not all campaign outcomes or original-runtime parity.

For paid commander hiring, use **http://127.0.0.1:8000/web/?seed=65**. Start Eka
and end the first turn. Berent von Melen arrives in 1528; acknowledge the event,
then open the regiment and its right-hand commander icon. Select the first row
under LEDIGA (around x=435, y=100), then ANSTÄLL. His rank-one cost is 200 silver,
leaving 850 after settlement. The seed-8 crossbow winner already employs Bagge
as an original contest reward; he must not be purchased a second time.

For culture/science progression, use **http://127.0.0.1:8000/web/?seed=4** and
start Eka. End the first turn and acknowledge Johannes Magnus's arrival. Open
the estate's culture/science panel, select the first culture row (x=320, y=120),
and hire him for 200 silver. Return to the province and finish the next three
turns, acknowledging events. Culture raises the estate to level 2 at the second
settlement; Willem Boy arrives during the fourth turn. Hire him from the first
science row (x=510, y=120) for 200, then upgrade the mine for 100 silver and
25 metal. This leaves 700 silver, 196 crops and 115 metal in 1543 with mine
level 2. This tested four-turn route does not establish full campaign playability.

The browser sends input and clock ticks to C, presents its 640×480 RGBA framebuffer,
and plays original PCM samples when C requests them. Game
state and drawing are in `src/game.c` and `src/menu.c`. `menu.pack` contains decoded original pixels
and score hit regions; it contains no executable original code. `menu.wasm` is the
compiled C implementation. The current matte transparency and input behavior still
need comparison with an original native game installation before claiming 1:1 parity.

The build also produces `build/wasm/trade.wasm` exporting
`sr_trade_price_step(index, triggerRoll, directionRoll)`. For example, in a browser
when serving the project directory over HTTP:

```js
const { instance } = await WebAssembly.instantiateStreaming(
  fetch('/build/wasm/trade.wasm')
);
instance.exports.sr_trade_price_step(9, 1, 2); // 8
```

The trade module is independent of the menu: its callers supply one-based rolls.
The recovered Windows RNG is now available in `src/random.c`.
Native tests cover all valid price transitions and all five deterministic family
starts. Firefox integration tests exercise the real frontend and compare menu,
welcome, pressed welcome, strategy, city/tax, farm, mine and military pixels against native C output.
Neither this comparison nor decompilation proves fidelity to the original runtime.
Future audio/video integration can use Emscripten, which supports
[C/C++ to WebAssembly](https://emscripten.org/docs/compiling/WebAssembly.html).

See [the reconnaissance notes](docs/RECONNAISSANCE.md) for the file map, evidence,
current limitations, and the next parity milestones.

## Validate the browser slice

```sh
cmake --build build
bash tools/build_wasm.sh
ctest --test-dir build --output-on-failure
mkdir -p analysis/previews
./build/svea-menu-probe build/wasm/menu.pack analysis/previews
python3 tests/browser_smoke.py
```

The browser test requires Firefox and permission to bind a temporary loopback
server. It uses a temporary browser profile and closes it afterward. It verifies
native/browser framebuffer equality, press cancellation, credits navigation,
default family, all five starting provinces/resources, welcome dismissal and
cancellation, ordered minimap hit regions, city navigation, tax bounds, slider
grab offsets, cancellation and release-only commits. Native economy tests cover
all five tax levels for every family, production modifiers, unrest and banner rounding.
Military tests cover original costs, prerequisites, technology-level changes,
capacity edge cases, error-modal blocking and atomic rejection of failed purchases.
It stubs pointer capture for synthetic events; real device capture remains a
manual check. Results are saved to `analysis/browser-test.json`.

See [progress](docs/PROGRESS.md) for completed and pending work.

`src/catalog.c` exposes 144 original event/person records and their ordered period
eligibility lists. `tools/export_catalog.py`, called by the WebAssembly build,
regenerates these tables and `analysis/catalog-provenance.json` from DATA.CST.
Legacy text bytes are preserved exactly because the source mixes encodings.
Random event-date initialization now uses the Windows projector's recovered RNG.
`src/events.c`, `src/family.c` and `src/turn.c` now translate event eligibility,
weighted selection/effects, succession and five-year settlement into C. The
controller waits for real player decisions, minigames and wars. Event screens,
crossbow/Linné completion, the three riot decisions and starvation dismissal now connect
to the browser. Quick and manual battle results now return to settlement;
unported minigames remain explicit waits. Use `/web/?seed=1` for a reproducible
new game (Eka's first family head is Ture Gustafsson Eka).

`tools/probe_projector_rng.py` verifies the C algorithm against unmodified
arithmetic instructions extracted from the supplied `SVEA95.EXE`, executed in a
small native i386 Linux program. It runs no emulator or script interpreter.
The checked-in golden vectors cover seed zero, signed boundaries, nonpositive
ranges and complete 17-draw new-game sequences. The browser supplies its own
startup seed because the original Windows clock/uptime seed is host-dependent.
