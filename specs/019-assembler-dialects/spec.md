# Feature Specification: Merlin Assembler Dialect

**Feature Branch**: `019-assembler-dialects`

**Created**: 2026-08-15

**Status**: Draft

**Input**: User description: "Broaden Casso's assembler beyond its single AS65-flavored dialect to also accept Merlin source — what the classic Apple II world writes in — so developers can bring existing source instead of porting it." (Seeded by GitHub issue #92.)

## Overview

Casso's assembler is a faithful AS65 reimplementation, and AS65 is a dialect
almost nobody writing Apple II code today uses. The one that matters most is
**Merlin** — what the classic Apple II world wrote in, and still writes in.

The practical consequence is that a developer with an existing project cannot try
Casso at all. Their source does not assemble, and the first thing Casso asks of a
prospective user is to port a codebase. That is a hard sell for a tool whose pitch
is convenience.

This feature delivers two things: a **general dialect mechanism**, and **Merlin**
as its first new profile. The mechanism is not Merlin overhead — a dialect cannot
be added without it — and once it exists, each further dialect is a profile rather
than surgery on the assembler. `023-ca65-dialect` is the next profile and depends
on the mechanism landing here.

Two axes are involved and they are independent:

- **Dialect** is *syntax*: comment characters, label rules, directive spellings,
  string encodings, macro grammar.
- **CPU target** is *which opcodes are legal*.

65C02 code can be written in any dialect, so a dialect profile MUST NOT imply a
CPU and the mechanism MUST NOT assume a profile has only one CPU available to it.
Casso's existing `--cpu` selection already covers the second axis; this feature
adds the first.

Independent axes do not mean identical controls, though. *Where* a dialect takes
its CPU from is part of that dialect's personality: AS65 has no in-source CPU
directive and so takes it from the command line, while Merlin has one and takes it
from there exclusively. Offering a command-line override under Merlin would let
Casso assemble source real Merlin rejects, which is the opposite of the
authenticity FR-005 demands.

### Relationship to disk file access

This feature and `020-disk-file-access` are the two halves of one migration story,
and neither is sufficient alone. Disk access without dialect support lets a
developer move their source to a modern host, where it then fails to assemble.
Dialect support without disk access lets them assemble source they have no way to
get off their disks or onto a bootable one.

**Stories 1 and 2 here, plus Stories 1 through 3 of 020, are the complete minimum
for a developer with an existing Merlin project.** Everything else in either
feature is a refinement on top of that.

Merlin's `DSK` directive names a file to assemble object code into rather than
buffering it in memory — a workaround for precisely the memory pressure that
drives a developer to a modern host in the first place. Its observable effect is
to name the output, which this feature can honor by itself; writing into a
mounted disk image rather than a host file is the part that wants 020. The
dependency is therefore a refinement, not a prerequisite.

## Clarifications

### Session 2026-08-15

- Q: Where do the reference bytes for SC-001 come from, given unit tests may not
  touch disk and a Merlin disk image cannot be committed? → A: Capture them
  offline from real Merlin 8 running under Casso's own emulation, once per corpus
  entry, and commit the source-plus-bytes pairs as compiled-in fixtures. The
  emulation dependency exists only at capture time and is discharged by
  cross-checking a sample against hand-derived expectations from the manual.
- Q: Does the file-and-column diagnostic requirement mean retrofitting every
  diagnostic in the assembler, or only Merlin's? → A: Neither. The position
  fields are added additively so existing diagnostics keep their shape, and every
  diagnostic this feature emits populates them. Backfilling the AS65 front end is
  a separate mechanical sweep.
- Q: Are Merlin's column rules literal column positions, or a field model? → A: A
  field model. Whitespace runs separate the label, opcode, operand, and comment
  fields; the only significant column is the first, which decides whether a line
  has a label. The fixed columns in Merlin listings are the editor's display
  formatting, not an assembler requirement.
- Q: How is the active dialect reported without breaking scripts that pipe the
  assembler's output? → A: On the diagnostic stream under verbose output, and in
  the listing header when a listing is produced — never unconditionally on
  standard output, which carries the listing when no listing file is named.
- Q: How is the dialect mechanism itself measured, given `023-ca65-dialect` gates
  on it being ready for a third profile? → A: By adding a synthetic, test-only
  third profile in the unit tests and showing it requires no change to the shared
  two-pass engine, expression evaluator, or opcode tables. Recorded as SC-009.
- Q: Does the Merlin dialect offer a command-line CPU flag? FR-002 implied yes;
  issue #92 said no. → A: No. The CPU comes from Merlin's in-source directive
  alone, and passing the flag anyway is refused with a message naming that
  directive. Accepting extended opcodes without it would not be authentic Merlin,
  violating FR-005. FR-002 is rescoped to mean the axes are independent in the
  mechanism, not that every dialect exposes a flag.
- Q: What makes the corpus "representative"? → A: A defined floor, recorded as its
  own subsection: one entry per FR-007..FR-015 construct, one per string-encoding
  directive rather than one for FR-010 as a whole, irregular-spacing entries,
  a multi-file inclusion entry, expression-evaluator entries, and a separate
  hand-authored negative class for refusals and diagnostics.
- Q: What does the second occurrence of Merlin's CPU-selection directive do, given
  it selects the 65802/65816? → A: It becomes a subset-boundary refusal. FR-015 is
  scoped to the first occurrence only, and FR-016 is generalized to "requires a
  linker or a CPU Casso does not emulate" so a future linker or 65816 core widens
  the boundary from one place.
- Q: Are Merlin's object-file, file-type, and save-object directives in scope? →
  A: They are three different cases, not one family. The object-file directive is
  in scope as an output name, with the command line taking precedence over it. The
  file-type directive is deferred to 020, which owns filesystem types. The
  save-object directive is refused as multi-output segmentation needing its own
  decision — explicitly not a 020 dependency.
- Q: How do the implementation and documentation derive the subset boundary from
  one place? → A: One table in code, exposed through a GetAll-style accessor like
  the existing directive and subcommand tables, with the help text generated from
  it so that pair cannot disagree at all. A unit test sweeps the accessor in
  memory. Markdown sync, if wanted, is a repository-level check — a unit test may
  not read files.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Assemble existing Merlin source unmodified (Priority: P1)

A developer with a Merlin project points Casso at their source and it assembles,
producing the same bytes Merlin produces — without editing a single line.

**Why this priority**: This is the entire feature. Merlin is what the classic
Apple II community writes in; supporting it converts "port your code first" into
"try it right now." Every other story here is a refinement of this one.

**Independent Test**: Assemble a corpus of real Merlin source and compare the
output byte-for-byte against the bytes Merlin itself produced for the same input.

**Acceptance Scenarios**:

1. **Given** a Merlin source file using column-position labels, `*` comment lines,
   and the `DFB`/`DA`/`DDB` data directives, **When** the developer assembles it in
   Merlin mode, **Then** the output matches the reference bytes exactly.
2. **Given** Merlin source using the high-bit text directives (`ASC`, `DCI`, `INV`,
   `FLS`, `STR`), **When** it is assembled, **Then** each string is encoded with the
   high-bit and terminator convention that directive specifies.
3. **Given** Merlin source using `]variables`, `:local` labels, `LUP` loops, and
   `DUM`/`DEND` dummy sections, **When** it is assembled, **Then** each construct
   behaves as Merlin defines it.
4. **Given** Merlin source using `MAC`/`EOM` macros with `]1`-style positional
   parameters and `<<<` invocation, **When** it is assembled, **Then** the macros
   expand correctly.
5. **Given** Merlin source that toggles the CPU with `XC`, **When** it is assembled,
   **Then** the extended opcodes become legal from that point forward.
6. **Given** Merlin source using `PUT` or `USE` to pull in another file, **When** it
   is assembled, **Then** the named file is included and resolved relative to the
   including source.

---

### User Story 2 - Choose a dialect explicitly (Priority: P1)

A developer states which dialect their source is written in, and the assembler
applies exactly that dialect's rules — strictly, with no lenient superset that
silently accepts a mixture.

**Why this priority**: The dialects cannot share one grammar unambiguously — the
`:` character alone means three different things across them — so an explicit
selection is a precondition for Story 1 rather than a convenience. It ships
together with it.

**Independent Test**: Assemble one source file under each dialect selection and
confirm that constructs valid in one and invalid in another are accepted and
rejected accordingly.

**Acceptance Scenarios**:

1. **Given** source containing a construct that only Merlin accepts, **When** it is
   assembled in AS65 mode, **Then** it is rejected with a diagnostic naming the
   construct and the active dialect.
2. **Given** a dialect that exposes a command-line CPU flag, **When** the developer
   selects both a dialect and a CPU target, **Then** both apply independently and
   neither overrides the other.
2a. **Given** a dialect that takes its CPU from source instead, **When** the
   developer passes a command-line CPU flag, **Then** it is refused with a message
   naming that dialect's in-source directive — never accepted and ignored.
3. **Given** no dialect selection, **When** the developer assembles, **Then** the
   dialect that was inferred is reported on the diagnostic stream under verbose
   output and in the listing header, and never unconditionally on standard
   output, so it is discoverable without guessing and without disturbing a
   piped listing.
4. **Given** any dialect selection, **When** the developer requests help, **Then**
   the help describes the flags and behavior of that dialect specifically.

---

### User Story 3 - Diagnostics that speak the developer's dialect (Priority: P2)

When source fails to assemble, the error names the construct using the vocabulary
of the dialect the developer selected, and points at the line and column that
caused it.

**Why this priority**: A dialect that assembles correct code but reports errors in
another dialect's terms is exhausting to use. It is P2 because correct assembly is
the precondition — a developer can live with an awkward message but not with wrong
bytes.

**Independent Test**: Introduce a known error into source for each dialect and
confirm the diagnostic names the right construct at the right position.

**Acceptance Scenarios**:

1. **Given** Merlin source where a label starts in the wrong column, **When** it is
   assembled, **Then** the diagnostic explains the column rule rather than
   reporting an unknown symbol.
2. **Given** source using a directive that exists in another dialect but not the
   selected one, **When** it is assembled, **Then** the diagnostic says which
   dialect does support it.
3. **Given** any diagnostic, **When** it is emitted, **Then** it carries the file,
   line, and column in a form an editor can parse to jump to the location.

---

### Edge Cases

- How does the system handle the `:` character, which is a label *terminator* in
  Casso's current dialect and a local-label *prefix* in Merlin? Its meaning MUST be
  resolved by the active dialect, and the same source text MUST be allowed to mean
  different things under different dialects. A third meaning arrives with
  `023-ca65-dialect`, so the resolution MUST be dialect-driven rather than a
  two-way special case.
- What happens when source is assembled in Merlin's relocatable mode, or declares
  entry or external symbols for its relocating linker? Casso emits one absolutely
  located image and has no linker, so this MUST be refused with the construct
  named and the reason given — never as an unknown-directive error, which reads
  as "Merlin support is broken" rather than "your source is relocatable."
- What happens to source written with tabs rather than spaces? Nothing special.
  Merlin's line structure is field-based, so a tab is whitespace and separates
  fields exactly as a space does; no tab-stop expansion is performed, because tab
  stops affect only display. The one column-sensitive rule — a label must begin in
  the first column — is unaffected, since a leading tab is leading whitespace
  either way.
- What happens when a dialect's directive spelling collides with an instruction
  mnemonic? The instruction MUST win where the dialect says it wins, and the
  resolution MUST NOT depend on which table happens to be consulted first.
- What happens when a macro defined in one dialect's grammar is invoked with the
  argument syntax of another? It MUST be rejected rather than partially expanded.
- Is a semicolon *required* to start the comment field, or does Merlin treat
  everything after the operand field as comment regardless of what it begins with?
  Both readings fit the source observed so far, and they differ for a line whose
  fourth field starts with an ordinary word. MUST be settled by capture.
- Does Merlin accept a form of its CPU-selection directive that resets the target
  back to 6502? If it does, that form is in scope and cheap to support; if it does
  not, there is nothing to do. This MUST be settled by capturing the construct
  against real Merlin rather than by reasoning from the manual.
- What happens when included files are written in a different dialect than the
  including file? The dialect MUST apply to the whole assembly. No detection is
  required or possible — an included file does not declare a dialect — so this
  resolves to ordinary parsing: source written in another dialect fails to parse
  under the active one, and the resulting diagnostic MUST name which dialect
  defines the offending construct (FR-022) and which file it came from (FR-025).
  The requirement is that the failure be *explained*, not that mixing be detected
  ahead of time.
- What happens to existing build scripts that invoke Casso's assembler today?
  Their behavior MUST be preserved, or the change MUST be stated explicitly in
  release notes — the project's user-experience principle forbids silent
  command-line changes.

## Requirements *(mandatory)*

### Functional Requirements

Requirements added during clarification carry numbers above FR-025 but sit in the
section they belong to, so they read out of numeric order. Existing numbers are
never reused or shifted: GitHub issue #112 cites FR-016 through FR-019 by number,
and `023-ca65-dialect` gates on this spec, so renumbering would silently break
references outside this file.

#### Dialect selection

- **FR-001**: The assembler MUST accept an explicit dialect selection covering, at
  minimum, the existing AS65 dialect and Merlin.
- **FR-002**: Dialect selection and CPU target selection MUST be independent *in
  the mechanism*: a dialect profile MUST NOT imply a CPU, and the mechanism MUST
  NOT assume a profile has only one CPU available to it. This does not oblige
  every dialect to expose a command-line CPU flag — a dialect whose own source
  defines CPU selection may take the CPU from there exclusively.
- **FR-003**: Where a dialect defines its own in-source CPU selection, that
  in-source directive MUST take effect for the remainder of the assembly.
- **FR-004**: The dialect in effect **and the CPU target in effect** MUST both be
  discoverable without guessing. An explicit dialect selection is self-documenting
  from the invocation itself; where either was inferred rather than stated — the
  dialect by fallback, or the CPU by an in-source directive — the tool MUST report
  it on the diagnostic stream under verbose output, and in the listing header when
  a listing is produced. Neither MUST be emitted unconditionally on standard
  output, which carries the listing when no listing file is named and is therefore
  piped by build scripts. SC-005 is measured entirely against this requirement.
- **FR-005**: Each dialect MUST be applied strictly and authentically; the
  assembler MUST NOT accept a lenient union of all dialects. "Authentic" is
  measured against the dialect the profile names, and for AS65 that dialect is
  what Casso's assembler accepts *today* — this requirement forbids admitting
  another dialect's constructs into AS65, and does not license tightening AS65
  against source it currently accepts. SC-004 holds AS65 output unchanged; this
  holds AS65 acceptance unchanged, which is what FR-023 depends on.
- **FR-006**: Dialect selection MUST be available to every entry point that
  assembles source, not only the command-line tool.
- **FR-026**: The Merlin dialect MUST take its CPU target from Merlin's in-source
  CPU-selection directive alone, and MUST NOT offer a command-line CPU flag.
  Accepting extended opcodes without the in-source directive would violate FR-005,
  because source real Merlin rejects is not authentically Merlin; it would also
  put SC-001 at risk. A command-line CPU flag passed to Merlin MUST be refused
  with a message naming the in-source directive, rather than accepted and
  ignored — a flag that is accepted and does nothing is worse than one that
  errors.

#### Merlin dialect

- **FR-007**: The assembler MUST accept Merlin's comment conventions: a whole-line
  comment introduced by an asterisk in the first column, and a comment occupying
  the trailing comment field. A semicolon does **not** introduce a comment "at any
  position" — **inside the operand field it is data**, and Merlin uses it to
  separate macro arguments. The distinction is the field boundary: a semicolon
  within the whitespace-delimited operand token belongs to the operand, while one
  beginning the field after it starts a comment. This reinforces the field model
  in FR-008 rather than qualifying it.
- **FR-008**: The assembler MUST accept Merlin's label rules and its local-label
  prefix. Merlin's line structure is field-based rather than literal-column-based:
  runs of whitespace separate the label, opcode, operand, and comment fields, and
  the only significant column is the first — a line beginning with whitespace has
  no label. The parser MUST NOT require any field to appear at a specific column.
- **FR-009**: The assembler MUST accept Merlin's data directives for bytes, words,
  and reversed-order words, and its raw hexadecimal data directive.
- **FR-010**: The assembler MUST accept Merlin's string directives and apply each
  one's character encoding — high-bit set or clear, inverse, flashing — and its
  terminator convention.
- **FR-011**: The assembler MUST accept Merlin's variable symbols and its loop
  construct.
- **FR-012**: The assembler MUST accept Merlin's dummy-section construct, which
  assigns addresses without emitting bytes.
- **FR-013**: The assembler MUST accept Merlin's macro definition, invocation, and
  positional parameter syntax.
- **FR-014**: The assembler MUST accept Merlin's file-inclusion directives and
  resolve included files relative to the source that names them.
- **FR-015**: The assembler MUST accept the *first* occurrence of Merlin's
  CPU-selection directive, which enables the 65C02 instruction set for the
  remainder of the assembly. The directive is cumulative in Merlin, and its second
  occurrence selects a CPU Casso does not emulate; FR-016 owns that case, so this
  requirement is deliberately scoped to the first.
- **FR-027**: The assembler MUST honor Merlin's object-file directive as naming
  the assembly's output. Where both the command line and an in-source directive
  name an output, the **command line MUST take precedence**, so a build script can
  override what the source asks for.

#### Subset boundary

Merlin's normal build is absolute, but it is not the only one: Merlin shipped a
relocating linker, and source can be assembled into a relocatable module with
entry and external symbols resolved later. Casso emits one absolutely located
image and has no linker, so that path is out of scope — and MUST say so rather
than failing as though the source were malformed.

The linker is not the only thing outside the boundary, and the boundary is
therefore defined by *why* a construct is refused rather than by a fixed list.
Three reasons appear here: a construct needs a linker, it needs a CPU Casso does
not emulate, or it needs a capability another feature owns. Each MUST be refused
with the construct named and the reason given.

- **FR-016**: The assembler MUST reject Merlin constructs that require a linker or
  a CPU Casso does not emulate, naming the specific construct. This covers
  relocatable-mode assembly and entry and external symbol declarations, which need
  the relocating linker, and the second occurrence of the CPU-selection directive,
  which selects the 65802/65816. The boundary is stated as a general condition
  rather than an enumeration so that a future linker (GitHub issue #112) or a
  65816 core widens it from one place.
- **FR-017**: A subset-boundary refusal MUST be distinguishable from a syntax
  error, so a developer can tell "Casso does not do this" from "your source is
  wrong."
- **FR-031**: Where a refused construct has a workaround, the refusal MUST name
  it. Specifically, a module using relocatable mode and entry symbols but **no
  external symbols** exports without importing, so it assembles absolutely once
  relocatable mode is removed and an origin supplied — the refusal MUST say so.
  A module declaring external symbols has no such workaround, because it
  references symbols defined elsewhere and resolving those needs the linker Casso
  does not have; that refusal MUST say that instead of offering a fix which would
  not work. Naming the construct (FR-016) does not by itself require naming a way
  forward, and the two cases are not interchangeable: the sample project shipped
  on the Merlin distribution disk is the export-only case, so the most likely
  first encounter with this boundary is the one that has a two-line fix.
- **FR-018**: A subset-boundary refusal MUST report every offending construct in
  the source, not only the first, so the scale of the gap is visible in one pass.
- **FR-019**: The supported subset MUST be defined by a single table in code,
  exposed through an enumerating accessor in the manner of the existing directive
  and subcommand tables, so tests sweep the whole boundary rather than a
  hand-picked sample. The tool's help text describing where the subset ends MUST
  be generated from that table, so help and implementation cannot disagree by
  construction rather than by detection. A unit test MUST sweep the accessor and
  assert that every entry produces the expected refusal, entirely in memory.
  Keeping the prose documentation in step is a repository-level check, not a unit
  test — test isolation forbids a unit test reading a file.
- **FR-028**: The assembler MUST refuse Merlin's file-type directive as outside
  the supported subset. It sets the output's filesystem file type, which has no
  meaning without a filesystem that has types; it is deferred to
  `020-disk-file-access`.
- **FR-029**: The assembler MUST refuse Merlin's save-object directive as outside
  the supported subset. It saves the object accumulated so far and continues, so
  it can appear repeatedly and produce *multiple* outputs from one assembly, with
  the object-address directive resetting between them. That is multi-output
  segmentation and needs its own decision. It is **not** a `020-disk-file-access`
  dependency and MUST NOT be documented as one — 020 landing will not make the
  right behavior obvious.

#### Diagnostics and compatibility

- **FR-020**: Diagnostics MUST describe constructs using the vocabulary of the
  active dialect.
- **FR-021**: Diagnostics emitted by this feature MUST carry file, line, and
  column in a machine-parseable form. The position fields MUST be added
  additively, so diagnostics that predate this feature keep their present shape
  and behavior; backfilling those with positions is a separate mechanical change
  and is out of scope here.
- **FR-022**: A construct rejected because it belongs to a different dialect MUST
  say which dialect defines it.
- **FR-023**: Existing invocations of the assembler MUST either behave as they do
  today or have their change documented in release notes.
- **FR-024**: Every dialect and its flags MUST be documented in the tool's help
  output, including where the supported Merlin subset ends.
- **FR-030**: The Merlin entry point MUST use the same exit-code vocabulary as
  the tool's existing subcommands: **0** for a clean run, **1** for a run that
  succeeded but emitted complaints, **2** for a run that produced no output. A
  script driving the tool must not need per-subcommand knowledge of what a given
  number means. This convention is shared with `020-disk-file-access`, which is
  defining exit codes for its own subcommand concurrently.
- **FR-025**: A diagnostic originating inside an included file MUST name that
  file, not the top-level input. The tool reports every diagnostic against the
  top-level input today, which misattributes errors from included source; Merlin's
  file-inclusion directives make multi-file assembly normal rather than
  occasional, so the misattribution MUST be corrected as part of this feature.

### Key Entities

- **Dialect Profile**: The complete syntactic personality of one assembler —
  comment introducers, label and column rules, directive spellings, string
  encodings, macro grammar, and local-label scheme.
- **Directive Vocabulary**: The mapping from a dialect's spelling of a directive to
  the operation it performs. Multiple spellings across dialects may name the same
  operation.
- **String Encoding Mode**: How a dialect's string directive converts source
  characters into bytes — high bit, inverse, flashing, and terminator handling.
- **Instruction Set Selection**: Which opcodes are legal, settable from the command
  line or from an in-source directive where the dialect defines one.

## Success Criteria *(mandatory)*

### Corpus Floor

SC-001 is measured against a defined floor rather than a judgment about what
"representative" means.

**Two sources, because each has the other's blind spot.** The *manual* enumerates
the vocabulary and tells you what to test **for**. The *disk* demonstrates idiom —
how constructs are actually written — and surfaces what the manual
under-documents; the semicolon serving as a macro-argument separator is not
something a manual states clearly, and vendor source showed it in minutes.

The disk's blind spot is symmetric and easy to miss: **it cannot report what the
vocabulary contains that this vendor did not use.** Absence from the disk is not
evidence of absence from the language. Where the manual lists a construct the
disk never exercises, that MUST become a corpus entry rather than an assumption.

**Vendor source is captured, not read.** Beyond what is needed to fix the field
model, the macro grammar, and the string family, the remaining source on the disk
MUST be added as corpus entries rather than studied for spec revisions. A
byte mismatch is a better signal than a reading: it is specific, it is attached to
a test, and it cannot be forgotten. Reading is reserved for constructs genuinely
ambiguous from bytes alone — those that change what the assembler *does* rather
than what it *emits*, which comparison cannot settle.

**Vendor source is used, never committed.** It is the disk author's copyrighted
work, on the same footing as the disk image itself, so it MUST NOT enter the
repository even though assembling it is legitimate and valuable. What may be
recorded is the *result* — for instance that re-assembling a vendor file
reproduces its shipped object byte-for-byte — which is this project's observation
rather than the vendor's text, and is the part that carries the information.
Committed corpus entries are authored here.

The corpus MUST contain, at minimum:

- **One entry per construct named in FR-007 through FR-015.** Breadth beyond the
  floor is opportunistic; the floor itself is not.
- **One entry per string-encoding directive**, not one for FR-010 as a whole.
  FR-010 covers five distinct encodings in a single requirement, and this is the
  highest-risk area in the dialect: a high-bit or terminator bug produces output
  that still looks plausible on inspection.
- **Irregular-spacing entries** — extra spaces, tabs, and mixtures of the two, as
  real source contains. These are what settle the field-based line model
  empirically. If byte-identity holds across them, the parse model is right.
- **At least one multi-file entry** exercising the file-inclusion directives
  through the injected file-reader seam, served from memory.
- **Expression-evaluator entries** covering Merlin's operator set, its precedence,
  and its current-program-counter form. The evaluator is shared with AS65, and
  Merlin's operators and precedence may not match it.
- **A harness that fails when the corpus is missing.** A comparison loop over an
  empty entry table reports success while covering nothing. The corpus MUST
  assert its own presence — a non-zero entry count, and an entry with no expected
  bytes treated as an error rather than a trivially satisfied comparison —
  because a corpus that silently covers nothing is worse than no corpus, it being
  indistinguishable from a passing one.
- **Entries that must discriminate, and are checked to.** Labels, origin,
  literals, and the expression evaluator are shared across dialects, so an entry
  built only from those assembles identically whether the Merlin profile works or
  is never consulted. A corpus can be large, entirely green, and vacuous.

  Every entry is therefore classified. An entry whose stated purpose is a
  **Merlin-specific construct** — the data and hex directives, the string family,
  variable symbols, the loop and dummy constructs, macros and their positional
  parameters, file inclusion, the CPU directive, or a semicolon inside the
  operand field — MUST be verified to **fail under the AS65 profile**. If it
  passes under both, either it is not exercising the construct it claims or the
  profile is not being consulted, and both are defects.

  A **shared-construct** entry is legitimate and is not required to discriminate;
  it is regression cover for the engine, which SC-004 already depends on.
  Recording the class per entry is what makes "passes under both dialects" read
  as a stated property rather than an open question.

  This gives the corpus a second job. Byte-identity alone shows Merlin's output
  is right; discrimination additionally shows the **profile** produced it — which
  is the claim SC-009 rests on, and precisely what a seam shaped by one dialect
  would otherwise satisfy trivially.
- **A separate negative class**, for subset-boundary refusals (FR-016 through
  FR-019) and diagnostic expectations (User Story 3). These expectations are
  **hand-authored, not Merlin-captured**, because Merlin produces no bytes for
  source it rejects. They MUST be kept distinct from the captured class so it is
  never unclear where a given expectation came from.

### Measurable Outcomes

- **SC-001**: A corpus of real, unmodified Merlin source meeting the **Corpus
  Floor** above assembles to output byte-identical to reference bytes **captured
  beforehand** from real Merlin. The comparison is against committed fixtures;
  nothing invokes another assembler at test time.
- **SC-002**: A developer with an existing Merlin project can assemble it in Casso
  without editing any source line.
- **SC-003**: Valid Merlin source is rejected only where the subset-boundary table
  says so. A rejection with no corresponding boundary row is a defect rather than a
  limitation, which makes the table the definition of "expected limitation" and
  gives this criterion something to measure against.
- **SC-004**: The existing AS65 dialect's output remains byte-for-byte identical
  for every source file in the current test corpus.
- **SC-005**: A developer can determine which dialect and CPU target were used for
  any assembly from the tool's own output alone, by the means FR-004 defines —
  the invocation itself where either was stated, and verbose output or the
  listing header where either was inferred.
- **SC-006**: Diagnostics for dialect-specific errors identify the correct line and
  column in every case covered by the test corpus.
- **SC-007**: Every construct outside the supported subset produces a diagnostic
  naming it — no such construct fails as an unexplained parse error.
- **SC-008**: A developer can determine whether their Merlin project is within
  the supported subset from the documentation alone, without attempting it.
- **SC-009**: A third dialect profile can be added without modifying the shared
  two-pass engine, expression evaluator, or opcode tables — demonstrated by a
  synthetic, test-only profile exercised in the unit tests. The claim is verified
  rather than asserted; `023-ca65-dialect` gates on it (023 SC-006).

  Two qualifications, both load-bearing. **Extending the dialect seam is not
  modifying the engine**: adding a virtual to the profile interface is how the
  mechanism absorbs a dialect it has not met, and is expected — what this
  criterion forbids is a dialect reaching into how the assembly *runs*. And the
  demonstration MUST be sequenced **after** the second real dialect lands: run
  against a seam shaped by one dialect, a synthetic profile passes trivially
  because it is written to fit the seam that exists, which proves only that the
  seam supports profiles shaped like the one already there.

  The corpus's discrimination checks are what give this criterion teeth. Matching
  bytes show only that the output is correct; an entry that also **fails under
  the other dialect** shows the *profile* produced those bytes rather than shared
  machinery that would have produced them regardless. Without that, a seam which
  has only ever met one dialect satisfies SC-009 for free.

## Assumptions

- Merlin is the only new dialect in this feature. ca65 was originally scoped here
  and has been split into `023-ca65-dialect`, because the honest ca65 story needs
  a linker and that decision deserves its own scoping rather than riding along as
  a low-priority tail. The dialect mechanism built here is what 023 consumes.
- The dialect mechanism MUST be built for more than two profiles even though only
  two ship here. Hard-coding an AS65-or-Merlin choice would have to be undone by
  the very next dialect.
- Scope is **absolute Merlin**. Merlin shipped a relocating linker, and its
  relocatable mode with entry and external symbols is used for library routines
  and run-time packages, so this boundary is real rather than theoretical.

  It is also closer to hand than assumed. The Apple PI sample project shipped on
  the Merlin 8 v2.47 distribution disk opens with `REL` and declares `ENT`
  symbols — refused constructs, in the disk's own flagship example. Real users
  will therefore meet the boundary early, which raises the stakes on FR-016
  through FR-019: the refusal must read as "your source is relocatable" the first
  time someone assembles the sample that came with their assembler. It also makes
  that project excellent negative-corpus material.

  Supporting it needs a linker Casso does not have; that linker would also serve
  ca65 (`023-ca65-dialect`) and relocatable object output (GitHub issue #58), so
  the boundary is expected to widen once one exists. It is specified as data
  (FR-019) for that reason.
- Backward compatibility with Casso's current assembler invocation is desirable but
  not paramount; the existing dialect has extremely limited adoption, so a
  documented change is acceptable where it buys a cleaner model.
- The two-pass engine, expression evaluator, and opcode tables are shared across
  dialects; only the front end varies. Dialects differ in how source is read, not
  in what the machine does.
- The reference for correctness is byte-identical output against the original
  assembler, not agreement with any published grammar.
- **The oracle is real Merlin 8, not Merlin 32.** Merlin 32 implements Merlin 16+
  syntax; validating against a dialect this feature is not implementing would bake
  the wrong expectations into the corpus. It may cross-check constructs the two
  dialects share; it is not the authority. Reading its source is governed by the
  project's clean-room rule: consult for behavior, never copy.
- **Reference bytes are captured offline, once per corpus entry**, and committed
  as compiled-in literals paired with the source that produced them. Multi-file
  entries are served through the injected file-reader seam. No test reads a file
  or invokes an assembler, so test isolation is satisfied by construction.
- **The Merlin disk image is never committed** — commercial software, the same
  grounds on which `dos33-master.dsk` is gitignored. A developer regenerating the
  corpus supplies their own copy, the way ROM images already work. Only source
  authored here and the bytes it produced are committed; those bytes are this
  project's output, not Merlin's.
- **Capture depends on Casso running Merlin correctly, but only at capture time.**
  Nothing at test time touches Merlin or the emulator. The dependency is
  discharged by cross-checking a sample of entries against hand-derived
  expectations from the manual; disagreement indicates either a corpus error or an
  emulator bug, both worth finding.
- **Capture is a documented, reproducible procedure, not a one-off.** Each entry
  records the exact Merlin version, because edge semantics differ across
  revisions.
- Capture runs Merlin under Casso, which needs source onto the Merlin disk and
  bytes back off it. Neither blocks on `020-disk-file-access`. Bytes come off with
  a small throwaway extractor, because the Merlin disk is a flat DOS-order image
  whose sectors sit at fixed offsets; source goes in by typing or pasting into
  Merlin's own editor. Paste is not trusted — it is **verified per entry** by
  saving the source back to the disk, extracting it, and comparing against what
  was intended, since the guest paste path is reported to garble input. The
  extractor is capture tooling, not a product capability, and is deleted if 020's
  extraction lands first.
- **The committed source for an entry is the copy read back off the disk**, not
  the text that was typed. That is what Merlin assembled, so it is the only text
  guaranteed to correspond to the captured bytes, and it makes each entry
  self-consistent by construction. Merlin's editor may normalize whitespace or
  column positions on save; committing the disk copy makes that harmless instead
  of making every entry fail a byte-exactness check nobody can satisfy.
- No new third-party dependency is introduced; dialect support is additional
  parsing, not a vendored grammar.
