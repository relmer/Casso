---
name: "checkpoint"
description: "Before compacting or ending a session: reconcile the spec artifacts against what was actually built, confirm the tree is clean and pushed, and hand back a /compact line carrying what only lives in the conversation."
argument-hint: "Optional: the spec directory, if the branch is not a numbered feature branch"
user-invocable: true
disable-model-invocation: false
---

## Why this exists

`/compact` cannot be configured. It takes freeform focus instructions per
invocation, but there is no settings key, hook or `CLAUDE.md` convention that
makes a summary preserve anything in particular, and a skill cannot shadow it
because it is a core built-in. Verified 2026-09-11 against the Claude Code
docs.

So this does not try to improve the summary. It makes the summary matter less.

**What gets lost across a compaction is almost never conversational.** It is a
decision that was made in chat, built in code, and never written into the spec
-- so the artifacts describe something the tree deliberately stopped doing. A
later session then reads the spec and is confidently wrong, which a better
summary would not have prevented, because the spec was already wrong before the
compaction. Three of these accumulated in one evening on 034: a requirement
still demanding a menu picker that had been removed on purpose, a task naming a
class that was never built, and a task planning a fold-out that had become a
deletion.

The fix is to write things down, then compact -- not to compact more carefully.

## Steps

1. **Find the feature.** Use `$ARGUMENTS` when given; otherwise read
   `.specify/feature.json`, else infer from the branch. If there is no spec
   directory, skip to step 4 -- the git checks still apply.

2. **Reconcile the artifacts against the tree.** Run `/speckit-analyze` and act
   on what it finds. It is far better at this than reading back over the
   conversation, because it compares the artifacts to each other and to the
   code rather than to anyone's memory. In particular:

   - Every task marked `[ ]` whose work is on disk, and every `[X]` whose work
     is not. Checkbox drift is what makes a later session redo finished work.
   - Every file a task names that does not exist. A task that cannot be
     executed is worse than a missing one, because it looks actionable.
   - Requirements that describe something the code deliberately stopped doing.
     These are the expensive ones: the spec outlives the chat and will be
     believed.
   - Work that shipped with no test task done, against the constitution's
     tests-first rule.

   A task that was superseded rather than completed is marked `[X]` with what
   replaced it and why, not deleted. Research records what was true when it was
   surveyed, so a decision it records that was not built gets an **Update**
   note rather than a rewrite.

3. **Write down what only exists in the conversation.** Decisions the user made
   in chat, options weighed and rejected, and anything parked for later. A
   decision goes in the spec's Clarifications as `- Q: ... -> A: ...` with the
   reasoning; parked work becomes a task or an issue. If it was worth
   discussing, it is worth a line somewhere a stranger will find it.

4. **Confirm the work is safe.** The tree is clean, the branch is pushed, the
   build is green and the suite passes. A compaction across uncommitted work is
   how a change gets rebuilt from a summary of itself.

5. **Hand back the `/compact` line.** Print a `/compact` invocation naming the
   few things the next stretch genuinely needs and that steps 2 and 3 could not
   file -- an in-flight diagnosis, a half-reproduced bug, what the user is
   about to look at. Do not run it: compacting is the user's call, and they may
   want to say something first.

   Keep it short. If the list is long, steps 2 and 3 were not finished.

## Done when

- [ ] The artifacts describe what the tree actually does
- [ ] Decisions from the conversation are in the spec, not just in the scroll
- [ ] Clean, pushed, building, green
- [ ] A `/compact` line offered, not run
