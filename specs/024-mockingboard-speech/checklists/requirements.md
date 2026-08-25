# Specification Quality Checklist: Mockingboard C — Sound/Speech

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-08-24
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

**On "no implementation details".** The spec names period hardware — the 6522
VIA, the AY-3-8910 PSG, the SSI-263 — and that is deliberate. For an emulator,
the chips are the *domain*, not the implementation: they are what the product
promises to reproduce and what its correctness is measured against. The line held
here is that no source file, class, function, or language construct appears in
any requirement. Where codebase specifics were needed to explain risk (the loose
slot-page decode, the interrupt path), they are described behaviorally in the
Overview rather than by symbol.

**Zero clarification markers, by choice not by omission.** Three questions were
weighed and resolved into documented assumptions rather than blocking markers:

1. *Which speech chip* — resolved to the SSI-263, since it is what the later
   speech-equipped boards carried and what surviving speech software targets. The
   Votrax SC-01 is explicitly scoped out.
2. *How faithful the voice must sound* — resolved by splitting the question
   across SC-001a/b/c/d rather than answering it with one number. See below.
3. *Whether the sound-only variant survives* — resolved yes, as User Story 3, so
   the two-variant model matches the real product line.

**Why speech quality is four criteria and not one.** The first draft of this
spec set a single bar: 90% of words correctly transcribed by an unfamiliar
listener. That was withdrawn on review as actively wrong for a fidelity project.
The voice chip is an early-1980s formant synthesizer whose authentic output is
rough, so an absolute intelligibility target can be **passed by sounding better
than the hardware** — infidelity, rewarded. It was also unautomatable, requiring
a listener on every regression run in a codebase whose emulation core is
otherwise machine-verified, and it presumed known-correct transcripts that for
game software could only be obtained by listening first, which defeats the
"unfamiliar listener" premise it rested on.

The criteria separate things that were tangled together: what a machine can check
forever (SC-001a behavioral conformance, SC-001b acoustic content), what a person
must judge once (SC-001c intelligibility, restated as a *margin against a
reference* so divergence in either direction fails), and what only real software
exercises (SC-001d, a title's pacing loop actually completing).

**Revised again 2026-08-25, after the datasheet arrived.** The first restructure
assumed a single automated criterion could be checked against "the formant
frequencies named in the datasheet". The datasheet names none — it documents the
register set, all 64 phoneme codes, and the duration and filter formulas, but the
formant parameters are internal to the part and were never published. SC-001a was
therefore unsatisfiable as written while looking perfectly rigorous.

Splitting it into SC-001a (behavioral, checked against published formulas) and
SC-001b (acoustic, checked against targets from the chip family's patent
literature and from acoustic phonetics, refined by measurement) makes both
honest. SC-001a is now *better* grounded than the original, since it tests real
published formulas rather than an assumed table.

**This is the second time a criterion on this feature failed on its source rather
than its substance**, which is the pattern worth carrying forward: the risk in a
hardware-fidelity spec is not writing an unmeasurable criterion, it is writing a
measurable one whose reference data does not exist.

**Open dependencies, now narrowed further.** Development blocks on nothing. The
datasheet supplies SC-001a outright; SC-001b starts from published literature and
improves; a reference recording (needed to refine SC-001b and to judge SC-001c)
is on the critical path but not a prerequisite for starting; and period speech
software gates only SC-001d and User Story 1 sign-off. All sourcing problems, no
specification gaps.

**Items marked incomplete require spec updates before `/speckit-clarify` or
`/speckit-plan`.** None are outstanding.
