# Feature Specification: Merlin and ca65 Assembler Dialects

**Feature Branch**: `019-assembler-dialects`

**Created**: 2026-08-15

**Status**: Draft

**Input**: User description: "Broaden Casso's assembler beyond its single AS65-flavored dialect to also accept Merlin and ca65 source — the two dominant Apple II assemblers, classic and modern — so developers can bring existing source instead of porting it." (Seeded by GitHub issue #92.)

## Overview

Casso's assembler is a faithful AS65 reimplementation, and AS65 is a dialect
almost nobody writing Apple II code today uses. The two that matter are **Merlin**
— what the classic Apple II world wrote in, and still writes in — and **ca65**,
the modern cross-development standard.

The practical consequence is that a developer with an existing project cannot try
Casso at all. Their source does not assemble, and the first thing Casso asks of a
prospective user is to port a codebase. That is a hard sell for a tool whose pitch
is convenience.

Two axes are involved and they are independent:

- **Dialect** is *syntax*: comment characters, label rules, directive spellings,
  string encodings, macro grammar.
- **CPU target** is *which opcodes are legal*.

65C02 code can be written in Merlin or in ca65, so selecting a dialect MUST NOT
imply a CPU, and selecting a CPU MUST work under any dialect. Casso's existing
`--cpu` selection already covers the second axis; this feature adds the first.

### Relationship to disk file access

This feature and `020-disk-file-access` are the two halves of one migration story,
and neither is sufficient alone. Disk access without dialect support lets a
developer move their source to a modern host, where it then fails to assemble.
Dialect support without disk access lets them assemble source they have no way to
get off their disks or onto a bootable one.

**Stories 1 and 2 here, plus Stories 1 through 3 of 020, are the complete minimum
for a developer with an existing Merlin project.** Everything else in either
feature is a refinement on top of that.

Merlin's `DSK` directive, which assembles straight to a disk image, becomes
implementable only once 020 lands. It is the one construct in this feature with a
hard dependency on the other, and it is deferred rather than blocking.

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

### User Story 4 - Assemble the absolute subset of ca65 source (Priority: P3)

A developer with a ca65 project that does not depend on the linker assembles it in
Casso and gets the expected bytes.

**Why this priority**: ca65 is the modern standard and worth reaching, but real
ca65 projects assume `ld65` — named segments resolved by a linker configuration,
object files, imports and exports. Casso emits one absolutely-located image.
Supporting the absolute subset is achievable and useful; full compatibility means
building a linker, which is a much larger feature and out of scope here.

**Independent Test**: Assemble a ca65 source file that uses no linker features and
compare against reference bytes; confirm that source which *does* require the
linker is rejected with a clear explanation.

**Acceptance Scenarios**:

1. **Given** ca65 source using `.setcpu`, `.res`, `.macro`/`.endmacro`, and
   `.repeat`, **When** it is assembled, **Then** the output matches the reference
   bytes.
2. **Given** ca65 source using `@`-prefixed cheap locals and unnamed `:` labels with
   `:+` / `:-` references, **When** it is assembled, **Then** the references resolve
   as ca65 defines them.
3. **Given** ca65 source using `.import`, `.export`, or a `.segment` that requires a
   linker configuration, **When** it is assembled, **Then** it is rejected with a
   message explaining that the absolute subset is supported and which construct
   exceeded it.

---

### Edge Cases

- How does the system handle the `:` character, which is a label *terminator* in
  Casso's current dialect, a local-label *prefix* in Merlin, and an *unnamed label*
  in ca65? Its meaning MUST be resolved by the active dialect, and the same source
  text MUST be allowed to mean different things under different dialects.
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

#### ca65 dialect

- **FR-016**: The assembler MUST accept the absolute subset of ca65 — source that
  does not require a linker to resolve.
- **FR-017**: The assembler MUST accept ca65's cheap-local and unnamed-label forms.
- **FR-018**: The assembler MUST reject ca65 constructs that require a linker, with
  a diagnostic naming the construct and stating that the absolute subset is
  supported.

#### Diagnostics and compatibility

- **FR-019**: Diagnostics MUST describe constructs using the vocabulary of the
  active dialect.
- **FR-020**: Diagnostics MUST carry file, line, and column in a machine-parseable
  form.
- **FR-021**: A construct rejected because it belongs to a different dialect MUST
  say which dialect defines it.
- **FR-022**: Existing invocations of the assembler MUST either behave as they do
  today or have their change documented in release notes.
- **FR-023**: Every dialect and its flags MUST be documented in the tool's help
  output.

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

## Assumptions

- Merlin is the priority; ca65's absolute subset is worth reaching but is
  explicitly a lesser goal, and full ca65 compatibility — which requires a linker
  and relocatable objects — is out of scope. Relocatable output is tracked
  separately as GitHub issue #58.
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
