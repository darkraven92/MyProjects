# P0.6 unattended vendor and full-bag audit

## Latest automatic episode qualification (2026-10-10)

**INSUFFICIENT EVIDENCE.** The food-only capture in
`runtime-captures/vendor-runtime-2026-10-10/` shows a merchant visit with no
purchase, one cooldown-qualified retry, combat preemption and a stopped service
wait. It does not prove completed maintenance, active-route deadline cancellation
or terminal retry exhaustion. Free slots (22, later 19) are not a full-bag
recovery qualification. No vendor code or evidence boundary changed. See the
[current runtime audit](RUNTIME_CONNECTION_RELIABILITY_AUDIT.md) for raw-line
chronology, source interpretation, hashes and the next required evidence.

## Publication dependency boundary

The independently validated prerequisite `b61e4962779a24a308b5f4388cfc71041b4b7ae9`
publishes only the shared consumable tooltip classifier and its focused test.
P0.6 uses that classifier in the sale script. The P0.6 checkpoint also adds
the existing auto-sell and manual-vendor policies, a bounded durability probe,
and targeted vendor/Grind changes. The dirty service-hub replacement,
quest-maintenance paths, pull-safety changes, and GUI work are not required by
the published Grind vendor path and are intentionally excluded.

## Runtime evidence and root cause

At starting HEAD `9226e4aedf7abbc19db8cb50bbb2c07c9020a5e1`, the newest
`build/wow-internal.log` recorded 48/48 bag slots, 47 items awaiting metadata,
a verified MerchantFrame, `quality=-1` and `classification=unknown` for the
sale pass, zero sales, and `freeSlots=0` afterward. The vendor correctly
failed, but Grind treated every vendor failure as non-terminal and resumed
`Grinding`/`Roaming` with the bag still full.

The sale script had called `GetItemInfo(link)` before `SetBagItem`. An
uncached or otherwise unresolved lookup was immediately treated as a final
no-candidate pass. The log proves missing metadata, not why the client
withheld it. The P0.6 change uses only existing client Lua APIs and does not
assert that tooltip priming or numeric-ID lookup is runtime-qualified yet.
The [WoW 1.12.1 FrameXML ContainerFrame source](https://github.com/MOUZU/Blizzard-WoW-Interface/blob/master/1.12.1/FrameXML/ContainerFrame.lua)
reads quality as the fourth `GetContainerItemInfo` result; it is used only to
corroborate an otherwise known item type, never
as sole evidence that an unknown item is safe to sell.

## Safety and boundedness

- Protected quest items, hearthstone, food/drink, trade goods, and uncommon+
  items retain their fail-closed classification. Unknown or conflicting
  metadata is never sold.
- The merchant retries missing metadata at four-second intervals for at most
  60 seconds; unchanged diagnostics are suppressed. It may finish sooner
  only when an authoritative bag read confirms at least two free slots, or
  no bag-pressure sale was requested.
- A failed vendor trip re-reads bags. A full bag, or unavailable fresh read,
  enters `WaitingForManualVendor`. Grind movement,
  acquisition, and seated recovery remain paused. Ordinary work resumes only
  after the existing verified maintenance/bag-space gate passes.
- A vendor trip marked done is not enough to resume: Grind performs another
  fresh bag read and requires more than the one-slot pressure threshold in
  free space. If that read is unavailable or space is still insufficient, it
  enters the same manual wait. Failed reads invalidate the prior bag snapshot
  while waiting, so stale free-space evidence cannot release the hold.
- No source-backed item class, quality, sale result, or bag space is
  synthesized. No item is dropped or destroyed to create space.

## Qualification

Source/static audit: SOURCE VERIFIED for the fourth bag-slot quality return
and existing `GetItemInfo`/tooltip APIs. Numeric-ID fallback and tooltip
priming are implemented but their effectiveness in this live client remains
RUNTIME PENDING. A normal unattended vendor trip must show resolved metadata,
an observed safe sale, and `freeSlots>=2` before calling this RUNTIME PASS.
If metadata remains unavailable, expected safe outcome is a bounded failure
and `GRIND FULL BAG BLOCK`, not ordinary Grinding.

Static validation: TEST PASS. Full dirty worktree: 98 strict C++ tests,
26 audit Python tests, 13 QuestDB Python tests, SQL fixture, nine Lua
fixtures including vendor metadata 9/9; results
`/tmp/wow-validation-hb5dmot9/results.json`. Fresh isolated staged tree:
49 strict C++ tests, 26 audit Python tests, SQL fixture, four Lua fixtures
including vendor metadata 9/9. Isolated MinGW BUILD PASS;
DIFF CHECK PASS. Live sale/free-slot behavior is RUNTIME PENDING and must
not be inferred from these tests.
