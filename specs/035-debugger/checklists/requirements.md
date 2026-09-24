# Specification Quality Checklist: Debugger

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-09-13
**Updated**: 2026-09-18 (expanded scope: window, memory editing, docking, trace, device panels, source-level debugging, expression breakpoints, GSSquared mode, profiling; design review: call-stack pane, WinDbg mode, step filter, engine-command markers)
**Updated**: 2026-09-21 (fit-and-finish review: code pane follow, fill, scroll, gutter, annotations; registers, stack, call-stack, memory, breakpoints and watch pane interactions; text size; context menus; command bar menus and icons)
**Updated**: 2026-09-23 (Visual Studio tab review: source documents, one per file; document and tool-window tab presentation; focus; shared tab strip)
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

- Command syntax (`300L`, `bpmr C019`, `BP addr IF <expr>`) and the command-line debug mode are user-facing contracts, not implementation details, so they appear in requirements.
- The local channel is specified by behavior (per user, per instance, structured replies and notifications) in the requirements; the named-pipe choice is recorded under Assumptions because external clients depend on it.
- cc65's debug-info format is a file-format contract that other tools consume, so its record names appear in FR-033; how Casso reads or writes it does not.
- Three facts are stated as assumptions to be verified during planning rather than as [NEEDS CLARIFICATION] markers, since each has one reasonable default and the answer changes no story: how real Merlin listings mark `PUT` files, which `line` record keys carry macro nesting in cc65's format, and whether the UI library can move a pane between windows without recreating it.
- The 2026-09-18 clarifications were settled in conversation before this revision and are recorded under Clarifications. `/speckit-clarify` (2026-09-18) settled five more: device panels are read-only; the output format is a setting separate from the input mode; one layout for all machines; three selectable keyboard schemes; Merlin 8/16 listings only, with Merlin 32 as a follow-up issue. The memory-edit scope and the step-over rule were confirmed in conversation and are in the spec as decisions.
- The 2026-09-21 fit-and-finish review added User Stories 16 and 17, FR-071 to FR-099 and SC-021 to SC-026, and amended two requirements that the review contradicted rather than extended: FR-026 (breakpoints are set from a gutter, not by double-click) and FR-068 (the pane shows hybrid with no selector; all three mechanisms stay available by command).
- Four decisions from that review are recorded under Assumptions rather than as [NEEDS CLARIFICATION] markers, since each has a working default and none changes a story: whether call-stack recording may start at boot (default: at attach, pending a measurement of the recorder alone); that backward disassembly may misalign over data; the annotation format (to follow a survey of other disassembly viewers); and whether text size is a font change or a zoom.
- Three review findings were bugs rather than requirement changes and are not in the spec: the command strip's extra top margin, the stack pane appearing to navigate (a mouse release reaching the hidden call-stack list), and the console's full-row selection.
- The 2026-09-23 review against a Visual Studio screenshot replaced the single source pane with source documents (FR-054, FR-057 and FR-059 amended; FR-113), set the document and tool-window tab presentation and focus (FR-114 to FR-116, FR-084 extended), and added SC-027 and SC-028, User Story 6 scenarios 11 and 12 and User Story 7 scenarios 8 to 11. FR-116's requirement that the debugger's tabs be Casso Explorer's tab strip is kept although it names a component: it is the owner's product decision that Casso has one kind of tab, and it changes what the user sees.
