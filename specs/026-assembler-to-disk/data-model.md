# Phase 1 Data Model: Assembler-to-Disk Output

**Feature**: `026-assembler-to-disk` | **Date**: 2026-08-29

Three entities from the spec, plus the two existing structures they attach to.
Field names below are the intended shape; wording of comments is the
implementation's business.

## SavePoint (new, `CassoCore/AssemblerTypes.h`)

One complete output produced during an assembly. An assembly produces exactly
one by default and may produce several.

| Field | Type | Meaning |
|---|---|---|
| `bytes` | `std::vector<Byte>` | The object for this save point alone. Per research finding 1, this is the span since the previous save, never a cumulative object. |
| `loadAddress` | `Word` | The address this span's first byte assembles to. FR-024. |
| `hasLoadAddress` | `bool` | False when the span has no origin to report. `$0000` is a legal address, so the value alone cannot say. |
| `name` | `std::string` | What the output is called: from `SAV`, from `DSK`, or from the command line, with the command line winning. Empty when nothing named it. |
| `fileType` | `Byte` | The ProDOS type in effect, from `TYP` or the command line. |
| `hasFileType` | `bool` | False when nothing stated one, so the sink applies the default rather than filing under type `$00`, which is a real DOS 3.3 type. |

**Why a struct rather than parallel vectors**: the fields are meaningless
apart. A load address without its bytes files nothing.

**Why `hasFileType` and not a sentinel**: `$00` is `Dos33Volume::kTypeText`. A
zero meaning "unset" would silently file binaries as text on DOS 3.3. This
follows the has-flag idiom `VolumeTypes.h` already states the case for:
"A caller that reads `loadAddress` without checking `hasLoadAddress` gets a
plausible lie."

### Invariants

- An assembly that emitted bytes has at least one save point. There is no
  "single output" special case; one save point is the ordinary case.
- Save points are in source order.
- No byte appears in two save points. This is the testable form of "the object
  area is empty after a save".
- A failed assembly's save points are not written anywhere, whatever they hold.

## AssemblyResult (existing, extended)

Gains `savePoints` (`std::vector<SavePoint>`), following the pattern
`outputFileName` already established: **reported, not acted on**. `CassoCore`
does not know a disk exists, and this is what keeps that true.

`bytes`, `startAddress` and `endAddress` keep their present meaning — the whole
assembly — so nothing that reads them today changes. A single-save assembly's
one save point mirrors them.

## ImageTarget (new, on `CommandLineOptions`)

What the invocation said about a disk. Lives with the assembler options rather
than under `disk`, because `disk`'s nested group is documented as holding fields
that "mean nothing to any other subcommand", and these mean something to two.

| Field | Type | Meaning |
|---|---|---|
| `imagePath` | `std::string` | The image to write into. Empty means no image target, and every image-only behavior is off. |
| `onDiskName` | `std::string` | What the object is called on the volume. Overrides `DSK` and `SAV`. |
| `typeName` | `std::string` | The type, as typed. Overrides `TYP`. Same accepted spellings as `disk put --type`. |
| `setStartup` | `bool` | FR-021. Refused when `imagePath` is empty (FR-023). |

**`imagePath` empty is the whole switch.** Whether an assembly targets a disk is
one question asked in one place, so the two sinks cannot disagree about it.

## FilePayload (existing, unchanged)

The boundary between this feature and the volume layer. A `SavePoint` becomes a
`FilePayload` and the disk layer takes it from there.

| `SavePoint` | `FilePayload` |
|---|---|
| `bytes` | `bytes` |
| `loadAddress` / `hasLoadAddress` | `loadAddress` / `hasLoadAddress` |
| `fileType`, defaulted | `type` |
| — | `encoding`, always `Verbatim` |

`encoding` is always `Verbatim` and there is no flag for it. An assembler emits
bytes, not text; a character conversion applied to an object would corrupt it.
This is the one place where the assembler path deliberately offers **less** than
`disk put`, which takes `--text` and `--basic`.

## State: composing a write

The transaction, which is inherited rather than built (research finding 5).

```
open image ─────────────► sectors₀        DiskImageSession::OpenImage
                             │            refuses: missing, unreadable,
                             │            no filesystem, held by another program
                             ▼
save point 1 ──► Write ──► sectors₁       IVolume::Write returns a COMPLETE
                             │            new buffer, or fails and returns none
                             ▼
save point N ──► Write ──► sectorsₙ
                             │
              (optional) SetStartupProgram ──► sectorsₙ₊₁
                             │
                             ▼
                        commit once        SaveAndCommit: render, verify the
                                           freshness stamp, replace atomically
```

**Nothing reaches the host until the last step.** A failure anywhere above
abandons the buffers and the image is byte-for-byte as it was — FR-014 and
FR-019 — because `IVolume` never mutates in place. The all-or-nothing property
is structural, not a discipline each step remembers.

**The corollary that matters for testing**: because it is structural, a test can
pass without the feature being correct. See the mutation note in
[plan.md](plan.md#testing-notes).
