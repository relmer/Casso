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

- Requirements were renumbered flat during the 2026-08-15 clarification pass, in
  three rounds as requirements were inserted after FR-011, FR-015, and FR-030.
  The spec now runs FR-001..FR-040 with no gaps or letter suffixes. Anything
  citing a number outside that range, or citing a number whose text does not
  match, predates the pass — re-read rather than reconcile. The volume-integrity
  requirements were appended as a final subsection rather than inserted next to
  volume access, deliberately, to stop the renumbering there.
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
- **The two filesystems are not at the same starting point, and planning must not
  assume parity.** ProDOS has both a reader and a writer already (declared inside
  the skeleton's header rather than in files named for themselves, which is why a
  filename survey misses them): read handles all three storage types, write
  handles two of them with real bitmap allocation. DOS 3.3 has neither — there is
  no reader at all, and its writer is a single zero-parameter method that emits
  one hardcoded greeting file, not a general writer with defaults. The
  countervailing point is that DOS 3.3's structures are far simpler — flat
  catalog, track/sector list, no tree, no subdirectories — so building its reader
  from nothing is small work, while ProDOS's remaining work is fiddlier per line.
  Net effort is comparable; it sits in different places. The specific risk is that
  **US3 is P1 and needs both readers, and only one exists.**
- Remaining gaps by filesystem: ProDOS needs tree growth, delete with free-space
  return, and subdirectory traversal; DOS 3.3 needs a reader and a real writer.
  Both existing classes were built for bootable-disk creation rather than as a
  general filesystem layer, so both are append-only and neither deletes.
- **Delete is on the P1 critical path**, not a later nicety, because FR-012
  requires that writing an existing name replace it, and replace cannot be built
  on an append-only writer.
- The volume-integrity pass (FR-037..FR-040) is one mechanism with four
  consumers — delete, listing, allocation, and the pre-commit check on every
  computed write — and should be built once as a first-class pass rather than
  three or four times inside its callers. The fourth consumer is the one that
  changes the feature's character: checking the computed result before committing
  makes the write path self-verifying. That is the structural answer to the class
  of defect where a write path reports success over output it never inspected,
  which is how the denibblization defect recorded below survived.
