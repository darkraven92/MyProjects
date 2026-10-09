# Progress

Target: a 1:1 C/WebAssembly port. **No emulator.**

Longest verified continuous browser route: **five turns**, from 1523 to 1548
in the seed-4 Eka game, including culture/science hiring, a mine upgrade,
Bagge's earned crossbow reward, Swedish artillery and manual retreat/settlement.
This is test coverage for one route, not a percentage of the full campaign or
proof that other seeds and later turns work.

Current milestone: family selection → initial state → original strategy scene.

`[████░░] 4 / 6 checkpoints`

These checkpoints differ in size; this is not a percentage of total porting effort.

- [x] Trace DATABASE → SVEA initialization and welcome/main frames.
- [x] Extract 28 provinces, 16 price tables and 60 relationship rows.
- [x] Translate deterministic starting values and bonuses for all five families.
- [x] Render and browser-test all five welcome/starting-province scenes.
- [ ] Render original dynamic text: year, resources, province rollover information.
- [ ] Complete remaining initialization and verify against the original game.

Crossbow contest: `[█████░] 5 / 6 checkpoints`

- [x] Recover original artwork and target silhouettes.
- [x] Translate wind, aiming, three five-shot rounds and result tiers.
- [x] Connect browser controls and render the original scene.
- [x] Decode and play original wind/hit/miss sounds.
- [ ] Render original score fields and verify timing/silhouettes against the original.
- [x] Connect the contest's result and reward to browser strategy turns.

Linné memory game: `[█████░] 5 / 6 checkpoints`

- [x] Recover the original fixed card layout and images.
- [x] Translate selections, five-second study, thirty-second timer and result tiers.
- [x] Play all six pairs in Firefox, including cancellation and mismatches.
- [x] Decode four original PCM samples and separate background/click channels.
- [x] Return earned results to strategy rewards and employment (historical C fixture).
- [ ] Render the original numeric field and verify against the original runtime.

| Area | Status |
| --- | --- |
| File inventory | 54 original files hashed; 42 Director archives validated |
| Script recovery | 497 scripts / 1,062 handlers recovered and indexed |
| C resource readers | Archive, cast links, bitmap metadata, BITD, score deltas, STXT styles, text boxes, font mappings and inline PCM sounds |
| Original graphics | 18 menu/setup, 497 strategy, 42 crossbow, 37 Linné images packed; 155 event images decoded, 144 referenced screens streamed |
| C/WebAssembly flow | Menu, families, province, five-year turn, event screens, crossbow/Linné, reward and settlement |
| Starting state | Resources, province ownership, buildings, troops, country bonuses, seeded event dates and genealogy |
| Browser verification | Passed for all five starts; tested framebuffer hashes equal native C |
| Trade logic | Buy/sell arrows, held repeat, draggable orders, shared capacity, upgrades and delivery at settlement; original prices and rounding preserved |
| Dynamic text/fonts | 496 linked fields / 614 style runs parsed; 72 live C field values and 111 original placements connected; original glyph rendering pending |
| Random numbers | Windows projector algorithm translated; 60 native original-code vectors and five initialization sequences matched |
| Remaining initialization | Random sections translated; complete original-runtime parity and presentation still pending |
| Event/person tables | 144 original records recovered into C; period eligibility available |
| City controls | Original panel, tax arrows/slider and income/unrest banners working |
| Military controls | Recruitment, regiment upgrades, troop dismissal and commander hiring with original costs/prerequisites and error blocking |
| Farm/mining controls | Original panels and production banners; city/science prerequisites and upgrade costs implemented |
| Estate/person controls | Estate and culture/science panels connected; hiring affects estate growth and mine upgrades |
| Economy/turn core | Five-year controller, annual events, riots, trade, upkeep, growth and succession translated; waits at real war/minigame boundaries |
| Event effects | A/B/C effects, weighted person arrivals and original eligibility rules translated and tested |
| Browser turns | Hourglass, annual waits, original event images, crossbow reward, riot decisions and settlement connected |
| Neighbor/diplomacy controls | Four-country overview, troop icons, upgrades and ±100 silver reservations connected to turn-end relations |
| Other strategy controls | Province purchases/management, family-tree presentation and other controls pending |
| War | Quick battles resume the turn; manual deployment/movement, attacks, both sides' artillery and troop turns connected; manual results and retreat return to the campaign |
| Minigames | Crossbow and Linné playable and connected to strategy; eight other minigames pending |
| Starvation/endings | Dismissal resumes upkeep without repeating income; original text/list scrolling and ending screens pending |
| Audio | Crossbow's three and Linné's four original PCM samples play; two independent channels; other game audio pending |
| Video/save files | Not implemented |
| Original-native parity | Not verified |

There is no reliable percentage for the full port yet. Handler count alone is
not an effort estimate, and the current menu translations include partial handlers.

Native CTest and address/undefined-behavior sanitizer checks pass. Firefox tests
cover welcome button cancellation, province/resources for all families, framebuffer
consistency, minimap rectangle precedence, city/tax input, recruitment and upgrades.
Twenty-seven native test suites pass under address/undefined-behavior sanitizers.
Firefox passes 892 checks, including the normal seed-4 route through Swedish artillery,
retreat and fifth-turn settlement, plus a complete seed-3 manual defeat,
manual deployment/movement, the enemy cannon shot and troop turn returning to player round two, the complete quick-war route,
original field values, accented person names,
diplomacy, trade orders and held-arrow repetition,
Linné completion, commander hiring and five consecutive turns
through culture/science hiring, estate growth, a mine upgrade and manual retreat.
Existing checks include events, succession, turn waits and a
complete crossbow contest returning its earned reward to the C turn controller.
Browser tests play both a standalone contest and a full first turn from an
unmodified seeded new game: original event, fifteen earned shots, reward dialog,
income and return to strategy in 1528. Native tests also exercise a 1560 king
change and the riot/error controls. Sound checks cover original sample lengths/rates and event dispatch;
they do not verify physical speakers or original mixer output.
Original asset hashes are unchanged.

Troop dismissal is tested for all five browser starts: provisional thousand-man
groups, undo, cancellation and a single commit without refunds. A native UI
fixture verifies that starvation repeats on insufficient dismissal, then settles
after removing enough troops, with income and upkeep each applied once.
The dismissal model tests multi-province selection in original acquisition order;
original list glyphs, wrapping and scrolling remain pending.

Commander hiring now has its original panel, selection, payment, pressed artwork
and error modal. A normal seed-65 Eka game introduces Berent von Melen in 1528;
native and Firefox UI tests hire him for 200 silver after settlement (1050 → 850).
Winning seed 8's crossbow contest already employs Bagge, as the original reward
requires. Shared purchase-rule tests cover all 92 people, including insufficient
silver, rank-five culture/science building requirements, list order and duplicate
purchase rejection. Culture/science hiring now uses its original estate route,
two mutually exclusive selections, hire/OK controls and error modal. The original
estate family shield is rendered for all five families, with its level picture
changing after growth.

Native and Firefox tests verify four consecutive turns in the seed-4 route from
a new Eka game. Johannes
Magnus arrives in the first turn and costs 200 silver to hire; his rank raises
the estate to level 2 at the next settlement. Willem Boy arrives on turn four;
hiring him adds one science rank and permits the original level-2 mine upgrade.
The resulting state is 1543, turn 5, silver/crops/metal 700/196/115, mining level
2 and estate level 2. This path uses no injected game state. Separate native
fixtures cover switching culture/science selections and rank-five building
errors. Text fields, line wrapping and scrolling are still unfinished.

Linné now has a separately playable browser route at `web/?minigame=linne`.
Firefox verifies the original five-second study period, six fixed pairs, a wrong
guess, name-first selection, matched-card removal, original art, result and four
sound samples. Native tests cover the strict thirty-second timeout boundary,
all result tiers and timer wraparound. A separate native historical fixture
(seed 5, year 1733, period 43) reaches the original Linné arrival in 1737, plays
the real minigame and verifies 1000 silver, five prestige modifier points and
employment of scientist 20 before the next annual wait. It also verifies the
original quirk: six wrong guesses followed by timeout shows failure yet returns
tier ten and earns the same reward. This fixture is not evidence that a full
campaign can reach 1737. Numeric glyphs, original hit-mask/timing comparison and
original mixer parity remain unverified. All 54 original file hashes are unchanged.

Dynamic field content now comes from C for year/resources, population, troop
counts/costs, map rollover, person lists/selections and provisional dismissal.
Original list endings are preserved: culture/science lists remove the last return;
commander and dismissal lists retain it. Empty person lists and selections contain
one space. Rank/price labels use `Rang:` and `Pris:`. Source inspection verified
Windows-1252 names for all 92 people, correcting the browser's former MacRoman
decoding. Year and most counters use Arial; MapInfo uses MS Sans Serif. Earlier
documentation had those font names reversed. Their original Windows glyph files
have not been found in the supplied assets. The canvas text and scrolling remain
unfinished, so this does not advance the new-game scene's 4/6 checkpoint count.
The normal seed-20 first-turn route supplies an accented-name browser case:
Laurentius Andreæ arrives in 1526, can be hired after settlement and moves from
available to employed with his original name and price intact.

Neighbor and diplomacy panels now use the original country order (Denmark,
Russia, Poland, Imperial States), troop technology icons and diplomatic art.
The normal seed-1 route upgrades Danish diplomacy, reserves 200 silver and
settles in 1528 with 750/124/110 resources; the queued expense clears and the
relation effect applies once. General button sounds and original numeric glyphs
remain pending.

Trade has its original panel, eight markers, four upgrade controls and arrow
repeat after 20 ticks, then every 10. Native and Firefox tests follow a normal seed-1 game:
upgrade Denmark, reserve a sale of ten crops and purchase of five metal, then
settle in 1528 with 945/114/90 and empty orders. Tests preserve original quirks:
arrow undo can fail its initial resource check, while slider repricing rounds
resources and payments differently. War/harbor delivery rules are tested in C;
quick battles now resolve through the war UI. Harbor construction is pending.

The normal seed-3 Tre Rosor route now declares war through 400 silver of original
diplomacy spending. Choosing quick battle, Skåne and the Nyland regiment wins,
applies 520 infantry/780 cavalry losses and acquires Skåne. Settlement resumes
with both provinces: 1528, 688/123/111 resources, 1480 infantry/1220 cavalry and
57 RNG calls. Native and Firefox tests verify the complete UI path without state injection.
Separate model tests cover defensive wins/losses, surrender, negotiations,
embassy exemption, territory offers, artillery losses, technology boundaries,
commander multiplication and fractional enemy survivors. Manual results now return to the campaign after battlefield play or retreat. War text fields, scrolling, general sounds and original-runtime
comparison are pending.

Next: recover dynamic text/list scrolling and implement remaining strategy
controls, battles and other minigames. They are necessary before all turns and
the full campaign can finish. Advisor presentation and general sound also remain.

Manual combat now has a tested C rules module (`src/battle_rules.c`), also
compiled successfully for wasm32. It covers all 63 original board positions,
ordered movement/attack ranges, original line-of-sight toggling, melee damage
and surviving defenders' counterattacks, artillery's two random draws, and all
twelve enemy deployment profiles. Original literal tables are reproducibly
extracted by `tools/prepare_battle_rules.py`. The deployment rule retains the
distinction between integer and floating-point enemy cavalry counts.
Army partitioning and placement now work in the C model (`battle_setup.c`),
including integer division losses, original staging coordinates, rejected drops,
enemy formation placement, gun groups and survivor accounting. `battle_ai.c`
translates individual enemy movement/attack decisions, priorities and original
random-draw quirks. Tests preserve INTEGER/FLOAT differences in enemy troop
partitioning. Both new modules compile for wasm32.
These are source-derived rule tests, not original-runtime comparisons. Browser
deployment is now connected: STRID opens the original battlefield, groups can be
dragged onto the first three rows, and completing placement creates the enemy
formation. Movement and attack/counterattack animation control are connected.
Both sides' artillery, reloads and troop turns are connected. Manual outcomes
and retreat now apply their result to the campaign. Battle audio, text fields and
original-runtime comparison remain pending.

The C asset decoder now reads 16-bit RGB555 BITD images, including the original
movement arrows that previously stopped KRIG extraction. Raw and compressed
byte layouts have native tests. All 39 KRIG bitmaps and its 137 score frames,
plus all fifteen country/technology troop casts, have been decoded offline.
The browser streams three of sixteen separate battle banks on demand: common
KRIG art, the Swedish technology cast, and the opponent/technology cast. These
contain 4,779 images in total; the main menu image bank remains separate.

The normal seed-3 Tre Rosor route now also tests STRID, Skåne, Nyland, loading
original assets, dragging/cancelling, rejecting occupied/outside drops, and
placing all six groups. Three cavalry groups in the center select enemy profile
4, row 7. The first enemy group occupies square 53. Background selection consumes
two random draws (24 total for the route); placement consumes none. Campaign
troops remain 2000 infantry/2000 cavalry and the turn stays pending. Firefox
framebuffers match native C before, during and after deployment. Native renderer
fixtures additionally load all sixteen banks across all countries and technology
levels. This does not verify a complete manual battle or original-runtime parity.

Manual battle implementation: `[████████░░] 8 / 10 checkpoints`

These are differently sized implementation checkpoints, not total project progress.

- [x] Translate board, damage, formations and individual enemy decisions.
- [x] Decode and stream original battlefield and troop artwork.
- [x] Connect troop deployment and enemy formation.
- [x] Animate player movement and consume original actions.
- [x] Connect attacks, surviving defenders' counters, death and forced advance.
- [x] Execute enemy troop turns (browser-tested) and detect board outcomes (native fixtures).
- [x] Connect both sides' artillery targeting, officer movement, animation and reloads.
- [x] Return manual casualties/results to the campaign, including retreat.
- [ ] Connect battle sounds, fields and remaining presentation.
- [ ] Verify full manual battles and original-game behavior/presentation parity.

`prepare_battle_animation.py` recovers 394 original film loops (4,853 frames)
and 315 troop pose/offset mappings, including artillery. Infantry movement uses 22 four-tick steps;
cavalry uses 18. Browser tests compare the original first/final movement images
with native C and reject extra commands while movement is running.

`battle_attack.c` schedules the source's six shooting waits, five shared
shooting/hit waits and seven hit/death waits. Damage applies after 24 ticks;
a surviving target begins its counter at 72 ticks, using its reduced strength.
Zero damage skips hit animation and starts the counter at 44 ticks. Countering
decrements actions even below zero; a primary kill requests a second, extra-cost
movement into the empty square. Dead units leave the board after the death waits.
Native tests cover these boundaries, commander bonuses, clock wrap, skipped
render frames, RNG order and remaining-action adjustments after Swedish deaths.

The normal seed-3 route now fires the enemy cannon after player movement, then
executes the enemy troop turn and returns to player round two. A separate native renderer fixture explicitly sets
both armies' artillery to zero, follows four enemy turns, performs a player
attack and enemy counter, then lets enemy turns reach a loss. Its screenshots
are named `battle-fixture-*`; they are not evidence of normal campaign progression
or browser attack coverage. Original battle sounds, intro timing, hit masks and
runtime animation comparison remain unverified.

A second native fixture uses commander bonus 10 to force a lethal primary shot.
It verifies that the defender remains visible through its death animation, is
removed after 72 ticks, and the attacker then moves for 88 ticks into its square.
The attacker's actions become -1, while the player order count remains 1.

Enemy artillery now uses ScoreScript 28's priority rules: last-row threats,
blockers near the enemy exit, successive forward rows and strongest infantry/
cavalry with the strict 500-man threshold. Threat penalties use literal square
offsets, including row wrapping. Each cannon has its own reload counter. The
normal seed-3 route consumes two shot RNG draws (24 → 26), deals 210 damage to
the last tied 666-man infantry group, moves enemy troops and returns to player
round two at 27 draws. Native tests also verify two subsequent non-firing reload
turns and a separate five-cannon fixture that retargets after each casualty.
Swedish artillery now completes its officer/target/shot sequence before troop actions.

Equal-property sorting in the original Director runtime is not yet verified.
The current target selector preserves insertion order for ties and selects the
last tied entry; the normal route's tied infantry target therefore remains a
fidelity assumption. The restored priorities, timing and damage tests are source
comparisons, not proof of complete original-game parity.

The seed-4 Eka route now continues beyond the four completed turns: after hiring
Willem Boy and building the level-2 mine/smithy in 1543, it buys 1000 infantry and
1000 artillery for 275 silver and reserves 400 for Danish war. The fifth turn
naturally introduces Bagge and the crossbow contest in 1547; the test earns tier
ten through all fifteen shots, then reaches manual war in 1548. No state is
injected to reach this battle. Manual battle now returns its result to the campaign;
retreat after the first shot and result confirmation finish the fifth turn.
Firefox verifies this route and byte-for-byte framebuffer agreement for deployment,
the officer's first walking frame, standing at the cannon, the sight, the first
shooting frame and the player turn after the officer leaves.

After deployment, the officer takes 26 four-tick steps from (10,336) to (106,404).
The original sight permits any enemy square; own/empty targets and clicks outside
the sight do not fire. The first cannon shot reduces Danish cavalry from 670 to
549, advancing RNG from 208 to 210. After the shot, the officer walks to (237,516)
and disappears; only then do Swedish troop actions reset. Native fixtures test
all three Swedish technology levels, five consecutive guns and a non-firing
reload turn. Film-loop copy ink is now preserved for the original SWE3 ArtShoot
subframes rather than treating every film-loop bitmap as transparent.

City and military controls are additional interactions; the 4/6 new-game count
has not advanced because text rendering and full initialization are still pending.
A first playable browser route is now verified, including earned minigame rewards
and settlement. This does not establish full campaign playability or 1:1 parity.

Manual result integration follows KRIG ScoreScript 45 and SVEA's
`fixBattleResult`: survivor subtraction includes initial division remainders;
INTEGER/FLOAT tags persist through enemy group formation, casualties and fee
calculations. A negative FLOAT HP value contributes INTEGER zero; exactly-zero
FLOAT HP retains its type. Tiny signed rounding remainders from survivor sums
are retained. Results apply once and reset country relations to 2. Losing the
last province exits before troop updates and the prestige draw, as in the source.

Retreat uses KRIG ScoreScript 23 and the original channel-48 hit rectangle. It
is available after deployment during troop selection or cannon aiming, and
blocked during animations. It keeps artillery while accounting for existing
soldier losses; ordinary defeat loses half the Swedish artillery. This differs
from surrender with no troops, which uses the separate turn-based fee.
Native model tests cover fee types, territory acquisition, defensive defeat,
last-province defeat and duplicate-result rejection. Separate scene fixtures
cover retreat timing and a six-shot Swedish artillery victory across reload turns.
These fixtures do not prove that a normal campaign can reach that victory.

The normal seed-4 fifth-turn browser route now retreats after the cannon shot,
returns directly to the campaign result screen, loses four infantry to initial
division and retains all 1000 artillery. Result processing advances RNG to 211;
OK settles 1548 (turn 6) at 574/219/135 resources with 996 infantry, 1000
artillery and 226 RNG calls. Native and Firefox framebuffers agree for the
result and settled province. No campaign state is injected on this route.

The normal seed-3 Tre Rosor route continues beyond player round two by ending
each Swedish turn. Enemy movement, artillery and combat reach a defeat without
retreat. The result applies 1280 infantry/335 cavalry losses, 491 enemy infantry
losses and a 500-silver fee at 40 RNG calls. Result confirmation settles 1528
at 138/108/104 resources, 720 infantry/1665 cavalry and 64 RNG calls. Firefox
verifies the whole route and matches native result/settlement images. This is
a verified losing battle, not evidence of a normal campaign victory or original
runtime parity.
