# Phase 1 Data Model: Merlin Assembler Dialect

**Feature**: `019-assembler-dialects` | **Date**: 2026-08-15

Entities are given in dependency order — later ones reference earlier ones.
Field names are indicative, not final; the binding contract is the behavior each
field carries.

## DialectId

The identity of a profile. An enum, so a `switch` over it is
exhaustiveness-checked and a new dialect fails the build at every site that must
handle it — the same reasoning that makes `Directive` a token rather than a
string.

| Value | Meaning |
|---|---|
| `As65` | Today's dialect. The inferred default, so existing behavior is unchanged. |
| `Merlin` | This feature's addition. |

`023-ca65-dialect` adds one enumerator here and one profile; SC-009 requires that
be the whole extent of its reach into the mechanism.

**Validation**: A profile must exist for every enumerator. Swept by test, not
asserted in prose.

## DialectProfile

The complete syntactic personality of one assembler. Abstract; one concrete
subclass per dialect.

**Data members** (the declarative majority):

| Field | Purpose | AS65 | Merlin |
|---|---|---|---|
| `id` | Which dialect this is | `As65` | `Merlin` |
| `name` | Spelling used in diagnostics and on the command line | `as65` | `merlin` |
| `lineComment` | Introducer plus the column rule that governs it | `;` anywhere | `*` in column 1; `;` anywhere |
| `labelRule` | How a label is recognized | `name:`, or bare word in column 0 | bare word in column 1; no terminator |
| `fieldModel` | How a line divides into fields | positional, as today | whitespace-run separated: label, opcode, operand, comment |
| `localLabelSigil` | The local-label scheme | none | `:` prefix |
| `variableSigil` | Reassignable in-source symbols | none | `]` prefix |
| `directiveSpellings` | Span of spelling → `Directive` rows | today's table | Merlin's table |
| `stringEncodings` | Span of spelling → `StringEncodingMode` rows | none | `ASC`/`DCI`/`INV`/`FLS`/`STR` |
| `cpuSource` | Where the CPU target comes from | command line | in-source directive only |

**Virtual hooks.** As built, there is **one**: parse a line into a `ParsedLine`.

That is narrower than this document originally specified, and the narrowing was
a finding rather than an oversight. Extracting the AS65 grammar showed that line
parsing is the honest boundary: everything downstream — the two-pass engine, the
expression evaluator, the opcode tables — consumes a `ParsedLine` without caring
which profile produced it. Field segmentation, local-label resolution, variable
symbols, and macro parameter substitution are all *internal* to a profile that
happens to need them, and AS65 needs none of them.

The four originally listed were a design sketch made before the extraction. They
are not missing; they were never required. Shipping them as speculative virtuals
would have made every profile implement behavior most dialects do not have, which
is the opposite of the goal.

**Virtuals are added when a dialect proves it needs one**, not in advance. Merlin
is expected to add some — its field model, operand-internal semicolons, quoted
operands, and first-character conditional all exert pressure the AS65 grammar
never did. That growth is the narrow-seam decision working, not failing. See the
plan's note on why extending the seam is distinct from modifying the engine,
which is what SC-009 actually forbids.

**Validation**: `cpuSource` of `in-source` and a non-empty command-line CPU flag
are mutually exclusive — that pairing is what FR-026 refuses.

**Relationships**: Referenced by `AssemblerOptions`. Owns spans into the
directive and string-encoding tables. Never owns the opcode tables, which are
shared — this is the SC-009 boundary in structural form.

## DirectiveSpelling

One row of a dialect's spelling table: a name and the shared `Directive` token it
resolves to.

Multiple dialects map different spellings to the same token where the *operation*
is identical — `DFB` and `DB` are both `Directive::Byte`. A new token is added
only for an operation the assembler cannot already perform.

**New tokens this feature adds**: reversed-order word, raw hexadecimal data,
loop and its terminator, dummy section and its terminator, CPU selection, and one
encoded-string token that carries a mode.

**Validation**: Within one dialect, a spelling resolves to exactly one token. A
spelling colliding with an instruction mnemonic is resolved by the dialect's own
rule and must not depend on table consultation order — the existing
`FromAmbiguousSpelling` split is the precedent.

## StringEncodingMode

How a string directive converts source characters to bytes. Parameters, not
separate operations, which is why the five Merlin spellings share one token.

| Field | Meaning |
|---|---|
| `highBit` | Set or clear the high bit on each character |
| `inverse` | Emit in the inverse character range |
| `flashing` | Emit in the flashing character range |
| `terminator` | None, high-bit on the final character, or a leading length byte |

**Validation**: `inverse` and `flashing` are mutually exclusive. Delimiter
handling — Merlin infers high-bit or low-bit from which delimiter quotes the
string — is part of the mode's resolution, not a separate field.

## InstructionSetProvider

Holds both opcode tables and answers "which is active now."

| Field | Meaning |
|---|---|
| `base` | 6502 table |
| `extended` | 65C02 table |
| `active` | Which one applies at the current point in the assembly |

**State transition**: `base → extended`, one-way within an assembly, triggered by
the first in-source CPU-selection directive or by the command-line flag where the
dialect allows one. A second in-source directive is not a transition — it is a
subset-boundary refusal.

**Validation**: The active table is recorded **per line** during pass 1 and
replayed during pass 2, never recomputed. Conditional assembly can move where a
directive is reached, so recomputation risks the two passes disagreeing about how
wide an instruction is.

## SubsetBoundaryEntry

One refused construct. The single source of truth FR-019 requires.

| Field | Meaning |
|---|---|
| `spelling` | The construct as written in source |
| `reason` | `NeedsLinker`, `NeedsUnemulatedCpu`, or `OwnedByAnotherFeature` |
| `explanation` | What the developer is told, in the dialect's own vocabulary |
| `widensWith` | What would remove this row — issue #112, a 65816 core, or spec 020 |

Rows at introduction: relocatable-mode assembly, entry symbols, and external
symbols (`NeedsLinker`); the second CPU-selection directive
(`NeedsUnemulatedCpu`); the file-type directive (`OwnedByAnotherFeature`, spec
020); the save-object directive (`OwnedByAnotherFeature`, multi-output
segmentation — explicitly *not* spec 020).

**Relationships**: The help text describing where the subset ends is generated
from this table, so the two cannot disagree. A test sweeps the accessor and
asserts each row produces a refusal.

**Validation**: A refusal is distinguishable from a syntax error (FR-017), and
every offending construct in a source file is reported in one pass (FR-018).

## AssemblyError (extended)

| Field | Status | Meaning |
|---|---|---|
| `lineNumber` | existing | Unchanged |
| `message` | existing | Unchanged |
| `file` | **new**, defaults to empty | The file the diagnostic originated in |
| `column` | **new**, defaults to 0 | Column within that line |

Defaults are what make this additive: existing diagnostics keep compiling and
existing tests keep passing, which is the condition FR-021 sets. An empty `file`
means "the top-level input," which is how the CLI keeps formatting AS65
diagnostics exactly as it does today.

## CorpusEntry

One unit of correctness evidence. Compiled in; never read from disk.

| Field | Meaning |
|---|---|
| `name` | Identifies the entry in test output |
| `sources` | One or more named source texts — more than one for inclusion entries |
| `entryPoint` | Which source is assembled |
| `expectedBytes` | Captured from real Merlin, or hand-authored for the negative class |
| `merlinVersion` | The exact version captured from; edge semantics differ across revisions |
| `class` | `Captured` or `NegativeHandAuthored` |

**Validation**: A `Captured` entry must carry a `merlinVersion`; a
`NegativeHandAuthored` entry must not claim one. The two classes stay distinct so
it is never unclear where an expectation came from.

**Relationships**: Multi-source entries are served through the injected
`FileReader` seam that `AssemblerOptions` already carries.

## AssemblerOptions (extended)

Gains `dialect`, defaulting to `As65`. This is where FR-006 is satisfied: every
entry point that assembles source selects a dialect through the same field, and
the subcommand parsers do nothing but populate it.

Today that means `CassoCli` and `UnitTest`. The `Casso` GUI has **no** assembler
entry point — it references neither `Assembler` nor `AssemblerOptions` — so FR-006
is about the shape of the seam rather than a third caller that exists now. Any
future in-app assembler inherits dialect selection without further work, which is
the point of putting it on the options struct rather than in a parser.
