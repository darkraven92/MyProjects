# Initial reconnaissance — 2026-10-05

The required outcome is a 1:1 browser port in C/WebAssembly. Completion means
matching observable game behavior and presentation against the supplied original,
including its quirks. A successful compile or plausible-looking screen is not
enough to claim parity.

## Observed files

The supplied directory contains 54 files, totaling 149,553,087 bytes: 19 `.DIR`
movies, 23 `.CST` casts, two QuickTime `.MOV` files, Windows executables, DLLs,
and Xtras. No original C source or project build files were present.

| Files | Evidence and role |
| --- | --- |
| `SVEA95.EXE`, `KRIG95.EXE` | Windows x86 executables with Macromedia Director strings; startup/projector behavior still needs inspection. |
| `SVEA.DIR` | Main strategy logic: 200 compiled scripts, 150 text resources. Recovered handlers include initialization, end turn, economy, trade, diplomacy, events, and family progression. |
| `KRIG.DIR` | Battle movie: 62 compiled scripts, 492 bitmap resources. |
| `HMENY.DIR`, `SETUP.DIR` | Menu and setup. Menu scripts explicitly transition to `SETUP.DIR`, `DATABASE.DIR`, and `SINTRO.DIR`. |
| `INTRO.DIR`, `SINTRO.DIR`, `INTRO.MOV`, `MB.MOV` | Intro/movie resources; exact startup sequence and codecs remain to be checked. |
| `DATA.CST` | 247 text fields containing provinces, people, events, and other game data. |
| `PRICES.CST` | 16 text fields; economic tables referenced by initialization logic. |
| `PICTS.CST` | 10,909 bitmap resources and 16 script resources. |
| `EVENT.CST`, `ADVISOR.CST` | Event and advisor assets/scripts. |
| `LJUD.CST` | 49 sound resources and one script. |
| `SWE1–3`, `DAN1–3`, `POL1–3`, `PRU1–3`, `RUS1–3` casts | Bitmap/film-loop collections. Nation/period grouping is inferred from names and needs confirmation against battle scripts. |
| `ARMBORST`, `BELLMAN`, `FROGGER`, `LASSE`, `LINNE`, `RIDDARE`, `RUSSINV`, `SALA`, `SLOTTET`, `WHICH` movies | Additional game scenes/minigames; exact integration follows their recovered handlers. |
| `SCRIPTS.CST` | Two scripts, including a database object. Most game scripts reside elsewhere. |
| `DATABASE.DIR`, `MEMFIX.DIR`, `KRIGTEST.DIR` | Auxiliary movies. Names alone do not establish whether each is used in normal play. |
| `SOLDIERS.CST` | Resource map contains no active cast members; many free slots. Do not assume data from its filename. |
| `BUDAPI*.DLL`, `XTRAS/`, `QuickTime/` | Native platform integrations; browser equivalents require a behavior audit. |

The local inventory records SHA-256 for every input file and resource offsets for
every archive. Across the 42 archives: **497 Lscr**, **516 STXT**, **17,552 BITD**,
**126 sound**, and **414 SCVW** resource entries. Counts are map entries, not
necessarily distinct assets.

## Format findings

- Both big-endian `RIFX` and little-endian `XFIR` containers occur. Forms are
  `MV93` (movie) and `MC95` (cast). ProjectorRays identifies these as Director 5.0.
- `imap` locates `mmap`. Map entries identify resources by slot, FourCC, length,
  and offset. `free` and `junk` entries can contain stale offsets and are skipped.
- Some files, such as `HMENY.DIR`, lack the final container alignment byte. The
  reader permits that specific size discrepancy, while requiring every active
  resource payload to fit within the actual file.
- Lingo name tables (`Lnam`) and observed text payloads (`STXT`) use big-endian
  fields even when their outer container is little endian.
- `Lscr` stores compiled Lingo. Readable strings alone do not reconstruct the
  control flow; the recovered `.lasm` remains important when reviewing `.ls`.
- Text previews use CP1252, supported by readable Swedish names in `DATA.CST`.
  Font mapping and rendering still need verification for exact presentation.

The parser was written for the locally observed standalone archive layout. It
does not support compressed Shockwave, older Director archives, Mac resource
forks, or projector-embedded archives. Those cases fail explicitly.

Format references consulted:
[ScummVM archive reader](https://github.com/scummvm/scummvm/blob/master/engines/director/archive.cpp),
[ScummVM text reader](https://github.com/scummvm/scummvm/blob/master/engines/director/stxt.cpp),
and [ProjectorRays](https://github.com/ProjectorRays/ProjectorRays).
ProjectorRays is an external analysis tool, not a linked runtime dependency.

## Recovered logic and first C translation

ProjectorRays commit `6f9bcebf626b43719abe2affcbbcb041d154d666` completed on all
42 standalone archives with exit status zero. It produced 497 `.ls` scripts
and matching `.lasm` files, with 16,421 recovered Lingo lines and 1,062 indexed
handlers. Successful
decompilation does not establish that every reconstructed expression is correct.

Useful starting references in `analysis/decompiled/SVEA/casts/Internal/`:

| Script | Use |
| --- | --- |
| `MovieScript 1 - init.ls` | Initialization, cast references, starting state and data loading. |
| `MovieScript 2.ls` | End-turn sequence, income, troop costs, trade and upgrades. |
| `MovieScript 4 - EventScripts.ls` | Event effects and minigame dispatch. |
| `MovieScript 247.ls` | Province interactions and advisor behavior. |

`src/trade.c` translates `MovieScript 2`'s `fixrandomPrice` (Lingo line 259;
disassembly line 1114). Its source behavior is:

1. Change the index only when `random(4)` returns 1.
2. Above 8, move upward on `random(3) == 1`, otherwise downward.
3. Below 4, move downward on `random(3) == 1`, otherwise upward.
4. From 4 through 8, use `random(2)` to choose downward/upward.
5. Clamp the result to 1–11.

The C interface takes rolls as inputs so tests need not invent the original RNG.
It is defined for valid indices 1–11 and rejects invalid arguments with zero.
The eventual caller must request the second random draw **only** when the first
roll is 1, preserving random consumption order. Tests cover the valid domain,
both thresholds, clamping, unchanged outcomes, and invalid inputs. The recovered
disassembly was checked against the translated branch structure. This is not yet
a comparison against traces captured from the running original game.

The initial remaining code was extraction infrastructure. No economy rounding, combat
formulas, event scheduling, or substitute game simulation has been invented.

## Path to 1:1 parity

1. Establish the original startup/movie sequence by inspecting both projectors.
   Use captures from an original native installation when available. Record
   native stage dimensions, palette/font behavior, input coordinates, audio,
   frame timing, and baseline screenshots/save states.
2. Resolve `CAS*`, `KEY*`, cast member metadata and external cast references;
   retain original IDs/names. Decode bitmaps, palettes, sounds, film loops,
   score timelines, and frame labels with traceable resource offsets.
3. Reproduce the original menu and new-game flow using original graphics and
   coordinates, then compare screenshots and click transitions.
4. Translate initialization and one complete turn. Explicitly model Lingo's
   integer/float conversions, division, strings, one-based lists, property lists,
   globals, handler dispatch, and RNG state before relying on C defaults.
5. Add deterministic replay fixtures: same starting state and input/random
   sequence, then compare state at each handler/turn boundary with the original.
6. Port battles, all minigames, events, save/load, sound and video; replace each
   native Xtra call with verified browser behavior. Audit every recovered handler.
7. Validate browser rendering, frame pacing, audio/input ordering and save
   compatibility. Browser restrictions on autoplay/fullscreen/local files need
   explicit integration; they are not grounds to silently change game behavior.

Native build/tests and all archive validation pass, including AddressSanitizer
and UndefinedBehaviorSanitizer runs over the tests and all 42 original archives.
The sanitizer runs required execution outside the process-tracing sandbox.
The first pure C routine
compiles to WebAssembly. The continuation below records subsequent progress.

## Menu/setup continuation

User constraint: **no emulator**. `src/menu.c` directly implements the supported
menu handlers. The score reader is an asset-analysis tool used during export;
the browser neither interprets Lingo nor emulates Director or Windows.

The new C asset layer resolves original `CAS*` numbers through `CASt` to `KEY*`
children, decodes 8-bit indexed and 32-bit `BITD`, and reads D5 score deltas. Both
menu archives exported successfully: 18 bitmaps, 23 menu frames, 3 setup frames.
Their stages are 640×480. Menu buttons are on frame 21; the setup interaction
regions are on frame 3. Cast names use MacRoman (`0x80` is Ä), unlike the observed
Windows-encoded text-field samples. Decoding must stay specific to resource type.

The browser slice supports the original background and press artwork, release-over
checks, credits and teaser, and the five family panels. Setup defaults to Eka
(family 3). The first menu slice stopped at the `DATABASE.DIR` handoff; the
new-game continuation below now implements part of that initialization and scene.
Load, intro, and quit emit explicit action requests; they are not working features.

Current rendering uses the system Mac palette selected by these bitmap records.
The palette is generated from its color cube and ramps. Family panel matte alpha
is derived from edge-connected white pixels; original opaque exports remain in
`analysis/assets/`. This interpretation, cast-replacement registration, stage
origin, real pointer dispatch, and missing sound/timing need original-native
reference checks before claiming 1:1 visual/behavioral fidelity.

Additional format references:
[bitmap metadata](https://github.com/scummvm/scummvm/blob/master/engines/director/castmember/bitmap.cpp),
[BITD decoding](https://github.com/scummvm/scummvm/blob/master/engines/director/images.cpp),
[score fields](https://github.com/scummvm/scummvm/blob/master/engines/director/frame.cpp),
[palette data](https://github.com/scummvm/scummvm/blob/master/engines/director/graphics-data.h).

Firefox tested the actual frontend and compiled C module. The menu, pressed
button, initial setup, and Sture-family setup framebuffer hashes match native C
byte-for-byte. Input tests exercise all families and transition boundaries.
This is cross-platform consistency evidence, not original-game parity evidence.

## New-game continuation

`DATABASE.DIR`'s exit-frame handler goes to SVEA frame 1. SVEA ScoreScript 68
calls `start()` and goes to `intro` (frame 4). ScoreScript 67 establishes puppet
positions; the welcome screen holds at frame 6. ScoreScript 230 handles its OK
button and goes to `main` (frame 11, holding at frame 12). These transitions are
translated directly into C states; score data is used during analysis only.

`src/game.c` translates the deterministic parts of MovieScript 1's
`initStartVariabels`, `initMilitary`, `initAreas`, `initPlayerVar`, `initCountries`
and `initFAmily`. `tools/prepare_strategy.py` regenerates the source tables from
DATA/PRICES and records input/member hashes in `analysis/world-data-provenance.json`.

| Family | Home | Silver | Crops / metal | Additional starting bonus |
| --- | --- | --- | --- | --- |
| Tre Rosor | Nyland (6) | 1000 | 100 / 100 | Military level 2; 2000 infantry and cavalry each |
| Brahe | Östergötland (4) | 1000 | 150 / 150 | All four trade levels 2 |
| Eka | Uppland (1) | 1000 | 100 / 100 | Farming level 3, index 2 |
| Grip | Södermanland (2) | 1250 | 100 / 100 | Silver bonus included |
| Sture | Västergötland (3) | 1100 | 100 / 100 | All four diplomacy levels 2 |

The Eka level/index discrepancy appears in the original disassembly and is
preserved. All starts are year 1523, turn 1, with headquarters level 1, one owned
province and 15 available Swedish provinces. This is partial initialization:
random advancement dates, events and genealogy were still pending at that stage;
the random initialization sections have since been translated as described below.
No replacement RNG is used.

The original PICTS welcome text is already embedded in bitmap member 365; its
pressed button is member 366. The strategy scene uses original province/building,
king, family, timeline and minimap graphics. The live `Karta` member is 499;
duplicate names also exist. Province stamp IDs are resolved by name because
their cast numbers contain gaps. Stamp compositing and ink/matte interpretation
still need original-runtime reference checks. The 35 ordered rollover rectangles
are translated from MovieScript 247, including first-match behavior on overlaps.

Resource/year/map-information fields are dynamic text, not part of the background.
Font mappings name MS Sans Serif and other fonts, but original glyph rendering
has not been recovered. Saved text member contents include stale authoring values
and are not a valid substitute for initial state. Those fields currently remain
blank; the correct deterministic values are available in C and browser test data.

All five welcome, pressed-welcome and province images match between native C and
Firefox WebAssembly. Family resources and farm quirks are tested in both paths.
These tests establish consistency of the port, not 1:1 parity with the original.

## City controls and first-turn dependencies

The town's ScoreScript 46 opens `stad`. Frame 47/ScoreScript 296 prepares the
slider and banners; frame 48 holds the panel. Its original PICTS background is
member 165, level-1 picture 173, slider 288, and pressed OK picture 166. The
50 `Banner.1` through `Banner.50` images supply the production/unrest indicators.
Duplicate `Stad norm` members exist; the renderer uses frame 48's member 165.

ScoreScripts 65/66 call `fixTax(±1)`, clamping levels to 1–5. ScoreScript 297
uses slider positions `[346,382,417,453,492]`, retains the initial grab offset,
and commits only after release. Its directional threshold behavior is preserved:
moving back from level 2 to x=350 does not lower tax until x=346 is reached.
The browser port settles the original polling loop at each pointer position.
Pointer cancellation rolls back an unfinished drag; timing and native input
dispatch still require original-game comparisons. ScoreScript 303 closes the panel.

`src/game.c` now calculates production before riots/upkeep and translates
`calcHappiness` and `fixBanner`. Despite its name, `calcHappiness` returns unrest.
Taxes increase silver and reduce grain/metal production. Troops and special
buildings reduce unrest, subject to the original tax-dependent minimum. The
population calculation omits the province's base riot modifier.

Numeric behavior is checked against the original disassembly and secondary
implementation evidence: ScummVM's own
[D5 integer conversion](https://github.com/scummvm/scummvm/blob/master/engines/director/lingo/lingo-builtins.cpp)
rounds floats, while its
[integer division](https://github.com/scummvm/scummvm/blob/master/engines/director/lingo/lingo-code.cpp)
truncates when both operands are integers. No ScummVM code or runtime is linked.
The current positive banner rounding is a direct C expression. The recovered
floating-point literals and multiplication order are retained in production.
These semantics remain subject to original-runtime verification.

First-turn dependency trace:

1. ScoreScript 8 enters `EndTurn`.
2. ScoreScript 14 occurs at frames 23, 25, 27, 29 and 31, advancing one year at
   each occurrence and checking scheduled/random events. The first turn therefore
   covers 1524–1528; it is not a one-year increment.
3. Frame 33/ScoreScript 7 invokes MovieScript 2's `endTurn`: relations, war,
   enemy troops, rebellion, province loss, trade settlement, random prices,
   income/upkeep, headquarters/city growth, points, family tree and timeline.
4. Control returns through the advisor flow, or enters game-over at turn 60.

Open dependencies at this stage included complete event/person initialization, random state and
consumption order, text rendering, modal outcomes and economic settlement. The tax
panel changes rates only; it neither awards income nor advances time. A playable
turn must handle these dependencies rather than silently skip their effects.

## Military controls and catalog recovery

ScoreScript 25 opens the military panel (holding at frame 88). The renderer uses
PICTS member 193 (`Militaria 1 norm`), level pictures 200–204, initial troop
picture 217, and the original recruitment/upgrade/OK highlights. Frame 88 supplies
the button rectangles. ScoreScripts 26, 27, 33 and 34 dispatch upgrade, infantry,
cavalry and artillery respectively. Dismissal and commander hiring are now
connected as described below; movement and the overview page remain pending.

MovieScript 5's recruitment handlers add 1000 troops per purchase. Base prices
are 25/100/250 silver multiplied by player troop level. Cavalry requires military
level 2; artillery requires a smithy in any owned province and checks its existing
artillery count against 9000. Capacity is checked against the existing total
before adding 1000. This allows a partially depleted regiment to exceed capacity
by less than 1000; the C port preserves that behavior.

MovieScript 2's military upgrades use PRICES tables 9/10 (one-based). Levels 3
and 4 require mining levels 2 and 3. Level 5 requires a castle or headquarters;
that branch has no mining requirement. Player troop technology advances 1→2 when
building level 3, and 2→3 when building level 5. Failed operations leave state
unchanged. Native fixtures cover these quirks and exact resource deductions.

The original `errorMes` background and OK button now block underlying actions.
Dynamic message glyphs remain pending, so English feedback and current troop/
resource values appear in the preview status outside the original canvas. This
is a temporary diagnostic presentation, not a claim of original message parity.
The initial troop icon follows frame 88; other-page return handlers that update
its technology picture are not yet translated. Sound remains pending.

`tools/export_catalog.py` and `src/catalog.c` recover DATA.CST's 32 culture people,
33 scientists, 27 commanders, 24 period-based events, 22 dated events A and six
events B. Numeric ranks, dates, record order and period membership are typed in C.
The generator checks that period lists have no duplicates before storing a
59-bit membership mask. Period queries scan in the original record order, matching
`fixWhenList`'s list insertion order. Random event-B dates add `random(15)`
at initialization; the tables retain their unmodified base years.

Names, year descriptions and other text retain exact original bytes. Inspection
of all 92 person-name rows confirms Windows Arial styles and Windows-1252 name
bytes. Some event strings mix encodings even within one name (for example
`Västerås arvf` followed by byte 0x9a), so a global normalization remains wrong.
Each extracted member
has an STXT resource ID and content hash in `analysis/catalog-provenance.json`.
Result/check handler names are stored as data for later explicit C translations;
there is no dynamic Lingo evaluator. Catalog recovery does not complete event
initialization, execution or random-consumption behavior.

## Windows RNG and new-game random state

The supplied `SVEA95.EXE` contains a CRT linear congruential generator at
`0x5010d1`: `state = state * 214013 + 2531011` modulo 2^32, returning bits 16–30.
The wrapper at `0x44e770` calls it twice, concatenates the two 15-bit results,
applies modulo only for a positive argument, and adds one. Seed zero is valid;
even `random(1)` advances twice. No 65535 range cap applies. This differs from
the Perlin/LFSR implementation in the general Director reference inspected.
The wrapper is registered at `0x46bb82` with builtin ID `0x11c`.

`tools/probe_projector_rng.py` extracts only these arithmetic routines, preserves
their virtual addresses and instructions, and links a native i386 ELF harness.
No Windows runtime, emulator, VM, or interpreter is executed. Sixty independent
vectors and five complete new-game sequences are captured in
`tests/projector_rng_vectors.h`; source hash and results are also recorded in
`analysis/projector-rng-probe.json`. Execution requires Linux i386 support and
permission for its syscalls; the regression tests themselves need neither.

`src/random.c` matches those vectors. `sr_game_begin` consumes six `random(15)`
calls for events B, eight alternating military-date `random(40)` calls, two
`random(20)` name choices, then `random(2)` for the family head's type. It also
initializes country conquest priorities and price modifiers, empty used/available/
owned person lists, economic modifiers, and the family-specific points baseline.
Points start at zero after subtracting the initial family's building/diplomacy/
trade total. Names preserve the original patronymic rule (remove a final `s`
before adding `sson`). New games continue the random stream unless explicitly
reseeded; invalid family selection does not consume it.

The projector's startup combines local Mac-epoch seconds with `GetTickCount() >> 4`
through `0x469660` and seeds through `0x470e60`. The browser supplies a host seed
using Web Crypto; `?seed=1` supports replay. This is an explicit platform adapter,
not a claim that independently launched Windows/browser sessions share a seed.
Original whole-game startup parity still needs observation.

## Text fields and font mapping

The C asset reader now validates D5 STXT text/style ranges and the 20-byte style
records: character offset, line height, ascent, font ID, face, point size and
16-bit RGB. Text CASt metadata supplies bounds, alignment, border, gutter,
scroll, shadows and flags. Fmap entries resolve platform-specific IDs to font
names. The exporter preserves these alongside original text bytes, without
guessing a universal character encoding.

All 42 archives passed linked-field extraction: 496 fields, 614 style runs.
This differs from the inventory's 516 raw STXT resources because not every
resource is a linked field. Original Year uses font ID 1, Arial, size 18,
line height 22, ascent 17 and RGB (0x3333,0,0); most resource counters use size 12.
MapInfo uses MS Sans Serif (ID 0x88). Earlier documentation reversed these two
font names; this was corrected against the original Fmap records. The installed Wine fonts are replacement glyphs,
and no original glyph files have yet been identified. Text layout extraction
therefore does not establish pixel-accurate font rendering.

## Farm and mining panels

Farm/mining panels now use the original score frames 78/83, their background,
level pictures, pressed buttons and two production banners. Upgrade handlers
are direct translations of MovieScript 2; farm level five checks its mill/HQ
requirement before city/resources. Mining sums employed scientist ranks and
five ranks per owned university, with thresholds 1/3/6/10. Both update their
production index to the new level, preserve failed state atomically, and consume
no randomness or time. Resource storage now preserves fractional upgrade costs;
full Lingo numeric type propagation remains to be checked during turn translation.

Nine native suites pass under sanitizers, and 334 Firefox checks pass. The browser
harness now generates fresh native screenshots on every run and waits safely
for iframe document creation. These checks establish native C/Wasm consistency.

A separate original-game copy and Wine prefix were prepared under `build/` for
reference comparisons. The attempted launches yielded no captured game window;
the isolated Wine services were closed. No Wine component is used by the browser
port, and no original-runtime parity was established by this attempt.

## Turn settlement stages

`src/economy.c` translates the separate relation, enemy-army, trade, price,
income, supply and growth stages. These are tested core functions; the browser
now executes an end turn through annual events, rebellion choices, genealogy,
crossbow completion and settlement. Wars, the other nine minigames and starvation
dismissal remain visible waits until their real gameplay is translated.

Recovered details preserved by the tests include:

- Player province acquisition order determines which province consumes each riot
  reduction draw. Only riot types 1 and 3 reduce income and draw `random(20)`.
- Upkeep truncates troop thousands per province. Food shortage pauses payment
  without mutating resources; a caller must resume after dismissal without
  awarding income again. Silver can become negative and costs can be fractional.
- A harbor protects trade during war. The trade stage settles previously reserved
  orders, updates country stock only for successful trades, then clears orders.
- Price updates alternate crops/metal per country; direction randomness is drawn
  only when the trigger succeeds.
- Without a bribe, the original relation clamp changes only its display variable,
  leaving the stored relation outside the displayed range.
- Enemy technology advances strictly after its scheduled year. Enemy troop caps
  are enforced through repeated 0.9 reductions before the artillery cap.
- HQ grows at most one level per settlement. City growth uses integer unrest/10
  and strict population cutoffs; at population <=500 the original leaves the
  existing city level unchanged rather than assigning level one.

Mutable province ownership/riot values now live in runtime state, separate from
the original definitions. Full original-game parity is still unverified; in
particular, future arithmetic must preserve Lingo integer/float type propagation.

## Event, succession and turn controller

`src/events.c` preserves ordered event pools (three tickets per eligible C event),
the minimum 20-slot random selection, forced early scientist/commander arrivals,
and the original A/B/C effects. `ResultC7` uses a different initial capitalization
in the original; handler binding checks account for Lingo's case insensitivity.
`src/family.c` translates name eras, inherited patronymics, title tiers and the
random death draw even when replacement is forced by age or negative points.

`src/turn.c` exposes explicit requests for annual delay, events, minigames, wars,
riot decisions and starvation. Repeated observation cannot consume randomness or
award income again. The controller rejects ordinary acknowledgments of wars and
minigames: their actual subsystem must finish before settlement can continue.
Food-shortage resumption, suppression costs, trade losses and last-province loss
have focused tests. Original async message overlap and complete native parity
remain unverified. Silver tracks integer/float provenance where implemented so
halving an integer truncates while halving a float retains its fraction.

## Crossbow contest

`src/crossbow.c` translates ARMBORST's MovieScript 3 and ScoreScripts 2, 4, 5, 6,
9, 10 and 28 directly. It implements three rounds of five shots, relative mouse
dragging with the original limits, all eight wind vectors (including direction
7's doubled vertical effect), two unconditional wind-offset draws and the extra
bolt-picture draw only on a board hit. The ten-step loading/lowering movement
keeps integer division, so its visible endpoint can differ from the logical aim
start. The 15-, 45- and 100-tick waits use 60 ticks per second.

`tools/prepare_crossbow.py` recovers 42 BITD images and six masks from the actual
frame-25 target/board artwork. The one-pixel collision sprite tests these masks,
inner target first. Edge-connected white matte behavior is still a candidate;
the source establishes the `intersects` operation and ink 8, while
[ScummVM's documented D6 tests](https://github.com/scummvm/scummvm/blob/master/engines/director/lingo/lingo-code.cpp)
provide secondary evidence for box-against-matte behavior. This is an inference
for D5, not a native D5 verification. No ScummVM runtime or interpreter is linked.

The browser preview uses original frames/artwork and calls only translated C
for rules, input and rendering. Update-stage animation steps currently follow
browser presentation cadence; native animation timing, nearest-neighbor pointer
scaling, the one-pixel instruction bitmap width discrepancy, original numeric
field glyphs and real pointer capture still need comparison. Score and bolt
counters are temporarily visible in the surrounding preview status text.

The C sound reader accepts a format-2 `snd ` resource containing one inline
buffer command and standard/extended uncompressed PCM headers. Its bounds,
sample widths, channel count, external-pointer rejection and truncated samples
are tested. Header layout follows Apple's
[Sound Manager reference](https://developer.apple.com/library/archive/documentation/mac/pdf/Sound/Sound_Manager.pdf).
The crossbow's three unsigned 8-bit mono sounds are all 22050 Hz: wind 15360
frames, hit 25553 and miss 10947. They are exported without resampling to WAV;
Web Audio uses the same rate and replaces channel 1 on each original sound cue.
The manifest retains PCM hashes and source resource IDs. Other sound formats
and the rest of the game's music/effects remain pending.

Native tests exercise a complete contest using the strategy RNG, earn 75/75
points, feed the real result tier 10 into the turn controller, and check the
1000-silver reward and commander hire occur once. Browser tests play fifteen
shots through actual pointer events and compare four framebuffers with native
C output. These establish implementation consistency, not complete original-game
parity. Browser strategy integration is now connected as described below.

## Browser turn presentation and streamed event bank

The original end-turn hit region is SVEA frame 12/channel 22, the hourglass at
(573,407), 58×59. The browser now invokes the C controller from that control,
waits the original 30 ticks per year, and blocks province actions while a turn
is active. Events use frame 144's original registered picture position and
EVENT cast's ScoreScript 29 OK control; that external script is unrelated to
SVEA's own ScoreScript 29 (diplomacy). Riot buttons use frame 248 and scripts
69/70/71, with the original error dialog on rejected suppression.

`tools/prepare_events.py` matches all 144 referenced event/person names to their
actual EVENT.CST member IDs, dimensions and registration. The runtime streams
one raw original RGBA bitmap into a bounded 641×481 bank, rather than allocating
the whole roughly 190 MB decoded event cast. A stale/wrong-member or truncated
load is rejected, and an event cannot be acknowledged before its bitmap arrives.
The original 640×481 / 641×480 exceptions are retained and clipped to the stage.

The main pack now has 489 images (18 menus, 429 strategy, 42 crossbow), including
all province backgrounds, all six stamp colors, 16 rulers and 60 timeline images.
The C buffer is 64 MiB and Wasm initial memory 80 MiB. The bank format is unchanged;
there is still no executable original code or Lingo interpreter in the port.
Images are presently uncompressed in transport; deployment optimization remains.

Crossbow events share the strategy random stream, return through the original
frame-276 reward dialog, and continue annual processing/settlement. Native and
Firefox tests use the normal seed-8 Eka start: commander 1 appears in 1524, the
test earns 75 points over fifteen shots, the tier-10 reward adds 1000 silver once,
and settlement returns to the province in 1528 with silver/crops/metal 2050/124/110.
The browser test checks original event pixels and post-contest/framebuffer
consistency with native C. Other fixtures cover the 1560 accession and riot
decisions; they are explicitly injected native test states, not browser shortcuts.

The province may now grow and the timeline/ruler pictures update from game state.
Dynamic text, adviser/end-of-turn presentation, general game sounds and all
remaining strategy/battle/minigame controls are still incomplete. A passing
first-turn route does not establish complete campaign playability or 1:1 parity.

## Troop dismissal and starvation continuation

`src/dismiss.c` translates MovieScript 5's setUpSendTroopsHome, clickHomeArea,
sendHome, sendBack and sendTroopsHomeOK. The provisional list follows
gPlayerAreas acquisition order. Arrows move exactly 1000; sub-thousand survivors
remain. Confirmation applies all selections once, without a refund or RNG use.
ScoreScripts 122–127 connect military frame 88 to dismissal frame 103, including
the return-time troopNiv replacement using the current technology level.
The original PICTS 382 panel and mTrop1/2/3 (228–230) retain score registration
and inks. Channels 24, 35–40 and 43 provide original list/arrows/OK rectangles.

MovieScript 2's fixIncome repeats dismissal until upkeep is affordable. The C
turn controller now reopens a warning with a fresh provisional selection after
an insufficient commit, and continues from supply after a sufficient one.
The test fixture begins with 25,000 infantry and no food, receives 24 crops and
50 silver once, dismisses 1000, then pays 24 crops/silver once. This is a native
UI fixture; browser tests cover ordinary dismissal, not that injected shortage.

## Person hiring

`src/people.c` translates MovieScript 3's buyPerson/buySciPerson and MovieScript
5's buyCommander. Rank prices come from original PRICES members (200/350/500/
650/800 silver). Rank-five culture requires an owned special building 3;
rank-five science requires building 4. These checks precede the silver check.
Commanders have no such requirement. Success preserves list order, moves the
record from available to employed, and subtracts silver once without RNG draws.

The commander panel follows SVEA frame 98: PICTS 231 at (390,245), hire/OK
pressed pictures 234/232, and original channels 27/35/36. ScoreScripts 25, 90,
42, 43 and 93 govern setup, entry, selection, purchase and return. Selection
clears when entering the military screen and after hiring; a failed purchase
keeps it. Returning restores the current troop technology picture.

The seed-65 first-turn route naturally introduces commander 2, Berent von Melen,
in 1528; hiring costs 200 and leaves 850 silver. Seed 8's successful crossbow
route already employs commander 1 via resolveMiniGame's original reward; the
hiring panel must reflect that ownership rather than charge again.
List text, wrapped-line hit testing and scrolling still need the original field
renderer. Current selection uses the recovered 14-pixel line height; the tested
single-person route does not prove multi-line list parity.

## Estate, culture and science screens

ScoreScript 16 enters the estate when the current area's Spec is 7; other
special-building screens remain pending. Frame 53 draws PICTS 133 at (404,256),
HQLevel 135 at (403,154) and HerreFam 354 at (312,281). The last two members are
replaced from HQLevel1–5 (136–140) and HerreFam.1–5 (360–364). All five source
variants share the original target member's dimensions and registration points.
The level strip uses matte ink 8; the family picture uses copy ink 0.

ScoreScript 18 clears culture/science selections and opens frame 69's panel,
PICTS 151 registered at (371,243), ink 8. Original channels 23/24 select free
culture/science, and channels 36/37 invoke ScoreScripts 17/22 for OK/hire.
The original pressed images are 152/153. Each nonempty-list selection clears
the other kind; clicking an empty list leaves the current selection intact.
Success resets both lists' selections. Failed purchases retain the selection
behind the original blocking error dialog. OK clears both and returns to the estate.
Both free-list fields use size-9 Arial with 13-pixel line height. That
height is used for current unwrapped row selection; field glyphs, wrapping and
scrolling remain pending, so multi-line selection parity is not established.

The seed-4 route starts an unmodified Eka game and follows four real turns.
Turn one introduces culture record 1 (Johannes Magnus); he costs 200 silver.
MovieScript 2's checkHQLevel, already translated in economy growth, raises the
estate from 1 to 2 at the next settlement, not at purchase time. Turn two has
period event 1, turn three has none, and turn four invokes the original forced
science record 1 (Willem Boy) arrival. Hiring him costs 200 and supplies one
science rank. A level-2 mine then costs 100 silver/25 metal and raises the
production index to 2. The final tested state is year 1543/turn 5, estate 2,
mine 2, and resources 700/196/115. No generated rewards or browser state
mutation APIs are involved. Additional injected native fixtures test rank-five
building errors and mutually exclusive list selections.

## Linné memory game

`src/linne.c` translates LINNE's ScoreScripts 10/11, 16, 18–21, 24–26 and
MovieScript 22. The six pairs are fixed, with no randomization: flower channels
19–24 use members 49/52/51/53/54/50; name channels 26–31 use
55/58/56/59/57/60. All register at (320,240), matte ink 8. Selected highlights
are channels 33–38 and 40–45. Successful pairs replace the loose flower and
name with completed book entries on channels 11–16. The C renderer uses these
original members; no generated artwork or script interpreter is involved.

The watch frame waits 300 ticks. The test frame exits on `ticks > start+1800`
or six correct pairs. A pair attempt is counted only when both selections are
present, regardless of which was selected first. Both selections reset after
the comparison; matched cards are no longer selectable. Success artwork requires
six pairs and at most twelve attempts. However, ScoreScript 26's bytecode returns
10/8/7/6/5/2/1 for 6/7/8/9/10/11/12 attempts without checking score or success.
Six wrong attempts followed by timeout therefore displays failure but returns
tier ten. That discrepancy is preserved, including the positive strategy reward
and employment of science record 20. Zero or more than twelve attempts return zero.

The intro uses 13+43, study uses 1, play uses 2 and result overlays use 35/36.
The original play and result-OK rectangles are (280,421,93,25) and
(269,274,97,25). The numeric score field (member 28) is still pending.
`tools/prepare_linne.py` regenerates pair arrays, verifies score sound channels,
exports audio through the C PCM decoder and records original-file provenance.
Thirty-seven used images bring the pack to 526 images / 65,061,840 bytes.

Sound member 30 occupies score channel 2 on frames 10/14/15; frames 24–26 clear
that channel. The CASt info flags are 0 for member 30 and 16 for 29/31/32;
all four sound loop bounds span their entire sample. The flag-layout and
play-once interpretation are supported by the format readers in
[movie.cpp](https://github.com/scummvm/scummvm/blob/master/engines/director/movie.cpp)
and [cast.cpp](https://github.com/scummvm/scummvm/blob/master/engines/director/cast.cpp).
Background looping and finishing the current iteration when the score clears it
are inferred from those flags and the behavior documented in
[sound.cpp](https://github.com/scummvm/scummvm/blob/master/engines/director/sound.cpp).
These are format/behavior references, not runtime dependencies. Original-native
mixer comparison is still pending. Click/success/failure puppet sounds replace
channel 1. Member 29 contains 7103 mono 16-bit frames; 30/31/32 contain
276482/68157/79417 mono 8-bit frames, all at 22050 Hz. WAV conversion preserves
sample values, including the 16-bit byte order conversion.

Firefox plays a seven-attempt completion and compares intro, watch, play,
selection, mismatch, completed pair, success and final images with native C.
C tests cover all reward tiers, timeout, invalid/duplicate selection and wraparound.
A native UI fixture begins in 1733/period 43 with seed 5, reaches the 1737 event,
plays LINNE through its actual pointer controls, verifies reward and employment
once, and resumes the next annual wait. Both a six-pair success and the six-wrong-
guess timeout quirk are covered. This is deliberately documented as an injected
historical fixture; a continuous browser campaign to that date is not verified.

## Live text-field bindings

`tools/prepare_fields.py` resolves 111 placements for 72 dynamic SVEA members
from frames 12/48/53/69/78/83/88/98/103/112/118/132. It preserves the score rectangle and
channel, ink, STXT font ID/size/face, line height/ascent, color and box alignment.
Supported fields have one plain style (bold for trade markers) and no border, gutter or shadow;
the generator asserts those constraints rather than silently discarding them.
`analysis/field-provenance.json` records the source hashes and all placements.
No glyphs are generated or substituted. Original font files are still missing.

`src/fields.c` translates `fixAreaInfo`, `setUpCultAndSci`, selection labels,
`setUpBuyCommanders`, map rollover and troop-dismissal field assignments. Reading
text does not alter game state or consume randomness. Resource values use the
existing Director-5 rounding rule, including negative half values. Formatting
uses bounded buffers, reports unsupported members/overflow and never falls back
to saved authoring values such as the Year field's stale `1547`.
Culture/science list construction removes the final return, whereas commander
and dismissal list construction retains it. Empty person lists and cleared
selections contain one space. Selected detail fields contain the original name,
`Rang: <rank>` and `Pris: <price>`. Dismissal reads provisional totals, leaving
province troops intact until commit.

All 92 person-name rows were checked against their DATA STXT styles: Windows
Arial, and name bytes compatible with Windows-1252 rather than MacRoman.
The browser previously displayed accented names incorrectly (`LinnÈ` instead
of `Linné`); person-event, list and selection decoding now uses Windows-1252. Raw source
bytes remain unchanged in C. This finding is specific to person names. Event
strings contain mixed legacy bytes, so their general decoding remains unresolved.
Province names retain the existing UTF-8 representation.

The C presentation API exposes content, font and original geometry by member
number for the active panel. Browser checks cover resource values for all five
families, the Year font/rectangle/color, map rollover, blank selections and
science hiring's simultaneous list/resource changes. Native tests also cover
non-ASCII source names, rounding limits, buffer rejection, original trailing
returns and provisional dismissal. These checks establish data and geometry;
they do not prove glyph rasterization, wrapping, scrolling or original-runtime
layout parity. Canvas glyphs remain unimplemented.

The seed-20 route introduces culture person 2, Laurentius Andreæ, in 1526 of
an unmodified new Eka game. The browser test follows that actual event through
settlement and hiring, checking the non-ASCII name in the event status, available
list, selected detail and employed list. Hiring leaves 850 silver. This supplies
end-to-end encoding evidence without introducing a browser state mutation API.
An additional static PE resource audit found no FONT/FONTDIR resources in the
seven supplied PE binaries (`analysis/font-resource-audit.json`); this does not
claim a general search of all possible embedded font formats.

## Neighbor, diplomacy and trade panels

Country IDs are Denmark 1, Imperial States 2, Poland 3 and Russia 4. Their
visual columns use order 1,4,3,2. ScoreScripts 110–113 open the neighbor overview
from the four foreign map shields. The browser's former war-country labels
used the wrong order; they now read the C country mapping.

Neighbor frames 111/112 use PICTS 281 and troop icons 835–846. Troop technology
advances only when the year is strictly greater than the country's threshold.
Diplomacy frames 117/118 use PICTS 295, levels 346–350, and relation members
323/325/324/326/327 (the second and third icons are not in numeric cast order).
Stored relation values are not clamped by drawing. MovieScript 2's diplomatic
upgrade costs and estate/embassy requirements live in `src/diplomacy.c`.
MovieScript 5's `bribe` reserves ±100 silver, capped at ±1000 per country.
It ignores `gRelationCostMod`; opposite clicks refund money. The relation change
is deferred to the existing end-turn routine. Positive spending corresponds to
FÖRKLARA KRIG, but does not bypass the original probabilistic war check.

Trade frames 131/132 use PICTS 287 (matte), puppet highlights 290–293 (copy),
and marker 288 (background transparent). ScoreScript 271 supplies 23 explicit
hit rectangles. Its buttons commit on any release; they do not perform the
release-over check used by many other panels. Arrow repetition starts after
20 ticks and continues every 10, even when the pointer leaves the arrow.
MovieScript 269 repositions all markers in country order 1,2,3,4 at centers
104/536/392/248; the saved score's marker positions are not the runtime layout.

`src/trade_orders.c` translates MovieScript 265's arrows, drag calculation and
reservations, plus MovieScript 2's trade upgrades. Positive loads reserve goods
for sale, negative loads reserve silver for purchases. Crops and metal share
the country's capacity: 10/25/50/100/250. The capacity table is now explicit
game state because buying each harbor adds 50 to every entry in the original.
Harbor construction itself is still pending. Level 3/4 requires estate 2/3;
level 5 requires an owned harbor, checked before resource sufficiency. It has
no additional estate requirement. Costs use PRICES tables 4/5 and preserve
fractional trade-upgrade modifiers.

Several original irregularities are intentional:

- Arrow resource checks run before deciding whether the action refunds a prior
  order: zero silver can prevent undoing a sale, and zero goods can prevent
  undoing a purchase. Dragging back to zero can still refund it.
- A drag first restores the old reservation. Its affordable purchase limit uses
  `integer(value * 100) / 100`, an integer division. Commit separately rounds
  silver and purchase cost; arrows retain fractional silver. Returning a drag
  to zero can therefore change rounded silver. The C preview remains separate
  from committed state so browser pointer cancellation can discard the drag.
- Setup rounds `center + shift`; arrows round the absolute shift before applying
  its sign. A negative half-pixel order can move one pixel on panel re-entry.
- The initial price fields are literal strings `1.0`/`3.0`; subsequent price
  updates use the original `floatPrecision=2`. Marker fields are bold red Arial
  10 and follow the marker position. Glyph rendering is still pending.

Native UI tests and browser routes exercise real seeded starts and end-turn
settlement. The diplomacy route leaves 750/124/110 resources in 1528 after a
100-silver upgrade and 200-silver reservation. The trade route upgrades Denmark,
sells ten crops and buys five metal, ending at 945/114/90 with orders cleared.
Neither route injects game state. Separate C tests cover war losses, owned
harbor delivery, upgrade prerequisites, shared caps and fractional prices.
Battles, general button sounds, original glyphs and original-runtime comparison
remain unfinished; framebuffer equality only compares this C port's two builds.

After the trade work, the asset bank had 570 images and 69 score hit regions.
C capacity is 72 MiB; Wasm initial
memory is 96 MiB. Regions span multiple screens, so the old total-region limit
of 48 was replaced with 1024; each individual sprite channel is still ≤48.

## War protocol and quick battles

`src/war.c` now translates ScoreScripts 47/49/50/72–79/84/100/105/243–246,
MovieScript 3's war handlers and MovieScript 5's `fixNPCArea`/`fixNewLevel`.
The C war state is separate from the suspended `SrTurn`: the turn resumes only
after the war's result/peace outcome. Manual combat remains a distinct pending
phase and cannot silently call the quick-battle routine.

Original UI phases use frames 201 (declaration), 197 (offer), 204 (no offer),
207 (no troops), 214 (enemy province), 219 (regiment), 221 (quick comparison)
and 225 (result). Hit regions use their original score scripts; list rows use
the source fourteen-pixel line height. Runtime country shields replace puppet
members. War-specific lists/messages/numbers are currently exposed below the
canvas through C read APIs; their original text-field bindings and glyphs are
still pending. The general error panel blocks other war controls.

Quick battle keeps the original weighted strengths (infantry 2, cavalry 5,
artillery 7), strict doubled-strength branches, multiplicative commander bonuses
and Windows random draw order. Decisive battles take five draws; close battles
take seven. Even an empty troop group consumes its loss draw. Casualties stay
floating until the original individual conversions: Swedish troop losses round
before subtraction, enemy infantry/cavalry losses do not. `SrCountry` now retains
fractional enemy infantry/cavalry until the subsequent `fixEnemyTroops` phase;
neighbor troop displays still use the original `integer()` formatting.

Opening the quick comparison sets relation to 2 and calculates the result but
does not yet apply losses. Its OK applies `fixBattleResult` once. The result's
OK resumes the turn. Victory may acquire the selected enemy province or award
compensation. Defeat when attacking pays compensation; defeat when defending
transfers the first Swedish province on the enemy's priority list. Quick-result
artillery losses are half the loser's guns. Enemy troop floors are preserved,
even when they immediately restore nominal losses. Remaining silver is clamped
to -250 except on the original early game-over path. Win/loss prestige consumes
one `random(5)`; winning also increments the war-win count.

Province acquisition appends in original order, then performs four random level
draws and one population draw. Mining/military levels are capped by the maximum
of the now-owned areas; farming/mining production indexes remain unchanged.
One source oddity is retained: defensive loss selects the first current owned
province before removing the lost one. If the lost province was already first,
the current-area variable can still refer to it. This requires original-runtime
comparison, not an unrequested corrective rule.

Negotiations cost 100 silver unless an owned embassy exists. The demand uses
the enemy's first desired owned province, `random(3000)` money capped by remaining
silver, a minimum money offer of 1000 and a strict `random(100) < 60` province
choice. Refusal returns to declaration; a failed offer removes negotiation from
that menu. Acceptance transfers the demanded province or silver and lowers
relation by five, with floor -3. No-troop surrender follows its separate tariff
(`turn * 50` when attacking) and defensive territory-loss path. Model tests cover
these paths; their full browser interaction coverage remains less extensive than
the verified quick-victory path. Original ending screens are still pending.

Normal seed-3 Tre Rosor route: reserve 400 silver for war with Denmark, end the
first turn, select SNABBSTRID, Skåne and Nyland. Seven quick-battle draws bring
the stream to 29 calls and calculate victory: 520/780 Swedish infantry/cavalry
losses and 800/630 Danish losses. Province development plus prestige brings it
to 35 calls. The second owned province participates in the same settlement;
the final state is 1528, turn 2, 688 silver, 123 crops, 111 metal, Nyland troops
1480/1220 and 57 RNG calls. Native and Firefox tests exercise this UI route with
no injected game state and compare the rendered pixels. This establishes a
playable quick-battle route, not parity with the original runtime.

Current bank: 594 images (18 menus, 497 strategy, 42 crossbow, 37 Linné),
83 regions, 74,232,120 bytes. Manual combat, retreat/result integration from KRIG,
war sounds, original war text and original-runtime validation remain unfinished.

## Manual battle rule foundation

`src/battle_rules.c` translates KRIG MovieScripts 1 (board and deployment),
58 (ordered ranges), 65 `troopControl.attack` (damage/counterattack), and 67
`ArtTroop.shoot` (artillery damage). `tools/prepare_battle_rules.py` extracts
literal tables without executing scripts and records source hashes in
`analysis/battle-rules-provenance.json`. Regeneration is byte-for-byte stable.

Board coordinates retain all 63 irregular source positions. Click selection
uses the original transposed half-table lookup and untransposed square table;
every original position selects its own square. Range-two movement checks only
the destination. Range-two shooting toggles visibility for each occupied
intermediate tile: two blockers re-enable a shot. River and rock are transparent.
Range three has no line-of-sight check, and range five uses a positive-row cone
for either nation. List order is retained for later AI selection.

Melee applies the original type, level, commander, repeated-attack and random
multipliers with final integer rounding. A surviving defender counters using its
reduced troop count, without increasing the original attacker's attacked count.
Deaths retain negative hit points until the controller removes the object.
Artillery consumes the overwritten `random(3)` before `random(100)`.

Enemy formation selection follows Swedish cavalry placement in the center,
flanks and third row, with strict 5:1 composition thresholds. All twelve original
ten-position lists are retained. Artillery, cavalry and technology modify the
starting row, clamped to 6–8; profiles 11/12 and enemies without artillery start
on row 6. The API explicitly distinguishes Lingo INTEGER and FLOAT enemy cavalry
because division by two can change the chosen row. The eventual caller must
preserve that numeric type even when a FLOAT's value happens to be integral.

The new native `battle-rules` suite tests these behaviors and all twelve
formations. All 25 native suites pass, including address/undefined-behavior
sanitizers (run outside the sandbox because LeakSanitizer rejects ptrace).
The rules module also compiles for wasm32. Browser deployment is now connected,
but this is not a complete playable manual battle or evidence of original-runtime
parity. Action accounting, artillery target selection/reloads, animation/audio
and result integration remain.

`battle_setup.c` now translates `fixTroopIcons`, `fixArtIcons`, ScoreScript 12's
staging, `setUpTrooper` placement, enemy formation application and ScoreScript 45's
survivor subtraction. Cavalry groups precede infantry. Totals up to 6000 produce
six groups; totals through 10000 divide by 1000; larger totals use ten. Integer
division discards remainders instead of redistributing troops. For example,
2000 infantry and 2000 cavalry become three 666-man groups of each type, already
leaving two men of each type to appear in final losses. Fractional enemy counts
retain Lingo numeric types through icon count, division and loop limits. A whole
valued FLOAT can differ from INTEGER: 7800.0 infantry gives eight groups of 975,
whereas 7800 INTEGER gives seven groups of 1114. These are source-derived cases;
the strategy caller now supplies explicit numeric type tags. `SrCountry.troops_real`
preserves FLOAT results of quick losses, clears individual tags when integer
troop floors replace those values, and resets at `fixEnemyTroops` integer rounding.

Swedish deployment permits only empty squares 1–21. Once placed, a group cannot
be repositioned during deployment. A failed drop preserves visible position and
board occupancy, but changes the logical square, as the source does. Original
staging puts the seventh group at (65,185), following six groups on the first
row. Enemy placement is permitted only when all Swedish groups are placed.
The model prepares enemy data in advance, but it must remain hidden until then.
Gun counts truncate at thousands, cap at five, and divide troop counts evenly
with integer truncation. Initial reload values are 4/3/2 by technology level.

`battle_ai.c` translates one square of `resolveEnemyActions`. Priorities are
rear attack, forward attack, side attack, forward movement, side movement,
wait. Infantry/cavalry attack thresholds are strict and differ by direction.
The source's center-column random branches both choose the same side; discarded
lower-priority decisions still consume random calls. The attempted retreat
tests the occupied forward square instead of the rear square, so it cannot
succeed, but consumes its random call. A rear target at square 63 is omitted
by the original strict boundary. Enemy presence on squares 1–7 wins before
checking action counts. The future controller must scan the live board in
ascending square order and evaluate decisions even for exhausted units, applying
actions only afterwards. Decision selection is tested; execution and animations
are not connected yet.

## KRIG image decoding

Offline export initially failed on KRIG members 42,43,69–72,83–86, which have
16-bit pixels. `assets.c` now supports RGB555, with big-endian pixel pairs for
raw data and separate high/low byte planes per decompressed row. This format
interpretation was cross-checked against the primary
[ScummVM Director bitmap decoder](https://github.com/scummvm/scummvm/blob/master/engines/director/images.cpp),
not by running ScummVM. Five-bit components expand to eight bits by replication;
the unused high bit is ignored. Native tests cover both layouts, padded rows,
primary colors, white/black/gray and buffer bounds. Existing 8/32-bit paths remain.

`python3 tools/export_assets.py SveaRike/KRIG.DIR SveaRike/SWE1.CST SveaRike/DAN1.CST`
now exports 39/324/321 bitmaps respectively, plus KRIG's 137 score frames. The
decoded move2Hi arrow was visually inspected. Assets remain under
`analysis/assets`; separate browser packs are described below. KRIG's actual map
member 92 is 584×338, with sprite location (31,115) and registration offset
(3,1), making its top-left (28,114). Board hit tests now use those actual
dimensions; they previously used 576×336, which still contained all 63 points.
Score ink 36 and member registration must be honored when the battle scene
renderer is connected.

## Browser battle deployment

`tools/prepare_battle.py` now exports KRIG plus all fifteen SWE/DAN/PRU/POL/RUS
technology casts and packs 4,779 images into sixteen SRB1 banks. The banks total
69,692,972 bytes, with a largest bank of 13,926,132 bytes. The browser requests
only the common art, the current Swedish technology and the current enemy cast;
each C input bank has a 16 MiB capacity. The original menu bank is unchanged.
WebAssembly initial memory is now 144 MiB to accommodate the three battle banks
alongside existing art and framebuffers. Loaders check IDs, dimensions, byte
offsets and sizes before use. A native renderer fixture loads every bank across
all 36 combinations of Swedish technology, opponent and enemy technology.

`battle_scene.c` draws original backgrounds, shields, map, deployment markers,
staging panel, infantry/cavalry and guns. Score frame 21 supplies deployment
sprites; frame 55 supplies the post-deployment static scene. Channel 3 remains
hidden. Ink 36 removes white; backgrounds/splash and the shield channel retain
copy ink. Original registration points are preserved. Enemy and player units
are sorted using `troopControl.sortTrooperSprites`' original 63-square order
after deployment. Before deployment, creation/sprite order is preserved.

Background selection follows ScoreScript 47: country background, Russia's
extra two-way draw, then the independent 1-in-10 and 1-in-20 overrides. Dragging
follows the pointer without a grab offset, as `Troop.drag` does. Board coordinates
use top-left (28,114) and dimensions 584×338. Browser cancellation restores the
staging position; ordinary invalid drops retain the source's logical-square
quirk without moving occupancy. Placement inputs wait for all required art.
Completing the sixth placement in the normal seed-3 Tre Rosor route selects
profile 4, row 7, first enemy square 53. This route uses troop technology 1;
Nyland's level-2 military building is a distinct value.

The deployment implementation initially passed 799 Firefox checks, including this full new-game-to-deployment route.
Native/browser framebuffer bytes match before dragging, during dragging, and
with both armies placed. The campaign remains suspended with its original
2000 infantry/2000 cavalry; random-call count is 24, including two background
draws. Subsequent movement/attack work is described below. Input-mask/runtime
comparison, original glyphs, intro timing, sounds and result
return remain pending. No emulator or original bytecode runtime is used.

## Original troop animation and combat scheduling

`prepare_battle_animation.py` decodes 394 type-2 cast members through the existing
C SCVW score reader: 4,853 one-bitmap frames and 315 pose/offset mappings from
MovieScript 87 and the artillery constructors 75/77. Output is `generated/battle_animation.h`; resource hashes and
source provenance are in `analysis/battle-animation.json`. Bitmap registration,
film-loop rectangles and source pose offsets determine each anchor. All supplied
subsprites have stretching disabled (asserted by the extractor), so the renderer
uses natural bitmap sizes even where stale score width/height fields differ.
Loop flag 32 clamps the last frame instead of repeating. One SWE3 InfShoot7
subframe has cast ID 0 instead of 65535; the metadata is retained and it currently
resolves locally. Original-runtime resolution for that frame remains unverified.

`battle_scene.c` uses Troop.animate's 18 cavalry or 22 infantry four-tick steps,
clears source occupancy during movement, and commits the new logical position
afterward. Selection arrows use the original four directions and green hover
images. Commands are blocked during movement and exchanges. Native and Firefox
checks verify movement images, logical-position boundaries and exhausted groups.
Frame presentation against Director's updateStage cadence is not verified.

`battle_attack.c` translates troopControl.shootTrooper/attack into timed C states.
It draws damage RNG when shooting starts, decrements the shooter's actions,
applies HP loss after six four-tick waits, restores the shooter after five more,
then restores/removes the target after seven further waits. Zero damage omits
the target animation and ends that shot after eleven waits. A living defender
counters with its reduced strength even with zero/negative actions; primary
death triggers movetrooper and its extra unit-action cost. Swedish deaths adjust
the current maximum actions by the dying group's remaining actions, including
negative values. `battle_rules.c` exposes a single-roll operation so the reply's
RNG draw occurs at the source's later counterattack boundary.

The scene scans the live board 1..63 for enemy actions. It preserves decisions
and RNG draws for exhausted enemies found again after sideways movement, and
the edge-victory check before action availability. ScoreScripts 27 and 26 reset
only the appropriate army at its turn boundary. Troop-count and edge outcomes
now return through the campaign result controller described below.
The Swedish escape click region is also translated; it has no browser test yet.

The seeded campaign route now fires enemy artillery before enemy troop actions.
Swedish guns run the officer/target sequence described below before player actions.
Native fixtures explicitly omit artillery to test four enemy turns, player shots,
counterattacks and a loss. A second fixture uses an explicit commander bonus of
10 to test a lethal shot and the extra-cost forced advance. These fixtures do
not prove that a normal campaign has completed a manual battle; the normal
seed-3 browser defeat described below supplies that separate evidence. Original
field presentation, sounds and original-runtime parity remain pending.

## Enemy artillery controller

`battle_artillery.c` translates ScoreScript 28's checkTarget and fixTreatList.
It prioritizes Swedish units in squares 57..63, then stronger blockers directly
in front of enemies on 8..14, then the source's ordered diagonals. Finding a weak
unit on the +diagonal skips the other diagonal; square 14 excludes +diagonal.
The next priorities are rows 50..56, 43..49 and 36..42, strongest infantry >500,
strongest cavalry >500, remaining infantry, then remaining cavalry. Row threats
start at 1000 and lose 20 per raw +/-1 or +/-7 enemy neighbor, 50 if stronger and
another 50 if more than twice as strong. No geometric edge filter is added.

Property-list sorting ties currently preserve insertion order and select the
last maximum. This is a provisional runtime assumption; original Director tie
behavior is not proven. It affects the normal test route, where three infantry
groups begin with equal strength. Tests intentionally record the assumption
instead of claiming a recorded original-runtime target match.

Each cannon starts ready at reload 4/3/2 for technology 1/2/3. A shot sets it to
one. Subsequent unready turns increment without firing; a ready gun with no
target stays ready. The scene processes each gun in creation order before
scanning enemy troops, so later guns see earlier casualties. Enemy artillery
does not run winnerCheck between shots; the first subsequent enemy troop
decision/action precedes that check, as in ScoreScript 28 and troopControl.

`battle_attack.c` now also schedules ArtTroop.shoot without troop action costs,
attack-count increments, counterattacks or forced advances. It consumes both
original random draws, applies damage at tick 24, restores the cannon at 44,
then restores/removes the target at 72. The zero-damage branch lasts 24 ticks;
normal initialized gun icons contain at least 1000 men and cannot round to zero.

The renderer uses original artillery film loops and the constructors' offsets:
Swedish (-14,16) at all levels; enemy (12,10), (40,10), (38,-2). Name lookup now
selects the earliest duplicate member: SWE3 ArtShoot member 226 precedes 263.
This agrees with the lookup behavior documented in ScummVM's
[Cast::rebuildCastNameCache](https://raw.githubusercontent.com/scummvm/scummvm/master/engines/director/cast.cpp).
Only its source was consulted; no interpreter/emulator was run or included.

The normal seed-3 route now reaches round two after a 210-damage cannon shot
(666 → 456 infantry), enemy movement and Swedish action reset. RNG draws are
24 before artillery, 26 after shooting, 27 after the troop turn. Native checks
also advance two more enemy rounds: reload increases to 2 then 3, without
another shot. Battlefield audio remains unfinished; retreat and campaign result
integration are described below.

## Swedish artillery and officer

ScoreScript 30 tests the first gun's readiness to decide whether the officer
appears at (10,336), then processes all guns in creation order. Each ready gun
enters the artillery scene: ScoreScript 35 calls moveArtOfficer with that gun's
position, ScoreScript 31 tracks an enemy square with the original `sight` member
94, and ScoreScript 21 fires at its square and returns. The controller checks
for victory after each Swedish shot, then moves on to the next gun. There is
no distance/cone restriction in this targeting path. Empty/own squares hide
the sight. Leaving the map rectangle preserves the last sight, as the source
only changes overSquare while rollOver(mapSprite) is true; outside clicks still
cannot activate its sprite.

After the last gun, the officer walks to (237,516), is hidden, and ScoreScript
26 resets Swedish troops' actions and attacked counts. Unready guns increment
their reload counters and do not request a target. The renderer uses channel
41 ordering above guns 36..40 and below the shield and sight. Both officer walks
use 26 four-tick steps and the source's (-5,20) walking offset. Stand/walk cast
members for all three technologies and all five gun-position lists are extracted
into `generated/battle_animation.h`; position-source hashes accompany the data.

Film-loop subframes now honor their recorded copy/background-transparent ink.
SWE3's earlier ArtShoot member 226 contains copy-ink frames. The renderer restores
opaque white from the image bank's retained RGB in those frames. This treatment
is consistent with independent subchannel rendering in ScummVM's
[FilmLoopCastMember::getSubChannels](https://raw.githubusercontent.com/scummvm/scummvm/master/engines/director/castmember/filmloop.cpp)
and [Window::renderChannel](https://raw.githubusercontent.com/scummvm/scummvm/master/engines/director/window.cpp).
Those sources were read only; original Director frame comparison is still pending.

The normal seed-4 route extends its four completed turns by buying 1000 infantry
and 1000 artillery after the mine/smithy upgrade, spending 400 on Danish war,
playing the naturally occurring 1547 Bagge crossbow contest, and entering manual
battle in 1548. It reaches war at 206 random draws, battlefield setup at 208,
and the first Swedish shot at 210. That shot hits enemy cavalry in square 39:
670 → 549 troops, with no target action spent or counterattack. The officer then
leaves and the six Swedish infantry groups receive their player actions. The
fifth campaign turn now settles after retreat and result confirmation. Separate
native fixtures exercise five
guns at all three technology levels, movement boundaries, invalid target clicks,
sequential shots and the next unready turn without another officer walk.

## Manual result return and retreat

KRIG ScoreScript 45 subtracts each side's surviving infantry and cavalry from
the original `gTroopsToBattle` counts, constructs the result property list and
uses `play done`. SVEA ScoreScript 87 sets relations to 2; ScoreScript 78 invokes
MovieScript 3's `fixBattleResult`. There is no extra battlefield confirmation.
`battle_scene_result` exposes the completed outcome and `sr_war_finish_manual`
applies it once, reusing territory development, compensation, enemy troop floors
and prestige processing. The campaign result's OK button resumes the real turn.

Original numeric types matter here: manual Swedish counts and losses are INTEGER;
enemy formation can promote cavalry to FLOAT through a FLOAT icon count even
when original cavalry is INTEGER. Each unit therefore retains an HP type flag.
ScoreScript 45 clamps negative HP to INTEGER zero but keeps the type of exactly
zero HP. Enemy loss type flags combine original army and survivor-sum types.
The monetary formulas truncate INTEGER division before multiplying, whereas
quick-battle losses retain their existing FLOAT divisions. Country subtraction
retains/promotes the enemy tags; applying original minimum troop floors resets
the corresponding tags. FLOAT survivor summation can leave a tiny signed loss
(e.g. seven groups from 6502 FLOAT infantry); this is preserved, not rejected.

KRIG ScoreScript 23 sets `gRunAway=1`, the enemy winner and jumps to `theEnd`.
Its channel-48 shape appears at (3,401), size 82×79, on frames 38, 42, 55, 56,
60 and 61; it is absent during deployment. Animation waits block input. The
translated control accepts retreat during player orders or cannon aiming.
In `fixBattleResult`, retreat leaves the artillery-loss locals unset; the port
uses its existing VOID-to-zero convention and retains both sides' artillery.
A normal defeat instead loses half the Swedish artillery. Neither case is
the separate no-troop surrender, whose attacking fee is `turn*50`. Original
runtime behavior, including VOID coercion, remains unverified end-to-end.

Two normal browser routes now return through this controller. Seed 4 Eka
retreats after the first Swedish cannon shot in 1548, loses four infantry from
formation division, keeps 1000 artillery and finishes turn five at 574/219/135
resources with 226 RNG calls. Seed 3 Tre Rosor ends each player turn until the
enemy wins: 1280/335 Swedish infantry/cavalry losses, 491 enemy infantry losses,
500 silver compensation and 40 RNG calls at the result. Its settlement reaches
1528 with 138/108/104 resources and 64 RNG calls. Native and Firefox result and
settlement images match. A separate native six-shot artillery fixture reaches
Swedish victory across reload turns; this is not a normal-campaign victory.

## Battle audio source inventory (not yet connected)

KRIG contains 29 sound cast members. The active script paths use background
members 154 (`SKOG.AIF`), 155 (`Skog2`) and 156 (`Vinter`) on channel 1.
ScoreScript 47 chooses the background sample from the already-selected battle
background; ScoreScript 6 starts it and ScoreScript 45 requests a 15-tick fade
on return. Troop movement uses channel 2: 171 (`walk`) or 179 (`hwalk`).
Infantry shots use 176 (`Hileb`) at level 1 and 184 (`Pang`) otherwise; cavalry
uses 181 (`Crosshit`) at level 1 and `Pang` otherwise. Cavalry's directional
specials are 182 (`Huggsab`) and 183 (`hästspark`), with different Swedish/enemy
direction numbers. Artillery uses 172 (`art1_2`) for levels 1–2 and 177 (`Kanon3`)
for level 3. The `myDieSound` property is initialized but has no playback call
in the recovered Troop handlers; do not add a death sound just from its name.

An offline scan of KRIG's main score and all 394 troop film loops covers 4,990
frames (`analysis/battle-score-sounds.json`). No frame names a nonzero sound
member: 1,239 film-loop frames contain cast sentinels `[65534,0,65534,0]`, and
the rest are zero. Sentinel semantics still need checking before interpreting
these as channel commands. MovieScript 57's random bird-sound handler has no
caller in the recovered KRIG scripts; do not add idle RNG draws without evidence.
PCM extraction, channel dispatch, loop/fade timing and browser audio tests remain
necessary before claiming that battlefield audio works.
