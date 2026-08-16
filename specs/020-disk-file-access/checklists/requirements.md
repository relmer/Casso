# Specification Quality Checklist: Disk File Access for the Build Loop

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-08-15 (first clarification pass)
**Feature**: [spec.md](../spec.md)

## Content Quality

- [x] No implementation details (languages, frameworks, APIs)
- [x] Focused on user value and business needs
- [x] Written for non-technical stakeholders
- [x] All mandatory sections completed

## Requirement Completeness

- [x] No [NEEDS CLARIFICATION] markers remain
- [x] Requirements are testable and unambiguous
- [x] Success criteria are measurable
- [x] Success criteria are technology-agnostic (no implementation details)
- [x] All acceptance scenarios are defined
- [x] Edge cases are identified
- [x] Scope is clearly bounded
- [x] Dependencies and assumptions identified

## Feature Readiness

- [x] All functional requirements have clear acceptance criteria
- [x] User scenarios cover primary flows
- [x] Feature meets measurable outcomes defined in Success Criteria
- [x] No implementation details leak into specification

## Notes

- Requirements were renumbered flat during the 2026-08-15 clarification pass when
  requirements were inserted after FR-015 and after FR-030. The former
  FR-016..FR-029 are now FR-019..FR-033. Anything referencing the old numbers
  predates that pass.
- **Spec 019 deliberately did NOT renumber flat, and that divergence is correct.**
  019's FR-016..FR-019 are cited externally from a GitHub issue, so renumbering
  would have broken a live reference; it kept an irregular sequence instead.
  Nothing outside this file cites 020's numbers, so flat renumbering was free
  here. Two specs, opposite choices, each right for its own constraint — do not
  "fix" one to match the other.
- The clarification pass resolved an internal contradiction: the former FR-029
  required detecting an image in use by a running emulator, while the Assumptions
  declared that scenario out of scope. The requirement now matches the scope —
  document the hazard, probe only for what the platform can observe, and re-verify
  the file between read and commit (FR-033, FR-034).
- FR-017 records a pre-existing defect rather than new behavior: the sector
  decoder currently discards undecodable sectors as zeros while reporting success,
  on a path the emulator already uses. It is captured under Dependencies and Known
  Defects, is a hard prerequisite for every write path here, and the
  emulator-side exposure is flagged to be tracked as its own defect.
- Three items are deliberately left to planning rather than clarified here: the
  Applesoft tokenizer's coverage boundary (US6, P3), the mechanism by which a
  bootable volume's startup program is set for each filesystem (US4, P2), and the
  accepted spelling of file types on the command line. None of them change the
  architecture or the acceptance criteria above.
- Exit-status vocabulary (FR-030) is shared with the assembler and run
  subcommands and was matched to their existing meanings rather than minted new,
  so a script driving the tool needs no per-subcommand knowledge. This is a
  cross-feature concern; spec 019 was given the same note. FR-031 deliberately
  stops the sharing at 2: codes of 3 and above stay subcommand-scoped, because
  requiring global uniqueness would couple subcommands that are otherwise
  independent — the property that let 019 and 020 be developed in parallel.
- The ProDOS substrate is further along than a filename survey suggests, because
  `ProDosReader` and `ProDosFileWriter` are declared inside the skeleton's header
  rather than in files named for themselves. Read already handles all three
  storage types; write already handles two of them with real bitmap allocation.
  The genuine gaps are tree growth, delete with free-space return, and
  subdirectory traversal — the existing classes were built for bootable-disk
  creation, not as a general filesystem layer, so they are volume-directory-only
  and append-only. **Delete is on the P1 critical path**, not a later nicety,
  because FR-011 requires that writing an existing name replace it.
