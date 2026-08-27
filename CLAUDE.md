# Casso — Claude Code Instructions

Project guidelines, code style, EHM patterns, build rules, and current feature context are in:

**`.github/copilot-instructions.md`**

Read that file at the start of every session.

<!-- SPECKIT START -->
**Active spec: `specs/024-mockingboard-speech`** (IMPLEMENTED, GH #123) — the
Mockingboard's SSI-263 voice chip shipped on branch `024-mockingboard-speech`:
clean-room `Ssi263` core + formant synthesis, the A/C variant split
(`mockingboard` / `mockingboard-c`), C as the default for ][+, //e, //e
Enhanced (deployed via the embedded-config version bump — editing
Resources/*.json alone ships NOTHING), the VIA CA1/CB1 seam, the speech demo
disk, and Hardware-tab product naming. First light achieved audibly.

Still open on the spec: T040/T060 (title regression + acceptance sets — need
acquired period software, local gates). Phase 8 accuracy: **the phoneme ROM
has been read off the visual6502 die shot** — matrix, method, and identified
semantics (mirrored column map, voiced/fricative/closure flags, duration
cluster) in `specs/024-mockingboard-speech/rom-extraction/`; open work is the
formant/amplitude field decode (patent route next; full-res master request
drafted at `specs/024-mockingboard-speech/visual6502-request-draft.md`,
unsent, now for confirmation). The voice still uses the phonetics-literature
formant table until the fields decode; the table is a swappable input.
Related: GH #125 (audio pops are DEVICE-PATH starvation, not synthesis —
proven with the `CASSO_AUDIO_DUMP` tap vs loopback capture; fix is a
dedicated render pump).

**Also open: `specs/020-disk-file-access`** (Draft) — disk file access for the
build loop: assembler binary output, DOS 3.3 / ProDOS file read+write, a
`disk` subcommand, and boot configuration. Next step is `/speckit-clarify` or
`/speckit-plan`.

Four further specs are drafted but NOT started, each to be picked up in its
own session:

- `specs/021-disk-manager` — graphical disk manager, live editing of the mounted
  disk, inspection tools, and CLI parity. Depends on 020.
- `specs/022-disk-image-formats` — 2MG, nibble, compressed/archived images, extra
  filesystems, larger media. Depends on 020/021; large-media parts gated on
  GH #101 / #93.
- `specs/023-ca65-dialect` — ca65's absolute subset, split out of 019. Builds on
  019's dialect mechanism, which has shipped; full compatibility needs a linker
  (GH #58).
- `specs/025-game-compat-patcher` — runtime patch table over live guest RAM,
  defusing title protection checks that fail by design on later machines
  (Choplifter on Enhanced //e and //c, the Karateka //c VBL spin, and a path to
  the wider Broderbund/Gebelli catalog). Tracked by GH #94. Work continues on
  branch `025-game-compat-patcher`; next step is `/speckit-clarify` or
  `/speckit-plan`.

One more is not yet written: **per-slot card configuration**
(GH #124) — a Hardware-tab dropdown selecting any supported card for any slot,
with default slot assignments modeling period-typical install locations. It is
where users will pick between the Mockingboard A and C, but 024 does not depend
on it (024 selects its variant by machine configuration).

**Sequencing.** 021 needs 020's filesystem layer and 022 needs 020/021. 023's
gate was 019's dialect mechanism (023 SC-006 requires that adding ca65 change
nothing in that mechanism), and that shipped in 1.18.0 — 023 can start any
time. 019 and 020 shared only the CassoCli command-line surface, which lives in
`CassoCore` with tests; 020 extends it from there. 025 is independent of both
the disk and assembler tracks and can start any time; it builds on the unmerged
`game-patch-table` proof-of-concept (see its `research/parked-branch.md` for the
gap analysis, including an unresolved scan-cost question).

**020 is partially delivered.** Its User Story 1 (assembler binary output) is
already done and on master: `--raw` and `--dos-bin` live in
`CassoCore/OutputFormats` alongside `WriteFlatImage`, with tests. A session
picking up 020 should plan US2 onward and treat US1 as complete.

Recent specs live under `specs/` (015 printer support, 016 Apple //c, 017
blank-disk creation, and 019 assembler dialects + Merlin are all complete and
shipped).
<!-- SPECKIT END -->
