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

65C02 code can be written in any dialect, so selecting a dialect MUST NOT imply a
CPU, and selecting a CPU MUST work under any dialect. Casso's existing `--cpu`
selection already covers the second axis; this feature adds the first.

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
2. **Given** any dialect selection, **When** the developer also selects a CPU target,
   **Then** both apply independently and neither overrides the other.
3. **Given** no dialect selection, **When** the developer assembles, **Then** the
   dialect used is stated in the tool's output so it is never ambiguous which
   rules were applied.
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
- What happens to source whose meaning depends on column position, when it is
  written with tabs rather than spaces? Merlin's column rules MUST be applied to a
  defined interpretation of tabs, and that interpretation MUST be documented.
- What happens when a dialect's directive spelling collides with an instruction
  mnemonic? The instruction MUST win where the dialect says it wins, and the
  resolution MUST NOT depend on which table happens to be consulted first.
- What happens when a macro defined in one dialect's grammar is invoked with the
  argument syntax of another? It MUST be rejected rather than partially expanded.
- What happens when included files are written in a different dialect than the
  including file? The dialect MUST apply to the whole assembly, and mixed-dialect
  inclusion MUST be reported rather than silently mis-parsed.
- What happens to existing build scripts that invoke Casso's assembler today?
  Their behavior MUST be preserved, or the change MUST be stated explicitly in
  release notes — the project's user-experience principle forbids silent
  command-line changes.

## Requirements *(mandatory)*

### Functional Requirements

#### Dialect selection

- **FR-001**: The assembler MUST accept an explicit dialect selection covering, at
  minimum, the existing AS65 dialect and Merlin.
- **FR-002**: Dialect selection MUST be independent of CPU target selection; each
  MUST be settable without constraining the other.
- **FR-003**: Where a dialect defines its own in-source CPU selection, that
  in-source directive MUST take effect for the remainder of the assembly.
- **FR-004**: The dialect in effect MUST be discoverable from the tool's output so
  a developer can never be uncertain which rules were applied.
- **FR-005**: Each dialect MUST be applied strictly and authentically; the
  assembler MUST NOT accept a lenient union of all dialects.
- **FR-006**: Dialect selection MUST be available to every entry point that
  assembles source, not only the command-line tool.

#### Merlin dialect

- **FR-007**: The assembler MUST accept Merlin's comment conventions, including
  whole-line comments introduced in the first column.
- **FR-008**: The assembler MUST accept Merlin's label rules, including its column
  conventions and its local-label prefix.
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
- **FR-015**: The assembler MUST accept Merlin's CPU-selection directive.

#### Subset boundary

Merlin's normal build is absolute, but it is not the only one: Merlin shipped a
relocating linker, and source can be assembled into a relocatable module with
entry and external symbols resolved later. Casso emits one absolutely located
image and has no linker, so that path is out of scope — and MUST say so rather
than failing as though the source were malformed.

- **FR-016**: The assembler MUST reject Merlin constructs that require
  relocatable output or a linker — relocatable-mode assembly, and entry and
  external symbol declarations — naming the specific construct.
- **FR-017**: A subset-boundary refusal MUST be distinguishable from a syntax
  error, so a developer can tell "Casso does not do this" from "your source is
  wrong."
- **FR-018**: A subset-boundary refusal MUST report every offending construct in
  the source, not only the first, so the scale of the gap is visible in one pass.
- **FR-019**: The supported subset MUST be defined in one place that both the
  implementation and the documentation derive from, so they cannot disagree.

#### Diagnostics and compatibility

- **FR-020**: Diagnostics MUST describe constructs using the vocabulary of the
  active dialect.
- **FR-021**: Diagnostics MUST carry file, line, and column in a machine-parseable
  form.
- **FR-022**: A construct rejected because it belongs to a different dialect MUST
  say which dialect defines it.
- **FR-023**: Existing invocations of the assembler MUST either behave as they do
  today or have their change documented in release notes.
- **FR-024**: Every dialect and its flags MUST be documented in the tool's help
  output, including where the supported Merlin subset ends.

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

### Measurable Outcomes

- **SC-001**: A representative corpus of real, unmodified Merlin source assembles
  to byte-identical output against reference builds.
- **SC-002**: A developer with an existing Merlin project can assemble it in Casso
  without editing any source line.
- **SC-003**: Every rejection of valid Merlin source is a reported defect, not an
  expected limitation — the dialect is complete or the gap is documented.
- **SC-004**: The existing AS65 dialect's output remains byte-for-byte identical
  for every source file in the current test corpus.
- **SC-005**: A developer can determine which dialect and CPU target were used for
  any assembly from the tool's own output alone.
- **SC-006**: Diagnostics for dialect-specific errors identify the correct line and
  column in every case covered by the test corpus.
- **SC-007**: Every construct outside the supported subset produces a diagnostic
  naming it — no such construct fails as an unexplained parse error.
- **SC-008**: A developer can determine whether their Merlin project is within
  the supported subset from the documentation alone, without attempting it.

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
  and run-time packages, so this boundary is real rather than theoretical — it is
  simply not where most published 8-bit Merlin source sits. Supporting it needs a
  linker Casso does not have; that linker would also serve ca65
  (`023-ca65-dialect`) and relocatable object output (GitHub issue #58), so the
  boundary is expected to widen once one exists. It is specified as data (FR-019)
  for that reason.
- Backward compatibility with Casso's current assembler invocation is desirable but
  not paramount; the existing dialect has extremely limited adoption, so a
  documented change is acceptable where it buys a cleaner model.
- The two-pass engine, expression evaluator, and opcode tables are shared across
  dialects; only the front end varies. Dialects differ in how source is read, not
  in what the machine does.
- The reference for correctness is byte-identical output against the original
  assembler, not agreement with any published grammar.
- No new third-party dependency is introduced; dialect support is additional
  parsing, not a vendored grammar.
