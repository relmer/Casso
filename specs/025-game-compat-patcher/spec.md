# Feature Specification: Game Compatibility Patcher

**Feature Branch**: `025-game-compat-patcher`

**Created**: 2026-08-26

**Status**: Draft

**Input**: User description: "Runtime game-compatibility patcher: a signature-to-replacement patch table applied to live guest RAM per frame (idempotent, through the memory bus), defusing title protection checks that fail by design on later machines. Initial scope: Choplifter's monitor-ROM integrity check on Enhanced //e and //c, the Karateka //c VBL shim from the parked game-patch-table branch, and a path to the wider Broderbund/Gebelli catalog documented by 4am's MIT-licensed anti-m (~25 titles, including Space Quarks GH #99). Open decisions the spec must settle: default-on vs opt-in, visible indication when a patch is active, per-title vs per-machine gating, and attribution policy for anti-m-derived rules."

## Context

Several period-authentic Apple II titles refuse to run on machines newer than
the ones they were written for — not because Casso emulates those machines
wrongly, but because the titles themselves reject the newer machine. The
failures are *faithfully reproduced*: real hardware of the same model fails
identically. Three are already diagnosed and documented in project memory:

- **Choplifter** (Broderbund, 1982) reboot-loops on the Enhanced //e and the
  //c. Its copy protection integrity-checks ~257 bytes across twelve monitor-
  ROM regions against a stored image selected by a machine-ID byte; the 1985
  Enhanced ROM differs in 40 of those bytes, so the check can never pass and
  the title deliberately reboots (leaving a single "M" on screen).
- **Karateka** (Broderbund, 1984) hangs on the //c. It polls `$C019` with the
  //e vertical-blank idiom, but on the //c that address is a sticky VBL-
  interrupt latch, so the poll never exits.
- **Space Quarks** (Broderbund, 1981) fails on the //e by the same class of
  copy protection as Choplifter (tracked as GH #99, currently closed
  by-design).

A community tool, 4am's **anti-m** (MIT-licensed), already defeats this whole
protection family by patching the titles in memory at load time. It documents
defuse points for ~25 Broderbund/Gebelli titles. Casso cannot ask a user to
boot a separate cracking disk and swap floppies; the emulator is positioned to
apply the same class of fix invisibly, from outside the machine, so that a user
who mounts one of these titles on an incompatible machine simply sees it run.

This feature is the delivery vehicle for that: a **game compatibility patcher**
— a small table of rules, each a byte-pattern to find in live guest memory and
a replacement to write over it, scanned each frame so a rule applies as soon as
the target code is present and re-applies if the guest reloads over it.

A proof-of-concept already exists on the unmerged `game-patch-table` branch
(a `GamePatcher` class with the Karateka //c rule). This feature takes it from
proof-of-concept to a shipped, governed capability.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - A protected title just runs on an incompatible machine (Priority: P1)

A user mounts Choplifter on the Enhanced //e (or the //c) and starts it. Today
the title reboot-loops forever with an "M" on screen. With this feature, the
title reaches its attract mode and is playable, with no disk swapping, no
separate tool, and no manual configuration.

**Why this priority**: This is the entire point of the feature — turning a
class of "the emulator looks broken" experiences into working software. It is
the minimum that delivers user value, and it exercises the whole mechanism
(detect the target code, apply the patch, keep it applied).

**Independent Test**: Mount Choplifter on the Enhanced //e, start it, and
confirm it advances past the title screen into attract/gameplay instead of
cycling to a near-blank screen with an "M". Independently, mount Karateka on
the //c and confirm it runs rather than hanging after load.

**Acceptance Scenarios**:

1. **Given** Choplifter mounted on the Enhanced //e with compatibility patches
   enabled, **When** the user starts the title, **Then** it advances into
   attract mode / gameplay and does not reboot-loop.
2. **Given** Karateka mounted on the //c with compatibility patches enabled,
   **When** the title finishes loading, **Then** it runs rather than hanging.
3. **Given** the same titles mounted on a machine where they already work
   (Choplifter on the ][+, an unenhanced //e), **When** the user starts them,
   **Then** behavior is unchanged from today — the patch does not alter a title
   that was already running correctly.
4. **Given** any title running on any machine, **When** no rule's pattern is
   present in memory, **Then** the patcher writes nothing and behavior is
   identical to the patcher being absent.

---

### User Story 2 - The user can tell when a title is being patched (Priority: P2)

Casso is built around faithful emulation. When the patcher alters a running
title, the user can discover that a compatibility patch is active for the
current session, so the emulator never silently misrepresents what the machine
is doing.

**Why this priority**: Transparency is the reconciliation between "runs
automatically" and Casso's fidelity ethos. Without it, a patched session looks
like the emulator is behaving unfaithfully with no explanation. It is P2 rather
than P1 because the fix itself (US1) delivers value first; the disclosure makes
that value trustworthy.

**Independent Test**: Start a title that triggers a rule and confirm the UI
exposes, somewhere discoverable, that a compatibility patch is active for this
session and which title/behavior it addresses. Start a title that triggers no
rule and confirm no such indication appears.

**Acceptance Scenarios**:

1. **Given** a title running with an active compatibility patch applied,
   **When** the user looks at the emulator's status surface, **Then** an
   indication that a compatibility patch is active is discoverable.
2. **Given** a title running with no rule applied, **When** the user looks at
   the same surface, **Then** no compatibility-patch indication is shown.
3. **Given** an active patch indication, **When** the user inspects it,
   **Then** it names, in plain language, what the patch addresses (e.g. "known
   protection incompatibility with this machine").

---

### User Story 3 - The user can turn compatibility patching off (Priority: P3)

A user who wants to observe the authentic, unpatched behavior of a title on an
incompatible machine — for study, curiosity, or verification — can disable
compatibility patching, and the title then behaves exactly as unpatched
hardware of that model would (i.e. it fails as designed).

**Why this priority**: The fidelity ethos cuts both ways: the feature must be
defeatable so the emulator can still show the genuine by-design failure. It is
P3 because the default experience (patched, transparent) serves the common
case; the opt-out serves a smaller expert audience.

**Independent Test**: With compatibility patching disabled, mount Choplifter on
the Enhanced //e and confirm it reproduces the original reboot loop; re-enable
and confirm it runs.

**Acceptance Scenarios**:

1. **Given** compatibility patching disabled, **When** the user runs a title
   that would otherwise be patched, **Then** the title exhibits its authentic
   unpatched behavior (the by-design failure).
2. **Given** the user changes the compatibility-patching setting, **When** they
   next start (or restart) the affected title, **Then** the new setting takes
   effect.
3. **Given** the setting exists, **When** the user has never touched it,
   **Then** it sits at the default established by this feature (see Decision
   D1).

---

### User Story 4 - Coverage extends to the wider protection family (Priority: P4)

Beyond the three diagnosed titles, the same mechanism covers additional titles
in the same Broderbund/Gebelli protection family as rules are added, without
re-architecting anything.

**Why this priority**: It establishes that the design is a table, not three
hard-coded fixes — but the initial release is judged on the diagnosed titles,
so broad coverage is the lowest priority and may land incrementally.

**Independent Test**: Add a rule for one further title from the documented
catalog and confirm it is defused by the same table mechanism with no code
change beyond the rule entry and its test.

**Acceptance Scenarios**:

1. **Given** the rule table, **When** a maintainer adds a new rule entry (a
   pattern, a replacement, and its applicability), **Then** no change to the
   scanning/apply mechanism is required for it to take effect.

---

### Edge Cases

- **A patch site is itself checksummed.** Some protections in this family
  checksum their own loader after loading. A rule that fires too early can be
  detected or overwritten. The feature must apply rules late enough, and
  re-apply them, so that the final running code is patched and any transient
  patched state is not present during a self-checksum. (anti-m documents having
  to un-patch its own hooks "because they're checksummed soon".)
- **A pattern matches unintended memory.** A byte pattern could, in principle,
  appear in a title it was not written for, or in transient data. A rule must
  be specific enough that a match is unambiguously the intended target;
  ambiguous patterns are a correctness defect.
- **The guest overwrites a patched region after it was patched** (reload,
  bank-switch, decompress-in-place). The patch must not be a one-shot; it must
  reassert on the next scan.
- **Banked memory.** On the //e///c the same address can resolve to different
  physical RAM depending on the MMU state. A patch must land in the bank the
  guest actually executes from, not a stale copy.
- **Machine switch / reset.** When the user changes machines or resets, rules
  that no longer apply must not linger, and rules that now apply must engage.
- **A patched title is saved/shared.** Patches are applied to live RAM only;
  they must never be written back to the user's disk image.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST maintain a table of compatibility rules, where
  each rule pairs a byte pattern to locate in guest memory with a replacement
  to write at a defined position within the match, plus the conditions under
  which the rule applies.
- **FR-002**: The system MUST scan guest memory for applicable rules
  repeatedly during emulation (not once), so a rule engages as soon as its
  target code is present in memory.
- **FR-003**: Rule application MUST be idempotent — a region already carrying
  the replacement MUST NOT match again, and re-scanning MUST NOT accumulate
  changes or cost beyond a comparison.
- **FR-004**: The system MUST re-apply a rule if the guest overwrites a
  previously patched region, so the running code stays patched.
- **FR-005**: Patches MUST target the memory the guest actually executes,
  honoring the machine's live memory banking; a patch MUST NOT land in a bank
  the guest is not running from.
- **FR-006**: The system MUST NEVER modify the user's mounted disk image or any
  persisted artifact; patches apply to live guest RAM only.
- **FR-007**: Each rule MUST carry its applicability so that it engages only
  where intended. Applicability MUST be expressible by target machine and by
  pattern specificity, such that a rule for one title/machine does not disturb
  another title or a machine where the title already works (see Decision D3).
- **FR-008**: A rule that finds no match MUST have no observable effect on the
  running title.
- **FR-009**: When at least one rule is actively applied in the current
  session, the system MUST make that fact discoverable to the user, in plain
  language, including what the patch addresses (see Decision D2).
- **FR-010**: The user MUST be able to disable compatibility patching, after
  which affected titles exhibit their authentic unpatched behavior. The setting
  MUST have a defined default (see Decision D1).
- **FR-011**: When the user changes machines or resets the machine, rules that
  no longer apply MUST NOT remain in effect and rules that now apply MUST
  engage.
- **FR-012**: The rule table MUST be extensible — adding a rule (pattern +
  replacement + applicability + provenance) MUST NOT require changing the scan/
  apply mechanism.
- **FR-013**: Choplifter MUST run past its title screen into attract/gameplay
  on the Enhanced //e and the //c with patching enabled.
- **FR-014**: Karateka MUST run rather than hang on the //c with patching
  enabled (carried forward from the parked proof-of-concept).
- **FR-015**: Each rule MUST record its provenance — what incompatibility it
  addresses and, where the specific patch choice is taken from an external
  reference, attribution to that reference (see Decision D4).
- **FR-016**: Applying compatibility patches MUST NOT measurably degrade
  emulation performance when idle (no title matched) or when patches are
  disabled.

### Key Entities

- **Compatibility Rule**: The unit of the feature. Represents "when THIS code
  appears on THIS kind of machine, change it THIS way, for THIS reason."
  Attributes: a locating pattern (with a notion of wildcard/don't-care bytes),
  the replacement and where within the match it goes, applicability (which
  machines), a human-readable description of the incompatibility it addresses,
  and provenance/attribution.
- **Rule Table**: The collection of rules the emulator knows about, filtered to
  those applicable to the current machine when a machine is built.
- **Patch Session State**: What is currently applied for the running title —
  the basis for the user-facing "a compatibility patch is active" disclosure
  and for not double-reporting.

## Open Decisions — Resolved by This Spec

The feature request named four decisions the spec must settle. Each is resolved
here with rationale; a reviewer can override any of these before planning.

- **D1 — Default-on vs opt-in → DEFAULT ON, defeasible.** The stated goal is
  that affected titles "run automatically without switching disks or other
  nonsense," which only holds if patching is on by default. The fidelity
  concern is met by D2 (disclosure) and D3-opt-out (US3), not by making the
  common case require configuration. A user who mounts Choplifter on a //e
  should see it run; a user who wants the authentic failure turns patching off.

- **D2 — Visible indication when a patch is active → REQUIRED, session-scoped.**
  Because Casso is a fidelity-first emulator, a patched session MUST advertise
  that it is patched (FR-009). The indication is shown only while a rule is
  actually applied for the running title (not merely because the setting is
  on), and it explains in plain language what it addresses. The exact surface
  (status area, indicator, menu affordance) is a planning/UX detail, not a
  spec-level commitment; the requirement is discoverability + plain-language
  reason.

- **D3 — Per-title vs per-machine gating → BOTH, via the rule, not a global
  mode.** A rule's applicability is the conjunction of a machine predicate
  (which models it targets) AND a sufficiently specific pattern match (which
  code, i.e. effectively which title). There is no separate global "per-title"
  vs "per-machine" switch: the pattern match is the title identification, and
  the machine predicate scopes it to the models where the incompatibility
  exists. This keeps a Choplifter rule from touching Karateka, and a //c rule
  from touching a ][+, without a title-database lookup. Rules that are
  genuinely machine-agnostic simply carry an all-machines predicate.

- **D4 — Attribution policy for anti-m-derived rules → CLEAN-ROOM + CITE; no
  dependency allowlist entry.** anti-m's source (6502 assembly for a different
  runtime) is never vendored, compiled, linked, or shipped, so per the
  project constitution it is not an "Approved Third-Party Dependency" and needs
  no allowlist entry — that allowlist governs shipped material. It is instead
  governed by the clean-room emulation doctrine: reading a reference is allowed
  and encouraged, and where a rule's specific patch choice is taken from
  anti-m, the rule records that attribution (FR-015). anti-m is MIT-licensed,
  which makes it a compatible reference. Rules SHOULD be grounded in Casso's own
  diagnosis (traces) where possible, with anti-m as corroboration/reference.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: With compatibility patching at its default, a user can mount
  Choplifter on the Enhanced //e and reach attract/gameplay with zero manual
  steps beyond mounting and starting the title (no disk swap, no setting
  change).
- **SC-002**: The three initially-scoped titles behave correctly for their
  case: Choplifter runs on the Enhanced //e and //c; Karateka runs on the //c.
- **SC-003**: On a machine where a title already works, enabling the feature
  changes nothing observable about that title (byte-for-byte identical running
  behavior), and on any machine a title with no matching rule is unaffected.
- **SC-004**: A user can determine, from the emulator's own interface, whether
  a compatibility patch is currently active for the running title, and what it
  addresses, without external documentation.
- **SC-005**: A user can disable compatibility patching and observe the
  authentic by-design failure of an affected title, then re-enable and observe
  it run.
- **SC-006**: A maintainer can add coverage for a further title by adding a
  single rule entry (pattern, replacement, applicability, provenance) and a
  test, with no change to the scan/apply mechanism.
- **SC-007**: Emulation performance with the feature present but idle (no rule
  matched) or disabled is indistinguishable from the feature being absent.
- **SC-008**: Every rule that ships carries a plain-language description of the
  incompatibility it addresses and, where applicable, its external attribution.

## Assumptions

- The initial rule set targets the three diagnosed titles (Choplifter, Karateka
  //c, and — as a stretch within US4 — Space Quarks / GH #99), with the wider
  ~25-title Broderbund/Gebelli catalog as follow-on coverage, not a release gate.
- The parked `game-patch-table` branch's `GamePatcher` proof-of-concept is the
  starting point, to be reconciled with the current codebase (the shell was
  substantially refactored after that branch was cut); "reuse or rewrite" is a
  planning decision, not a spec commitment.
- Rules are authored from Casso's own execution traces where possible, using
  anti-m (MIT) as a documented reference for defuse points; no anti-m source is
  vendored.
- The disclosure surface and the settings surface reuse Casso's existing UI
  conventions; this spec commits to the requirements (discoverable, defeasible,
  plain-language), not to a specific control.
- "Faithful by default except where a named, disclosed, defeasible
  compatibility rule applies" is an acceptable stance for the project's fidelity
  principles; if the constitution's fidelity principle is read to forbid any
  default-on alteration, D1 must be revisited.

## Dependencies

- Builds on the emulator's live-memory access through the memory bus (so
  patches honor banking) and on the per-machine build/teardown lifecycle (so
  rules engage and disengage with the machine).
- Supersedes / absorbs the unmerged `game-patch-table` branch.
- Relates to GH #99 (Space Quarks): if that title is covered, the issue moves
  from "closed by-design" to "addressed by compatibility patch."

## Supplementary Material

Implementation-level detail that would clutter this business-focused spec lives
alongside it in this directory, for the planning/implementation session:

- `research/choplifter-diagnosis.md` — the full traced mechanism of
  Choplifter's ROM check, the verified defuse signature, and the machine-ID
  fingerprint bytes.
- `research/antim-reference.md` — how anti-m defeats this family, its documented
  hazards (self-checksumming loaders), its ~25-title catalog, and how to re-read
  its MIT-licensed source.
- `research/parked-branch.md` — what the `game-patch-table` proof-of-concept
  contains and how it must be reconciled with the current codebase.
