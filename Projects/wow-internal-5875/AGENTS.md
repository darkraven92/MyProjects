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

Current development branch:

codex/wow-internal-continuation

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

Do not commit automatically unless explicitly requested.

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

## AI handoff protocol

For every substantial task, maintain:

docs/AI_HANDOFF.md

At the end of each task, update that file with the current project handoff.

The handoff must contain:

- branch
- starting checkpoint
- current HEAD, if committed
- task / roadmap phase
- concise result
- exact files changed
- validation results
- runtime qualification status
- unresolved blockers
- recommended next task
- whether manual runtime evidence is required
- whether the work is safe to commit
- whether the work is safe to merge

Keep these evidence classes explicitly separated when relevant:

- SOURCE VERIFIED
- RUNTIME OBSERVED
- INFERRED
- UNKNOWN

Rules:

- AI_HANDOFF.md is a concise handoff, not a replacement for detailed audit docs.
- Never claim runtime PASS from static tests.
- Never silently upgrade UNKNOWN evidence.
- Record SOURCE GAP explicitly where applicable.
- If the worktree has uncommitted changes, say so.
- Do not commit automatically unless explicitly instructed.
- Keep the handoff free of passwords, credentials, account secrets, or other sensitive data.

When parallel subagents are used, summarize their reconciled conclusions rather
than copying their full reports.

At the end of a task, AI_HANDOFF.md must be sufficient for another AI agent to
understand the current checkpoint and continue the project without needing the
previous Codex conversation.
