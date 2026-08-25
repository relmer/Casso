# Casso — Claude Code Instructions

Project guidelines, code style, EHM patterns, build rules, and current feature context are in:

**`.github/copilot-instructions.md`**

Read that file at the start of every session.

<!-- SPECKIT START -->
**Active spec: `specs/024-mockingboard-speech`** (Planned, GH #123) — the
Mockingboard's SSI-263 voice chip, plus a sound-only (Mockingboard A) /
sound+speech (Mockingboard C) variant split with the C as the default for the
][+, //e, and //e Enhanced. Plan: `specs/024-mockingboard-speech/plan.md`, with
`research.md`, `data-model.md`, `contracts/`, and `quickstart.md` beside it. Next
step is `/speckit-tasks`.

Two Phase 0 findings a later session should not have to rediscover: the
`Via6522` control-line seam **does not exist** (PCR is stored but inert, and
nothing can drive CA1/CB1 from outside), and the voice chip's parameter tables
must come from the SSI-263 datasheet — which is the feature's highest-value
input and is **not yet in hand**. See `research.md` F1 and PENDING-1.

**Also open: `specs/020-disk-file-access`** (Draft) — disk file access for the
build loop: assembler binary output, DOS 3.3 / ProDOS file read+write, a
`disk` subcommand, and boot configuration. Next step is `/speckit-clarify` or
`/speckit-plan`.

Three further specs are drafted but NOT started, each to be picked up in its
own session:

- `specs/021-disk-manager` — graphical disk manager, live editing of the mounted
  disk, inspection tools, and CLI parity. Depends on 020.
- `specs/022-disk-image-formats` — 2MG, nibble, compressed/archived images, extra
  filesystems, larger media. Depends on 020/021; large-media parts gated on
  GH #101 / #93.
- `specs/023-ca65-dialect` — ca65's absolute subset, split out of 019. Builds on
  019's dialect mechanism, which has shipped; full compatibility needs a linker
  (GH #58).

A fourth is not yet written: **per-slot card configuration**
(GH #124) — a Hardware-tab dropdown selecting any supported card for any slot,
with default slot assignments modeling period-typical install locations. It is
where users will pick between the Mockingboard A and C, but 024 does not depend
on it (024 selects its variant by machine configuration).

**Sequencing.** 021 needs 020's filesystem layer and 022 needs 020/021. 023's
gate was 019's dialect mechanism (023 SC-006 requires that adding ca65 change
nothing in that mechanism), and that shipped in 1.18.0 — 023 can start any
time. 019 and 020 shared only the CassoCli command-line surface, which lives in
`CassoCore` with tests; 020 extends it from there.

**020 is partially delivered.** Its User Story 1 (assembler binary output) is
already done and on master: `--raw` and `--dos-bin` live in
`CassoCore/OutputFormats` alongside `WriteFlatImage`, with tests. A session
picking up 020 should plan US2 onward and treat US1 as complete.

Recent specs live under `specs/` (015 printer support, 016 Apple //c, 017
blank-disk creation, and 019 assembler dialects + Merlin are all complete and
shipped).
<!-- SPECKIT END -->
