# Specification Quality Checklist: Casso Explorer

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-09-10
**Feature**: [spec.md](../spec.md)

## Content Quality

- [x] No implementation details (languages, frameworks, APIs)
- [x] Focused on user value and business needs
- [x] Written for non-technical stakeholders
- [x] All mandatory sections completed

## Requirement Completeness

- [x] No [NEEDS CLARIFICATION] markers remain (2026-11-13: Story 43's three, in FR-274, FR-275 and FR-284, answered by the owner; 2026-11-09: LANceGS kept, language servers cut; 2026-10-14: CassoLink's command-line question resolved; Casso Explorer only)
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

- The graphics-buffer rule in FR-012 quotes addresses and lengths because
  they are the user-facing rule, not an implementation choice.
- Dependence on the preceding feature's command widgets is recorded in
  Assumptions rather than restated as requirements.

- 2026-10-03: User Story 7, FR-053 to FR-060, SC-016, SC-017 and three assumptions added (navigation pane sections, shell folders, network images, the pane's menu, the address bar's drop-down, search). Checked against every item above: all pass, no clarification markers. The Windows search index and File Explorer's query syntax are named because the owner asked for parity with them; they are the behavior asked for, not a design choice.

- 2026-10-04: User Story 8 added (FR-061 to FR-066, SC-018, SC-019): disk image registration, navigation pane options following File Explorer's, Apple file-type icons. Rechecked against every item above; all still pass, with no clarification markers.

- 2026-10-12: User Stories 10-13 (shell previews, File Explorer's menus, archives, drop targets), FR-073 to FR-081 and SC-022 to SC-025 added. Reviewed against every item above: testable, no implementation detail, no clarification markers, scope bounded by File Explorer's own behavior.

- 2026-10-14: User Stories 14-16 (CassoLink: files and disks to and from a real Apple II, bootstrap and the self-updating Apple side, the link inside Casso), FR-083 to FR-098, SC-027 to SC-033, four key entities and seven assumptions added. Emulating the Super Serial Card and the Uthernet II are separate features, merged into this branch when the link's work on each card starts. The hardware and the protocol cited (6551, W5100, ADTPro, TCP, COM port) are what the owner asked to support, not design choices. The one clarification marker, whether the link has a command-line entry, is resolved: Casso Explorer only. Whether ADTPro's Ethernet client must work against the emulated Uthernet II is a question for the Uthernet II feature.

- 2026-10-30: User Story 17 (Full Send), FR-107 to FR-116, SC-038, SC-039 and one key entity added. Reviewed against every item above: testable, no clarification markers. The fingerprint and Disk II references are required behavior, not a design choice.

- 2026-11-08: User Story 18 (every common Apple II file type), FR-156 to FR-171, SC-047 to SC-049 and edge cases added. Reviewed against every item above: testable and bounded by the list in FR-156. The formats, RTF, CSV and PNG come from the owner's request, not design choices. The one clarification marker, whether p-code is disassembled, is resolved: it is.

- 2026-11-08: FR-156 extended to CiderPress II's converters other than the Macintosh formats, and FR-172 (shape tables) and FR-173 (character sets) added. Rechecked against every item above; all pass.

- 2026-11-08: Stories 19 to 21 (large images, resource forks, ShrinkIt and Binary II), FR-174 to FR-189, SC-050 to SC-052 and the Roadmap added. Rechecked against every item above; all pass. The tools and emulators listed are the owner's comparison set and test targets, not design choices.

- 2026-11-09: Stories 22 to 38 (the Roadmap's remaining steps), FR-190 to FR-242, SC-053 to SC-066, edge cases, key entities, two clarifications, and the assumptions they replace added. Every other item above passes. The hardware, emulators and formats listed are the owner's comparison set and test targets, not design choices.
