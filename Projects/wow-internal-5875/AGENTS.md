# wow-internal-5875

## Scope

This AGENTS.md applies only to:

/home/ludvig/Programming/Projects/wow-internal-5875

The Git repository root is /home/ludvig/Programming, which contains unrelated
projects.

NEVER modify, stage, delete, rename, or commit files outside
Projects/wow-internal-5875 unless explicitly instructed.

In particular, do not touch sibling projects such as:
- pixelbot-wow
- SMB1-recompiled
- svearikedecomplied
- vmangos-core
- wow-bot-cachyos

## Environment

- Linux / CachyOS
- fish shell
- C++20
- CMake
- MinGW/Wine
- WoW Vanilla 1.12.1 build 5875
- Wine/X11
- Project path:
  /home/ludvig/Programming/Projects/wow-internal-5875

Primary build command:

cmake --build build

## Development rules

Inspect the current source and Git history before changing code.

Work from the current implementation. Do not reconstruct files from assumptions
or old phase backups.

Prefer the smallest targeted change that fixes the verified problem.

Do not perform broad rewrites unless explicitly requested.

Preserve existing architecture and controller ownership boundaries.

Do not silently remove or weaken behavior from previous phases.

Do not assume a Vanilla 1.12.1 Lua/API function exists. Verify it from existing
source or runtime evidence before depending on it.

Separate:
- static/source verification
- build verification
- runtime verification

A successful build does not prove runtime behavior.

Never claim a runtime problem is fixed without runtime evidence from WoW.

## Important existing systems

Preserve these systems unless the current task explicitly requires changing them:

- DeathRecoveryController
- GrindModeController
- CombatController
- RecoveryController
- RuntimeRobustnessSupervisor
- WorldMonitor
- ActiveBotAfkSafeguard
- GenericNavMeshPathFollower
- DetourNavigationProvider
- NavigationHazardMemory
- VendorController
- WarriorRotationController

Important existing phases include:

- 14G.4.3 death recovery precision reclaim/liveness
- 14K.1.7 active bot AFK safeguard
- 14L.2.1 startup vendor grace
- 14M.0.2 low-HP finisher / hard-stall ownership

## Navigation

Keep Detour/NavMesh as the authoritative long-distance navigation system.

Do not replace it with hard-coded waypoint movement.

Do not bypass existing navigation ownership without a documented reason.

## Git

Reliability checkpoint branch (verify the actual branch before work):

codex/vendor-afk-long-navigation

Before editing:
- inspect git status
- inspect relevant git history
- inspect the current source

Never force-push.

Do not stage unrelated files.

Do not commit:
- build/
- .phase-backups/
- .cache/
- data/state/
- generated Python cache files

After source changes:
- inspect git diff
- run relevant tests
- run the full project build

Use GitHub for major feature/reliability/roadmap checkpoints, significant
architecture changes, before switching branches/worktrees or a risky large
refactor, or when explicitly requested. Do not push after each small task.

The user authorizes one coherent checkpoint commit and push of the current
branch at a completed major milestone, after full validation, a separate full
project build, and an AI_HANDOFF.md update. Verify local HEAD, the tracking ref
and the remote SHA after pushing. Local safety commits are allowed when useful;
avoid excessive tiny commits. Do not merge automatically.

## Milestone workflow

Work as one agent; do not spawn subagents unless the user changes this instruction.
Complete a substantial roadmap-relevant subsystem milestone before returning,
rather than stopping after a small research finding or helper fix. Prefer
targeted changes within the existing architecture over broad rewrites.

Use targeted component tests during development and broader tests for changes
across subsystems. Run the full validation suite and separate full build before
the major checkpoint, not after every small edit.

If evidence cannot defensibly close a real blocker, document it and continue
with independent productive work. Do not keep polishing bounded reliability
issues or repeating speculative reverse-engineering loops. AI_HANDOFF.md holds
the current release decision and next major roadmap task.

## Runtime debugging

When diagnosing a bug:

1. Find concrete runtime evidence.
2. Trace it to the exact control-flow path in current source.
3. State the root cause and affected ownership/state transition.
4. Make the minimal patch.
5. Add useful telemetry for runtime verification.
6. Build.
7. Specify what runtime evidence would prove or disprove the fix.

Do not guess about causes when logs/source can answer the question.

Batch related behavioral changes before requesting meaningful runtime
qualification. Keep RUNTIME OBSERVED, SOURCE VERIFIED, INFERRED and UNKNOWN
explicit; source/build tests never supply runtime proof.
