# Tasks: Disk File Access for the Build Loop

## State of play

Written for whoever picks this up next, including a later session of me after a
context loss. Current as of the last commit on `020-disk-file-access`.

**135 of 136 tasks are done, and every user story is delivered.** The one that
is not is T135, a disassembler, which the reverse loop stops one step short of
([#121](https://github.com/relmer/Casso/issues/121)).

T134, the command for direct boot, was deferred and then done: the reason for
deferring it was that it "needs its own container handling", and that turned out
to be false the moment it was checked. The builder already returns the same
sector buffer the other one does.

**Every user story is delivered.** Phases 17 to 19 at the end of this file were added after the original
plan and cover what running the feature turned up: nothing created the disk the
worked example wrote to, and the executable held 3,639 lines no test could reach.
Both were found by using the tool rather than by reading it.

**Done and on the branch.** Phase 2 (foundational, T002–T013), Phase 3
(US3 — read, T014–T022), **both** Phase 4 chains — T023 + T024 (DOS 3.3 write
and delete) and **T025 + T026** (ProDOS write with tree growth, and delete) —
T027 + T028 (replace on both filesystems, and the pre-commit self-check), T029
(the bit-stream write path, its four-format matrix, and whole-operation
refusal), T030 + T031 (the file-level commit policy and its wiring into the
runner), **T032** — `put` and `delete`, which is what makes any of it
reachable by a user — and now **T033**, the real-CPU gate. That is the whole P1
extraction story — both filesystem
readers, the decode report, the integrity pass, the `disk` subcommand, the CLI
edge — plus **T034**'s uniform failure-mode gate over the whole of it, plus
placement, replacement and removal on both filesystems, each
verifying its own output, an edited volume rendered back into any of the four
containers it came from, a crash-safe commit that leaves the target
byte-identical after every failure it can be made to suffer, a command line
that reaches all of it, and a real 6502 cataloging and loading what that command
line placed. Suite was **3223 Debug / 3220 Release** green at the end of that
phase, and is 3242 / 3239 now that Phase 5 has landed; the 3-test
delta is the pre-existing GH #113, not this work. (It was 3220 / 3217 before
T033 added 3, 3212 / 3209 before
T034 added 8, 3191 / 3188 before
T032 added 21 tests, 3163 / 3160 before T030/T031 added 28, 3152 / 3149
before T029 added 11, 3134 / 3131 before T027/T028 added 18, 3109 / 3106 before
T025/T026 added 25, and 3088 / 3085 before T023/T024 added 22 and removed the
now-obsolete `Write_IsNotYetImplemented_AndSaysSoRatherThanSucceeding`.)

**T029 found a live defect, which is what it was ranked first for.** The `.po`
reorder shipped in Phase 3 was wrong and had been silent ever since — see the
divergences block. Read that before assuming the sector-order question is
settled anywhere else in the tree.

**Phase 5 is done, and the suite is 3242 Debug / 3239 Release.** T036–T039
shipped together: DOS 3.3's greeting field, ProDOS's directory reorder, the
`boot` verb, and a real-CPU gate that boots each image and **types nothing at
all**. The two mechanisms were kept apart on purpose and the difference is not
cosmetic — one writes thirty bytes of high ASCII into DOS's own code on track 1,
the other writes no name anywhere and swaps two 39-byte directory records. (The
counts were 3223 / 3220 before this phase added 19 and removed the
now-pointless `UnbuiltVerb_ReportsFailureRatherThanDoingNothingQuietly`.)

**The finding a session picking up Phase 6 should carry: a booting DOS 3.3 RUNs
its greeting.** Naming a BINARY in that field sets the name perfectly and the
guest never executes it — measured on the stock master, and the disk shows no
complaint anybody would connect to the setting. The name field is all this
feature patches, so the command stays RUN. `disk boot` therefore succeeds with
the complaints status and a sentence saying so, rather than refusing: a disk
whose boot command has been patched by hand is a real thing. **This is what US5
(direct boot) is actually for** on the assembler-output path — a developer's
binary is not a DOS 3.3 greeting, and no amount of naming makes it one.

**Phase 6 is done, and the suite is 3261 Debug / 3258 Release.** T040–T042 shipped
together: `DirectBootBuilder`, its refusals, and a real-CPU gate that boots a disk
with no filesystem on it at all. **R-010's deferred question is answered: the
loader's capacity is 183 sectors — 46,848 bytes — and it is a MEMORY limit rather
than a media one.** The payload has to live between $0900 and $BFFF, so the
capacity is `$C000 - loadAddress` and 183 is its value at the bottom of that
window; the disk still has 544 free sectors at that point and never binds. The
lower edge is not a preference: the boot ROM's read loop terminates against the
byte at **$0800** and jumps to **$0801**, so page $08 belongs to the loader for as
long as anything is being read, and $0300–$03FF is the ROM's own decode table.
(The counts were 3242 / 3239 before this phase added 19.)

**The finding a session picking up SC-007 should carry: the 25% bar cannot be met
against the whole boot, by any disk, including an empty one.** Measured on this
machine, deterministically: the controller ROM spends **1,647,741 emulated cycles**
recalibrating the head and reading track 0 sector 0 before either disk's first
byte executes — about a second and a half of a real machine, and **more than a
quarter of the 6,366,505 cycles** a DOS 3.3 boot spends reaching a BRUN'd binary.
The gate therefore applies SC-007's quarter to what the two DISKS spend, measuring
that constant on both images and requiring it to be identical before subtracting
it. Past the hand-off the direct-boot image reached the payload in **209,147**
cycles against DOS 3.3's **4,718,764** — **4.4%**, a factor of twenty-two. Whole
boots are 1,856,888 against 6,366,505, which is 29.2%. See the divergences block.

**SC-007 IS NOW AMENDED IN `spec.md` RATHER THAN ONLY REINTERPRETED HERE.** The
criterion states the disk-contribution form the gate actually measures — each
boot's whole cost less the fixed entry cost, which must be measured on both
images and be the same number on both — with one paragraph recording why: the
ROM constant is common to both images and is not something this feature can
influence, so measuring it in tells you about the controller rather than about
the feature. The gate is unchanged and is not weakened; it still asserts the
arithmetic that makes the whole-boot form unreachable, so the finding cannot
outlive its evidence.

**Phase 7 is done, and the suite is 3297 Debug / 3294 Release.** T043–T046 shipped
together: `CassoCore/ApplesoftTokenizer.h/.cpp`, its refusals, `--basic` wired
through `put` and `get`, and a real-CPU gate that boots a disk carrying a placed
listing and LISTs it. (The counts were 3261 / 3258 before this phase added 36.)

**The oracle for the tokenizer is APPLESOFT, not our own detokenizer, and every
rule below was measured rather than looked up.** Lines were typed into a booted
DOS 3.3 master and the bytes read back out of the memory between TXTTAB ($67)
and VARTAB ($69). Three of the four answers are not the obvious ones:

- **Spaces outside a string, a REM or a DATA payload are DROPPED — including in
  the middle of a keyword.** `PR INT` is stored as the single PRINT token, and
  `20 A = 1` is three stored bytes rather than five. The spacing a listing shows
  is put there by LIST, which writes a space before AND after every token.
- **A REM's text and a DATA statement's payload are stored VERBATIM** from the
  character after the keyword, spaces included. DATA ends at the first colon
  outside a quoted string; REM runs to the end of the line and a colon does not
  end it.
- **`?` is the PRINT token**, and it is the one abbreviation that does not come
  back.
- **`AT` is special-cased against the two keywords it collides with.** `ATN` is
  one token and `A TO` comes apart into the letter A and TO — both confirmed on
  the machine, and both necessary precisely because the stored form has no
  spaces to disambiguate with. `TOTAL` is NOT special-cased and tokenizes as TO
  followed by TAL, which is Applesoft's own behavior and is reproduced
  deliberately.

**THE ROUND-TRIP QUESTION IS SETTLED, AND THE ANSWER IS BETTER THAN THE TASK
FEARED — because of one decision in the DETOKENIZER.** `tokens -> listing ->
tokens` is **exact** for every program Applesoft itself can have saved, verified
byte-for-byte against the stock master's own 419-byte HELLO as well as against
hand-authored cases. What buys it is that the detokenizer writes a space *before*
every token but *after* every token except REM and DATA. A space in normal
position is dropped on the way back and costs nothing; a space written after REM
or DATA is swallowed into the payload and the listing grows one space per trip.
Emitting LIST's spacing exactly — which was the obvious choice and is what the
guest prints — would have made the round trip lossy and cumulative. The
`RoundTrip_TheNormalizedFormIsStable_SoASecondTripChangesNothing` case exists
because a one-trip comparison cannot tell a stable normalization from a
cumulative one.

The other direction, `listing -> tokens -> listing`, is **deliberately not the
identity**, and the losses are exactly the normalizations Applesoft performs when
a line is typed in: spacing outside the three verbatim contexts is dropped, `?`
becomes PRINT, lowercase outside those contexts is upper-cased, and lines are
ordered by number. **`ApplesoftTokenizer::kRoundTripHelpText` says all of this in
the tool's own help output**, beside the code, so the claim and the capability
cannot drift — the same arrangement `kInUseHelpText` already uses.

Two narrower things worth carrying. **A DATA payload's trailing space is
significant and is the last character on its line**, so an editor that strips
trailing whitespace on save changes the program; there is a case pinning it.
And **the detokenizer refuses rather than guessing** — an undefined token byte, a
token byte inside a string or a REM or a DATA payload, a link that disagrees with
the layout, bytes past the null link. Everything it accepts round-trips exactly,
which is what makes the guarantee unconditional instead of hedged.

**One thing the guest could not settle, recorded rather than solved.** Lowercase
typed at the guest's keyboard arrives at Applesoft already upper-cased — string
literals, REM text and DATA payloads included — so the machine cannot say whether
Applesoft or the input path does it, and it cannot be used as an oracle for case.
The shipped rule is chosen rather than measured: **case is preserved inside
strings, REM text and DATA payloads, and upper-cased everywhere else**, on the
ground that those three contexts hold the user's data while everywhere else holds
code Applesoft would reject in lowercase. The guest-anchored oracle therefore
uses an all-uppercase listing, and case is pinned by unit tests instead.

**Next.** Every phase is complete, **T035 included**. It was done by hand,
because crash safety cannot be unit-tested, and it needed an interruption switch
inside `Win32DiskFileIo` before it could be done at all — killing the process
from outside never once reached the window, over seventy attempts. The original
image came out byte-identical and bootable after a hard abort in the pre-replace
window; the **temporary survives, and no later run reclaims it**, so read the
task's "no temporary remains" as not satisfied. See T035 for the measurements and
for why that is a real if minor defect rather than an expected cost.

**The switch is no longer armable by accident, and T035 was re-run through the
new gating.** It was first gated on `#ifdef _DEBUG` and aimed by an environment
variable, which kept it out of shipping builds and did nothing about the real
hazard: an environment variable stays set. One `setx` and every Debug `put` on
that machine aborts, with nothing in the code wrong. Arming now takes a define
no project configuration sets, so a stock Debug build contains no hook code and
none of the strings that name it — measured in the binary, the same way Release
was. `quickstart.md` §US2 carries the build command and the byte-level check.

Phase 4's write work is the work here where a bug **destroys data** rather than
failing loudly. Everything through Phase 3 was reads: wrong output was the worst
case. From T023 on, the worst case is a
user's disk image. The two ranked risks that were still open — the `.po` reorder
and the file-level commit — are now both closed; what remains is reachability,
not mechanism.

**Order within Phase 4.** T023 → T024 and T025 → T026 were independent chains
(DOS 3.3 and ProDOS), joining at T028. Delete ships with write on both because
replace depends on it. Both chains, T027, T028, T029, T030, T031 and T032 are
done, and so are **T034**'s failure matrix against the fake, **T033**'s
real-CPU gate and **T035**'s manual interrupted-write pass, which is by hand
because crash safety cannot be unit-tested. The write path a user actually travels is `RunPut` /
`RunDelete` → `OpenImage` → `IVolume::Write` or `IVolume::Delete` →
`SaveAndCommit` → `VolumeImage::Save` → `CommitImage`, all in
`DiskCommandRunner`, and T034 exercised it end to end rather than building
anything new.

Three things earned during Phase 3 that Phase 4 depends on, so do not treat them
as background:

- `SectorDecodeReport` is what makes writing safe at all. `Denibblize` used to
  zero-fill undecodable sectors and return `S_OK` (GH #115); the three-argument
  form now **fails** on data loss, and `TrackWritability` refuses any track that
  is not provably standard. A write path that reaches around either reintroduces
  the defect it was built to prevent.
- `VolumeIntegrityReport` carries both slices — claims by owner and claimants by
  unit. T024/T027 (delete with free-space return) need the second, which is why
  it exists; do not simplify it back to a count.
- FR-012 is non-negotiable and **now implemented at both levels**: the volume
  layer never writes through to its input, and `CommitPlan` +
  `DiskCommandRunner::CommitImage` (T030/T031) carry the file level. A failed
  operation leaves the image byte-for-byte unchanged and no temporary behind.
  T032 must route `put`/`delete` through `CommitImage` rather than calling
  `IDiskFileIo::WriteAllBytes` on the image, which would defeat all of it.

### Where Phase 4 is riskiest

One view, formed while building the read half. Ranked by how quietly each one
fails, because that is what decides whether it reaches a user.

1. **A `.po` write that reverses the sector order wrong is self-consistent and
   wrong** (T029, and the write half of SC-004). This is the worst of the set.
   If the reorder is applied inconsistently, the image reads back *correctly
   through our own reader* — same transform on the way out and the way in — and
   is garbage on a real Apple II. Every round-trip test passes. The gate has to
   be **guest-visible**: boot it, or at minimum re-read through a path that does
   not share the reorder. T029 already calls for the four-format write matrix;
   the point is that a round-trip is not the check, it is the thing that hides
   the bug.

   **This was not hypothetical. The defect was already there**, in the read
   path, since Phase 3, and it read back perfectly — see the divergences block.
   The gates that found it are the two that consult evidence the code does not
   own: the published on-disk layout read at an absolute file offset, and
   `ProDosSkeleton`'s block map, which is a separately derived table corroborated
   against real ProDOS volumes. Everything else — every round trip, every
   nibblize-and-decode, the real-CPU boot — passed against the defect *until the
   mapping was made a composition rather than a restatement*, at which point
   they became identities and can no longer fail. **That is the general lesson:
   downstream checks turn tautological the moment the duplication they guard
   against is removed, so the standing gate has to be the one anchored outside
   the code.**

2. **The commit path's failure modes are untested by construction** (T030,
   T031). The happy path runs on every invocation; "the write failed halfway"
   runs only when something has already gone wrong — and FR-012's whole
   guarantee is about that path. `FakeDiskFileIo` has `failNextWrite`,
   `failNextReplace` and `mutateStampOnNextStat` for exactly this; they exist to
   be used. The assertion that matters is **the target file is byte-identical to
   what it was**, plus `HasNoTemporaryFiles()`. Asserting only that an error was
   returned would pass against a half-written image.

   *(Closed by T030/T031. Every documented failure — held by another program,
   stale by time, stale by size, no stamp recorded, write failed, replace
   failed — asserts the image byte-for-byte against the FIXTURE rather than
   against the fake's own copy, so a commit that corrupted the target cannot
   also corrupt the oracle. Two things had to change before those assertions
   could discriminate. **The fake was failing too cleanly**: `failNextWrite`
   returned an error without creating anything, so a commit that never cleaned
   up after a failed write passed. It now leaves a partial file, which is what
   the platform does, and the mutation "clean up only when the write itself
   succeeded" then goes red. And **the invocation tag had to be added to the
   temporary name**, because the attempt counter this file prescribed cannot
   separate two live invocations — see T030's decisions. Twenty-eight mutations
   were run and all twenty-eight were caught.)*

3. **Skipping an unwritable track is worse than refusing the operation** (T029).
   When a write needs a track whose outcome is `Partial`, refusing *that track*
   and proceeding produces exactly the half-written image FR-012 forbids. The
   refusal has to be whole-operation, before anything is committed —
   `TrackWritability` already refuses the whole image for quarter-track data for
   the same reason, and per-track refusal must not become the looser sibling.

   *(Closed by T029. `VolumeImage::Save` judges every track the edit needs
   before re-encoding any of them and produces nothing at all when one is
   refused. The assertion that carries it is `written.size() == 0`, not the
   error code: three separate mutations — filter the bad track out and commit
   the rest, note the refusal and write anyway, and serialize before judging —
   all return a plausible-looking failure or success while leaving a file
   behind, and only the emptiness check separates them.)*

4. **Delete's free-space return breaks a different file, later** (T024, T027).
   Freeing a sector a second file also claims corrupts that file, and nothing
   says so until someone reads it — possibly weeks on. `IsUniquelyOwnedBy` is
   the guard and the reason both slices are kept. Free only what the report
   shows this file uniquely owns; report the rest as leaked. Leaked space is
   recoverable, a cross-linked free is not.

5. **DOS 3.3 sector slack will fail the wrong assertion** (T032). A file occupies
   whole sectors and the bytes past its recorded length are whatever was there
   before. Gate `--verbatim` on **file** equality, never image equality — an
   image comparison fails for sector slack and for reallocation, neither of
   which has anything to do with character conversion. Assert sector reuse
   separately if image stability is what is wanted. I got this wrong once and
   was corrected; the task text carries the ruling.

   *(Closed by T032, and the two assertions are deliberately in two tests.*
   `PutVerbatim_RoundTripsTheFILEBytes_NotTheImageBytes` *extracts* `MAKE DUMP`
   *verbatim, puts it back unchanged, re-reads it through a FRESH runner over
   the committed image, and compares against the independently extracted
   fixture copy — 589 bytes, byte for byte. It then states the arithmetic that
   makes an image comparison the wrong check rather than leaving it to prose:
   the file occupies four sectors, so its footprint is 1,024 bytes against a
   589-byte file, and comparing footprints would compare 431 bytes that are not
   the file.* `PutVerbatim_ReusesTheSpaceItFreed_AssertedSeparatelyFromTheBytes`
   *is the allocation question, kept apart — it asserts the volume's free-space
   report is unchanged by the round trip, which is what "reuse" actually means
   here and says nothing about contents. The mutation that carries the pair is
   making verbatim mask the high bit: four tests go red, and none of them is an
   image comparison.)*

6. **Zero-sector catalog entries are a write hazard too.** Twenty of the sixty-
   three on Merlin's own disk. They have no chain, so delete must not try to
   free sectors for one, and write must not treat the slot as reusable free
   space on the assumption that an entry occupying nothing is not really an
   entry. The read half already discriminates on the entry's own sector count;
   the write half needs the same discriminator, not the `$7F/$7F` pointer.

**Method that actually caught things this phase**, offered because three of the
above are only findable this way: after writing a test, break the thing it
covers and confirm it fails. Two real saves came from it — a corruption helper
that XOR-ed a 4-and-4 checksum was a no-op for half the sectors on a track, and
a two-list chain fixture could not distinguish a correct pointer read from a
buggy one because the bug happens to work on the first hop. **When a mutation is
not caught, suspect fixture depth before suspecting the assertion. Three links
is the minimum for any chain-walking test.**

It earned its keep again on T023/T024. Ten mutations were run; nine went red
first try and **one did not**: replacing the "does this entry claim zero
sectors?" discriminator with "is its pointer `$7F/$7F`?" passed the whole suite.
Every decorative entry on Merlin's disk carries that sentinel, and so did the
synthetic fixture built to imitate it, so the wrong rule was right on every
sample available. The case that separates them had to be constructed on purpose:
a zero-sector entry whose pointer names *another file's* track/sector list.
`Delete_AZeroSectorEntryWhosePointerIsNotTheSentinel_StillFreesNothing` is that
case. **A fixture copied from the real world tests what the real world happens
to contain, not what the rule says.**

And again on T025/T026, where fourteen mutations were run and all fourteen went
red — but one of them went red the WRONG WAY, and that is the finding. Making
`OverheadBlocksFor` forget the tree's master index took the test host down
rather than failing an assertion: the block list was then one short of what the
layout consumed, and `PlaceFile` answered by indexing past the end of it. A
crash in a debug run is a silent overwrite in a release one. **A harness that
reads "0 failures" out of a run that never finished is the same defect one level
up**, and the first pass of the mutation script did exactly that, reporting NOT
CAUGHT for a mutation that had crashed. The fix in production code is a shape
check at the top of `PlaceFile` whose expected count is derived *there* rather
than borrowed from the allocator's helper — borrowing it would have made the
check agree with the mutation.

And again on T027/T028, where twenty-three mutations were run: eighteen were
caught, two took the test host down rather than failing an assertion, and three
were not caught at all. **The two crashes are the finding about the harness**,
not about the code — deleting the verified hand-off leaves `outBuffer` empty, a
later test indexes it, and the STL's hardened `vector` fails fast, so the run
produces no tally. A harness that read "no `Failed:` line" as "not caught" would
have scored the loudest possible detection as a miss. This one requires a
complete tally whose passed-plus-failed accounts for the expected total and says
**RUN DID NOT COMPLETE** otherwise. **The three genuine misses are all the same
mutation**, one per write and delete path: replacing the verified hand-off with a
bare `outBuffer = result;`. No input can distinguish it, because the check exists
for buffers correct code cannot produce — so the answer was structural (fold the
hand-off into the check) rather than another test. One mutation was also caught
only after a test was added for it: comparing `claimedButFree` by size instead of
membership passed the whole suite until a case was built where one referenced
sector is marked free before and a *different* one after.

And again on T029, where fourteen mutations were run: thirteen were caught first
try and **one was not** — deleting the whole-image writability refusal, which
changes the message and not the outcome, because the per-track answer inherits
the image verdict. A test asserting that a reason was *given* could not see it;
one asserting **which** reason can, and does. Two things about the harness are
worth carrying forward. **A mutation that makes the emulated 6502 execute
garbage floods the log**: the illegal-opcode trace is one line per bad opcode,
and a single forty-megacycle boot of a mis-ordered image wrote over a gigabyte,
which killed the first harness while it tried to read the file to score the run.
The harness now streams rather than slurps and drops that line as it goes; the
boot test itself runs in slices, stops the moment the guest reaches a known
screen, and checks the cheap surface comparison *first* so a wrong image never
gets booted at all. **And the fix that removes a duplicated constant removes the
mutation with it** — once the `.po` mapping became a composition, every
downstream check of it became an identity, and only the two gates anchored
outside the code can still fail.

And again on T030/T031, where twenty-eight mutations were run and all
twenty-eight were caught — which is not the finding. **The finding is that two
of them are only catchable because the test SUBSTITUTE was made harsher, not
because a test was added.** `failNextWrite` used to return an error without
creating a file, and against that fake "clean up only when the write itself
succeeded" and "never advance the progress record past the re-verify" are both
invisible: there is nothing to leave behind, so nothing distinguishes cleaning
up from not. Making the fake leave a partial file — which is what a failed
`ofstream` write actually does — turns both into red runs. **A fake that fails
more cleanly than the platform is a fake that certifies a bug.** The harness
requirement from T027/T028 carried over unchanged and earned its keep as a
control: a deliberately crashing `IsStale` produced no tally at all and was
reported **RUN DID NOT COMPLETE**, not as a miss.

And again on T032, where nineteen mutations were run: eighteen were caught first
try and **one was not**, for the reason this file keeps rediscovering — a
substring assertion is satisfied by more than the thing it names. The test for
"with no `--as`, the on-disk name is the host file's own last component" looked
for `PROG.BIN` in the listing, and an implementation that stripped nothing
passed it: a DOS 3.3 catalog name may hold colons and backslashes, so
`C:\BUILD\SUB\PROG.BIN` is a perfectly legal catalog name and it *contains* the
leaf. The fix is to assert the whole rendered line and, separately, that no part
of the host path survived. **Ask of a `find` assertion what ELSE could satisfy
it**; three of this feature's weak tests have now been of exactly this shape.
One other thing worth carrying: the harness restores the source but does not
rebuild, so the binaries left behind after a mutation run contain the last
mutation. Running the suite at that point reports a confident green against code
that is not on disk — rebuild before any gate that follows a mutation pass.
*(That is now fixed rather than merely recorded: `scripts/RunMutation.ps1`
restores, verifies the restore by hash, and rebuilds. Every mutation from T034
onward should go through it rather than through a hand-rolled harness.)*

And again on T034, where **twenty mutations were run — the same ten twice, once
against the whole suite and once filtered to the new gate — and all twenty were
caught first try.** Three findings.

**The one that matters is that byte equality against a rebuilt oracle is blind
to the material.** Where a case's image is synthetic, the same pure builder
produces both the seed and the oracle, so a defect in the builder corrupts them
identically and the comparison passes over two piles of garbage. That is not
hypothetical either: stamping a different volume number in `Dos33Skeleton::Write`
leaves every byte assertion in the gate satisfied, and only the "does it still
mount and still list what it listed" check goes red. **The second of the three
questions is not a courtesy restatement of the first; it is the one that fails
when the fixture stops being a disk.**

**The second is that three invariants about the IMAGE cannot see a refusal that
did not stop.** Deleting `RunPut`'s or `RunDelete`'s bail-out after a volume
refusal leaves the image untouched regardless, because `VolumeImage::Save`
refuses the zero-length buffer that reaches it — so bytes, mountability and the
absence of a temporary are all still satisfied while the command has quietly
travelled two more layers than it should have. The tell is a **second diagnostic
line**: one naming the real cause and one blaming the render. Asserting that a
refusal names exactly ONE reason is what catches it, and it is a property worth
holding anyway, since two candidate explanations for one refusal is precisely
what FR-014 exists to prevent.

**The third is a hazard in the shared harness, not in the code.**
`RunMutation.ps1` writes its build and test logs to a FIXED path
(`%TEMP%\CassoMutation`) with fixed file names, and 019 was running its own
mutation battery from another worktree at the same time. The two runs overwrite
each other's logs, and the verdict is computed by reading a log back immediately
after writing it — so a sufficiently unlucky interleave scores one branch's
mutation against the other branch's suite. What made it detectable here was
passing `-ExpectedTotal`: 019's suite reports a different total, so a crossed
read surfaces as **RUN DID NOT COMPLETE** rather than as a plausible verdict.
**Always pass `-ExpectedTotal`**, and treat the log paths the harness prints as
unreliable when another session may be running.

And again on T033, where **ten mutations were run: nine were caught and one was
not** — and the one that was not is a finding about a helper rather than about
an assertion, which is a shape this file has not recorded before.

**The nine cover the two claims a guest can settle and nothing else can.** A
catalog entry claiming one sector fewer, a stored load address off by a page, a
ProDOS auxiliary type that forgets where the file loads, a `B` that resolves to
Applesoft, a `BIN` that resolves to `TXT`, a lock the writer stops noticing, a
refusal that stops saying what it refused, every container written in ProDOS
block order, and the cached master made unreachable — all nine went red, and the
new gate is among the named failures for every one of them. **The master-made-
unreachable mutation is the one that proves the fail-rather-than-skip rule**: it
leaves exactly the two master-dependent cases red, where the skipping harness
beside them would have gone green.

**The miss is that the prompt discriminator's two clauses cannot be separated by
any screen these disks produce.** `IsAtBarePrompt` asks for a short bottom row
AND for it to start with `]`; dropping the second clause changes no verdict,
measured by instrumenting every decision the loop makes. Where DOS 3.3's catalog
pager stops on the stock master the bottom row is the last catalog entry, which
the length clause rejects on its own, and the ProDOS slideshow's pages end in
`press RETURN for more`. The clause is kept rather than deleted because the
glyph is the RULE and the shortness is a proxy for it — the same pager also
reaches a state where the cursor sits alone on the following line, which is what
made a `BLOAD` get eaten while this file was being written. Recorded rather than
solved: **the mutation is invisible because the available screens do not
distinguish a correct rule from a weaker one that happens to agree.**

And again on T036–T039, where **fifteen mutations were run: fourteen were caught
and the fifteenth is the one this file already predicted.** Replacing
`HandBackVerifiedResult` with a bare `outBuffer = result;` passes the whole
suite, exactly as T028 recorded for `Write` and `Delete` — no input can
distinguish it, because the check exists for buffers correct code cannot
produce. It is recorded rather than solved for the third time, and the
structural answer stays the one T028 gave: the check and the hand-off are one
call, so DELETING it is loud even though REPLACING it is silent.

Two things worth carrying from the caught fourteen. **The guest gate is among
the named failures for every mutation that moves the field or the record** —
moving the greeting offset by one byte, patching it into the catalog track,
never swapping, swapping with the last candidate rather than the first, and
copying one byte of a record instead of thirty-nine — so the boot cases are
discriminating rather than decorative. **And the ProDOS control had to be
constructed rather than assumed**: see T039's decision 2, where "placed but not
nominated" turned out not to be a control at all.

And again on T043–T046, where **thirty-two mutations were run and all thirty-two
were caught** — but the interesting result is not the tally, it is which witness
caught what.

**Eleven mutations are invisible to a round trip and are caught only by the
Applesoft-anchored vectors.** Storing spaces in normal position, matching
keywords without skipping spaces, letting REM tokenize its own text, dropping
the `?`-to-PRINT mapping, removing either half of the AT special case, scanning
the token table backwards — every one of them produces a tokenizer that
round-trips through its own detokenizer perfectly. Only the bytes typed into a
booted machine separate them. **That is the general lesson restated once more:
the standing gate has to be anchored outside the code**, and for a tokenizer the
outside is the machine, not the inverse function.

**Two mutations separate the two spacing rules from each other**, which is why
they are two rules rather than one. Removing the space the detokenizer writes
BEFORE a token is caught only by the exact-text case, because a normal-position
space is dropped on the way back and the round trip cannot see it. Adding the
space AFTER REM and DATA — LIST's own rule, and the obvious thing to write — is
caught by four round-trip cases and by the master's own greeting, because that
space is swallowed into the payload. A single "detokenize looks right" assertion
would have caught the first and missed the second.

**Two more are only catchable because the ProDOS case exists.** Leaving the
auxiliary type at zero and letting `--basic` inherit the binary default are both
invisible on DOS 3.3, which records neither. The ProDOS test asserts the whole
rendered row rather than searching it, so the type, the size, the exact
tokenized length and the load address are all pinned at once.

**One anchor did not match on the first pass and is worth recording as a harness
note**: the anchor text was written with single-space assignment where the file
carries the column alignment this project requires, so `ok = ...` did not find
`ok    = ...`. The harness reported **ANCHOR NOT FOUND** rather than a verdict,
exactly as designed — scored as a result it would have read as a miss. Re-run
with the aligned text, it was caught.

And again on T040–T042, where **sixteen mutations were run and all sixteen were
caught** — but two of them are findings and one is about the harness rather than
the code.

**The one that matters is that a cheap check has to come before the boot, and it
had to be ADDED here rather than inherited.** The first pass of the battery moved
the payload one track further out. The guest tests caught it, correctly and
loudly — by making a 6502 execute whatever it managed to read, which this build
traces one line per illegal opcode. It wrote **over three gigabytes** before
anything noticed, the test host had to be killed by hand, and the mutation
harness was left mid-run with the source still mutated. T029 recorded exactly
this hazard and T033 recorded exactly the remedy; the remedy simply was not in
this file yet. `AssertThePayloadIsWhereTheLoaderWillLookForIt` now decodes every
image through the DRIVE and compares the first sector the loader will ask for
against the payload's first page, before any processor starts, and the same
mutation is caught in **fifty-nine seconds with no trace at all**. The measuring
ceiling came down from 60M cycles to 20M for the same reason — a generous cap is
paid for in gigabytes by whoever next breaks this on purpose.

**The second is a blind spot in the guest gate, measured rather than assumed.**
Starting the loader's stepper from phase 1 instead of phase 0 lands the head on
quarter-track **6** — half a track past where it should be — and **every guest
case still passes**. `DiskImage::ResolveQuarterTrack` maps a nibblized sector
image with `qt / 4`, so quarter-track 6 resolves to track 1 and the read
succeeds. The mutation is caught only by the structural test that reads the
byte.

**And this is a property of the sector formats, not a defect and not a limit of
the emulation.** Half and quarter tracks work: `WozLoader` reads the WOZ TMAP
into a real per-quarter-track map, so on a WOZ distinct quarter-tracks address
distinct flux and copy-protected disks read correctly. `qt / 4` is the synthetic
default for `.dsk` and `.po`, which physically cannot carry half-track data, so
it is the only thing that mapping could be. What follows is a rule about
fixtures rather than about the product: **a gate meant to be sensitive to where
the head ends up has to run against a WOZ**, because on a sector image an
off-track head reads the neighboring whole track perfectly where a real drive
would read nothing. Written up as `research.md` R-013 and at
`DiskImage::ResolveQuarterTrack`; its long-term home is
`UnitTest/Fixtures/Disks/README.md`, which is on `master`.

The rest behaved as designed and two are worth naming. **Making the interleave
the identity is invisible to a single-sector payload** — page 0 is logical sector
0 under either rule — and is caught only by the twenty-page case and by the
master-anchored oracle, which is why both exist. **And the master-made-unreachable
mutation again proves the fail-rather-than-skip rule**, leaving eight cases red
across this phase and the two before it.

And again on T047/T048, where **eighteen mutations were run: seventeen were
caught and one was not, and the interesting result is a mutation that WAS caught
by the suite while the assertion built for it stayed silent.**

Fourteen went through `RunMutation.ps1`. Thirteen were caught first try by the
filtered run over the new gate. **The fourteenth is a warning about reading a
filtered verdict**, and it is the shape this file keeps rediscovering for the
fourth time. Renaming the `rm` alias to `del` in `s_kDiskVerbs` left all nine
help-text tests green, because the sweep asked whether the help *contains* each
verb name and `delete` contains `del` — so the one assertion whose entire
purpose is "a verb the grammar accepts that the help never mentions" could not
see it. Run unfiltered it is CAUGHT, by `Disk_TerseAliasesResolveToTheDescriptive
Verbs`, which has covered the alias since T032; the filtered run is what made
the gap visible at all. **A verdict of CAUGHT over the whole suite says the tree
noticed, not that the test you just wrote noticed.** The fix is
`ContainsAsWholeToken` — the match must be delimited, with dashes counted as
part of a token so `-o` cannot be answered by the `-o` inside `--out` — and the
same mutation now fails the sweep as well as the alias test. **Ask of a `find`
assertion what else could satisfy it**; the answer has now been "a longer word
that contains it" four times on this branch.

**A second harness note, and it cost two runs.** The Dormann 65C02 case
downloads its source from `raw.githubusercontent.com` on every run, and GitHub
was returning 503s and a `429 Too Many Requests` body during this phase.
`DownloadFile` treats the error page as a successful download, so the assembler
is handed HTML and the case fails with `Invalid mnemonic: TOO` and a Terms of
Service URL among its diagnostics. It is a genuine instance of this file's own
rule — a degraded state reading as a healthy one — and it is not this feature's
code. Treat a Dormann failure whose errors quote a web page as an outage, and
re-run.

And again on T035, where **five mutations were run: three through
`RunMutation.ps1` and two whose only possible judge is the manual pass itself.**
The split is the finding, and it is the whole reason T035 exists as a separate
task.

**Two suite mutations confirm the commit path is covered where it can be.**
Turning the atomic hand-off into a direct write over the target —
`ReplaceAtomically (tempPath, imagePath)` becomes
`WriteAllBytes (imagePath, newImageBytes)` — is CAUGHT by nineteen tests, named
by `Commit_NeverWritesTheTargetDirectly` and
`Commit_OnTheHappyPath_ReplacesTheImageAndLeavesNoTemporary` among others.
Deleting the `WriteTemporary` progress record is CAUGHT by exactly one,
`Commit_WhenTheWriteFails_LeavesTheImageByteIdenticalAndSweepsThePartial` —
narrow, and precisely the assertion built for it.

**The third is NOT CAUGHT, by construction rather than by weakness, and running
it was the point.** Deleting the `TerminateProcess` call leaves all 3306 tests
green, because `CassoCli` is not linked by the test assembly and no test can
reach a line in it. That is the same fact that makes T035 a manual task, now
stated as a measurement instead of an assertion: the suite is structurally blind
to this file, so a green suite is not evidence about it and must not be read as
any.

**So the last two mutations are judged by re-running T035, and both flip its
verdict.** Neutering the hard stop into a no-op leaves the run exiting 0 with
the original image **changed** — so a passing T035 is not a procedure that would
pass either way. Moving the `before-replace` call one line later, after
`MoveFileExW` instead of before it, leaves the original changed and no temporary
behind — so the green result is about the chosen instant and not merely about
the switch working. A manual check can be mutation-tested; it just needs a
different judge.

The other four ran the T047 script gate against a Release build. Three were
caught: `boot` no longer complaining about a greeting DOS cannot RUN, `--raw`
writing a DOS header after all, and the catalog listing dropping its sector
count. **The fourth was not, and it marks the boundary of what a launch step
can witness.** Making the emulator ignore `--disk1` entirely — so it starts with
no disk at all — leaves the gate green, because the launch step asserts the
process came up and stayed up and nothing more. Reading the guest's screen from
a script would be pixel matching, and the mount is already gated where a gate
can see it: T033, T039, T042 and T046 boot a real 6502 over images this same
runner produced. The script says so in its own header rather than leaving the
limit to be discovered.

And once more afterwards, on the three boot gates that never got the bound this
file has now recorded three separate times — `BootDiskTests`,
`CatalogReproductionTest` and `GameBootTests`. **One mutation, and the finding is
that the check has to be a comparison against something the decoder does not
own.** Stepping the nibblizer's sector placement one position — `interleave
[logical]` becomes `interleave[(logical + 1) % kSectorsPerTrack]` in
`NibblizeWithMap` — leaves every address field valid and every sector decodable,
so nothing about "can the drive read this?" notices. It is the exact shape T029
and T040 measured at over a gigabyte of trace apiece: the controller ROM reads
track 0's first sector, believes it, and jumps into another sector's data. The
bound that catches it is `GuestSession::AssertTheDrivePresentsWhatWasMounted` —
the drive must hand back the bytes it was given — and the mutation is now
**CAUGHT in 7.2 seconds across the twenty-eight gates in those four classes,
with a 17 KB log and no trace at all**. The three retrofitted cases fail in 67,
65 and 110 milliseconds, all of them before a processor is started. The "before"
figure was deliberately not reproduced: the demo gate alone spends 10M cycles,
which at two cycles per illegal opcode and a 257-line look-back apiece is tens
of gigabytes, and this file already records what that costs.

**And the weaker sibling exists because a measurement said it had to.** The
first draft asked all three for their boot sector, and Choplifter, Karateka and
Lode Runner all went red — every one of which the controller ROM boots
perfectly. `Denibblize` recovers **zero** sectors from track 0 of all three: it
abandons a track at the first sector whose data field it cannot locate, having
spent the revolution looking, and a protection that rewrites data prologues puts
one of those in its way inside the first revolution. So a protected disk is
asked `AssertTheDriveHoldsWrittenTracks` instead — as many written tracks as the
case is about to require the head to visit, and address fields on track 0 — which
is the strongest question this decoder and that medium agree on. **A gate whose
oracle is our own reader cannot be applied to media our own reader is not
faithful to**, and this is the second time on this branch that reading a WOZ
through the sector layer has produced a confident wrong answer (see R-013).

**R-007's open half was re-examined and is still open, with the obvious fix now
ruled out rather than merely undone.** Deriving the temporary's name
deterministically from the target — so a later `put` finds and reclaims the
orphan — is exactly what T030 diverged from its own task text to avoid, and the
trade is not close: the current defect leaves a file a person deletes, and the
proposed cure lets two concurrent invocations on one image commit each other's
bytes. Written up in `research.md` R-007 along with what a real fix needs, in
both of the shapes available — a sweep of stale siblings, which needs directory
enumeration on `IDiskFileIo`, a Win32 implementation the test assembly cannot
reach, an orphan-versus-live rule, and reconciliation with a shipped test that
pins the opposite; or a kernel-owned temporary, which needs none of that and a
seam that carries a handle instead. Neither is small; neither is forced.

**Phase 8 is done, and the suite is 3306 Debug / 3303 Release.**
T047–T051 shipped together: `scripts/RunBuildLoopGate.ps1`,
`DiskCommandRunner::BuildHelpText`, `UnitTest/EmuTests/DiskHelpTextTests.cpp`,
the CHANGELOG and README updates, and a comment on GH #115. (The counts were
3297 / 3294 before this phase added 9.) T035 landed afterwards and adds no
tests: it is a manual pass over the real file edge, and the interruption switch
it needed lives in `CassoCli`, which the test assembly does not link — so the
counts are unchanged at 3306 / 3303. Re-gating that switch behind a deliberate
define and re-running T035 through it left the counts where they were, for the
same reason.

**Phase 15 is done, and the suite is 3431 Debug / 3428 Release.** T115–T122
shipped together: a surplus argument is refused in all three grammars, a bare
`-h` mid-command-line is refused by owner ruling, and the two places that read a
value, failed to understand it, and substituted one anyway now say so. (The
counts were 3412 / 3409 before this phase added 19.) Three findings from the
same sweep were measured and left for the owner rather than decided — per-verb
option applicability on `disk`, the `--name=value` long-option form that is
claimed in a comment and implemented nowhere, and "N lines assembled" reporting
0 unless `-l` was given. All three are written up in `research.md` R-014.

**T047 found two defects in this feature's own documented loop, and both are the
mistake a reader makes rather than a mistake in the code.** The five steps in
`quickstart.md` did not produce a running program, and nothing had ever run
them:

- **Step 1 said `--dos-bin`.** `put --type B --addr` writes the DOS 3.3
  four-byte header itself, so a `--dos-bin` file arrives carrying a second one.
  `BLOAD` places the stale inner header at the load address and `BRUN` executes
  it: its first byte is the low half of that address, which for anything in page
  `$60` and below is a `BRK`. Measured on a booted //e — the screen reads
  `6002-` with a register dump, two bytes past `$6000`, which is where `BRK`
  pushes from. The correct flag is `--raw`.
- **Step 4 named the binary as the boot program.** T038 already recorded that a
  booting DOS 3.3 RUNs its greeting; what was not noticed is that the quickstart
  loop depends on it. The step sets the name correctly, exits **1**, says the
  disk will boot without running it, and the loop ends at a `]` prompt. It takes
  a sixth step — a one-line greeting placed with `--basic` that `BRUN`s the
  binary — to close it, and the fix is in `quickstart.md`, in the gate, and in
  the help output.

Both are now warnings in `BuildHelpText`, both are asserted by
`DiskHelpTextTests`, and both are exercised by the gate's "documented traps"
section — because a warning nothing runs is a warning that can quietly stop
being true.

**SC-006 is met with room: 6.65 s against the 10 s budget**, measured on
`relmer-desktop`, Release x64. The split is what matters, and only one third of
it is ours: **0.12 s** of command-line work across five invocations, **0.29 s**
from launching the emulator to its first window, and **6.24 s** of guest boot —
the 6,366,505 emulated cycles T042 measured for the DOS 3.3 route, at the
machine's own 1,020,484 Hz. **The disk's boot is five sixths of the budget and
nothing in this feature can move it**, which is a second, practical argument for
the direct-boot route: 209,147 cycles past the hand-off rather than 4,718,764.

**SC-001 is checked mechanically rather than asserted.** Each step names one
executable and an argument vector; the gate resolves every executable inside
this repository's own build output and refuses any argument carrying shell
punctuation. A loop that quietly needed a pipeline, a helper script, or
something off the PATH could not be written in that table.

**T048's help had a defect of its own, and it is the one SC-002 is actually
about.** `PrintUsageHeader` substituted the prefix the reader typed into the
disk options, so `CassoCli /?` documented `/long`, `/addr`, `/as` and
`/verbatim` — **none of which `ParseDiskOptions` accepts**. A newcomer working
from the tool's own output got an option silently ignored: `disk list d.dsk
/long` prints the short listing and exits 0.
`HelpText_SpellsDiskOptionsWithTwoDashes_WhichIsTheOnlySpellingAccepted` drives
the parser with both spellings and then checks the help, so the two halves have
to agree.

**The help text moved into the library, and that is what makes any of this
checkable.** `PrintUsage` lives in `CassoCli`, which the test assembly does not
link, so every claim in it was unverifiable by construction — the same reason
`kInUseHelpText` and `kRoundTripHelpText` already sat in core.
`DiskCommandRunner::BuildHelpText` composes the whole disk section, quotes both
of those constants rather than restating them, and the executable prints it
verbatim. `CommandLineParser::GetAllDiskVerbs` was added for the same reason
`GetAllSubcommands` exists: the sweep has to run over the grammar's own table,
because a verb added there and left out of the help is a capability nobody can
find.

**Blocked on nobody; 019 is blocked on this branch.** Spec 019 runs
concurrently. Measured overlap is three files — `CassoCore/CassoCore.vcxproj`,
`UnitTest/UnitTest.vcxproj` and `CassoCli/CommandLine.cpp` — and the two sides
of `CommandLine.cpp` are in different regions today, so it merges cleanly until
019 registers `as65` in the usage text. Keep both sides everywhere; taking one
drops the other session's files from the build and still passes, with fewer
tests in it. Check the merged count against the sum, not merely that it is
green: this branch is **3088 Debug / 3085 Release**.

019 holds its T049 (`as65` fallback removal) until this branch's command-line
work reaches `master`, so landing that is the thing another session is waiting
on. `UnitTest/CommandLineTests.cpp` carries
`BareWordThatIsNotASubcommand_StaysAs65`, placed here deliberately as a tripwire
so removing the fallback has to be a decision. **Deleting it is the intended
outcome — do not defend it at merge.** See `docs/coordination.md`.

**Known divergences from the task text, so they do not read as gaps:**

- **SC-007's "under 25% of the emulated CPU cycles" is UNREACHABLE as written, and
  the gate measures the two disks' own contributions instead.** Every 5.25-inch
  disk in this machine is entered the same way: the controller ROM recalibrates
  the head with eighty half-steps, each waiting through the monitor's own delay
  routine, then reads track 0 sector 0 into $0800 and jumps to $0801. That costs
  **1,647,741 emulated cycles** and it is **25.9%** of everything a DOS 3.3 boot
  spends getting to a BRUN'd binary. No disk can beat a quarter of a DOS boot,
  including a disk holding nothing at all, so a criterion applied to the whole
  boot is a statement about the ROM rather than about this feature.
  `ADirectBoot_ReachesTheProgramInUnderAQuarterOfWhatTheDos33RouteSpends`
  therefore measures the hand-off cost **on both images**, asserts the two are
  equal — the subtraction is only legitimate if it is the same constant on both
  sides — and applies the quarter to what remains. It also **asserts the
  arithmetic that makes the whole-boot form unreachable**, so if that ever stops
  holding the case goes red and the gate gets tightened rather than the finding
  quietly outliving its evidence. Both numbers are printed by the run.

- **Direct boot is NOT reachable from the command line, and no task in this phase
  asks for it.** FR-029 requires every capability to be reachable from the tool;
  Phase 6's three tasks are the mechanism and its gates, and `s_kDiskVerbs` has no
  verb for this. `DirectBootBuilder` is therefore complete, tested and unreachable
  by a user, and there is **no CHANGELOG entry** for it because nothing a user can
  type changed. Whoever adds the verb should note that `Build` already produces the
  refusal sentence, so the runner arm needs no second implementation of the
  capacity arithmetic — and that the natural spelling collides with nothing, since
  `boot` already means "set the startup program".

- **T030's own sentence asked for something its stated inputs cannot deliver.**
  It prescribes deriving the temporary name "from the target path and an attempt
  counter" and, in the next clause, that names "must not collide between
  concurrent invocations". Those two cannot both hold. Both invocations start at
  attempt zero; both check whether that name is free; both can be told yes in
  the same instant, and the loser's bytes are then committed under the winner's
  name as though they were the winner's. The existence check is a
  time-of-check/time-of-use race and always was. The shipped derivation takes an
  **invocation tag** as a third input — process id plus a per-runner sequence —
  so two invocations never start from the same name, and the attempt counter is
  left doing the job it is actually good at, which is stepping over a temporary
  somebody abandoned earlier. `TemporaryPathFor` is still pure; the tag source is
  the one impure function in `CommitPlan` and is separated for that reason. **The
  test that discriminates compares two tags at EQUAL attempt** — varying the
  attempt as well passes against a derivation that ignores the tag entirely.

- **`contracts/disk-subcommand.md` said the CLI shell OWNS the five commit
  steps.** Taken literally that puts the whole commit policy in the one project
  `UnitTest` does not link — Principle VI's litmus failing precisely where a bug
  destroys a user's image. The shell owns the *syscalls*; every decision in the
  sequence is core's, above `IDiskFileIo`. The contract has been amended in
  place rather than silently reinterpreted.

- **The `.po` sector reorder was WRONG from Phase 3 until T029, in both
  directions, and no test could see it.** `VolumeImage` held its own
  DOS-logical-to-ProDOS-file table. Both interleave tables the track layer owns
  are indexed by **physical** sector; that copy was applied to **logical**
  sectors, which is a different table. A `.po` written through it put the ProDOS
  volume directory nine sectors from byte 1024, where ProDOS looks, and a
  genuine `.po` was misread on the way in — `DetectFilesystem` would have called
  it Unknown. The emulator was never affected: `DiskImageStore` mounts a `.po`
  through `NibblizationLayer::NibblizePo`, which uses the correct table, so this
  never reached a released build and needs no CHANGELOG entry. **Why nothing
  caught it**: the reorder and its inverse shared the one table, so a round trip
  is the identity whatever the table says, and the only `.po` any test held was
  produced by the same function that read it back. All eight cross-format
  extraction tests pass against the defect, measured under mutation. **The fix
  is structural**: `NibblizationLayer::PoFileIndexForDosLogicalSector` composes
  the two interleaves in the file that owns them, so a file reorder cannot
  disagree with the layout the drive sees. Do not "simplify" it back to a
  literal table.

- **The DOS 3.3 `IsClean()` asymmetry is FIXED, and it was a code defect rather
  than a property of the filesystem.** `ProDosVolume::BuildIntegrityReport`
  credited blocks 0–6 to a reserved volume owner; `Dos33Volume`'s did not,
  although its own comment said it did, so tracks 0–2 and track 17 — 64 sectors
  no catalog entry names — came back `allocatedButUnclaimed` and `IsClean()` was
  false on every healthy DOS 3.3 volume. `Dos33Volume::ClaimVolumeStructures`
  now credits them, both filesystems share
  `VolumeIntegrityReport::kVolumeOwner`, and a freshly formatted volume reports
  clean. Two tests pin it, one on the whole-volume verdict and one on the sixty-
  four sectors being *attributed* rather than merely skipped — skipping them
  would empty the same set while letting a file's chain reach into DOS without
  the pass calling it a cross-link.
- **T028 still does not gate on `IsClean()`, and the reason never rested on that
  defect.** A correct delete that declines to free a cross-linked unit leaves
  space allocated with nothing claiming it, which is exactly what FR-011
  requires, so a whole-volume yes/no would refuse the outcome the spec demands.
  `VolumeIntegrityReport::IsSafeToCommitAfter` compares the computed result
  against the input instead.
- **The set comparison this file previously prescribed was wrong, and two
  existing tests prove it.** "`crossLinked` and `claimedButFree` empty" would
  refuse a correct write on any volume that already carried such a
  disagreement — and `Write_NeverHandsOutASectorTheCatalogStillClaims` and
  `Volume_Write_NeverHandsOutABlockTheDirectoryStillClaims` construct exactly
  that state on purpose, since working around it is what the allocator is for. A
  gate demanding an empty set would make a disk with one bad sector permanently
  uneditable, which is the opposite of what a recovery tool owes its user. The
  shipped rule is **not worsened** on all four sets: `crossLinked` and
  `claimedButFree` compared by MEMBERSHIP (a result may carry the input's
  disagreements and no others), `allocatedButUnclaimed` and
  `unfollowableChains` by size — the first two name units, and the last names
  owner indices that an edit renumbers by construction. `catalogFullyParsed` is
  asked separately, because a result that lost half its catalog looks *tidier*
  on every set rather than worse.
- **A refusal from that check is unreachable from any input, which is a property
  of the check and not a gap in the tests.** It fires only when our own writer
  produced a bad buffer, so the suite drives it by handing
  `HandBackVerifiedResult` a deliberately corrupted result — one case per clause,
  each constructed so only that clause can reject it. The wiring is protected
  structurally instead: the check and the hand-off to `outBuffer` are one call,
  so deleting the check leaves every write returning nothing and the suite goes
  red. Replacing the call with a bare assignment is still invisible to the suite;
  that is recorded rather than solved, because no input can distinguish it.
  `AssertEditLeftTheVolumeConsistent` remains in both test suites as the
  test-side twin of the same comparison.
- **ProDOS subdirectory entries on Merlin's own disk have destroy-enable
  clear**, which decided an ordering in `ProDosVolume::Delete`. Testing the lock
  before the "this layer cannot walk into a directory" refusal reports a
  permission problem for something that would be refused anyway once unlocked,
  so the capability refusal goes first. Found by a test against `/MERLIN`, not
  by reasoning.
- **`/MERLIN`'s volume directory holds an inactive record BEFORE an active
  one.** ProDOS reuses slots in place, so a listing is not densely packed and a
  newly placed file lands in the hole rather than at the end. Any assertion
  comparing a before-and-after listing on a real ProDOS disk must compare by
  membership, not by position; the positional version was written first and
  failed on this disk.
- **`ProDosVolume::Write` does not extend `ProDosFileWriter`.** The existing
  writer is the bootable-disk helper: it writes through the caller's buffer,
  allocates from the bitmap without consulting the integrity report, and asserts
  on bad arguments — all correct for a caller supplying its own constants and
  all wrong for one relaying what a user typed. The two are kept from drifting
  by reading every file the new writer produces back through `ProDosReader`,
  which neither of them shares.
- **A multi-list file's second and later list sectors were never claimed**
  (found by T023's three-list write test, fixed with it). `CollectDataSectors`
  returned only data sectors and `BuildIntegrityReport` added the head list
  sector from the catalog entry's own fields, so a file over 122 sectors left
  every later list sector allocated and unowned. Delete would have leaked those
  sectors permanently *without even reporting them as leaked*, since nothing knew
  they belonged to the file. `CollectDataSectors` now returns list sectors in a
  second out-parameter; `Read` ignores it, the integrity pass claims it.
- **T001 was not done as written, and is now ticked as done differently.** It
  called for EHM-conformant stubs for every file up front; files were instead
  created and wired as each task reached them, which meant the build never
  carried a stub that did nothing. Its `.vcxproj.filters` clause was moot — this
  repository has no `.filters` files. The intent was audited by counting rather
  than asserted: 9 of the 15 new production `.cpp` files carry EHM, and the 6
  that do not declare no function that can fail. The numbers and the names are
  on T001's own line, so the tick reads as "done differently, deliberately, and
  here is the evidence" rather than as a box someone waved through.
- T009 split `FilePath` into its own header/pair rather than folding it into
  `VolumeTypes.h`, per the one-type-per-pair style rule.
- `--text` is now wired on **both** paths (T032). Placing text writes the
  high-ASCII convention on both filesystems, which is what the read path
  decodes, so `get --text` then `put --text` is the identity. A host byte with
  no Apple II representation refuses the whole placement and names the offset,
  rather than masking to seven bits and producing a different, plausible
  character.
- `--basic` **now works on both paths** (T045); the refusal it used to carry is
  gone. What replaced it is not merely a conversion: the flag also chooses the
  file type, sets ProDOS's auxiliary type, and refuses `--addr`, because each of
  those left alone would place a file the guest cannot run while reporting
  success.
- **`E_INVALIDARG` is the code the tokenizer returns for a bad listing, and that
  is the local convention rather than a lapse.** `CassoCore` is deliberately
  platform-free — it does not include `windows.h`, so `HRESULT_FROM_WIN32` does
  not exist there — and both neighbors, `AppleTextCodec::Encode` and
  `CommandLineParser`, answer user-input failures the same way with the
  NON-asserting `CBREx`. The distinction a caller needs is carried by
  `ApplesoftListingError::reason`, which is what reaches the user; the code is
  never printed.
- **The full-volume refusal does not name the shortfall**, which quickstart
  §US2 case 4 asks for. See T032's decisions: the number is knowable only inside
  the allocator, and re-deriving it in the runner is a second implementation of
  the overhead arithmetic.

**Do not retry, with reasons recorded next to the thing they concern:**

- **Validating catalog names as printable text** — `research.md` R-012 and
  `UnitTest/Fixtures/Disks/README.md`. It rejects 20 of the 63 entries on a disk
  Merlin shipped. Their names are a high-ASCII `A` then eight backspace
  characters, which is how DOS draws a heading at column zero, so eight of
  thirty bytes are below `$20` and byte-for-byte indistinguishable from garbage.
- **Automating the binary-output check at any level** — `quickstart.md` and the
  comment at `CassoCli/Win32DiskFileIo.cpp`. Neither the byte assertion nor the
  narrower "handle is in binary mode" assertion is reachable: the translation
  happens below the seam, and inspecting or changing the mode touches a console
  handle and mutates process state, which the test rules forbid.
- **Tightening filesystem detection to reject copy-protected disks.** `The Print
  Shop Color side B.woz` lists as clean DOS 3.3 at exit 0 with a garbage
  catalog. It is out of scope, the corroboration already rejects nine of the
  eleven demo `.woz` files, and the one tightening attempted for it is the first
  item above.

  **The open question attached to T028 is now MEASURED, and the answer is
  positive — which changes nothing about the ruling above.** R-012's conclusion
  was about *names*, so the two signals T028 computes were checked against the
  same disks. Every image below was loaded through `VolumeImage::Load` and read
  by `Dos33Volume`; only three of the demo `.woz` files are detected as DOS 3.3
  at all.

  | image | entries | chains unfollowable | cross-linked | free units | allocated-but-unclaimed |
  |---|---|---|---|---|---|
  | `The Print Shop Color side B.woz` | 7 | **7 of 7** | 1 | 0 | 496 of 560 |
  | `The Print Shop Color side A.woz` | 0 | 0 of 0 | 0 | 0 | 496 of 560 |
  | `Merlin-proDos2.23.dsk` (healthy) | 63 | **0 of 63** | 0 | 37 | 17 of 560 |
  | a volume this branch writes | any | 0 | 0 | rest of disk | 0 |

  **Chain reachability separates them outright**: not one of side B's seven
  entries has a track/sector list that can be walked, against nought of Merlin's
  sixty-three. **Free-map agreement separates them too**, though as a magnitude
  rather than a yes/no: side B declares no free sectors at all while 88.6% of the
  volume is allocated and claimed by nothing, against 3.0% on Merlin and 0% on
  anything written here. Side A is the same shape with an empty catalog, so it is
  caught by the free map where reachability has nothing to work with. Note that
  the DOS 3.3 half of the free-map figure only became meaningful once the
  reserved-track fix landed — before it, every healthy volume carried 64 unowned
  sectors of its own.

  **Recorded, not acted on.** Both signals are magnitudes, and turning either
  into a refusal is copy-protection detection by another name — explicitly out of
  scope, and a threshold nobody has evidence to place, since the tree holds
  exactly one healthy real-world DOS 3.3 disk to calibrate against. What this
  does settle is that "we cannot tell damage from decoration" is false as stated:
  it is true of names and false of structure. A later feature that wants to warn
  rather than refuse — spec 021's inspection tools are the obvious home — has the
  measurement and does not need to redo it.

**Two artifacts worth reading before starting, not after:** `quickstart.md`
§US3 now carries the recipe for constructing a damaged image from the host, and
`UnitTest/Fixtures/Disks/README.md` on `master` carries the on-disk format
findings measured against these volumes — including the load-address asymmetry
between the two filesystems, which is the shape most likely to produce a
four-byte bug on one of them only.

---

**Input**: Design documents from `/specs/020-disk-file-access/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: INCLUDED — Constitution Testing Discipline is non-negotiable; every
pure component ships its unit suite in the same phase. Guest-visible gates reuse
the real-CPU DOS-boot harness and **FAIL when its cached asset is absent** — a
test that cannot reach its data must not pass, or "N passed" stops meaning N
things were checked.

**Organization**: Phases follow plan.md's **dependency order**, not story order.
User Story 1 is **DELIVERED** (shipped `--raw` / `--dos-bin`) and has no tasks.
US3 (read) precedes US2 (write) because extraction is what a migrating developer
needs first and because a write path must not be built before the decode report
exists. Constitution commit discipline: commit + push after each completed phase.

## Format: `[ID] [P?] [Story] Description`

---

## Phase 1: Setup

- [x] T001 **Done differently, on purpose — see the divergence note in "What the
  build found" and the audit below.** Files were created and wired as each task
  reached them instead of arriving up front as stubs, so the build never carried
  a file that did nothing; the `.vcxproj.filters` clause was moot because this
  repository has no `.filters` files. What the task was *for* — every new file
  EHM-conformant and the solution compiling clean — is met: of the 15 new
  production `.cpp` files, **9 carry EHM** (`Win32DiskFileIo`, `DiskCommand`,
  `ApplesoftTokenizer`, `AppleTextCodec`, `DirectBootBuilder`,
  `DiskCommandRunner`, `Dos33Volume`, `ProDosVolume`, `VolumeImage` — 8 of them
  `HRESULT` + `Error:` + the `CBR`/`CHR`/`CWR` family, and `DiskCommand::Run`
  consuming an `HRESULT` and mapping failure to an exit code, which is what a
  `main`-adjacent boundary should do). The other **6** — `ChainWalkGuard`,
  `CommitPlan`, `FilePath`, `SectorDecodeReport`, `TrackWritability`,
  `VolumeIntegrityReport` — are predicates, accumulators and pure functions with
  **no failure mode**: not one of them declares a function that returns
  `HRESULT`, and giving them one would be error handling with nothing to handle.
  That is a principled boundary, not an oversight. Original wording: Add
  EHM-conformant stubs for every new file and wire them into `CassoEmuCore/CassoEmuCore.vcxproj(.filters)`, `CassoCore/CassoCore.vcxproj(.filters)`, `CassoCli/CassoCli.vcxproj(.filters)`, `UnitTest/UnitTest.vcxproj(.filters)`: `CassoEmuCore/Devices/Disk/{IVolume.h, VolumeTypes.h, SectorDecodeReport.h, Dos33Volume.h/.cpp, ProDosVolume.h/.cpp, VolumeIntegrityReport.h/.cpp, TrackWritability.h/.cpp, IDiskFileIo.h, DiskCommandRunner.h/.cpp, CommitPlan.h/.cpp, DirectBootBuilder.h/.cpp}`, `CassoCore/{AppleTextCodec.h/.cpp, ApplesoftTokenizer.h/.cpp}`, `CassoCli/{Win32DiskFileIo.h/.cpp, DiskCommand.h/.cpp}`, `UnitTest/{AppleTextCodecTests.cpp, ApplesoftTokenizerTests.cpp}`, `UnitTest/EmuTests/{Dos33VolumeTests.cpp, VolumeIntegrityTests.cpp, DiskCommandRunnerTests.cpp, CommitPlanTests.cpp, DirectBootTests.cpp, FakeDiskFileIo.h}` — x64 Debug compiles clean

---

## Phase 2: Foundational (blocking prerequisites — plan.md Phase A)

**Nothing in any later phase may consume denibblized output until this phase is
complete.** T003–T006 are the fix for a defect that is live on the emulator's
flush path today (GH #115); building a write path on top of the current decoder
would ship it a second time.

**Scope boundary on T005 — deliberate, not unfinished.** T005 is
freeze-and-sidecar: write the recovery file, name it in the notification, leave
the mount alone. **Promotion** — switching the running mount over to the recovery
image — is explicitly NOT in this feature. It ripples into six surfaces (drive
widget label, write-protect menu text, tooltip, recent-disks MRU, the
`DiskImageStore` entry, and the persisted `disk1Path` / `disk2Path`), and getting
the last of those wrong loses the user's work on next launch. That is an
emulator-behavior change, and it belongs to #115 as a follow-up with spec 021's
disk-manager owner in the loop. There are two features here and only one of them
is this one — record that next to the implementation so a later reader does not
mistake the stopping point for an oversight.

- [x] T002 [P] Define `SectorDecodeReport` and `TrackDecodeOutcome` (`Complete` / `Unformatted` / `Partial`) with per-track 16-bit `coverage`, `duplicated`, `hasDataLoss`, `unrecoveredCount` in `CassoEmuCore/Devices/Disk/SectorDecodeReport.h` per data-model.md. **Track layer, not `VolumeTypes.h`** — these describe nibble decoding, and putting them in the filesystem-layer header would make `NibblizationLayer` include a header from the layer above it
- [x] T003 Rewrite the per-track decode loop in `CassoEmuCore/Devices/Disk/NibblizationLayer.cpp` (currently lines 762–780): continue past a failed sector and resynchronize on the next address prologue instead of `break`; maintain the coverage mask; classify the track by **coverage** (`Complete` iff all sixteen logical sectors filled, each exactly once), using address-fields-found to separate `Unformatted` from `Partial`. Add the four-argument `Denibblize` overload carrying `SectorDecodeReport`
- [x] T004 Rewire the existing three-argument `Denibblize` in `CassoEmuCore/Devices/Disk/NibblizationLayer.h/.cpp` to **forward to the reporting form and fail when the report shows data loss**, succeeding only when every track is `Complete` or `Unformatted`. It must not remain a reportless passthrough — `DiskImage::Serialize` (`DiskImage.cpp:434`) is the sole production caller and the one place the defect matters. Verify all twelve existing call sites still compile unchanged. **This is a user-visible behavior change on a constantly-running path**: `DiskImage::Serialize` today returns `S_OK` over a partly-zeroed buffer and the flush completes silently; afterwards it fails on a partially-decodable track, and `DiskImageStore::FlushEntry` already routes persist failures to the user through the shared EHM notifier. Users will now see an error where they previously got a quietly corrupted image. Requires the CHANGELOG entry in T049 and the recovery affordance in T005
- [x] T005 Give the refusal from T004 a recovery path, because a correct refusal that leaves the user stuck is only half the fix. At flush time the in-memory image holds the session's guest writes while the file on disk holds the last good copy: refusing preserves the original but strands the session, and writing anyway is the defect. In `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, on a flush refused for data loss, write a recovery file beside the target — `<name>.recovered.woz` — via `WozLoader::Serialize (image, bytes)`, leave the original untouched, and name that path in the notification. **Serialize to WOZ, not to the rejected sector buffer.** The sector buffer is the denibblized output holding zeros exactly where the damaged track should be — it discards the very content that caused the refusal. `WozLoader::Serialize` takes the image's live per-track bit streams verbatim and is format-agnostic, so a `.dsk`-sourced mount round-trips **losslessly**, damaged track included; the file is genuinely useful (mount it and the odd track is still there) and the notification needs no lossiness caveat. Same cost — one call on data already in memory. Handle name collisions; never overwrite an existing recovery file
- [x] T006 In `UnitTest/EmuTests/NibblizationTests.cpp`: narrow the comment on `Denibblize_UnformattedTrack_ZeroFillsThatTrackAndKeepsOthers` (line 308) so it claims correctness **only** for the wholly-unformatted case — as written it generalizes to "missing sectors read back as zeros … not silent corruption", which is the standing license that let the defect survive. Add the three siblings that pin what it did not cover: `Denibblize_PartiallyDecodableTrack_ReportsDataLossAndDoesNotZeroTail`, `Denibblize_OutOfRangeSectorNumber_ReportsIncompleteCoverage`, `Denibblize_DuplicateSectorNumbers_ReportsIncompleteCoverage` — each built by nibblizing a valid image then patching one address field's 4-and-4 sector value. The existing test must keep passing unchanged
- [x] T007 [P] Implement `TrackWritability` in `CassoEmuCore/Devices/Disk/TrackWritability.h/.cpp`: whole-image refusal first (quarter-track map resolving any position off `qt / 4` via `DiskImage::ResolveQuarterTrack`; image metadata declaring timing-sensitive capture), then per-track writable iff `Complete` or `Unformatted`. Positive proof only — never protection-scheme recognition. Tests in `UnitTest/EmuTests/NibblizationTests.cpp`
- [x] T008 [P] Implement `NibblizationLayer::RenibblizeTracks` (re-encode only the listed tracks into `DiskImage::GetTrackBitsForWrite`, leaving every other track's packed bits byte-identical) in `CassoEmuCore/Devices/Disk/NibblizationLayer.cpp`; test that an unrelated track's bits are bit-for-bit unchanged after a write elsewhere
- [x] T009 [P] Define `FilePath` (path-based from the outset per FR-009), `FileEntry`, `FilePayload`, and `VolumeListing` in `CassoEmuCore/Devices/Disk/VolumeTypes.h` per data-model.md — fields a filesystem does not store are absent, not zero, so "no load address" is distinguishable from "loads at $0000". **Delivered with `FilePath` in its own `FilePath.h/.cpp` pair rather than in `VolumeTypes.h`**: it has methods, and the style rule that a class with behavior gets its own pair named for it outranks this task's wording. `VolumeTypes.h` holds only the plain-data types. Recorded so the split reads as a decision rather than a discrepancy someone "corrects" by moving the file back
- [x] T010 Define the `IVolume` seam in `CassoEmuCore/Devices/Disk/IVolume.h` per contracts/volume-api.md: `Enumerate`, `Read`, `Write`, `Delete`, `BuildIntegrityReport`, `SetStartupProgram`. Every mutating call takes the current sector buffer and yields a **new** one — nothing mutates in place
- [x] T011 Implement `VolumeIntegrityReport` in `CassoEmuCore/Devices/Disk/VolumeIntegrityReport.h/.cpp`: `claimedBy`, `crossLinked`, `allocatedButUnclaimed`, `claimedButFree`, `unfollowableChains`, `catalogFullyParsed`, `isClean`. **Traversal MUST terminate on any input** (FR-038) — visited set plus a ceiling derived from volume capacity; a chain hitting the bound is recorded unfollowable, never followed. Tests in `UnitTest/EmuTests/VolumeIntegrityTests.cpp` including cyclic and self-referential chains (SC-010)

**Checkpoint**: the decoder no longer loses data silently, and the integrity pass
exists for all four of its consumers.

---

## Phase 3: User Story 3 — Read a disk image's contents (P1)

**Goal**: list a volume and extract files from it, across every mountable format.

**Independent test**: quickstart §US3 — list a known disk and confirm name, type,
size, and lock state for every file plus free space; extract one and compare
byte-for-byte; repeat against `.dsk`, `.do`, `.po`, and `.woz` of the same content.

**Why first**: a migrating developer's source is on Apple II disks and cannot be
edited until it comes off. Nothing else in the feature is reachable for them
until this exists.

- [x] T012 [US3] Implement `Dos33Volume` read side in `CassoEmuCore/Devices/Disk/Dos33Volume.h/.cpp` — **no DOS 3.3 reader exists today**: walk the VTOC (T17 S0) and catalog chain (T17 S15→S1), decode each entry's name (30 bytes high ASCII), type byte with the `$80` lock bit masked off, sector count, and track/sector list; `Enumerate` and `Read`. Reuse `Dos33Skeleton`'s geometry constants rather than restating offsets. Tests in `UnitTest/EmuTests/Dos33VolumeTests.cpp`
- [x] T013 [P] [US3] Implement `Dos33Volume::BuildIntegrityReport` (walk every catalog entry's T/S chain into the claim map; compare against the VTOC free bitmap) in `CassoEmuCore/Devices/Disk/Dos33Volume.cpp`; damaged-volume cases in `UnitTest/EmuTests/VolumeIntegrityTests.cpp`
- [x] T014 [US3] Implement `ProDosVolume` read side in `CassoEmuCore/Devices/Disk/ProDosVolume.h/.cpp`: wrap the existing `ProDosReader::ExtractFile` behind `IVolume`, add `Enumerate` over the volume directory, and add **path-based traversal** so a file inside a subdirectory is reachable (FR-009). Where traversal is not yet supported, refuse a multi-component path with a clear reason — never silently truncate. Extend `UnitTest/EmuTests/ProDosVolumeTests.cpp`
- [x] T015 [P] [US3] Implement `ProDosVolume::BuildIntegrityReport` (walk seedling / sapling / tree block chains into the claim map; compare against the volume bitmap) in `CassoEmuCore/Devices/Disk/ProDosVolume.cpp`
- [x] T016 [P] [US3] Implement `AppleTextCodec` in `CassoCore/AppleTextCodec.h/.cpp`: high-ASCII ↔ host text with line-ending normalization both directions (FR-021); round-trip tests in `UnitTest/AppleTextCodecTests.cpp`
- [x] T017 [US3] Extend the command-line surface **additively**: add `Disk` to `CommandLineOptions::Subcommand`, a `DiskVerb` enum and operand fields in `CassoCore/CommandLineOptions.h`; **one row** `{ "disk", …Subcommand::Disk }` in `s_kSubcommands` and **one arm** calling a new `ParseDiskOptions` in `CassoCore/CommandLineParser.cpp`. Do not reshape the dispatcher. Accept `ls`→`list` and `rm`→`delete` aliases. **`UnitTest/CommandLineTests.cpp` must stay green — spec 019 is being developed concurrently against these files**; add disk-grammar cases beside the existing ones
- [x] T018 [US3] Define the file seam `IDiskFileIo` in `CassoEmuCore/Devices/Disk/IDiskFileIo.h` — read all bytes, write all bytes, stat (size + modification time), exists, delete, atomic replace — plus a `FakeDiskFileIo` in `UnitTest/EmuTests/` that serves synthetic images from memory and can be told to fail, to report a changed stat, or to already hold a colliding name. **`UnitTest` does not link `CassoCli`** (verified: `UnitTest.vcxproj` references CassoCore, CassoEmuCore, Casso, Dxui only), so anything placed in `CassoCli` is unreachable by tests and violates Principle VI's litmus
- [x] T019 [US3] Implement `DiskCommandRunner` in `CassoEmuCore/Devices/Disk/DiskCommandRunner.h/.cpp`: takes `CommandLineOptions` and an `IDiskFileIo &`, and returns a result carrying the exit status, the stdout payload, and the diagnostic text. It owns every **decision** — verb dispatch, exit-status mapping (`0` clean, `1` succeeded-with-complaints, `2` produced no output, matching `as65` and `run`), and failure-message construction naming image, file, and reason (FR-031, FR-033). Covers spec US3 acceptance 4. Tests in `UnitTest/EmuTests/DiskCommandRunnerTests.cpp` against the fake seam
- [x] T020 [US3] Implement `list --long` and `get --out <file>` from the grammar in contracts/disk-subcommand.md — both are in the contract and neither had a task. Then reduce `CassoCli` to the irreducible edge: `Win32DiskFileIo` implementing `IDiskFileIo` over `ifstream`/`ofstream`/`ReplaceFileW`/`GetFileAttributesExW`, and a `DoDisk` that constructs it, calls `DiskCommandRunner`, writes the returned payload to stdout and diagnostics to stderr, and returns the status. No branching on outcomes and no message building in the exe — if a decision appears here, it belongs in T019. **Both go in a NEW `CassoCli/DiskCommand.h/.cpp`, not in `CommandLine.cpp`.** Two reasons: `CommandLine.cpp` is 1,222 lines and is the specific file GH #85 names, so appending to it worsens the condition this task exists to fix; and spec 019 is editing that same file (diagnostic positions, the help-text block), so a new file shrinks the overlap to the single `PrintUsage` registration line. **`Win32DiskFileIo::WritePayloadToStandardOutput` MUST switch the stream to binary mode before writing, and the call MUST carry a comment saying why.** This platform opens standard output in text mode, so the runtime rewrites every 0x0A byte as 0x0D 0x0A — extracting a binary containing 0x0A would silently produce a longer file with bytes that were never on the disk. **The fake cannot catch this**: the corruption happens in the runtime below the seam and only in real use, so the substitute records correct bytes and passes. That is precisely why binary payload is routed through the seam at all — one implementation makes the mode decision, instead of each call site making it and the second one omitting it. Scope the mode to the payload write; diagnostics and listings are text and need the translation **The 589-to-618 byte assertion CANNOT be automated — do not write it against the fake.** The translation happens in the runtime below the seam, so the substitute does not perform it and such a test passes whether or not the bug exists, while looking exactly like coverage. It cannot be raised a level either: unit tests may not touch real system state and no test may run the console binary. The byte check is therefore the manual procedure in quickstart.md, and what the automated suite pins instead is the runner-level payload against the extracted oracle plus the fact that the payload carries 29 line-feed bytes — the property that makes the edge's mode matter
- [x] T021 [US3] Cross-format extraction gate (SC-004): the same content as `.dsk`, `.do`, `.po`, and `.woz`; every file extracts byte-identically from each. Plus the damaged-volume cases from quickstart §US3.5–7 — partial track reports unrecovered sectors as unrecovered (never zeros), wholly unformatted track reports blank not damage. **Two shapes need synthetic fixtures because no real volume can reach them**, recorded so neither reads as a gap: ProDOS *tree* storage (needs more than 256 data blocks; the fixture volumes hold 280 and their largest file is 60) and *random-access* text (auxiliary type as record length; all 24 text files on the fixture volumes are sequential with auxiliary type 0). Real disks answer whether the reader understands the format; synthetic ones construct a chosen shape — a synthetic fixture cannot answer the first question, having been built by the same understanding it would test
- [x] T022 [US3] Runtime validation pass over quickstart §US3; fix what it finds

**Checkpoint**: US3 is complete and independently shippable — a developer can get
their source off an Apple II disk, which is the first thing they need.

---

## Phase 4: User Story 2 — Put a file onto a disk image (P1)

**Goal**: place a binary on a disk, replacing any file of the same name, with
every failure leaving the image byte-for-byte unchanged.

**Independent test**: quickstart §US2 — place a 512-byte binary as `PROG` at
`$6000`, boot, confirm `CATALOG` lists `B 004 PROG` and `BLOAD PROG` lands the
bytes; then run every failure mode and confirm the image hash is unchanged.
(`B 002` was this heading's arithmetic until T033 measured it — see that task.)

**Note**: delete lands here, not later. Replace is delete + write, and neither
filesystem can delete today.

- [x] T023 [US2] Generalize DOS 3.3 writing into `Dos33Volume::Write` in `CassoEmuCore/Devices/Disk/Dos33Volume.cpp`: allocate from the VTOC free bitmap, build the track/sector list, write data sectors, create the catalog entry, leave the bitmap consistent. `Dos33FileWriter::WriteHello` is a zero-parameter hardcoded emitter — treat it as a worked example of the structures, not a base to extend. Tests in `UnitTest/EmuTests/Dos33VolumeTests.cpp`. **Four decisions, recorded so they read as choices rather than gaps.** (1) **An existing name is REFUSED** with `ERROR_FILE_EXISTS` rather than replaced. FR-012 requires replacement to be computed as one whole and that is T027; refusing is the honest interim, and it is specifically the failure mode — a duplicate catalog entry — that must not exist in the meantime. (2) **Allocation consults the integrity report, not only the free bitmap.** A sector the bitmap calls free while a catalog entry still points at it is exactly what a bad delete leaves behind, and handing it out destroys the other file silently; those sectors are skipped and left for the report to name. (3) **A name being CREATED is validated** — non-empty, at most 30 characters, first character a letter, no comma, no control characters, and upper-cased on the way in. This is *not* the reverted printable-text check on names being READ: reading still imposes no rule at all, and the twenty backspace-drawn headings on Merlin's disk still list. The direction is what differs — DOS's own SAVE refuses these, so creating one produces a file the guest cannot open. (4) **Illegal input maps to Win32 codes, not `E_INVALIDARG`**, diverging from `contracts/volume-api.md`'s error table: `E_INVALIDARG` marks a coding error and asserts, and a filename is user input. `ERROR_INVALID_NAME` / `ERROR_DISK_FULL` / `ERROR_ACCESS_DENIED` / `ERROR_FILE_EXISTS` carry the same meanings without the assert
- [x] T024 [US2] Implement `Dos33Volume::Delete` with free-space return in `CassoEmuCore/Devices/Disk/Dos33Volume.cpp`: free **only** sectors the integrity report shows this file uniquely owns, report the rest as leaked, and remain available for a file whose T/S chain is damaged so a bad file cannot strand the volume (FR-011). Warn distinctly when `catalogFullyParsed` is false (FR-040). **Two decisions.** (1) **`IVolume::Delete` keeps its two-argument shape**; the account of what was freed, what leaked, and why lives on a `Dos33Volume`-only three-argument overload taking a `DeleteOutcome` (new, in `VolumeTypes.h`). Widening the interface mid-flight would have broken T025/T026 in another session for no benefit yet — nothing consumes the account until `put`/`delete` reach the runner in T032. **T026 should take the same shape, and T032 is where the pair gets lifted onto `IVolume` if it wants to.** (2) **The entry is tombstoned the way DOS does it** — `$FF` into the track byte, the original track stashed in the last byte of the name — so an undelete tool still finds the file. A zero-sector entry frees nothing because the chain walk returns before it starts; the discriminator is the entry's own sector count and **not** the `$7F/$7F` pointer, which is the one mutation the suite failed to catch until a fixture was built to separate them
- [x] T025 [US2] Add **tree growth** to the ProDOS writer (master index block of index blocks) in `CassoEmuCore/Devices/Disk/ProDosSkeleton.cpp` / `ProDosVolume.cpp` — the existing writer handles seedling and sapling only; extend `ProDosVolume::Write` to grow storage type as size requires (FR-008). Block-accounting asserted against `BuildIntegrityReport`, not by inspection. **Four decisions, recorded so they read as choices rather than gaps.** (1) **Placement is implemented in `ProDosVolume`, not by extending `ProDosFileWriter`** — see the divergences block; the existing writer's argument handling asserts, which is wrong for user input, and its allocator consults the bitmap alone. (2) **A tree is constructible on a 280-block volume after all**, which the task text and `research.md` both imply it is not: 257 data blocks plus two index blocks plus a master is 260 of the 273 free, so the shape is reachable with a payload of 131,073 bytes. What the fixture volumes cannot do is *carry* one, their largest file being 60 blocks — a distinction worth keeping, because it is what makes the tree tests self-consistency checks rather than format evidence. (3) **The one piece of real-format evidence available is the sapling arithmetic**: `/APPLESOFT`'s `WHATSIT.A.Q` is 29,798 bytes in 60 blocks, written in 1985, and a file of that length written here must occupy 60 too. `Volume_Write_ASaplingSizedFile_MatchesTheShapeRealProDosGaveOne` is that check, and it is the only test in the pair that can fail because Apple disagreed with us. (4) **`PlaceFile` validates the block list against the shape before consuming it**, deriving the expected count itself rather than from `OverheadBlocksFor` — mutation testing showed that a wrong overhead count otherwise walks off the end of the list rather than failing
- [x] T026 [US2] Implement `ProDosVolume::Delete` with volume-bitmap free-space return in `CassoEmuCore/Devices/Disk/ProDosVolume.cpp`, under the same unique-ownership rule as T024. **Four decisions.** (1) **The three-argument `DeleteOutcome` overload matches T024's shape** rather than widening `IVolume`; lifting the pair onto the interface stays T032's call. (2) **The tombstone is ProDOS's own** — the storage-type nibble goes to zero and the name length and name stay, so an undelete tool still finds the file. That forced a correctness fix in the readers: `CollectEntries` and `ProDosReader::ExtractFile` treated only a wholly-zero type/name byte as an unused slot, so a deleted entry would have kept listing and kept resolving by name after its blocks were handed to somebody else. Both now test the nibble. (3) **Removal is gated on destroy-enable (`$80`), not write-enable (`$02`)** — the access byte can express one without the other and ProDOS gates the two operations separately, so `Write` and `Delete` consult different bits. (4) **A subdirectory is refused** with `ERROR_DIRECTORY_NOT_SUPPORTED`, ahead of the lock check: this layer does not traverse into one, so the report credits it with its key block alone and deleting it would free that single block while silently orphaning everything beneath
- [x] T027 [US2] Implement replace in both volumes (FR-012): compute the complete post-replacement buffer as a **whole** — never a delete applied to the target followed by a write, which would free the old file and lose it outright if the write then failed. Test that a write failing after the delete step leaves the original file intact. **Three decisions.** (1) **The removal is staged into a working buffer and the placement runs over that**, rather than being open-coded as a third path. The forbidden shape is a removal APPLIED to the caller's image; staging is not that, because nothing here writes through to the input and only a finished buffer is ever assigned to `outBuffer`. What it buys is that one code path — `AddFile` on each volume — lays every file down, so placement cannot drift between the add case and the replace case. `Write_AReplacementThatCannotBeCompleted_LeavesTheOriginalWhereItWas` is the failure case: the volume is filled so the replacement cannot fit even once the original's sectors come back, and the assertions are that nothing is handed back, the source image is byte-identical, and the file that would have been replaced still reads. (2) **On ProDOS a replacement needs destroy permission as well as write permission**, because it releases the old file's blocks and ProDOS gates the two on different bits of the access byte. A file marked writable but not destroyable is refused — conservative, and correct while this layer rebuilds a file rather than rewriting it in place. Recorded because it is a consequence of the shape, not an independent rule. (3) **`Write_ProducesNothingWhenItRefuses_AndNeverTouchesTheInput` was re-pointed at an illegal name on both filesystems**; it asserted the `ERROR_FILE_EXISTS` interim this task removes, and the property it exists for — a refusal touches neither the output nor the source — is unchanged
- [x] T028 [US2] Wire the pre-commit self-check (FR-039): every `Write` and `Delete` runs `BuildIntegrityReport` over its **computed result** and refuses to return a result that fails it — sector claimed twice, free map disagreeing with the catalog, chain broken by the edit. Feed the path a deliberately corrupted result and confirm refusal (SC-009). **Do not gate on `IsClean()`** — see the divergences block above. **Four decisions.** (1) **The prerequisite landed first**: the DOS 3.3 integrity pass now credits the volume its own boot and catalog tracks, so a healthy disk and a damaged one are distinguishable at all. (2) **The comparison is "not worsened", not "empty"** — the wording this file previously carried would have refused correct writes on a volume already carrying a disagreement, and two existing tests construct exactly that state. `VolumeIntegrityReport::IsSafeToCommitAfter` compares membership on `crossLinked` and `claimedButFree`, size on `allocatedButUnclaimed` and `unfollowableChains`, and asks `catalogFullyParsed` separately. (3) **The check and the hand-off are one call.** `HandBackVerifiedResult` verifies and then assigns `outBuffer`; a check sitting beside the assignment can be deleted while the assignment stays, and no input can make correct code produce a bad buffer, so the suite could never see that. Routing the hand-off through the check makes deletion loud. Replacing the call with a bare assignment is still invisible — measured by mutation, recorded rather than solved. (4) **Refusal is driven through the public entry point with a deliberately corrupted buffer**, one case per clause, each built so only that clause can reject it — including two that keep every set the same SIZE and move which unit is in it, which is what makes membership comparison the rule rather than counting

  **The open question this task carried is answered.** Free-map agreement and
  chain reachability DO separate `The Print Shop Color side B.woz` from a
  healthy DOS 3.3 volume, decisively, where names could not — the measurement is
  in the "do not retry" block above. Nothing was implemented from it: both
  signals are magnitudes, acting on either is copy-protection detection under
  another name, and the name check stays reverted
- [x] T029 [US2] Implement the bit-stream write path in `CassoEmuCore/Devices/Disk/`: diff the pre- and post-edit sector buffers, call `RenibblizeTracks` for **only** the changed tracks, and refuse via `TrackWritability` when the write needs a track that is not writable (FR-016, FR-017). Test that writing to a WOZ leaves untouched tracks bit-identical, and that a `Partial` track refuses the write with the image unchanged (SC-008). **Write and re-read across all four formats** — `.dsk`, `.do`, `.po`, `.woz` (FR-015): extraction is covered across formats by T021, but sector ordering differs between DOS and ProDOS order and a mistake there is silent, so the write side needs its own matrix. **One defect found and six decisions.** (1) **The `.po` reorder was already wrong** — see the divergences block; the fix shipped as its own commit ahead of this one, because it is a read-path bug as much as a write-path one. (2) **The write path is `VolumeImage::Save`**, the mirror of `Load` and in the same pair, because the question "which container is this and what does its layout mean" is one question and had one answer already. It derives the pre-edit sector buffer itself from the original file bytes rather than taking it from the caller, so the two sides of the diff cannot disagree about what the image held. (3) **`ChangedTracks` is public and diffs whole tracks**, since a bit stream is written a track at a time; a mutation that compared only each track's first sector is caught by the four-format matrix. (4) **A sector-order container is never judged for writability.** There are no tracks to be unwritable — `.dsk`/`.do`/`.po` hold sectors and nothing else — so `TrackWritability` is consulted on the `.woz` arm only. (5) **The refusal reason is an out-parameter, and it matters which one is used.** Asserting only that *something* was said passes when the whole-image check is deleted: `AreTracksWritable` inherits the image verdict, so the write is still refused and only the explanation degrades, from "the image holds data between tracks" to blaming a track for it. Measured by mutation; the test now pins the wording. (6) **The FR-017 test blanks a track first.** A WOZ produced by nibblizing a sector image re-encodes to itself, so an implementation rewriting all thirty-five tracks passes a bit-comparison against it. The sentinel track is one whose bits a re-encode could not reproduce, and the test asserts it is not in the changed set before asserting its bits survived
- [x] T030 [US2] Implement the **commit policy** in `CassoEmuCore/Devices/Disk/CommitPlan.h/.cpp` as pure functions over data: temporary-name derivation from the target path and an attempt counter, the staleness comparison (`IsStale (recordedSize, recordedTime, observedSize, observedTime)`), and the ordering rule that the temporary is removed on any failure. Names must not collide between concurrent invocations (FR-013). Tests in `UnitTest/EmuTests/CommitPlanTests.cpp` — these are decisions, not syscalls, and they must be unit-testable. **Three decisions.** (1) **The name derivation takes an invocation tag as well as the attempt counter, diverging from this task's own wording** — because the two inputs it names cannot deliver the property the same sentence demands. Two invocations both start at attempt zero and both look before they leap, so both can find the name free in the same instant and one then commits the other's bytes. The attempt counter is what steps over a temporary somebody *abandoned*; the tag is what separates two *live* invocations. `TemporaryPathFor` stays a pure function of its arguments; `NextInvocationTag` (process id plus a per-runner sequence) is the one impure thing in the file and is kept separate for exactly that reason. The discriminating test compares two tags **at equal attempt** — varying the attempt as well would pass against a derivation that ignored the tag completely. (2) **The ordering rule is keyed on the furthest step ATTEMPTED, never the furthest COMPLETED.** A write that fails partway has already created the file and put some of the bytes in it, so "the write failed" must mean "a temporary may exist". `FakeDiskFileIo::failNextWrite` now leaves a partial file behind for the same reason: a substitute that fails cleanly lets a commit path that never cleans up after a failed write pass every test written against it. (3) **The rule is asked once, at the single exit, from a `Progress` record** — a removal written beside each bail-out is a removal the next bail-out forgets
- [x] T031 [US2] Wire the staleness re-verify and the best-effort probe into `DiskCommandRunner` (T019): record size and modification time at read via `IDiskFileIo`, re-verify immediately before commit and refuse if either changed (FR-036); best-effort exclusive-open probe refusing when **another** holder has the file open, with help text that does not imply it detects Casso (FR-035). Tested against the fake `IDiskFileIo`, which can report a changed size or time on demand. **Four decisions.** (1) **`OpenImage` and `CommitImage` are PUBLIC on the runner**, and `OpenVolume`'s five out-parameters are folded into one `OpenedImage`. `put`/`delete` do not reach the runner until T032, so without a public entry point the entire commit path would have shipped untested — and the stamp only means something if it is taken at READ time, so keeping the two in one object makes it awkward to write a commit that forgot to record one. **T032's `put` is `OpenImage` → `IVolume::Write` → `VolumeImage::Save` → `CommitImage`**, and needs no new seam. (2) **A stamp the platform declined to give is recorded as ABSENT and refuses the commit**, rather than defaulting to zero and comparing two zeros — a guarantee quietly not applied is indistinguishable from one applied. Reading does not need it and is unaffected, which is its own test. (3) **Staleness returns `STG_E_NOTCURRENT`**, which means precisely "the object changed since it was last read". The Win32 table has no code for it and a near-miss from that table reads as a different problem in a log. Not `E_INVALIDARG` — this is external state, not a coding error. (4) **The in-use help text is a constant on `DiskCommandRunner` that the console executable prints.** `UnitTest` does not link `CassoCli`, so a claim written in the help block there is a claim nothing can check — and FR-035's actual demand is about the *wording*. The test pins the disclaiming clause, not merely that something was said
- [x] T032 [US2] Add `put` and `delete` verbs to `ParseDiskOptions` and `DiskCommandRunner` (`--as`, `--type`, `--addr`, `--text`/`--basic`/`--verbatim`), with write-protect and locked-file refusals reported in intelligible terms rather than raw platform codes (FR-014). Use `--verbatim`, **not** `--raw` or `--binary`: both already name assembler output shapes (`OutputFormat::Raw`, `OutputFormat::Binary`) in this same parser, and `--verbatim` says what it does — the other two selectors transform the bytes, this one does not. **`--verbatim` means no CHARACTER conversion — it does NOT mean raw sectors.** Length and header semantics still apply, because those are how a filesystem records where a file ends: they are the file's identity, not a transformation of it. So `get --verbatim` yields the file's logical bytes with high bits and line endings untouched, never trailing sector slack, because "give me this file" must not hand back whatever happened to be in the rest of the last sector. **Gate it on FILE equality, not image equality**: `get --verbatim`, `put --verbatim` unchanged, re-read, and assert the bytes match. Image equality is the wrong assertion — a DOS 3.3 file occupies whole sectors, so slack past the recorded length differs from whatever was there before, and a `put` that reallocates changes the image for reasons having nothing to do with conversion. **Assert sector reuse separately** if image stability is wanted; do not conflate the two. Extract-edit-replace is the workflow US3 exists for, and it must not perturb bytes the user did not touch. **Five decisions.** (1) **The three-argument `Delete`/`DeleteOutcome` pair is now ON `IVolume`**, the call both T024 and T026 deferred here. The runner holds only an `IVolume &`, and the account is exactly what a caller reporting to a user needs; leaving the pair concrete would force every such caller to re-derive which filesystem it holds and call the type directly, reinstating the branch this seam exists to remove — once per consumer, with spec 021's disk manager the next one. The two-argument form stays, because leaked-space accounting is not information to force on a caller that does not want it. Measured by mutation: swapping the three-argument call for the two-argument one leaves a delete over a damaged chain reporting a clean success. (2) **The grammar was already complete.** T017 landed `put`, `delete`, `--as`, `--type`, `--addr` and all three encoding selectors in `ParseDiskOptions`, so this task added no parser rows — only the one case the grammar lacked a test for: `--verbatim` overriding an earlier `--text`, rather than being merely the default and therefore indistinguishable from a flag that was parsed and dropped. (3) **`--type` takes each filesystem's OWN spellings and refuses anything else**, naming what that filesystem does take. `A` is Applesoft on DOS 3.3 and means nothing on ProDOS, so one shared table would resolve one filesystem's letter against the other's numbering. With no `--type`, the type follows the conversion asked for — text for `--text`, a binary otherwise — because defaulting to a binary would refuse a text placement for want of the load address a binary needs, which is a confusing way to be told the type was wrong. (4) **Write protection is described at the REPLACE, not by the volume layer.** Nothing about an image's contents says it may not be written, so the volume computes a perfectly good result and the platform denies access at the last step; the message has to come from there or the user is left with two candidate causes. `FakeDiskFileIo::nextReplaceError` exists so that refusal can be driven — a fake answering one code for every replace failure would let a message naming the wrong cause pass. (5) **The full-volume refusal does NOT name the shortfall**, which quickstart §US2 case 4 asks for, and this is a deferral rather than an omission. How many units a placement needs is known only inside the allocator — data sectors plus track/sector lists on one filesystem, index and master-index blocks on the other — and computing it in the runner is a second implementation of that arithmetic, which is precisely the shape that produced the `.po` interleave defect. Reporting it honestly means the allocator returning a shortfall, which changes `IVolume::Write`'s signature; left for T034/T048 rather than guessed at here
- [x] T033 [US2] Guest-visible gate (SC-003): real-CPU tests — place a binary on a DOS 3.3 image, boot, `CATALOG` shows the placed file, `BLOAD PROG` lands bytes at `$6000`; same payload on ProDOS shows type `BIN` aux `$6000`. Attempt to overwrite the stock master's locked `HELLO` (type `$82`) and confirm refusal. **`B 002 PROG` is wrong arithmetic and this task and quickstart §US2 both carried it.** A 512-byte binary stores 516 bytes once DOS's four-byte load/length header is inside the file, which is three data sectors, and the track/sector list is a fourth: `CATALOG` reads **`B 004`**. `002` is what a payload of 252 bytes or fewer produces. Measured against the implementation in T023 (`Write_BinaryOntoAFreshVolume_ReadsBackByteForByte` asserts four sectors); assert what the file actually occupies rather than the number in the prose. **The corrected arithmetic is now confirmed by the guest itself**: DOS 3.3 prints ` B 004 PROG` and `BLOAD PROG` puts all 512 bytes at `$6000`; ProDOS prints ` PROG            BIN       1  <NO DATE>        <NO DATE>            512 A=$6000` and `BLOAD PROG` with no address given lands them at `$6000` again. Both rows are asserted whole. **Six decisions.** (1) **These cases FAIL when the cached master is absent, and the harness beside them skips.** `CatalogReproductionTest` predates the rule and is deliberately left alone; a case whose entire subject is what a guest does cannot borrow a skip, because the skip is indistinguishable in the output from a run. Verified by mutation rather than by inspection: renaming the cache file leaves exactly the two master-dependent cases red. (2) **The ProDOS half runs against the committed `/APPLESOFT` fixture rather than a built ProDOS blank.** `BlankDiskBuilder`'s bootable ProDOS path needs the ProDOS Users Disk, which is not cached on this machine, and side A of the Merlin set launches `MERLIN.SYSTEM` instead of BASIC.SYSTEM — so `/APPLESOFT` is the only ProDOS image reachable here that offers a prompt where `CATALOG` and `BLOAD` can be typed. It also removes the cache dependency from that half entirely. (3) **Eighty columns before the ProDOS listing.** `CAT`'s short form stops at the block count, so the auxiliary type — the whole ProDOS claim in this task — is simply not on a forty-column screen; the long form carries it and needs the width. Measured: at forty columns the row ends at `1  <NO DATE>`. (4) **DOS 3.3's catalog pager is indistinguishable from a prompt to `MachineIdle`** — drive stopped, screen still, and a short bottom row. A test that stops there types its next command into the pager, where the FIRST CHARACTER is swallowed as the keypress the pager was waiting for and the rest arrives as a different command. That is not hypothetical: the first draft's `BLOAD PROG` became `LOAD PROG`, the payload never reached `$6000`, and the screen still looked plausible. Every typed line therefore pages to a prompt before the next one, and a prompt is identified by its glyph rather than only by its length. (5) **The hazard bound is a cheap check asked BEFORE any machine starts**: the written container is decoded through the DRIVE, enumerated, and the placed file read back off it. An image written in the wrong sector order fails there in milliseconds instead of by a 6502 executing whatever it managed to load — the failure mode T029 measured at over a gigabyte of trace. (6) **The locked case takes the GUEST's word for the lock.** Our own reader would only restate the `$82` it parsed; DOS drawing `*A 003 HELLO` beside its own greeting is evidence from outside the code, and the refusal is then asserted over those same bytes — one reason, in words, no platform code, image byte-identical, no stray file
- [x] T034 [US2] Failure-mode gate (SC-005) as **unit tests against the fake `IDiskFileIo`**, not against real files: for **every** documented failure — volume full, locked file, write-protected image, illegal name, unwritable track, stale target — assert the target's bytes are unchanged, the resulting image still parses as a mountable volume, and no temporary remains in the fake's file table. Test Isolation is NON-NEGOTIABLE, so the seam is what makes this suite legal; the real-file version of the same checks is the single manual pass in T035. **Eight decisions.** (1) **The gate is its own file** — `UnitTest/EmuTests/DiskFailureModeTests.cpp`, eight cases, one per mode plus a delete-side locked case. The per-mode tests already in `DiskCommandRunnerTests.cpp` stay and are not duplicated away: those pin the **wording** a user reads, this pins the **image** left behind, and the three invariants are asked as **one call** so a case cannot answer two of the three and still look complete. (2) **The in-use case is the seventh mode** and belongs here even though the task text lists six: it is documented, `FakeDiskFileIo::reportHeldByOther` exists for it, and until now nothing exercised it through a verb. (3) **Mountability is NOT redundant with byte equality — it guards the MATERIAL.** For the synthetic volumes the oracle is a re-derivation rather than a fixture read, so a builder that stopped producing a volume would corrupt the seed and the oracle identically and every byte comparison would stay green. Measured: mutating `Dos33Skeleton::Write` to stamp a different volume number leaves byte equality satisfied and is caught only by the mount check. (4) **A refusal must name ONE reason**, asserted in every case, and this is the assertion that carries the deleted-bail-out mutations. Removing `RunPut`'s or `RunDelete`'s bail after a volume refusal leaves the image untouched anyway — `VolumeImage::Save` refuses a zero-length buffer — so the three invariants cannot see it; the tell is a **second** diagnostic line, one naming the real cause and one blaming the render. (5) **The full-volume case must be placed as TEXT, not as a binary.** A DOS 3.3 binary records its length in two bytes, so an oversized binary is refused for being longer than the filesystem can record *before* the volume's room is consulted — a perfectly good refusal, and not the one this case is asking about. Written as a binary first, and the mode was wrong until the assertion said so. (6) **The stray-file check is exact membership, not only `HasNoTemporaryFiles()`.** The sweep answers for the name this tool derives; comparing the whole file table answers for anything left under a name it does not. (7) **The unwritable-track case runs two passes** for T029's reason — the track it damages has to be one the write genuinely needs, and damaging one first moves the allocation. It erases half a track's bit stream rather than patching a checksum, which needs no 4-and-4 knowledge and still leaves address fields standing in the surviving half, so the outcome is `Partial` and the case asserts that before using it. (8) **The stale case perturbs the RECORDED stamp, not the observed one.** A whole verb runs in one call and the read is the first stat it makes, so the fake's one-shot switch lands there; `IsStale` is an inequality, so the comparison reached is the same one either way. The observed-side version stays in `DiskCommandRunnerTests`. **What this task did NOT do**: T032 parked the full-volume shortfall here, and it stays parked — reporting it honestly changes `IVolume::Write`'s signature, which is mechanism rather than verification and does not belong in a task whose whole definition is "unit tests against the fake". T048 is its home
- [x] T035 [US2] Runtime validation pass over quickstart §US2 plus the **manual** interrupted-write check — the one sanctioned real-file exercise: kill the process mid-commit, confirm the original is intact and bootable and no temporary remains. Crash safety cannot be unit-tested, which is exactly why it is the part done by hand; everything else in T034 runs against the seam. **Killing the process from outside cannot reach the window, and that is measured rather than assumed.** Seventy attempts across two methods — polling, and a `FileSystemWatcher` armed on the temporary's creation, which fired on 39 of 40 tries — produced zero runs ending byte-identical to the original: every kill landed *after* the replace, because the gap between the temporary's last byte and the rename is shorter than the time Windows takes to stop a process from another one. FR-012 was never violated and no temporary ever survived, but nothing about the instant in question was demonstrated, because the instant was never reached. **So the interruption is now raised from inside the code under test.** `Win32DiskFileIo` reads `CASSO_DIAG_DISK_ABORT` and stops at one of two named stages: `during-write`, with half the temporary written and flushed, and `before-replace`, with the temporary complete and the target not yet touched — the point nothing had ever exercised in the real code. The stop is `TerminateProcess` on the process itself: no unwinding, no destructors, no cleanup, because that is what a crash is and a clean `exit()` would run exactly the tidying this is meant to do without. It prints one line to standard error and exits `0xDEAD` (57005). **IT CANNOT EXIST IN ANY BINARY NOBODY DELIBERATELY ARMED, which is stronger than where this started.** The first version was gated on `#ifdef _DEBUG`, which kept it out of shipping builds and left the actual hazard untouched: the stage was chosen by an environment variable, and an environment variable stays set — one `setx` and every Debug `put` on that machine aborts, with nothing in the code wrong. Declarations, definitions and both call sites now sit inside `#if defined(_DEBUG) && defined(CASSO_DIAG_DISK_ABORT)`, and **no project configuration sets that define**, so arming takes `$env:CL = '/DCASSO_DIAG_DISK_ABORT'` and a `-Target Rebuild` — an act, not an inheritance. The variable of the same name still aims an armed binary at a stage and can arm nothing by itself, because without the define there is no code to read it. Verified in the bytes rather than in prose: `CASSO_DIAG_DISK_ABORT`, `during-write`, `before-replace` and the `stopping hard at` diagnostic are **absent from a stock `x64\Debug\CassoCli.exe` and from `x64\Release\CassoCli.exe`** — searched in eight-bit and in both UTF-16 alignments, since the stage names are wide literals — and **all four are present** in a Debug binary built with the define, which is what stops the clean result from being a search that never finds anything. The build command and the check are in `quickstart.md` §US2 and at the code. **This does not overlap the substitute file interface and does not replace it.** The substitute covers every decision above the seam — ordering, each refusal, the cleanup rule — deterministically over nothing real, which is T034; what it structurally cannot cover is `Win32DiskFileIo`, since the test project does not link the console executable and the property at stake is what a real file looks like once the process has stopped existing. Both reasons are written into the code. **Measured, x64 Debug, the DOS 3.3 System Master as the target.** `before-replace`: exit 57005, target SHA-256 unchanged, and the target still **boots** — mounted in the emulator it reaches `APPLE II / DOS VERSION 3.3 SYSTEM MASTER` and a `]` prompt, which a `disk list` would not have shown. `during-write`: exit 57005, target SHA-256 unchanged, temporary 71,680 bytes, exactly half the image. Re-running the same `put` after either abort succeeds and produces an image byte-identical to one from an uninterrupted run, which also boots. **The procedure discriminates, checked by mutation rather than by inspection**: neutering the hard stop leaves the run exiting 0 with the original *changed*, and moving the `before-replace` call one line later — after `MoveFileExW` instead of before it — leaves the original changed and no temporary, so the green result is about the instant and not about the mechanics of the switch
  **Limitation, and it is a real one: the temporary SURVIVES a hard abort, so "no temporary remains" is NOT satisfied.** A hard stop cannot run cleanup, so the leftover itself is the honest expected outcome; what is not expected is that **no later run reclaims it either**. Each invocation stamps its own tag into the temporary's name, so an orphan sits at a name no future invocation will ever choose or even look at — a subsequent `put` in the same directory succeeds, commits correct bytes, and steps straight past the orphan, which then stays until somebody deletes it by hand. Both abort stages leave one: a full-length file after `before-replace`, a half-length one after `during-write`. This contradicts research.md's R-007, which states the intent as "a hard kill must not litter" and calls a leftover temporary "a real if minor violation". Recovery is unaffected and no user data is at risk — the guarantee FR-012 makes is about the *target*, and the target was byte-identical every time — but the litter claim is currently false. Not fixed here: a sweep of sibling temporaries is a behavior change to the shipping commit path with its own tests to write, and this task's job was to make the check performable and report what it found. **Re-measured after the re-gating, and the leftover is now demonstrated rather than reasoned**: the work directory finished the pass holding exactly the two orphans — 71,680 and 143,360 bytes — after two aborts followed by *two successful puts*, the recovery run and an uninterrupted control. The successful runs cleaned up after themselves perfectly and swept nothing

**Checkpoint**: the minimum viable loop is closed — assemble, place, boot, run.

---

## Phase 5: User Story 4 — Make the disk boot the program (P2)

**Goal**: boot straight into the developer's program with no typing.

**Independent test**: quickstart §US4 — set a boot program, boot the image, the
program runs unattended.

- [x] T036 [US4] Implement `Dos33Volume::SetStartupProgram` in `CassoEmuCore/Devices/Disk/Dos33Volume.cpp`: patch the greeting filename **in place at T01 S09 `+$75`**, 30 bytes, high ASCII, `$A0`-padded (verified against the stock master in research R-003). No catalog change, no chaining file. **Three decisions.** (1) **The name written is the one the CATALOG stores, byte for byte, not the one the caller typed.** DOS matches the greeting against catalog names on the disk, so a lower-case spelling — or one of the control characters a real disk legitimately carries in a name — would name a file DOS then fails to find, and the disk would boot to an error with the field looking perfectly plausible. The bytes are copied out of the entry rather than re-encoded. (2) **A volume with nothing on the tracks DOS occupies is REFUSED** (`ERROR_NOT_SUPPORTED`). The format reserves tracks 0–2 whether or not anything was installed in them, so the patch would land in space nothing reads and the command would report success for a disk that still cannot boot at all. (3) **The patch goes through `HandBackVerifiedResult` like every other mutating call**, though it moves no sector: the standard belongs to the seam, not to the operation. The test that carries the task compares the WHOLE image and requires every differing byte to fall inside the thirty, which is what makes "no catalog change, no chaining file" an assertion rather than a claim. The offset is restated in the test from the published layout rather than borrowed from the code, and is corroborated a second time by the Merlin DOS 3.3 fixture, which carries `HELLO` in exactly the same place
- [x] T037 [US4] Implement `ProDosVolume::SetStartupProgram` in `CassoEmuCore/Devices/Disk/ProDosVolume.cpp`: reorder the volume directory so the chosen `SYS` file is the first the boot path finds. **Deliberately not shared with T036** — the two mechanisms differ in kind, and a unified "write the boot name" helper would be wrong for both. **They stayed separate, and what each writes differs in kind**: T036 writes thirty bytes of high ASCII into DOS's own code on track 1 and touches nothing else; T037 writes no name anywhere and swaps two 39-byte directory records. **Three decisions.** (1) **The records are SWAPPED, not repacked.** Every other record keeps its position, its dates and its pointers, and the volume header's tally and every block pointer are untouched — which is also why the integrity comparison passes trivially. (2) **The kernel is excluded from the candidates.** `PRODOS` is a `SYS` file like any other, the boot block finds it by NAME rather than by position, and nominating it would ask a running ProDOS to load ProDOS. Both shipped fixtures corroborate the rule from outside the code: `/MERLIN` lists `MERLIN.SYSTEM` first and launches it, `/APPLESOFT` lists `PRODOS` first and launches `BASIC.SYSTEM`. Naming the kernel is refused with `ERROR_BAD_FILE_TYPE`, the same code a non-`SYS` file gets, under one message covering both. (3) **A program already in front leaves the volume byte-for-byte as it was**, rather than being rewritten to what it already says
- [x] T038 [US4] Add the `boot` verb to `ParseDiskOptions` and the runner (FR-024); refuse a program not present on the volume, naming the missing file (FR-025). **The grammar was already complete**, as it was for T032: T017 landed `boot` in `s_kDiskVerbs` and the usage block in `CassoCli/CommandLine.cpp` already documents `disk boot <image> <path>`, so this task added the runner arm, its refusals and its tests, and changed no parser row. **Two decisions.** (1) **A binary named as a DOS 3.3 greeting is a COMPLAINT, not a refusal.** Measured with a real processor on the stock master: the disk boots and the program is never executed, because the command a booting DOS 3.3 issues is RUN — which `Dos33FileWriter::WriteHello`'s own comment has said since spec 017. Refusing would be wrong, since a disk whose boot command has been patched by hand is a real thing and its owner knows what they did, so the name is set, the status is 1, and the message says what DOS will do with it. (2) **`UnbuiltVerb_ReportsFailureRatherThanDoingNothingQuietly` is DELETED rather than moved again.** It had migrated from verb to verb as each was built; `boot` was the last one, so pointing it anywhere now leaves it green for a reason unrelated to its name. `UnknownVerb_SuggestsTheOnesThatExist` covers the property it existed for
- [x] T039 [US4] Boot gate: real-CPU tests — a DOS 3.3 image with a set boot program runs it unattended after DOS loads; a ProDOS image launches the chosen system program. **Five decisions.** (1) **Nothing is typed, at all.** US4's claim is that the program runs unattended, so a test that helped the guest along would be testing something else — which also keeps the whole case clear of the pager hazard T033 records. (2) **Every case carries its own control, and on ProDOS the control had to be the same disk pointed at the OTHER system program.** A placement alone can land the new file in front of `BASIC.SYSTEM`, because ProDOS reuses directory slots in place and `/APPLESOFT` has a hole early in its directory: the first draft used "placed but not nominated" as the control, the kernel launched it anyway, and the case would have passed with the reorder doing nothing. (3) **The primary witness is two bytes the program writes to memory, not the screen.** A screen can be made to say anything; those bytes are written only by the instructions that write them, so they say the program EXECUTED rather than merely got loaded. (4) **The DOS 3.3 program is Applesoft, and the second case is why.** A booting DOS 3.3 RUNs its greeting, so a binary named there is never executed — measured, and now pinned by a case that asserts the complaint and then asks the guest whether it agrees. (5) **The placed program initializes the text screen itself.** The harness enters at the boot ROM rather than through RESET, so the monitor's cold start never ran and the output hook, the text window and the cursor are whatever powering on left in zero page; a program printing through `COUT` writes into memory that is not the screen, and the screen shows the noise it powered up with. Measured that way first. The harness the placement gate carried moved into `UnitTest/EmuTests/GuestSession.h/.cpp`, so the paging rules exist once rather than in two copies that can drift

---

## Phase 6: User Story 5 — Boot with no operating system (P3)

**Goal**: an image that boots directly into a binary, with no DOS or ProDOS.

**Independent test**: quickstart §US5 — the payload runs measurably sooner than
the equivalent OS boot.

- [x] T040 [US5] Implement direct-boot image generation in `CassoEmuCore/Devices/Disk/DirectBootBuilder.h/.cpp`: a boot-sector loader that pulls the payload's sectors and jumps to it (FR-026). **Resolve the loader's sector capacity before starting** — R-010 deferred it, and FR-027's reported number is undefined until it is settled. Unit tests in `UnitTest/EmuTests/DirectBootTests.cpp` asserting the generated image's structure and the capacity boundary. **R-010 IS SETTLED AND THE ANSWER IS 183 SECTORS — 46,848 bytes — AND IT IS A MEMORY LIMIT, NOT A MEDIA ONE.** The disk holds 544 sectors past the loader's track and none of that is the constraint. What binds is that the payload has to fit between $0900 and $BFFF, so the capacity is `$C000 - loadAddress` bytes and 183 is simply its value at the lowest address a payload may load at. **Five decisions.** (1) **The loader re-enters the boot ROM's own read routine rather than carrying a sector reader.** The Disk II ROM already has one, its loop returns to $0801 after every pass with the sector number, the buffer page and the terminating count all in memory the loader owns, and re-entering it is what DOS 3.3's own boot0 does — measured by reading the stock master's track 0 sector 0, which builds a `JMP ($3E)` to `$Cs5C` exactly this way. What it buys is room in one sector for the part the ROM does not do, which is stepping the head. (2) **THE WINDOW'S LOWER EDGE IS A CONSEQUENCE OF (1), NOT A PREFERENCE.** The ROM's loop terminates by comparing against the byte at **$0800** — an absolute address inside the ROM — and it jumps to **$0801**, so page $08 has to stay the loader's for as long as anything is being read; $0300–$03FF is the ROM's decode table and secondary buffer; $C000 up is not memory. A payload at $0800 would be overwritten by its own load. (3) **The payload begins at track 1 sector 0, leaving track 0's fifteen spare sectors empty.** A first pass that sometimes reads a partial track costs more code than fifteen sectors are worth on a disk where memory is what runs out. (4) **Page N is written to the sector the DRIVE presents Nth, which is not the Nth sector of the buffer.** The loader asks the ROM for consecutive address-field numbers, and those are laid down in physical order; a buffer is in DOS logical order. Writing page N at logical sector N reads back perfectly through our own reader and hands the guest its pages shuffled — the same shape as the `.po` defect. `NibblizationLayer::DosFileIndexForPhysicalSector` answers it from the table that layer already owns rather than restating the skew. (5) **The layout oracle is Apple's.** `DirectBootTests` reads DOS 3.3's own logical-to-physical table out of the master's boot sector at absolute offset `$4D` and requires our mapping to be its inverse — evidence from outside the code, since every round trip through our own reader is an identity whatever the skew says. It also asserts the two orders actually differ, so a builder ignoring the skew cannot satisfy it vacuously
- [x] T041 [US5] Refuse a payload exceeding what the boot path can load, reporting the available capacity (FR-027); support an entry address different from the load address (FR-028), in `CassoEmuCore/Devices/Disk/DirectBootBuilder.cpp`. **Four decisions.** (1) **The capacity is stated by the thing that computes it.** `Build` takes an out-refusal string and fills it with a sentence naming the number; a caller re-deriving it would be a second implementation of the window arithmetic, which is the shape T032 parked for exactly this reason. (2) **Order matters and is asserted.** Empty payload, then load address, then size, then entry: an address outside the window has a capacity of zero, so asking about size first would blame the payload's length for an address problem. `Build_ARefusalNamesExactlyOneReason` drives a call that is *both* out-of-window and oversized and requires the address sentence, plus that the refusal carries no newline. (3) **A refusal hands back nothing.** The caller's buffer is compared against a sentinel it could not have produced, so "produced no image" is an assertion rather than a vacuous pass over an empty vector. (4) **The entry must lie inside the loaded bytes.** An entry past the payload has the guest execute memory nothing loaded, which is precisely the failure mode this branch spent T029 learning to avoid. Load-address and entry refusals share one Win32 code on purpose — both say the boot path was handed an address it cannot use — and the sentence says which. **Also: an unaligned load address is supported rather than refused.** The ROM reads whole pages into page-aligned buffers, so the payload rides behind a lead-in of the bytes from the start of its page up to it; that costs three lines and avoids an artificial restriction
- [x] T042 [US5] Gate (SC-007): real-CPU test — the direct-boot image reaches the payload in **under 25% of the emulated CPU cycles** the equivalent DOS 3.3 boot of the same program takes. Emulated cycles, not wall clock, so the result is deterministic across hosts. **THE 25% BAR IS NOT REACHABLE AGAINST THE WHOLE BOOT, AND THE REASON IS MEASURED RATHER THAN ARGUED — see the divergences block.** The gate applies the quarter to what the two DISKS spend, which is each whole boot minus the cycles the controller ROM spends before either disk's first byte executes. **Five decisions.** (1) **The witness is the program counter, not the screen and not a byte in memory.** The payload's last instruction jumps to itself, so the processor sitting there means the guest reached the developer's code. A memory sentinel polled during a boot would be satisfied the moment the ROM's own decode buffer happened to hold those two bytes — and the whole point of a cycle measurement is that it polls continuously. (2) **The shared constant is measured on BOTH images and required to be equal**, because subtracting it is only legitimate if it is the same number on both sides; both disks are entered at $0801 by the same ROM. (3) **The equivalent DOS 3.3 route is the binary placed as a type-B file plus a one-line Applesoft greeting that BRUNs it**, set through the `boot` verb. That is the whole of the alternative, given T038's finding that a booting DOS 3.3 RUNs its greeting. (4) **The payload calls nothing.** A direct boot never runs the monitor's cold start, so zero page holds whatever powering on left, and the monitor's character output masks every character with `INVFLG` at `$32`: measured, `CASSO DIRECT` arrived on screen as `B@RRN`DHRDBT`. The payload sets the video soft switches, clears the text page and stores its banner into screen memory itself. This generalizes T039's lesson — it is not only `COUT`'s hook that is uninitialized, it is everything. (5) **The arithmetic that makes the whole-boot form unreachable is asserted, not written down**: the case requires the ROM's fixed cost to exceed a quarter of a DOS 3.3 boot, so if that ever stops holding the gate goes red and somebody tightens it

---

## Phase 7: User Story 6 — BASIC source as a runnable program (P3)

**Goal**: place an Applesoft listing written as host text as a program the guest
can `RUN`.

**Independent test**: quickstart §US6 — place a listing, boot, `LIST` reproduces
the source.

- [x] T043 [US6] Implement Applesoft tokenization in `CassoCore/ApplesoftTokenizer.h/.cpp`: host-text listing → tokenized on-disk form with line-link fixups (FR-022). Settle the coverage boundary against a real listing — token spellings inside strings, `DATA` payloads, `REM` text. **Settle round-trip loss deliberately before writing the tokenizer**, and record the answer: detokenize/retokenize is the least likely of the three conversions to be exact, because tokenizers normalize spacing and canonicalize forms. It is also where loss is most surprising — someone extracting a text file opted into a conversion, while someone whose saved program comes back subtly reformatted did not. If exact round-tripping is not achievable, say so in the help text rather than letting a user discover it
- [x] T044 [US6] Implement detokenization (the reverse direction, FR-022) in `CassoCore/ApplesoftTokenizer.cpp`; round-trip tests. **The decision that decides the whole task is the spacing rule, and it is not LIST's.** A space is written BEFORE every token and AFTER every token except REM and DATA. Normal-position spaces are dropped on the way back and cost nothing; a space after REM or DATA is swallowed into the payload, so LIST's own rule would grow the listing by one space per round trip. The detokenizer also **refuses rather than guessing** — undefined token bytes, a token byte inside a string or a REM or a DATA payload, a link disagreeing with the layout, bytes past the null link — which is what makes "everything it accepts round-trips exactly" unconditional
- [x] T045 [US6] Refuse an untokenizable listing with the offending line number and text quoted (FR-023); wire `--basic` through `put` and `get`. **Four decisions.** (1) **`--basic` chooses the file type** — Applesoft on DOS 3.3, BAS on ProDOS — rather than inheriting the binary default a build loop wants, because a tokenized program under any other type is one the guest will not RUN. (2) **ProDOS's auxiliary type is set to $0801**, which is where that filesystem records a BASIC program's load address; DOS 3.3 records nothing and ignores it, so the value is set on both paths rather than branching in the runner. (3) **`--addr` with `--basic` is REFUSED, not ignored.** Applesoft keeps its program at $0801 and nowhere else, so accepting the flag would leave the caller believing something false about the result. (4) **The refusal quotes the line and names its number**, and where there is no number — a listing line that carries none — it names the line's position in the file instead, because inventing a number would point at the wrong line
- [x] T046 [US6] Gate: place a known listing, boot, `LIST` in the guest, confirm it matches the source. **Two witnesses, answering different questions, in `UnitTest/EmuTests/GuestVisibleBasicTests.cpp`.** The first types the listing into a booted machine and compares OUR tokenizer's bytes against the bytes Applesoft stored for the same text, read out of $0801 — the only oracle for tokenization that is not our own code, since a tokenizer checked against its own detokenizer agrees with itself perfectly while storing something no guest recognizes. The second places the listing through the command line a user types, boots the written image and reads LIST, then RUNs it: LIST alone is satisfied by a program whose tokens are right and whose links are not. The guest printed `10  HOME`, `20  PRINT "CASSO"`, `30  FOR I = 1 TO 3: PRINT I: NEXT` and `40  END`, and every row mentioning each needle is asserted to be exactly that row. A third case round-trips the master's own 419-byte HELLO — real vendor Applesoft nobody here wrote — byte for byte. The cheap structural questions are all asked before any processor starts, per T040's finding

---

## Phase 8: Polish & Cross-Cutting

- [x] T047 Close the loop end to end (SC-001, SC-006): a gate that runs quickstart's five steps — assemble, put, list, boot, launch — as one scripted sequence, asserting each is a single invocation with no third-party tool and recording elapsed time against the 10-second budget. Neither criterion had a task; both were asserted in prose only. **`scripts/RunBuildLoopGate.ps1`, and it runs SIX steps rather than five, because the five in `quickstart.md` did not produce a running program.** Two decisions and one limit. (1) **SC-001 is a property of the step table, not a sentence.** Each step names one executable and an argument vector; the gate resolves every executable inside this repository's own build output and refuses any argument carrying shell punctuation, so a loop that needed a pipeline, a helper or something off the PATH could not be expressed. (2) **The budget is reported in three parts because only one is ours** -- 0.12 s of command-line work, 0.29 s to the emulator's window, and 6.24 s of guest boot, which is T042's measured 6,366,505 cycles at the machine's 1,020,484 Hz rather than a sleep chosen to look right. 6.65 s against 10 s. (3) **The launch step witnesses liveness and nothing more, and the mutation that proves the limit is recorded**: an emulator built to ignore `--disk1` passes it. Reading the guest's screen from a script is pixel matching; the mount is gated where a gate can see it, by T033/T039/T042/T046. A "documented traps" section runs the two mistakes the help now warns about, because a warning nothing exercises can quietly stop being true
- [x] T048 Help output (FR-034, SC-002): every capability documented, with a **worked example of the whole loop** — assemble, put, boot — not just a flag list. Assert mechanically that the help text contains that example and that every verb and option it uses also appears in the help output; whether a newcomer succeeds is a review gate, not a test. Document the exit statuses `disk` returns, including that it defines **none above 2** (FR-032) — the requirement is to document the subcommand's scoped codes, and "there are none" is the documentation. Say that `put`/`get` are named from the disk's perspective. **Three decisions.** (1) **The help moved into the library.** `PrintUsage` lives in `CassoCli`, which the test assembly does not link, so every claim in it was unverifiable by construction -- the same reason `kInUseHelpText` and `kRoundTripHelpText` already sat in core. `DiskCommandRunner::BuildHelpText` composes the whole disk section and QUOTES those two constants rather than restating them, so the executable printing it verbatim is the only wiring that could drift. (2) **The example's options are gathered from its command lines alone**, not from the block: the prose beneath uses `--` as a dash, and a scanner over the whole block would collect it and then need explaining away. The extracted set is asserted exactly, because a scanner that found nothing would satisfy the loop beneath it. (3) **`CommandLineParser::GetAllDiskVerbs` was added so the sweep runs over the GRAMMAR'S table**, not a list retyped in the test -- a verb added there and left out of the help is a capability nobody can find. Two defects fixed rather than properties confirmed: the header substituted the reader's prefix into disk options, so `/?` documented `/long` and `/addr`, which `ParseDiskOptions` rejects; and the section was a flag list with no example, which is the half a newcomer cannot supply
- [x] T049 Update `CHANGELOG.md` and `README.md` (user-visible feature, test-count change); document the deliberate asymmetry that command-line writes are crash-safe while emulator flushes are not, and that in-use detection is out of scope. **Include a CHANGELOG entry for the T004/T005 flush change**, phrased as what it prevents rather than what it refuses — "a damaged track no longer silently truncates your disk image on eject", not "flush now fails". Read the README's current test count at the time of writing rather than adjusting a remembered figure: the suite baseline is in flux independently of this work (the Dormann data was missing from some worktrees, so recent figures measured a suite doing less work, and a fix is in flight elsewhere)
- [x] T050 Pre-merge gates: `scripts/RunTests.ps1 -Build` for x64 Debug **and** Release (different test sets — Release is not a substitute), `scripts/Build.ps1 -RunCodeAnalysis` clean, `scripts/CheckStyle.ps1` clean; merge to master with `--no-ff`. The gate is **all tests passing**, never a particular total — the suite baseline is moving for unrelated reasons, so a changed count is not by itself evidence of anything. The boot-gated tests (T033, T039, T042, T046) fail rather than skip when the cached master image is absent, so a green suite already proves they ran — no separate confirmation needed. **The merge is NOT done here** -- this branch conflicts with 019 in `CassoCore/CommandLineParser.cpp` (additive on both sides, keep-both), and integration belongs to whoever holds both. Gates run and reported: Debug **3306/3306**, Release **3303/3303**, `Build.ps1 -RunCodeAnalysis` clean, `CheckStyle.ps1` clean
- [x] T051 Reference GH **#115** — already filed, OPEN, `bug` / `priority: high` / `impact: user` — from the Phase 2 commits: `Refs #115` on T002/T003, `Closes #115` on whichever commit lands T004 + T005 together, since the fix is not complete until the refusal has a recovery path. **Do not file a duplicate**; research R-002's evidence is already on the issue, and a second one splits the discussion. **The Phase 2 commits were already made before this task ran, so the trailers could not be added without rewriting pushed history** -- which would be a worse trade than the trailer is worth. The references are on the issue as a comment naming each commit and what shipped instead. **Closing it is the user's call, not this session's**: the defect is fixed and has a recovery path, but the issue is theirs to close

---

## Phase 9: Owner-requested command-line usability

Four changes the project owner asked for after using the shipped subcommand
against their own disk collection. None of them changes what an existing option
MEANS; the help reorganization is the gating deliverable, and the option
semantics themselves are being reviewed category by category separately.

- [x] T052 Verb aliases (`dir`, `cat`, `catalog` → `list`; `del` → `delete`; `read` → `get`; `write` → `put`), following the pattern `ls` and `rm` already use. **`cat` is a reversal of a recorded decision** and the reasoning is worth keeping: it was left out because it collides with the Unix meaning of printing a file's contents, which weighed a convention from another platform above the literal command of the machine this tool exists to serve. On an Apple II, `CAT` lists the disk, and somebody who used one types it first. `catalog` is there for the same reason; `dir` and `del` for what the host shell teaches. The primary verbs are untouched — the owner has not decided whether to rename them. The existing `Disk_CatIsNotAVerb` case is inverted rather than deleted, so the reversal is visible to whoever reads the test next
- [x] T053 Prefix parity, in both directions. **The help was not the whole defect.** It spelled disk options with the reader's chosen prefix while the grammar took only `--`, and the previous fix took the spelling away and documented the exception in the help itself — a concession is what a rule looks like when it is not being kept. The grammar now ACCEPTS `/`, so the help can honestly print it: `CommandLineParser::CanonicalLongFlag` maps a `/name` to `--name` **only when `name` is an exact entry in an option table**, which is what keeps a ProDOS path (`/VOLUME/STARTUP`) an operand instead of losing it to a flag. The same defect existed for the assembler's own long options — `/raw` was read as the concatenated flags `-r -a -w` — so the table-driven canonicalization is applied there too, including the attached-value form `/cpu=65c02`. One flag deliberately keeps `--` at every prefix: `--disk1` belongs to the emulator, which accepts only that spelling, and printing `/disk1` would be a promise this executable is not the one keeping
- [x] T054 Help reorganized to **overview → subcommands → options grouped by category → examples at the end**. The old order led with the command shapes, then the disk commands, then the worked example, then the switches alphabetically. Alphabetical is the one ordering guaranteed to separate related things: `-l`, `-p`, `-c` and `-m` all shape the same listing and landed four places apart, while `-i` and `-l` sat adjacent with nothing in common. The categories are general, output, listing and diagnostics, assembly, run, and disk — a reader arrives with a goal, and a grouping by goal answers it in one place. `DiskCommandRunner::BuildHelpText` splits into `BuildSubcommandHelp` / `BuildOptionsHelp` / `BuildExampleHelp` so the surrounding page can group by KIND rather than by subcommand, and the example can go last where a reader returns to it. Every flag documented before is still documented; **nothing was renamed, defaulted differently or removed** — the option review is a separate exercise
- [x] T055 `disk list` on an image with no filesystem reports **what it can still determine** instead of only the negative. The old message fired on twelve of fourteen real images the owner tested, this project's own demo disk among them, and every one of them boots. `WozLoader::Describe` reads the INFO and META chunks the loader already walks past — creator, disk type, write protection, synchronized, cleaned, boot-sector format, and META's title/publisher/developer/copyright/version/language/machine/RAM — plus how many of the 160 quarter-track positions carry data, which is what a half-track protection looks like from outside. The decoded-track counts come from the read that already happened. **Three claims are deliberately NOT made.** Zeros in the sector buffer mean "blank" only when the track decoded, so a track that did not decode reports that instead of being called empty. No cause is assigned to a track that will not decode — protected, unread and damaged are not separable from here — only the count, plus the note that a booting disk can legitimately read this way. And the exit status and the stream are unchanged: still 2, still the error stream, so a script that pipes a listing is unaffected
- [x] T056 Fix the non-ASCII filename echoed back mangled. `Space Quarks (1981)(Brøderbund)(II-II+)[48K].woz` came back as `Br?derbund`. **The bytes were correct the whole way through**: argv arrives in the process's narrow code page, the console was set to UTF-8, and it read the single byte `$F8` as a broken sequence. Fixed at the output boundary in `Core/TextEncoding`, because a code page is a property of the DESTINATION and only the layer holding the stream knows which one it has — the source encoding is not the lever, and the `/utf-8` compiler switch is deliberately not used here. WOZ META is UTF-8 by specification and is converted into the narrow code page as it is read, so a diagnostic is in one encoding by the time it reaches that boundary rather than a mixture. `disk get`'s payload deliberately does not pass through it: those are a file's bytes, not text

---

## Phase 10: Command-line defects surfaced by the reorganization

Four defects the Phase 9 reorganization exposed. Each is wrong under every
answer to the four option-semantics questions still with the owner, which is
what separates them from that review: none of them changes what an option
MEANS, only whether it does anything and whether the tool says so.

- [x] T057 `-h <lines>` paginates. **The measurement contradicted the report and the diagnosis changed with it.** The report said the attached form worked and the separated form was dropped; a 42-line listing produced with `-h 10`, with `-h10` and with no `-h` at all was byte-identical and contained no form feed, so BOTH forms did nothing. Two independent defects stacked: the separated form was never parsed (`case 'h'` read a glued value and discarded the flag otherwise), and `pageHeight` was carried from `CommandLineOptions` into `AssemblerOptions` and **read by no code anywhere in the tool** — there was no pagination to reach. Pagination is now `Assembler::FormatListing`, in the library rather than beside the ostream, because a page break in the console executable is one the test assembly cannot see; the source's own `.page` directive and a filled page are the same event there, so both emit the form feed, repeat the title and reset the counter. `CommandLineParser::TakeCountValue` takes the value glued or separated, and takes a separated one **only when it parses as a number**, so `-h` before a source file does not eat the source file
- [x] T058 `-w [<width>]` measured for the same defect and found to have it twice over. The separated form was dropped exactly as `-h`'s was, and a bare `-w` set nothing at all although the help documented it as the wide listing — so the one spelling the help singled out was the one that did nothing. Both fixed through the same `TakeCountValue`, with `CommandLineParser::kWideListingColumns` naming the 133 the help quotes so the two cannot drift. **`pageWidth` is still read by nothing**, and deliberately so: making it truncate would change the rendering of every listing produced today, default included, which is a decision about output and not a defect fix. Recorded here rather than fixed quietly
- [x] T059 `-g` takes its filename separated as well as glued. Verified before changing anything: `-g out.dbg` wrote the derived `<source>.dbg` and dropped `out.dbg` **without a word**, which is worse than refusing it. It now follows the rule `-l` already used — the next argument when that is not itself a flag — and the help gains the `[<file>]` it was missing. The cost is stated rather than hidden: `-g` immediately before the source file now claims the source file as the debug name, exactly as `-l` in that position already does
- [x] T060 `disk --help` is help. It reached the verb table, was told `--help` is not a verb, and answered a question about the grammar by refusing to run and complaining about the grammar — exit 2 for something the tool knows the answer to, because help was recognized only as `argv[1]`. A help request is now found anywhere in the disk arguments, in every spelling and either prefix, and resolves to `Verb::Help`, which the runner answers with the disk section of the help on the output stream and a clean status. **`/HELP/STARTUP` stays a ProDOS path**: the spellings are matched exactly and in lower case. `Verb::Help` is deliberately NOT a row in the verb table — that table is swept to check every verb is described in the help, and a help request is not one of the things being described
- [x] T061 The unknown-verb refusal names what is actually accepted. It listed the five original verbs long after eight aliases were added, so a user who mistyped `catalog` was offered five words that did not include it. Built from `CommandLineParser::GetAllDiskVerbs` now, and swept against that table by a test, so a verb added to the grammar cannot go unoffered
- [x] T062 A diagnostic can no longer report success. `run prog.a65 --cpu 65c02` printed `Unknown option` twice and **exited 0**. The audit found six more on the same path — every bad value in the run grammar (`--load`, `--entry`, `--stop`, `--max-cycles`, `--fill`) and a bad `--cpu` target in the assembler grammar, which printed an error, printed the whole help, and reported success. The parser now records a `ParseVerdict` alongside the diagnostic it prints, and the three values are the three statuses this tool already documents in `DiskCommandResult`: clean, succeeded with complaints, produced no output. A run refusal is 2 and **nothing executes** — an option the grammar did not recognize might have moved the load address, so the program that ran was not the program asked for. An unknown as65 flag is 1: the flag is dropped, the assembly still writes its output, which is what "assembled, but warned" is for. The second line of the original complaint (`Unknown option: 65c02`, the flag's own value) is left as it is, because removing it requires the decision about whether `run` takes `--cpu` at all

---

## Phase 11: Owner-directed help restructure and the output default

The owner read the Phase 9 help end to end and returned a list of structural
changes, plus one option-semantics decision they had held twice. The option
review itself is still theirs; what is here is the restructure they asked for,
the one default they settled, and two defects the restructure surfaced.

- [x] T063 Verb lines lead with every spelling, then one short description
      (`cat | catalog | dir | list | ls`), in place of a primary verb with its
      aliases trailing in an "also spelled" clause. A reader coming from an
      Apple II types CATALOG, one from the host shell DIR, one from a Unix
      shell LS -- all three are accepted, and each of them scans the left
      margin for their own word rather than reading to the end of a sentence
      about somebody else's. The grammar block still shows where the operands
      go; only the descriptions moved
- [x] T064 The "put and get are named from the DISK's point of view" paragraph
      is deleted. It existed to rescue two verb names the descriptions did not
      state the direction of; "Read a file from the disk" and "Write a host
      file to the disk" state it outright, so the paragraph was explaining
      wording it had already been made unnecessary by. The test is inverted
      rather than dropped -- it now asserts the paragraph is absent AND that
      both directions are stated on their own verb lines
- [x] T065 Disk options sit with the disk subcommands. They were three sections
      away, so the one subcommand was split across the top and the bottom of
      the page with the assembler's flags in between, and a reader assembling
      the two halves walked over those flags twice
- [x] T066 Output and Listing fold into Assembly as sub-groups. **A top-level
      section is a claim about scope.** "Output options" sitting between the
      disk commands and the run flags gives a reader every reason to read `-o`
      as naming a disk extraction or a run's artifact; neither is true, both
      groups exist only while a source file is being assembled, and an indent
      says so without spending a sentence
- [x] T067 One help entry, `-h, --help, -?`, rather than three. **The `-h`
      collision is NOT resolved here -- it is a held decision** -- but the entry
      does not get to imply there is none: it says `-h` is help only as the
      FIRST argument and is the listing page height everywhere else, which is
      exactly what the grammar does (`CommandLineParser::Parse` matches the
      literal two-character `-h` at `argv[1]`; the as65 walk reads every other
      occurrence as a page height)
- [x] T068 `--cpu` drops the opcode list. Anyone writing 6502 knows what 65c02
      means, and three lines naming STZ/BRA/RMBn were three lines a reader
      scanned past to reach the flags they came for. Target and default only
- [x] T069 Exit statuses are stated once, for every mode, in the overview --
      `CommandLineParser::kExitStatusHelpText`, beside the grammar so the test
      assembly can read the same sentence the user does. They were under `disk`
      alone, which read as though that subcommand had invented them and left
      anyone assembling a file with no answer. **Measured against the code
      before being written down**, and it is four statuses rather than three:
      0 clean, 1 succeeded with complaints, 2 produced no output, and 3 for an
      illegal opcode, which only `run` returns. The disk section's own copy and
      its "defines no status above 2" sentence are gone -- two statements of
      one vocabulary are two statements that can disagree
- [x] T070 The `--basic` paragraph moves up to its own flag and is rewritten.
      It sat below the exit statuses, two paragraphs from the row it explains,
      and it read as a list of damage: **`--basic` IS real tokenization**, and
      `CassoCore/ApplesoftTokenizer.cpp` emits the binary token stream the
      machine itself stores. The substitutions listed are what a LISTING loses
      on the way through that form and back, not anything the conversion
      invents, and the text now leads with which of the two it is describing.
      `basic` is deliberately the last row of the option table so the paragraph
      is adjacent to it
- [x] T071 The in-use paragraph is dropped, having first confirmed that a
      locked image IS reported intelligibly: `CommitImage` refuses at the probe
      and `RefuseCommit` names the image and says what to do -- *"is open in
      another program -- close it and try again"* -- and a host read-only
      attribute surfaces separately as *"is write-protected -- clear its
      read-only attribute and try again. Nothing was written"*. The wording is
      now a named constant (`kInUseRefusalText`) that `DiskFailureModeTests`
      asserts against end to end, so the claim did not lose its test when it
      lost its paragraph. **One thing IS lost and is recorded rather than
      buried**: the paragraph's second sentence said the probe cannot see a
      MOUNTED image, which no error message says because a mounted image
      produces no refusal at all. FR-035 asked for that sentence in the help;
      it is in the CHANGELOG and in this task, and the requirement needs the
      owner's amendment rather than a silent reinterpretation
- [x] T072 Examples move into the sections whose flags they use -- one in the
      output group, one in listing, one in run. The worked disk loop stays
      whole and stays with `disk`: it is the only example that spans more than
      one group, and splitting a five-step loop across three sections destroys
      the thing that makes it worth having
- [x] T073 **`--raw` becomes the default and `--flat` asks for the old shape.**
      Naming no shape wrote a 65,536-byte padded image, so a 200-byte routine
      came out as 64 KB to be sliced down by hand. **The name is `--flat`, not
      `--image`**: `--image` collides with `<image>` on the same help page,
      where it means a disk image and nothing like a memory image, and the
      writer this selects is already called `WriteFlatImage`. `--raw` is kept
      as an accepted spelling of the default so existing command lines and
      makefiles are untouched. **The load-bearing change is not the default.**
      `ResolveOutputFormat` decided whether a `.s19` filename could pick the
      format by testing `shape == Binary`, which meant "nobody named a shape"
      only while nothing on the command line could spell Binary; with Raw as
      the default and `--raw` spelling it, that test would have read an
      explicit flag as silence. `outputFormatNamed` is now its own bit, set by
      every shape flag, so the extension fallback still applies exactly when no
      flag was given
- [x] T074 A trailing `-o` hung the tool forever, found while writing down what
      `-o` actually defaults to. With nothing glued to the flag and nothing
      after it, neither branch of `case 'o'` ran and neither advanced `pos`, so
      the walk over concatenated flags reread the same character until the
      process was killed -- no output, no diagnostic, no exit. Every other
      value-taking flag already had the missing `pos++`. **Verified by running
      the shipped binary before the fix and killing it at five seconds**, not
      inferred from the code. The regression test hangs rather than fails if
      the fix is reverted, which is the honest cost of pinning it where the
      defect lives
- [x] T075 The real `-o` default, written down after measuring it: the source
      file's own name with its extension REPLACED -- `.bin`, or `.s19` under
      `-s` and `.hex` under `-s2` -- and derived from the auto-extended input,
      so `casso build` yields `build.bin` by way of `build.a65`. The help said
      "the input, with a .bin extension", which is wrong twice over. **The
      owner's `-o [<file>]` spelling is NOT adopted**: `-o` is not
      optional-valued the way `-l`, `-g` and `-w` are -- it takes the next
      argument unconditionally, flag or not -- so the help keeps `-o <file>`
      and the discrepancy is reported rather than documented as a feature

---

## Phase 12: Owner decisions on the option review

The owner returned the option review Phase 11 handed them. Three options are
deleted outright, the exit statuses are restated per mode because they were
never shared, and the two silent failures around `-o`/`--out` are fixed by
diagnosing rather than by accepting the other flag.

**The governing decision: there are NO existing CassoCli command lines to
preserve except as65's.** Nothing here is kept for backward compatibility with
this project's own past spellings, and where uniformity and as65 compatibility
conflict, as65 wins.

- [x] T076 **`--raw` is deleted.** It named the default, and an option that
      selects what naming nothing already selects costs a line of help and buys
      no capability. It was kept in T073 so command lines already carrying it
      would go on working; the owner's ruling that no such command lines need
      preserving removes the only argument for it. `outputFormatNamed` stays
      and stays load-bearing -- it is what keeps a `.s19` filename from
      overriding `--flat` or `--dos-bin`, which is a question about those two
      flags and not about the one that went
- [x] T077 **`--verbatim` is deleted.** Verbatim is the default, so the flag's
      only surviving effect was cancelling a `--text` or `--basic` earlier on
      the same line -- a combination nothing needs and no caller writes. The
      test that pinned it (`Disk_VerbatimIsASelectorOfItsOwn`) went with it.
      **`ResolveFileType` was checked, since that cancellation was a real path
      through it**: `--text --verbatim` reached the default branch and landed a
      DOS `B` where `--text` alone landed a `T`. With the flag gone the only
      way into that branch is naming neither conversion, so it is now pinned by
      a test of its own -- `Put_WithNoConversionAndNoNamedType` -- alongside
      the `--text` and `--basic` cases that were already covered. The
      `Encoding::Verbatim` enumerator stays: it is the default value, not a
      spelling
- [x] T078 **`--long` is deleted and the ProDOS columns always print.**
      `ProDosVolume::Enumerate` fills `eof=` and `aux=` unconditionally, so the
      flag bought nothing and cost a reading of the help plus a second run of
      the command -- and the two fields it withheld are the two a build loop
      most wants, being the exact length of a file and the address a binary
      loads at. The widest measured row is 51 characters, and the listing test
      now asserts every line fits 80 rather than trusting that. DOS 3.3 has its
      own formatter and records neither field, so it is untouched -- asserted
      rather than assumed
- [x] T079 **Exit statuses are stated per mode, under each mode.** The single
      combined block claimed to hold for all three and held for none: an
      assembly error exits **2** under the assembler and **1** under `run`, and
      status 1 means "the output was written anyway" in one mode and "nothing
      ran" in the other, so a script reading the shared block and branching on
      1 learned the opposite of the truth in whichever mode its author was not
      picturing. Three constants replace one --
      `kAssembleExitStatusHelpText`, `kRunExitStatusHelpText`, and
      `DiskCommandRunner::kExitStatusHelpText`, each beside the code that
      assigns it. **Every line was measured by running the built binary**:
      assemble 0/1/2 (clean, unknown flag, redundant-`.org` warning, no input,
      unreadable input, assembly error, unwritable output, refused line); run
      0/1/2/3 (stop address, cycle limit, assembly errors, no input,
      unreadable input, refused option, illegal opcode); disk 0/1/2 (list, get,
      a DOS 3.3 `boot` naming a binary, bad verb, missing file, unreadable
      image, refused option)
- [x] T080 **Concatenation is as65-only, and was already so.** The `pos`-walk
      lives in `ParseAs65Flags` and nowhere else; `ParseRunOptions` and
      `ParseDiskOptions` take one option per argument. Nothing had to be
      confined -- what changed is the help, which stated the packing as a
      property of the tool and now states it as a property of the assembly
      grammar, naming the two that do not pack
- [x] T081 **`--out` in assembly mode is refused instead of half-obeyed.** It
      warned `Unknown flag: --`, set the output file to `ut`, and consumed the
      next argument as the input -- three wrong decisions and exit 1, the
      status meaning "assembled, and the output was written". Every `--` option
      this grammar does not have is now refused, not only the one that collides
      with `disk`, since a refusal special-cased to `--out` would leave
      `--output` walking the same path. **The `/` forms deliberately still fall
      through**: `/oFILE` is the glued spelling as65 documents, so `/out`
      genuinely means `-o ut` and must keep meaning it. The refusal does NOT
      set `showHelp`, unlike the bad-`--cpu` refusal beside it -- the usage
      page is 180 lines and the sentence explaining the mistake scrolls away
      above it
- [x] T082 **`-o` in disk mode is refused instead of swallowed.** It fell into
      the positional block as operand three, with its value as operand four;
      this grammar has neither, so both were dropped, the extracted file went
      to standard output, and the exit status said the command had worked. Any
      argument beginning with `-` that the disk grammar does not know is now
      refused, and the suggestion is built from the option table so it cannot
      go stale. **Only a dash** -- a ProDOS path is `/VOLUME/FILE` and stays an
      operand, which is the same reason `CanonicalDiskFlag` matches a table
      instead of rewriting every leading slash. `DiskCommandRunner::Run` honors
      the refusal by running nothing, so the diagnostic reaches the script and
      not only the screen
- [x] T083 **`-o` and `--out` are NOT unified**, by owner ruling: as65 argument
      compatibility beats uniformity. T081 and T082 fix the collision by
      diagnosing it in both directions, and neither flag learns the other's
      spelling -- asserted, so a later "helpful" alias fails a test
- [x] T084 **`-h` is untouched.** The collision between the help request and
      the listing page height is being resolved separately, by spec 019's
      requirement of an explicit assembler profile. The behavior and the help
      note both stand

---

## Phase 13: as65 command-line compatibility gaps

Five differences from the as65 1.42 manual, found by reading its OPTIONS,
RETURNS and DIAGNOSTICS sections against the built binary. Every claim below
was measured by running `CassoCli`, before and after.

**Nothing here withdraws a spelling.** `--cpu` keeps working beside the `-x`
this phase adds, and `-i` and `-n` stay accepted no-ops, because another
feature pins all three. Implementing `-i` and `-n` for real is deferred.

- [x] T085 **A bare `?` shows the usage text.** as65's DIAGNOSTICS section:
      "Help message if only parameter is a question mark, or if an illegal
      option has been specified." The prefixed `-?` and `/?` were already
      accepted; the unadorned one was read as a source filename, so `CassoCli
      ?` went looking for a file called `?` and exited 2 saying it could not
      open one. **The "only parameter" condition is as65's own and is kept
      literally** -- `? -q` leaves the question mark as the input file, because
      a `?` further along a command line is somebody's operand and a host that
      allows the character allows it in a filename
- [x] T086 **REVERSED BY OWNER DECISION -- see T103. The "illegal option" half
      of that sentence IS implemented now.** This task originally recorded the
      opposite, and the reasoning it gave was that the branch had already
      settled the other behavior: an unrecognized assembler flag dropped, the
      assembly run, the output written, status 1. The owner's ruling was to
      "choose parity over an incorrect shipped decision", and the reason the
      shipped one was incorrect is that a dropped flag is one that may have
      shaped the output -- so a makefile passing a flag this assembler does not
      have got a binary shaped by the flags that survived, reported under the
      same status an ordinary assembler warning earns.
      **Its citation was also wrong**: it credited the earlier decision to
      T072, which is the task about moving examples into their sections. The
      decision was T062's
- [x] T087 **An assembly error exits 3, not 2.** as65's RETURNS list: "2 -
      Unable to open input or output file. 3 - Assembly gave errors." Both
      failures left the assembler with nothing to write, so both returned 2,
      and a build script branching on the status sent every syntax error down
      the "your path is wrong" arm. The two are now distinguished by whether
      the source was ever read. **The neighbors were re-measured and hold**: 0
      clean, 1 an unknown flag or a warning, 2 an unreadable or unnamed input
      and every failed write. **`run` and `disk` are untouched** -- `run`
      spends 3 on an illegal opcode and 1 on an assembly error, `disk` has no
      3, and the help states each mode's statuses under that mode
- [x] T088 **The decision moved into `CassoCore/As65ExitStatus`.** It sat in
      `DoAs65`, which the test assembly does not link, so the status the tool
      returns was a claim nothing could check -- which is how "assembly error"
      answered "could not open a file" for as long as it did. The four statuses
      are named constants and `DoAs65` reads them rather than spelling out
      integers
- [x] T089 **Exit status 4 is named in the help as as65's, and stated not to be
      produced.** as65 defines "4 - No memory could be allocated" for a 16-bit
      tool allocating a symbol table out of a real-mode heap. A 64 KB image on
      a virtual-memory host does not reach that condition, and an allocation
      that did fail would take the process down before a status could be
      returned. Naming it costs two lines and saves a script porting from as65
      from wondering which status replaced it
- [x] T090 **`-x` selects the 65SC02 extensions.** as65: "Use 65SC02
      extensions. This CPU has several additional instructions. When this
      option is not specified the assembler rejects the 65SC02 extensions." It
      was not accepted at all -- it fell through to the unknown-flag warning
      and was dropped, so the source then failed on a strict 6502 with a
      diagnostic about the opcode rather than about the flag. It selects the
      same tier `--cpu 65c02` does, concatenates the way as65's flags do, and
      takes either prefix; the two spellings were checked to produce
      byte-identical output
- [x] T091 **`--cpu` is NOT withdrawn in favor of `-x`.** The tidier reading --
      one as65-shaped switch -- would break the assembler-dialects work in
      flight, which pins `--cpu` in as65 mode with its own tests and whose
      Merlin path refuses `--cpu` **by name**. Withdrawing it would leave that
      refusal naming a flag no other mode had. Both spellings stand; unifying
      them is somebody's later decision, not this phase's
- [x] T092 **A bare `-d` defines `DEBUG` as 1.** as65: "Define a label before
      the first source line is read. If no name is specified, DEBUG is defined.
      The label is EQUated to be 1." It defined nothing at all, because it took
      whatever followed unconditionally -- and the two things that follow are
      the source file and the next flag. `-d demo.a65` defined a label called
      `demo.a65` and then reported "no input file specified", a diagnostic
      about the argument the flag had eaten; `demo.a65 -d -o out.bin` defined a
      label called `-o`, lost the output name, and wrote the derived one while
      reporting success. Both were reproduced against the binary before the
      fix. **The separated-name half of this task was reversed -- see T104.**
      It kept `-d NAME` working via a heuristic (take the next argument unless
      it looks like a flag or like a source file); as65 has no such rule, its
      `-d<name>` is glued, and the heuristic went with it
- [x] T093 **`caseSensitive` is renamed `ignoreOpcodeCase`, in both
      `CommandLineOptions` and `AssemblerOptions`.** `-i` means *ignore* case in
      opcodes and set a field called `caseSensitive` to **true**, so the record
      of the flag stated the reverse of the flag. **The flag stays a no-op** --
      implementing it is deferred -- and the rename exists so whoever picks that
      up does not inherit a name arguing against the behavior they are writing.
      The comment where the field lives records the asymmetry that makes it
      non-trivial: as65 folds case in the **mnemonic table only**, and "Labels
      are still case sensitive", so it is two comparison rules in one pass over
      one line, not a mode
- [x] T094 **`-h` is untouched, again.** Its help/page-height collision stays
      where the previous phase left it

---

## Phase 14: Owner-directed help tiering

The owner read the 180-line help -- four screens -- and asked for a short
default with the detail behind per-mode help. What is here is that tiering, the
routing table the owner specified, and the two moves that make the general page
fit on one screen.

**The governing shape**: the general page is a table of contents -- banner,
one line per mode, the route to each mode's page, and the loop the tool exists
to run. Every flag is described on the page of the mode that takes it, once.
Anything two pages both need is one function called twice.

- [x] T095 **The help is four pages, and the routing is the owner's table.**
      `--help`, `-?`, `/?`, `-h` as the first argument, and a bare `CassoCli`
      open the **general** page; a lone `?` opens the **assembler's**;
      `run` and `disk` open their own when asked. `CommandLineOptions::HelpPage`
      carries which page a request means, so `PrintUsage` reads a decision the
      parser made rather than making one of its own
- [x] T096 **A lone `?` opens the assembler's page, and is the only route to
      it.** It is as65's own request -- its manual gives usage when the only
      parameter is a question mark -- and assembling IS as65 mode, so the
      request lands on the page describing the grammar it comes from. Every
      other spelling, including `--help` typed beside a source file, opens the
      general page
- [x] T097 **`-h` and `/h` join the help spellings that `run` and `disk`
      accept, and the top-level `-h` is untouched.** The page height it collides
      with exists only inside the assembler's own flag walk, and no argument of
      either modern grammar reaches that walk -- so the two characters a reader
      most likely types are free to mean help there. The collision at the top
      level is still a held decision and is still held
- [x] T098 **`run` answers a help request at all.** `run --help` was an option
      that grammar does not have: a diagnostic, a refusal, and exit 2 -- a
      question the tool knows the answer to, answered by complaining about being
      asked. It is now looked for before anything else in the run arguments, and
      anywhere among them, on the rule the disk grammar already used: a reader
      asks for help after typing the thing they wanted help with at least as
      often as before it
- [x] T099 **`<source>` and `<binary>` move to the assembler's page, and the
      `-h` collision note moves to the page-height row that owns it.** Both were
      on the general page, where the operand descriptions cost four lines and
      the "General options" block cost five to describe two flags the page was
      already demonstrating. **Reported rather than smoothed over**: `<binary>`
      is `run`'s operand and now stands on the assembler's page, which is where
      the owner's instruction puts it; it is not repeated under `run`, because
      one description in two places is the thing this restructure exists to
      prevent
- [x] T100 **The general page is composed in `CassoCore/CommandLineHelp`, so a
      test can read the page a user reads.** The test assembly does not link the
      console executable -- the same reason the disk help is assembled in
      `DiskCommandRunner` -- and the general page is the one carrying a size
      promise, which nothing could measure while it was a run of `println`s in
      the exe. The banner is passed IN rather than built there, because it
      carries the build's own version and architecture; it is still counted with
      the page, since a page is what the reader sees
- [x] T101 **The worked loop is one block on two pages.** The general page shows
      the five commands because they are the one thing on it that is not a table
      of contents; the disk page shows them and then explains the two traps in
      them, because every flag they use is described there. So the commands are
      `CommandLineHelp::BuildExampleCommands` and the prose stays in
      `DiskCommandRunner::BuildExampleHelp`. The usage lines are shared the same
      way: a mode's page opens with the line the general page lists it by, from
      `UsageLineFor`, so the two cannot come to describe different grammars.
      `DiskCommandRunner::LongPrefix` is gone in favor of the one in
      `CommandLineHelp`, for the same reason
- [x] T102 **A test per route, in both prefixes, plus the size promise.**
      `HelpRoutingTests` sweeps every spelling at the top level, under `run` and
      under `disk`, asserting the page each one opens and that a slash-spelled
      request keeps the slash. Two of its claims are structural rather than
      textual: a disk help request must NOT set `showHelp`, or the executable
      would print a general page over the verb the runner answers, and asking
      for help must leave the parse verdict clean. The general page is asserted
      to be under thirty lines including its banner -- it is twenty -- so it
      cannot grow back into four screens a reasonable addition at a time

### Phase: exact as65 argument parity

The governing sentence, quoted from the as65 1.42 manual and the rule every
task below is measured against:

> Commandline options can be catenated, but no other option can follow one that
> may have a string parameter. Other options can follow one that has a numeric
> parameter.

**EVERY as65 PARAMETER IS GLUED.** There is no separated form anywhere in the
manual's notation -- `-d<name>`, `-h<lines>`, `-w<width>`, `-l<filename>`,
`-o<filename>` -- and `-g` and `-s` take no parameter at all. Every separated
form this tool accepted was its own invention, and by owner ruling they are all
withdrawn: "where we invented something as65 does not have, remove it."

The boundary that did NOT move: `--flat`, `--dos-bin` and `-s2` are this
project's and stay. They name output shapes as65 has no equivalent for, and
matching as65's lack of long options would delete features rather than fix
compatibility. Parity means as65's grammar works unchanged, not that ours is
reduced to only what as65 had.

`--cpu` is the one long option that DID go, and for the opposite reason: it was
not a capability as65 lacked, it was a second spelling of one as65 already had.
See T114.

- [x] T103 **An illegal option prints usage and assembles nothing.** as65's
      DIAGNOSTICS: "Help message if only parameter is a question mark, or if an
      illegal option has been specified." Reverses T086/T062 by owner decision;
      see T086 for why the shipped behavior was wrong. **The ASSEMBLER's page is
      the one printed**, because the assembler's grammar is the one violated,
      and the complaint goes to stderr while the page goes to stdout so a caller
      redirecting either keeps the sentence instead of losing it above the page.
      The walk STOPS, so a flag packed behind the illegal one is not obeyed and
      the argument after it is not consumed. **`ParseVerdict::Complaint` was
      removed with the behavior** -- it existed for this one case, nothing else
      produced it, and leaving it would have been an arm nothing reaches
- [x] T104 **`-d`'s name is glued and the flag never reads past its own
      argument.** as65 notates `-d<name>`, a STRING parameter, which the
      catenation rule singles out as the case nothing may follow. Reverses the
      separated half of T092: `-d NAME` was this tool's invention and its
      supporting heuristic is deleted. `CassoCli -d prog.a65` defines DEBUG and
      assembles prog.a65 on the plain reading, with no rule needed to get there
- [x] T105 **`-h` is glued AND lets other options follow it.** Its parameter is
      NUMERIC, and the manual's own worked example is "`-h80t` which specifies
      80 lines per page and a symbol table". `case 'h'` ran `pos` to the end of
      the argument and discarded the trailing letters silently, so `-h80t`
      produced a listing with no symbol table at status 0 -- measured against
      the binary before and after. The separated `-h 60` is gone; it was added
      on the strength of this tool's own help text, which documented a form the
      parser did not read, and the help was the thing that was wrong.
      `TakeGluedCount` consumes exactly the leading digits and returns how many,
      which is what says where the value ends and the next flag begins
- [x] T106 **`-w` follows the same numeric rule, and its bare form STAYS.** The
      manual is explicit that the bare form is as65's own: "If the -w option is
      given without a number following it, then the listing will be 133 columns
      wide, otherwise it will be the number of colulmns specified (between 60
      and 200)." So `-w` alone = 133 is parity, not invention, and only the
      separated `-w 100` was withdrawn. **The DEFAULT was also wrong**: as65
      says "Normally, the listing is printed using 79 columns for output to a 80
      column screen or printer", and this was 80 -- the screen's width rather
      than the listing's, the one column that does not fit. This project's own
      002 contract said 79 too, so the 80 was drift from both authorities at
      once. **Not implemented, and reported rather than invented**: the manual
      states a valid range of 60..200 and does not say what as65 does outside
      it, so no refusal was fabricated for a case the authority is silent on
- [x] T107 **`-o`'s filename is glued, and a bare `-o` is refused with the form
      to type.** as65 notates `-o<filename>`; a bare `-o` names nothing and
      there is no such form. It is the option callers meet first -- this
      project's own examples were written separated -- so the refusal names
      `-oprog.bin` rather than reporting an unknown option, the way the `--out`
      refusal already does. It does NOT set `showHelp`, on that same rule: those
      three lines ARE the answer and the usage page would bury them. Refusing
      also retires the trailing-`-o` hang permanently instead of falling back to
      the derived name and reporting success
- [x] T108 **`-l`'s filename is glued, and its bare form STAYS.** The manual
      lists the flag twice -- "-l  Generate pass 2 listing" and "-l<filename>
      Listing file name" -- so a bare `-l` is a real as65 form rather than a
      missing argument, unlike a bare `-o`. Its parameter is a string, so
      nothing follows it in a group: `-lt` names a listing file called `t`.
      **Reported, not implemented**: as65 also documents "The filename - can be
      used to direct the listing to standard output", where this tool spells
      that with a bare `-l`
- [x] T109 **`-g` takes NO parameter at all, so both filename forms go.** Its
      entire as65 entry is "Generate source-level debug information file. This
      file can then be used in in-system debugging or a software simulator" --
      no filename, no extension, no format. Both `-g <file>` and `-g<file>` were
      added by an earlier slice, so **this removes a capability as65 never had
      rather than matching one**; naming the debug file will need a spelling of
      this project's own if the owner wants it back. Taking no parameter also
      means other options follow it in a group, so `-gt` is `-g -t`, and
      `-gout.dbg` now reads as `-g -out.dbg` -- pinned by a test, because that
      is surprising enough to be worth saying out loud
- [x] T110 **A bare `CassoCli` returns 2.** It printed the general page and
      exited 0 while `main`'s own comment claimed a missing subcommand exits 1
      -- an arm made unreachable because the parser sets `showHelp` for an empty
      command line, so the "user asked" branch claimed it. **Decided on this
      tool's own exit table, NOT as65 parity**: nothing has entered the
      assembler's grammar, so as65's statuses do not govern, and 2 is what this
      table already spends on "no file was opened". The verdict carries it, so a
      parser test can read the decision the executable acts on; `main`'s wrong
      comment is replaced by the rule it now follows -- asking for the page exits
      0, being shown it exits 2
- [x] T111 **`run` with an unreadable input returns 2, not 1.** `run` documents
      2 for "an input that could not be read" and returned 1, because `DoRun`
      treated an unreadable source as an assembly failure. Nothing was
      assembled, so nothing failed to assemble; `AssembleResult::wasRead`
      already carried the distinction and the binary path beside it already
      returned 2 for the identical mistake. **The same false claim was in the
      message channel** -- "Assembly failed with 0 error(s)" printed under
      "Cannot read input file", naming a count of zero because there was nothing
      to count -- and is gated on the same flag
- [x] T112 **`<binary>` moves to the run page.** It was on the assembler's page,
      where an assembled image is not something the assembler can be given; it
      is `run`'s operand. It is not repeated on both, because an operand
      documented twice is one whose scope a reader has to guess
- [x] T113 **The assembler's exit-status block now says which authority each
      code comes from.** After this phase the statuses are as65's inside the
      assembler grammar and this tool's own everywhere else, and the block
      listed both without distinguishing them. 0, 2 and 3 carry as65's meanings;
      1 does not -- as65 spends it on a bad command line and this assembler
      spends it on an assembly that warned, refusing the bad command line under
      2. Status 1 no longer covers a dropped flag, because there is no longer a
      command line that assembles and complains at the same time
- [x] T114 **`--cpu` is withdrawn and `-x` replaces it.** REVERSES T091, which
      kept both spellings, by owner decision: `-x` is as65's own name for the
      switch and selects the same instruction set, so the tool carried two ways
      to ask for one capability. Both `--cpu` and `/cpu` are matched BY NAME and
      answered with a message pointing at `-x`, rather than left to the generic
      `--` refusal -- command lines carrying the flag already exist, and `/cpu`
      would otherwise reach the concatenation walk and be read as `-c -p -u`,
      which is a true reading of as65's grammar and a useless answer to somebody
      migrating.
      **The long-option table stays at two entries and is NOT scaffolding.** It
      is what stops the single-character normalization from reading `/flat` as
      -f -l -a -t; one entry would still need it. Neither `--flat` nor
      `--dos-bin` changed spelling.
      **KNOWN COST ON SPEC 019, recorded here because it cannot be fixed from
      this branch.** 019 pins `--cpu` in as65 mode with its own tests, and its
      Merlin path refuses `--cpu` **by name** so the user is pointed at Merlin's
      `XC` directive. With the flag gone from as65 mode those tests need
      retargeting at `-x`, and that refusal needs rewording or its guidance
      degrades to a bare "unknown option". **Whoever merges the two branches
      owns this**; it was not touched from here because 019's worktree is not
      visible to this one. Closes the deferred half of GH #118

---

## Phase 15: The silent-discard sweep

`CassoCli pg.a65 -opg.bin -h 60` assembled, wrote the binary, exited 0, and
never said that `60` had gone nowhere. That was one instance of a class, and
this phase is the class: **anything the user typed that was accepted and then
discarded without a diagnostic.**

**as65 settles none of it.** Its synopsis is `as65 [-cdghilnopqstvwxz] file` --
one file -- and it documents nothing about a surplus argument, so what happened
to one was this tool's own answer. The owner's ruling is that it is an error.
Each grammar refuses under **its own** documented status: the assembler's 2
("no file was opened -- ... a command line that was refused"), `run`'s 2
("nothing could be started"), and `disk`'s 2 ("nothing was done").

- [x] T115 **A surplus positional in the assembler grammar is refused.** The
      first bare argument is the source file and the second used to be dropped
      where it fell. It is now a refusal at exit 2 with nothing assembled and
      nothing written
- [x] T116 **The refusal names the likely cause when it can see one.** The cause
      is nearly always a value typed with a space in front of it, for an option
      that glues its value, and two things say so: a surplus argument that is
      all digits, and a parameter-taking option standing in front of it. Either
      one earns the glued spelling by name -- `-w100, not -w 100`. A word that
      is neither gets the plain message, because guessing at a cause there would
      invent one
- [x] T117 **`run` names a surplus argument as one.** It was already refused,
      which is the right verdict, but under the words "Unknown option" -- and a
      filename is not an option, so the reader was sent looking for a flag they
      had not typed
- [x] T118 **`disk` refuses an operand its VERB has no slot for**, and the count
      is the verb's own: `list` names a disk, every other verb names a disk and
      a file. Two slots were filled whatever the verb, so `disk list img.dsk
      PROG` cataloged the whole disk and never mentioned PROG, and `disk get
      img.dsk PROG extra` extracted PROG and never mentioned extra -- both at
      exit 0. A verb the table does not recognize is left alone, so the runner's
      "unknown disk verb" is not preempted by a complaint about operand three
- [x] T119 **A bare `-h` mid-command-line is refused, by owner ruling.** as65
      documents the bare form of `-w` -- "If the -w option is given without a
      number following it, then the listing will be 133 columns wide" -- and
      documents no bare form of `-h`, on the same page, by the same author.
      Read that as `-h` not having one. It silently did nothing here: the height
      kept whatever it had and the flag might as well not have been typed. The
      message names `-h0`, because a reader who wants no page breaks has a real
      spelling for it and would otherwise reach for the bare flag to ask.
      **THE TOP-LEVEL `-h` IS UNTOUCHED** -- a leading `-h` is the help request
      and never reaches the assembler's flag walk. The four bare forms as65
      genuinely documents (`-w`, `-l`, `-d`, `-g`) are swept in a test to keep
      them that way
- [x] T120 **`disk --addr` refuses a value it cannot read.** It dropped it, and
      the result was a message contradicting the command line it was answering:
      `disk put img prog.bin --addr zzz` replied "is a binary, which has to be
      told where it loads -- give --addr $XXXX" to somebody who had just given
      `--addr`. The rest of the tool already states this rule -- Refused covers
      "a value that could not be read" -- and `run` applies it to every address
      it takes; this one option was the exception
- [x] T121 **`-dNAME=VALUE` refuses a value it cannot read, and a missing name.**
      The `=VALUE` half is this tool's own -- as65 documents only `-d<name>`,
      equated to 1 -- so nothing about parity required the fallback that was
      there: an unreadable value became 1 in silence, so `-dADDR=$6000` and
      `-dVER=1.0` each defined the symbol as 1 and assembled a source that then
      took a branch nobody chose. The whole text after the `=` has to be
      consumed, so a trailing fragment cannot be dropped either. A bare `-d` is
      still the DEBUG default as65 documents
- [x] T122 **An option that ran out of command line is no longer reported as an
      unknown one**, in either grammar that has value-taking options. The
      refusal was right and the words were not: `disk list img --addr` answered
      "unknown disk option: --addr" and then listed `--addr` among the options
      to try instead -- a message that contradicts itself in two lines

**Left for the owner, measured and not decided** -- see the sweep notes in
`research.md`:

- **A disk option is accepted under every verb, including the verbs it does not
  serve, and two of them silently REPURPOSE an operand.** `disk get img BASIC
  --as STARTUP` reads STARTUP, not BASIC, because `--as` writes the same field
  the second operand does; `disk put img aaa.txt --out bbb.txt` puts bbb.txt.
  `--type` and `--addr` on `list` are accepted and ignored at exit 0. Per-verb
  option applicability is a grammar decision, so it is reported rather than
  taken
- **The `--name=value` long-option form is claimed and not implemented.**
  `CanonicalLongFlag` carries an attached value across on the stated grounds
  that "a long option may be spelled `--name=value`", and no arm of any grammar
  matches one: `run prog.a65 --load=$1000` is refused as an unknown option.
  Either the form is wanted or the comment is wrong; both are cheap, and which
  one is a decision
- **"N lines assembled" reports 0 unless `-l` was given.** The count is the
  listing's length, and the listing is only built when a listing was asked for,
  so an ordinary assemble reports "0 lines assembled" over a binary it just
  wrote. Not a discarded input, so out of this phase's class, but it is the same
  kind of confident wrong statement

---

## Phase 16: The shell that cuts a glued flag in half

Measured before anything was written, by handing each argument to a native
executable that prints its argv -- under PowerShell 7.6.5 and Windows PowerShell
5.1, with cmd.exe as the control:

```text
-oprog.bin        ->  -oprog     .bin        (cut at the FIRST dot)
-oprog.bin.x      ->  -oprog     .bin.x
-osub\x.bin       ->  -osub\x    .bin        (a separator does not cut)
-o.bin            ->  -o         .bin
-oC:\out\prog.bin ->  arrives whole          (a colon suppresses the cut)
/oprog.bin        ->  arrives whole
--flat.x          ->  arrives whole
cmd.exe           ->  everything arrives whole
```

PowerShell parses a token beginning with a single `-` as a parameter name, and a
parameter name may not contain a dot. **Nearly every output name has an
extension, so the glued form as65 documents does not survive being typed
unquoted in the shell this tool is typed into most.** It does not fail
consistently either, which is worse than failing: an absolute path glues fine
because its colon suppresses the cut.

- [x] T123 **`-o` accepts a separated filename as well as an attached one**, by
      owner decision. It accepts MORE than as65 does and never less, so no as65
      command line changes meaning and the glued form is untouched. **`-o`
      only.** `-l`, `-d`, `-w` and `-g` each have a bare form as65 documents --
      a listing to standard output, a DEBUG definition, 133 columns, and no
      parameter at all -- so for any of them the following word is genuinely
      ambiguous with the bare reading, and separating the two takes a guess
      about what that word looks like. That guess was here for `-d`, defined a
      label called `demo.a65`, and was deleted this session for being
      unprincipled; it is not coming back through this door. `-o` has NO bare
      form, which is exactly what leaves nothing to guess about, and the
      asymmetry is recorded at the flag rather than left to read as an
      oversight. The value is taken VERBATIM -- skipping one that looks like a
      flag would be the same guess arriving another way. An `-o` with nothing
      after it at all is still refused
- [x] T124 **A surplus argument the shell cut off a flag explains itself.**
      `Error: surplus argument: .bin` is true and useless: `.bin` is not on the
      command line the reader remembers writing, so the message reads as a
      defect in the tool. It now names both halves and the whole they came from,
      says why the cut was made, and offers the two spellings that survive it --
      quoting, and (for `-o`) the separated form. **The shell is not detected**;
      the signature is a shape in argv -- a surplus argument beginning with a
      dot, standing behind a single-dash flag group that attached a NAME and
      carries no dot or colon of its own. Reading the parent process would be
      fragile, untestable, and wrong the moment the command line came from a
      script. The last condition is what keeps ordinary command lines out:
      `./prog.a65` behind `-oout.bin` has the shape and is not a fragment, and
      the front half proves it, because it still carries the dot the shell would
      have cut at. **The whole command line is searched, not the pair at hand**,
      because which argument ends up surplus depends on the typing order:
      `prog.a65 -oprog.bin` leaves `.bin` surplus, while `-oprog.bin prog.a65`
      lets `.bin` fill the source slot and makes `prog.a65` surplus
- [x] T125 **`run` and `disk` were checked for the same signature and do not
      carry it.** Every value in both grammars is SEPARATED, so a value is its
      own token, never begins with `-`, and is never a parameter name to be cut;
      no valid command line in either can be mangled. The only way to reach the
      shape there is a glued spelling neither grammar accepts, where the right
      answer is "that flag takes its value separately" rather than "quote it" --
      which is what both already say. Wiring the recognition in would have been
      an arm nothing reaches, so it is recorded at the predicate instead

---

## Dependencies


```text
Phase 1 (Setup)
    ↓
Phase 2 (Foundational) ──── blocks EVERYTHING; T003–T006 are a data-loss fix
    ↓
Phase 3 (US3, P1 read) ──── independently shippable
    ↓
Phase 4 (US2, P1 write) ─── needs US3's readers + delete; closes the MVP loop
    ↓
    ├── Phase 5 (US4, P2)
    ├── Phase 6 (US5, P3) ── independent of 4/5; needs only Phase 2
    └── Phase 7 (US6, P3)
    ↓
Phase 8 (Polish)
```

**Within Phase 2**: T002 ∥ T007 ∥ T008 ∥ T009 are parallel; T003 → T004 → T005 →
T006 is a chain; T010 needs T009; T011 needs T009. **T004 and T005 ship
together** — T004 alone converts silent corruption into a refusal with no
recourse, which is half a fix.

**Within Phase 3**: T012 ∥ T014 ∥ T016 (different files); T013 needs T012; T015
needs T014; T017 → T018 → T019 → T020.

**Within Phase 4**: T023 → T024 and T025 → T026 are two independent chains that
can run in parallel; T027 needs both; T028 needs T027; T030 → T031.

## Implementation Strategy

**MVP = Phases 1–4.** US1 is already shipped; US3 then US2 closes the loop the
feature exists to provide. Stop there and the feature is genuinely useful:
extract source off old disks, assemble on the host, place the result back,
boot it.

Phases 5–7 are refinements, each independently valuable, none gating the
migration. Phase 6 (US5) depends only on Phase 2 and can be pulled forward if
direct-boot turns out to matter more than boot configuration.

**The one ordering that is not negotiable** is Phase 2 before any write path.
The decoder currently returns success over data it silently zeroed; building a
write path on top of that ships the same defect from a second direction.

---

## Phase 17: The disk nobody was making

**Goal**: the worked example runs from an empty directory.

Found by running it. Every step began `disk put mydisk.dsk` and nothing anywhere
made `mydisk.dsk`, so following the example this feature ships failed at the
second command. `BlankDiskBuilder` had been in `CassoEmuCore` since spec 017 and
was reachable only from the GUI.

- [x] T126 [US7] Add `create` and `init` to the disk grammar in `CassoCore/CommandLineParser.cpp`, with `new` and `format` as aliases, and the options `--type`, `--format`, `--volume` and `--bootable` on `CommandLineOptions::DiskOptions`. **Three decisions.** (1) **`--type` names two different things and stays one word.** Under `put` it is the file type the catalog records; under `create` it is the container the image is written as. They never appear on the same command line, the verb is always known by the time the option is read, and the help states which under each. (2) **`init` refuses `--type` rather than ignoring it.** The container was decided when the file was made; a reader who wants a different one wants a different file, which is what `create` makes. (3) **`--bootable` is optional-valued.** A following argument beginning with a dash is the next option rather than a filename, so `--bootable --format prodos` reads as both and not as a master called `--format`.
- [x] T127 [US7] Implement `RunCreate` and `RunInit` in `CassoEmuCore/Devices/Disk/DiskCommandRunner.cpp` over `BlankDiskBuilder`, reaching the host only through `IDiskFileIo`. **Three decisions.** (1) **`create` refuses to write over an existing image.** A disk somebody still wanted is one keystroke from a disk they no longer have, and `create` is the verb reached for when not thinking about what is already there; the refusal names `init`. (2) **`--volume` is read as a number under DOS 3.3 and a name under ProDOS, decided by the format rather than by how the word looks.** A ProDOS volume legitimately called `254` would otherwise silently become a DOS volume number. (3) **An unknown container or format is refused by name with the ones that exist**, because somebody who typed `--type 2mg` meant it and handing them a `.dsk` is a disk they did not ask for under a name they did. Seven tests in `UnitTest/EmuTests/DiskCommandRunnerTests.cpp` driving both verbs through `FakeDiskFileIo`.
- [x] T128 [US7] Teach `CommitImage` that a target may not exist yet. Its freshness check asks whether the image changed between being read and being written, and a file nobody has read cannot have: with no stamp to compare, the commit refused every new disk with "could not be checked for changes". The in-use probe still runs, so a name another program is holding is still refused.
- [x] T129 [US7] Move locating the operating-system masters into the library as `CassoEmuCore/Devices/Disk/StockBootDisks.h/.cpp`, so a bare `--bootable` finds the one the emulator already downloaded. It sat in `Casso.exe`'s `AssetBootstrap`, which the command line cannot link. **Downloading stays in the GUI** — it needs consent, a progress report and a network stack, none of which belong in a library — so the split is at the line where the platform actually starts. **It creates nothing**, which is the one behavioral difference from the code it came out of: asking where a file would be is not arranging for it to exist, and every `create` without `--bootable` would otherwise have left an empty folder behind.
- [x] T130 [US7] Add the create step to the worked example in `CommandLineHelp::BuildExampleCommands` and to the README, and assert the whole loop in `DiskHelpTextTests`. Measured from a clean directory: create, assemble, put, put, boot, list — six commands, every one exit 0, catalog lists HELLO, PROG and STARTUP.

---

## Phase 18: The executable that nothing could test

**Goal**: `CassoCli.exe` holds only what cannot live in a library.

Recorded against [#85](https://github.com/relmer/Casso/issues/85). The criterion
is UT-reachability rather than portability, so the Win32 file layer moved too;
what stayed is the entry point the linker demands.

- [x] T131 Delete the second exit-code mapper. `AssemblerExitCode` mapped a failed assembly to 2 and a warning to 1; `As65ExitStatus` maps them to 3 and 5. **Both had full green test suites asserting contradictory numbers**, and neither could see which one the executable called, because the test assembly does not link an executable. The wiring was the one thing uncovered and the wiring was the bug: every page of the help documented statuses the tool did not return. Deleting the loser is the fix that holds — there is one mapper now and no second function for a call site to reach for. Warnings move onto it as a third argument.
- [x] T132 Move the command-line application layer from `CassoCli` to `CassoEmuCore/Cli`: `CommandLine`, `Win32DiskFileIo`, `DiskCommand`, `ArtifactWriter`, the four mode runners, `SourceAssembler` and `HostFile`. `main`'s body becomes `CliMain`. **3,639 lines to 57.**
- [x] T133 Add `UnitTest/CliMainTests.cpp`: six tests asserting what the tool returns to a shell, through the dispatch rather than through the mapper it is supposed to ask. **It failed on its first run** — a bare `CassoCli` exited 0 while the banner directly above the code said it exits 1. The arm read `showHelp`, which is set whether the page was asked for or merely printed at you; the parse verdict is what tells those apart.

---

## Phase 19: Deferred, with the reason written down

- [x] T134 [US5] `disk create --boot <binary>`, the command surface for `DirectBootBuilder`. **Deferred once and then done, because the reason for deferring did not survive being checked.** The stated blocker was that it "needs its own container handling"; `DirectBootBuilder::Build` returns the complete 143,360-byte DOS-ordered sector buffer, which is the exact shape `BlankDiskBuilder` holds before its own container switch. **Three decisions.** (1) **That switch became `BlankDiskBuilder::WrapInContainer`**, shared rather than copied: a second copy of the `.po` reordering or the WOZ nibblization is a second place for the sector skew to be got wrong. (2) **It is its own path in `RunCreate` rather than a flag on `BuildAndWrite`**, because there is no filesystem here to put the binary into. (3) **The `--boot` and `--bootable` exclusion is checked twice on purpose.** `ResolveBoot` has it, and this path never reaches `ResolveBoot` because it builds no filesystem, so the pair silently honored `--boot` and dropped `--bootable` until the check was repeated here. Measured, not reasoned. Six tests in `DiskCommandRunnerTests` drive the command through `FakeDiskFileIo`, one of them asserting the written `.dsk` is byte-for-byte what `DirectBootBuilder::Build` produces, which ties the command to the output the guest-visible tests already boot a 6502 over.
- [x] T136 [US5] `--entry <addr>` on the disk grammar, refused rather than dropped when it will not parse, for the reason `--addr` gives: a dropped value leaves the runner answering a command line the caller did not type.
- [ ] T135 A 6502 disassembler. **Deferred to [#121](https://github.com/relmer/Casso/issues/121).** The loop runs in reverse as far as bytes and stops one step short of source. Deciding code from data is undecidable in general, so the design is verifiable rather than complete: disassemble, reassemble, compare bytes. Shared with #51 and #59.

