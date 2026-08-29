# Feature Specification: ca65 Assembler Dialect

**Feature Branch**: `023-ca65-dialect`

**Created**: 2026-08-15

**Status**: Draft

**Input**: User description: "Accept ca65 source, the modern Apple II and 6502 cross-development standard, as a dialect profile on top of the dialect mechanism built for Merlin." (Split out of `019-assembler-dialects`; seeded by GitHub issue #92.)

## Overview

ca65 is the modern 6502 cross-assembler standard. A developer writing new Apple II
code today is more likely to be using it than anything else, so it is the dialect
that decides whether Casso is relevant to *new* projects rather than only to
archived ones.

It is also the dialect with a catch, and the catch is why this is its own feature
rather than a tail on `019-assembler-dialects`.

### The linker problem

ca65 does not stand alone. It is half of a toolchain whose other half is `ld65`:
source declares named segments, imports, and exports, and a linker configuration
decides where any of it lands. Casso's assembler emits a single, absolutely
located image with no relocation and no symbol resolution across translation
units.

That leaves three honest options:

1. **Absolute subset only.** Accept ca65 source that never needs the linker.
   Achievable, and genuinely useful for single-file programs and for developers
   who prefer ca65's syntax. But by the assessment recorded in issue #92, *most
   published ca65 projects will not assemble unmodified*, they use `.segment` and
   `.import` as a matter of course.
2. **Add relocatable output and a linker.** Full compatibility, and a much larger
   feature. Relocatable object output is already tracked as GitHub issue #58.
3. **Do nothing.** ca65 users keep using ca65.

The risk with option 1 taken alone is reputational rather than technical: a
developer who reads "Casso supports ca65," points it at a real project, and
watches it fail on the first `.import` concludes the claim was false. That is a
worse outcome than not claiming ca65 support at all.

This specification therefore treats **how the subset is communicated** as a
first-class requirement, not a documentation afterthought, and leaves the
question of whether to pursue option 2 open, tied to issue #58.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Assemble single-file ca65 source (Priority: P1)

A developer writes or brings ca65 source that does not depend on the linker, and
Casso assembles it to the bytes ca65 itself would produce.

**Why this priority**: It is the deliverable. Single-file programs, test cases,
and code written specifically for Casso all live here, and this is the portion of
ca65 that is honestly achievable without a linker.

**Independent Test**: Assemble a corpus of linker-free ca65 source and compare
byte-for-byte against output from ca65 itself.

**Acceptance Scenarios**:

1. **Given** ca65 source using `.setcpu`, `.res`, `.macro`/`.endmacro`, and
   `.repeat`, **When** it is assembled, **Then** the output matches the reference
   bytes exactly.
2. **Given** ca65 source using `@`-prefixed cheap locals, **When** it is assembled,
   **Then** each local resolves within its enclosing named label's scope.
3. **Given** ca65 source using unnamed labels with `:+` and `:-` references,
   **When** it is assembled, **Then** each reference resolves to the correct
   nearest unnamed label in the given direction.
4. **Given** ca65 source using `.proc` and `.scope` with `::` global references,
   **When** it is assembled, **Then** symbol visibility follows ca65's scoping
   rules.
5. **Given** ca65 source that selects a CPU in-source, **When** it is assembled,
   **Then** the selection takes effect without a command-line CPU flag.

---

### User Story 2 - Be told clearly when source exceeds the subset (Priority: P1)

A developer whose ca65 project *does* use the linker gets an immediate, specific
explanation rather than a confusing parse error, naming the construct, and
stating that Casso assembles the absolute subset.

**Why this priority**: This ships with Story 1 rather than after it. The subset
boundary is the single most likely thing a real ca65 user encounters, and how it
is reported determines whether they understand Casso's scope or conclude it is
broken. A precise refusal is the feature, not a consolation.

**Independent Test**: Assemble source using each linker-dependent construct and
confirm every one produces a specific, accurate diagnostic naming it.

**Acceptance Scenarios**:

1. **Given** ca65 source using `.import` or `.export`, **When** it is assembled,
   **Then** it is rejected with a message naming the directive and explaining that
   cross-module symbol resolution requires a linker Casso does not have.
2. **Given** ca65 source using `.segment` with a name that a linker configuration
   would place, **When** it is assembled, **Then** the refusal explains the
   difference between a named segment and Casso's absolute output.
3. **Given** any refusal for exceeding the subset, **When** it is emitted, **Then**
   it points at the file, line, and column, and is distinguishable from a syntax
   error.
4. **Given** source that exceeds the subset in several places, **When** it is
   assembled, **Then** the developer sees the full set rather than only the first,
   so they can judge the scale of the gap in one pass.

---

### User Story 3 - Know the boundary before spending time (Priority: P2)

A developer evaluating Casso can find out what "ca65 support" means here before
they try it and form their own conclusion.

**Why this priority**: This is the reputational safeguard the Overview describes.
It is P2 only because it is worthless without Story 1 working; in terms of whether
the feature succeeds socially, it matters as much as either P1.

**Independent Test**: Confirm the documented boundary matches the implemented one
by assembling every construct the documentation names as supported and as
unsupported.

**Acceptance Scenarios**:

1. **Given** the tool's help output, **When** a developer reads the ca65 entry,
   **Then** it states that the absolute subset is supported and where the boundary
   lies.
2. **Given** the project documentation, **When** a developer looks up ca65 support,
   **Then** the supported and unsupported construct sets are listed explicitly.
3. **Given** the documented boundary, **When** it is compared against actual
   behavior, **Then** they agree exactly, no documented-but-missing and no
   working-but-undocumented constructs.

---

### Edge Cases

- What happens when ca65's unnamed-label `:` meets Merlin's local-label `:` prefix
  and the existing dialect's `:` terminator? The dialect mechanism from 019 MUST
  resolve all three; this feature adds the third meaning and MUST NOT special-case
  it against the other two.
- What happens when source uses `.segment` with a name whose placement happens to
  be unambiguous? It MUST still be refused unless the feature explicitly defines a
  supported interpretation, silently guessing a location is worse than refusing.
- What happens when a cheap local is referenced outside the scope of its enclosing
  named label? It MUST be reported as out of scope rather than treated as an
  undefined symbol.
- What happens when `.repeat` or a macro expands to something that exceeds the
  subset? The diagnostic MUST point at the source construct that caused it, not at
  an expansion the developer never wrote.
- What happens if relocatable output (issue #58) later lands? The subset boundary
  moves. This feature MUST NOT encode the boundary in a way that makes widening it
  a rewrite.

## Requirements *(mandatory)*

### Functional Requirements

#### Dialect

- **FR-001**: The assembler MUST accept ca65 as a dialect selection using the
  mechanism established by `019-assembler-dialects`.
- **FR-002**: The assembler MUST accept ca65's directive vocabulary for the
  constructs within the supported subset.
- **FR-003**: The assembler MUST accept ca65's cheap-local labels and resolve them
  within their enclosing named label's scope.
- **FR-004**: The assembler MUST accept ca65's unnamed labels and resolve forward
  and backward references to the correct nearest match.
- **FR-005**: The assembler MUST accept ca65's scoping constructs and apply its
  symbol visibility rules, including explicit global references.
- **FR-006**: The assembler MUST accept ca65's in-source CPU selection, which MUST
  compose with the dialect mechanism rather than bypassing it.
- **FR-007**: The assembler MUST accept ca65's macro and repetition constructs.

#### Subset boundary

- **FR-008**: The assembler MUST reject constructs that require a linker, naming
  the specific construct.
- **FR-009**: A subset-boundary refusal MUST be distinguishable from a syntax
  error, so a developer can tell "Casso does not do this" from "your source is
  wrong."
- **FR-010**: A subset-boundary refusal MUST report every offending construct in
  the source, not only the first.
- **FR-011**: A refusal caused by macro or repetition expansion MUST report the
  source construct responsible, not the expanded text.
- **FR-012**: The supported subset MUST be defined in one place that both the
  implementation and the documentation derive from, so they cannot disagree.

#### Communication

- **FR-013**: The tool's help output MUST state that ca65 support covers the
  absolute subset and indicate where the boundary lies.
- **FR-014**: Project documentation MUST list the supported and unsupported
  construct sets explicitly.
- **FR-015**: The documented boundary MUST be verified against actual behavior.

### Key Entities

- **ca65 Dialect Profile**: The ca65 instance of the dialect mechanism from 019,
  its directive spellings, label schemes, scoping rules, and macro grammar.
- **Symbol Scope**: A ca65 lexical scope, its visibility rules, and how cheap
  locals and explicit global references resolve against it.
- **Subset Boundary**: The authoritative set of constructs Casso assembles and the
  set it refuses, together with the explanation attached to each refusal.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A corpus of linker-free ca65 source assembles to byte-identical
  output against ca65's own builds.
- **SC-002**: Every construct outside the subset produces a diagnostic naming it;
  no construct fails as an unexplained parse error.
- **SC-003**: A developer evaluating Casso can determine whether their ca65 project
  will assemble without attempting it, using the documentation alone.
- **SC-004**: The documented subset and the implemented subset agree exactly,
  verified by test rather than by review.
- **SC-005**: Merlin and AS65 output remain byte-for-byte identical, verified
  against the corpora from `019-assembler-dialects`.
- **SC-006**: Adding ca65 requires no change to the dialect mechanism itself, if
  the mechanism needs modifying, that is a defect in 019's design, recorded as
  such.

## Assumptions

- The dialect mechanism from `019-assembler-dialects` exists and is capable of more
  than two profiles. This feature is a profile, not an extension of the mechanism;
  SC-006 makes that testable.
- Full ca65 compatibility is out of scope and requires relocatable object output
  plus a linker, tracked as GitHub issue #58. Whether to pursue it is an open
  question this feature deliberately does not answer.
- The reference for correctness is byte-identical output from ca65 itself, not
  agreement with its documentation.
- The absolute subset is expected to exclude most published ca65 projects. That is
  an accepted limitation of this feature, which is why communicating the boundary
  is a requirement rather than a nicety.
- Should issue #58 ever land, the subset boundary widens. The boundary is therefore
  specified as data (FR-012) rather than as scattered checks.
- No new third-party dependency is introduced.
