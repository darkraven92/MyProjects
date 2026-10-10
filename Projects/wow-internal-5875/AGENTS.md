# wow-internal-5875

## Scope

This AGENTS.md applies only to this project, including its corresponding
`Projects/wow-internal-5875` directory in Git worktrees. Primary checkout:

/home/ludvig/Programming/Projects/wow-internal-5875

The primary Git repository root is /home/ludvig/Programming, which contains
unrelated projects. In a worktree, use that worktree's repository root.

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

Determine the actual current branch with `git branch --show-current`; never
assume the historical `codex/wow-internal-continuation` branch is checked out.
Each coordinator may commit/push only its own current worktree branch.
Never stage files from another worktree or project.

Before editing:
- inspect git status
- inspect relevant git history
- inspect the current source

Do not commit:
- build/
- .phase-backups/
- .cache/
- data/state/
- generated Python cache files
- other build artifacts
- runtime captures unless explicitly requested
- credentials or secrets

After source changes:
- inspect git diff
- run relevant tests
- run the full project build

### Automatic task checkpoints

For substantial tasks, after implementation/research is complete:

1. Inspect `git status --short` and run `git diff --check`.
2. Inspect the full diff and all changed/untracked files, including anything
   already staged. Verify every file belongs to the current task. If any change
   is unrelated or ambiguous, STOP and report the conflict without committing;
   do not delete, reset, stash or discard it.
3. Run all validation/build required by this file. For substantial checkpoints,
   run `python3 tools/validate.py --jobs 4` and `cmake --build build`, plus any
   task-specific validation. Keep source, build and runtime results separate.
4. Automatically checkpoint only when validation and diff checks pass, no
   unrelated changes exist, the result is internally consistent, and the actual
   current branch is a `codex/` development branch:
   - Update `docs/AI_HANDOFF.md` with the FINAL task state.
   - Stage ONLY the exact task files using explicit paths.
   - Run `git diff --cached --check` and inspect `git diff --cached --stat`;
     confirm the staged file list matches the reviewed task files.
   - Create a concise descriptive commit.
   - Push only the CURRENT branch to `origin` using an explicit branch refspec.
   - Verify local HEAD equals the live remote branch SHA (for example with
     `git ls-remote`) and `origin/<current-branch>`; inspect final worktree status.
5. Record the commit SHA in the handoff when practical. To avoid an infinite
   self-updating commit loop, the handoff may state that the Git commit
   containing it is the authoritative checkpoint. Report the actual SHA, branch,
   push verification and worktree status in the final response.

If the branch is not a `codex/` development branch, do not automatically commit
or push; report why the checkpoint conditions were not met.

If push fails, inspect the remote branch and fetch if appropriate. Do not force,
merge, rebase or rewrite history to resolve it automatically. Report the exact
conflict/blocker and distinguish a local checkpoint from a verified remote one.

A runtime-pending feature MAY be committed and pushed as an intended checkpoint
when static/source status is accurately documented, runtime PASS is not falsely
claimed, and tests/build pass. Pushing does not establish merge readiness.

Never:
- force-push or rewrite published history
- merge branches automatically
- rebase shared branches automatically
- push to main/master automatically
- stage unrelated files
- delete/reset/stash unrelated work or discard local modifications
- use `git clean`
- commit the excluded artifacts, captures or secrets listed above

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

- actual branch
- starting checkpoint
- current HEAD, if practical, or the containing-commit checkpoint convention
- task / roadmap phase
- concise result
- exact files changed
- validation results
- runtime qualification status
- unresolved blockers
- recommended next task
- Git checkpoint status (local commit versus verified push, or exact blocker)
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
- Follow the automatic task-checkpoint workflow above; do not create recursive
  handoff-only commits merely to record their own SHA. Publication verification
  occurs after committing; report it in the final response when using the
  containing-commit convention, without asserting success before verification.
- Keep the handoff free of passwords, credentials, account secrets, or other sensitive data.
- When parallel subagents are used, summarize their reconciled conclusions rather than copying their full reports.
- At the end of a task, AI_HANDOFF.md must be sufficient for another AI agent to understand the current checkpoint and continue the project without needing the previous Codex conversation.
