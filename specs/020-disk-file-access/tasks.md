# Tasks: Disk File Access for the Build Loop

## State of play

Written for whoever picks this up next, including a later session of me after a
context loss. Current as of the last commit on `020-disk-file-access`.

**Done and on the branch.** Phase 2 (foundational, T002–T013), Phase 3
(US3 — read, T014–T022), **both** Phase 4 chains — T023 + T024 (DOS 3.3 write
and delete) and **T025 + T026** (ProDOS write with tree growth, and delete) —
T027 + T028 (replace on both filesystems, and the pre-commit self-check), T029
(the bit-stream write path, its four-format matrix, and whole-operation
refusal), and now **T030 + T031** (the file-level commit policy and its wiring
into the runner). That is the whole P1 extraction story — both filesystem
readers, the decode report, the integrity pass, the `disk` subcommand, the CLI
edge — plus placement, replacement and removal on both filesystems, each
verifying its own output, an edited volume rendered back into any of the four
containers it came from, and a crash-safe commit that leaves the target
byte-identical after every failure it can be made to suffer. Suite is **3191
Debug / 3188 Release** green; the 3-test delta is the pre-existing GH #113, not
this work. (It was 3163 / 3160 before T030/T031 added 28 tests, 3152 / 3149
before T029 added 11, 3134 / 3131 before T027/T028 added 18, 3109 / 3106 before
T025/T026 added 25, and 3088 / 3085 before T023/T024 added 22 and removed the
now-obsolete `Write_IsNotYetImplemented_AndSaysSoRatherThanSucceeding`.)

**T029 found a live defect, which is what it was ranked first for.** The `.po`
reorder shipped in Phase 3 was wrong and had been silent ever since — see the
divergences block. Read that before assuming the sector-order question is
settled anywhere else in the tree.

**Next.** Phase 4 has four tasks left (T032–T035), and it is the work here where
a bug **destroys data** rather than failing loudly. Everything through Phase 3
was reads: wrong output was the worst case. From T023 on, the worst case is a
user's disk image. The two ranked risks that were still open — the `.po` reorder
and the file-level commit — are now both closed; what remains is reachability,
not mechanism.

**Order within Phase 4.** T023 → T024 and T025 → T026 were independent chains
(DOS 3.3 and ProDOS), joining at T028. Delete ships with write on both because
replace depends on it. Both chains, T027, T028, T029, T030 and T031 are done;
**T032 is the next thing** — it wires `put`/`delete` to the verbs, and the
commit path it needs is already built and public: `OpenImage` →
`IVolume::Write` → `VolumeImage::Save` → `CommitImage`. Note `VolumeImage::Save`
is still not called from `DiskCommandRunner`; that wiring is T032's.

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
- **T001 was not done as written and stays unchecked.** It called for
  EHM-conformant stubs for every file up front; files were instead created and
  wired as each task reached them, which meant the build never carried a stub
  that did nothing. Its `.vcxproj.filters` clause was moot — this repository has
  no `.filters` files.
- T009 split `FilePath` into its own header/pair rather than folding it into
  `VolumeTypes.h`, per the one-type-per-pair style rule.
- `--text` and `--basic` are wired on the **get** path only (T022). The `put`
  side is T032.
- `--basic` currently refuses: there is no Applesoft tokenizer until Phase 7.

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

- [ ] T001 Add EHM-conformant stubs for every new file and wire them into `CassoEmuCore/CassoEmuCore.vcxproj(.filters)`, `CassoCore/CassoCore.vcxproj(.filters)`, `CassoCli/CassoCli.vcxproj(.filters)`, `UnitTest/UnitTest.vcxproj(.filters)`: `CassoEmuCore/Devices/Disk/{IVolume.h, VolumeTypes.h, SectorDecodeReport.h, Dos33Volume.h/.cpp, ProDosVolume.h/.cpp, VolumeIntegrityReport.h/.cpp, TrackWritability.h/.cpp, IDiskFileIo.h, DiskCommandRunner.h/.cpp, CommitPlan.h/.cpp, DirectBootBuilder.h/.cpp}`, `CassoCore/{AppleTextCodec.h/.cpp, ApplesoftTokenizer.h/.cpp}`, `CassoCli/{Win32DiskFileIo.h/.cpp, DiskCommand.h/.cpp}`, `UnitTest/{AppleTextCodecTests.cpp, ApplesoftTokenizerTests.cpp}`, `UnitTest/EmuTests/{Dos33VolumeTests.cpp, VolumeIntegrityTests.cpp, DiskCommandRunnerTests.cpp, CommitPlanTests.cpp, DirectBootTests.cpp, FakeDiskFileIo.h}` — x64 Debug compiles clean

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
`$6000`, boot, confirm `CATALOG` lists `B 002 PROG` and `BLOAD PROG` lands the
bytes; then run every failure mode and confirm the image hash is unchanged.

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
- [ ] T032 [US2] Add `put` and `delete` verbs to `ParseDiskOptions` and `DiskCommandRunner` (`--as`, `--type`, `--addr`, `--text`/`--basic`/`--verbatim`), with write-protect and locked-file refusals reported in intelligible terms rather than raw platform codes (FR-014). Use `--verbatim`, **not** `--raw` or `--binary`: both already name assembler output shapes (`OutputFormat::Raw`, `OutputFormat::Binary`) in this same parser, and `--verbatim` says what it does — the other two selectors transform the bytes, this one does not. **`--verbatim` means no CHARACTER conversion — it does NOT mean raw sectors.** Length and header semantics still apply, because those are how a filesystem records where a file ends: they are the file's identity, not a transformation of it. So `get --verbatim` yields the file's logical bytes with high bits and line endings untouched, never trailing sector slack, because "give me this file" must not hand back whatever happened to be in the rest of the last sector. **Gate it on FILE equality, not image equality**: `get --verbatim`, `put --verbatim` unchanged, re-read, and assert the bytes match. Image equality is the wrong assertion — a DOS 3.3 file occupies whole sectors, so slack past the recorded length differs from whatever was there before, and a `put` that reallocates changes the image for reasons having nothing to do with conversion. **Assert sector reuse separately** if image stability is wanted; do not conflate the two. Extract-edit-replace is the workflow US3 exists for, and it must not perturb bytes the user did not touch
- [ ] T033 [US2] Guest-visible gate (SC-003): real-CPU tests — place a binary on a DOS 3.3 image, boot, `CATALOG` shows the placed file, `BLOAD PROG` lands bytes at `$6000`; same payload on ProDOS shows type `BIN` aux `$6000`. Attempt to overwrite the stock master's locked `HELLO` (type `$82`) and confirm refusal. **`B 002 PROG` is wrong arithmetic and this task and quickstart §US2 both carried it.** A 512-byte binary stores 516 bytes once DOS's four-byte load/length header is inside the file, which is three data sectors, and the track/sector list is a fourth: `CATALOG` reads **`B 004`**. `002` is what a payload of 252 bytes or fewer produces. Measured against the implementation in T023 (`Write_BinaryOntoAFreshVolume_ReadsBackByteForByte` asserts four sectors); assert what the file actually occupies rather than the number in the prose
- [ ] T034 [US2] Failure-mode gate (SC-005) as **unit tests against the fake `IDiskFileIo`**, not against real files: for **every** documented failure — volume full, locked file, write-protected image, illegal name, unwritable track, stale target — assert the target's bytes are unchanged, the resulting image still parses as a mountable volume, and no temporary remains in the fake's file table. Test Isolation is NON-NEGOTIABLE, so the seam is what makes this suite legal; the real-file version of the same checks is the single manual pass in T035
- [ ] T035 [US2] Runtime validation pass over quickstart §US2 plus the **manual** interrupted-write check — the one sanctioned real-file exercise: kill the process mid-commit, confirm the original is intact and bootable and no temporary remains. Crash safety cannot be unit-tested, which is exactly why it is the part done by hand; everything else in T034 runs against the seam

**Checkpoint**: the minimum viable loop is closed — assemble, place, boot, run.

---

## Phase 5: User Story 4 — Make the disk boot the program (P2)

**Goal**: boot straight into the developer's program with no typing.

**Independent test**: quickstart §US4 — set a boot program, boot the image, the
program runs unattended.

- [ ] T036 [US4] Implement `Dos33Volume::SetStartupProgram` in `CassoEmuCore/Devices/Disk/Dos33Volume.cpp`: patch the greeting filename **in place at T01 S09 `+$75`**, 30 bytes, high ASCII, `$A0`-padded (verified against the stock master in research R-003). No catalog change, no chaining file
- [ ] T037 [US4] Implement `ProDosVolume::SetStartupProgram` in `CassoEmuCore/Devices/Disk/ProDosVolume.cpp`: reorder the volume directory so the chosen `SYS` file is the first the boot path finds. **Deliberately not shared with T036** — the two mechanisms differ in kind, and a unified "write the boot name" helper would be wrong for both
- [ ] T038 [US4] Add the `boot` verb to `ParseDiskOptions` and the runner (FR-024); refuse a program not present on the volume, naming the missing file (FR-025)
- [ ] T039 [US4] Boot gate: real-CPU tests — a DOS 3.3 image with a set boot program runs it unattended after DOS loads; a ProDOS image launches the chosen system program

---

## Phase 6: User Story 5 — Boot with no operating system (P3)

**Goal**: an image that boots directly into a binary, with no DOS or ProDOS.

**Independent test**: quickstart §US5 — the payload runs measurably sooner than
the equivalent OS boot.

- [ ] T040 [US5] Implement direct-boot image generation in `CassoEmuCore/Devices/Disk/DirectBootBuilder.h/.cpp`: a boot-sector loader that pulls the payload's sectors and jumps to it (FR-026). **Resolve the loader's sector capacity before starting** — R-010 deferred it, and FR-027's reported number is undefined until it is settled. Unit tests in `UnitTest/EmuTests/DirectBootTests.cpp` asserting the generated image's structure and the capacity boundary
- [ ] T041 [US5] Refuse a payload exceeding what the boot path can load, reporting the available capacity (FR-027); support an entry address different from the load address (FR-028), in `CassoEmuCore/Devices/Disk/DirectBootBuilder.cpp`
- [ ] T042 [US5] Gate (SC-007): real-CPU test — the direct-boot image reaches the payload in **under 25% of the emulated CPU cycles** the equivalent DOS 3.3 boot of the same program takes. Emulated cycles, not wall clock, so the result is deterministic across hosts

---

## Phase 7: User Story 6 — BASIC source as a runnable program (P3)

**Goal**: place an Applesoft listing written as host text as a program the guest
can `RUN`.

**Independent test**: quickstart §US6 — place a listing, boot, `LIST` reproduces
the source.

- [ ] T043 [US6] Implement Applesoft tokenization in `CassoCore/ApplesoftTokenizer.h/.cpp`: host-text listing → tokenized on-disk form with line-link fixups (FR-022). Settle the coverage boundary against a real listing — token spellings inside strings, `DATA` payloads, `REM` text. **Settle round-trip loss deliberately before writing the tokenizer**, and record the answer: detokenize/retokenize is the least likely of the three conversions to be exact, because tokenizers normalize spacing and canonicalize forms. It is also where loss is most surprising — someone extracting a text file opted into a conversion, while someone whose saved program comes back subtly reformatted did not. If exact round-tripping is not achievable, say so in the help text rather than letting a user discover it
- [ ] T044 [US6] Implement detokenization (the reverse direction, FR-022) in `CassoCore/ApplesoftTokenizer.cpp`; round-trip tests
- [ ] T045 [US6] Refuse an untokenizable listing with the offending line number and text quoted (FR-023); wire `--basic` through `put` and `get`
- [ ] T046 [US6] Gate: place a known listing, boot, `LIST` in the guest, confirm it matches the source

---

## Phase 8: Polish & Cross-Cutting

- [ ] T047 Close the loop end to end (SC-001, SC-006): a gate that runs quickstart's five steps — assemble, put, list, boot, launch — as one scripted sequence, asserting each is a single invocation with no third-party tool and recording elapsed time against the 10-second budget. Neither criterion had a task; both were asserted in prose only
- [ ] T048 Help output (FR-034, SC-002): every capability documented, with a **worked example of the whole loop** — assemble, put, boot — not just a flag list. Assert mechanically that the help text contains that example and that every verb and option it uses also appears in the help output; whether a newcomer succeeds is a review gate, not a test. Document the exit statuses `disk` returns, including that it defines **none above 2** (FR-032) — the requirement is to document the subcommand's scoped codes, and "there are none" is the documentation. Say that `put`/`get` are named from the disk's perspective
- [ ] T049 Update `CHANGELOG.md` and `README.md` (user-visible feature, test-count change); document the deliberate asymmetry that command-line writes are crash-safe while emulator flushes are not, and that in-use detection is out of scope. **Include a CHANGELOG entry for the T004/T005 flush change**, phrased as what it prevents rather than what it refuses — "a damaged track no longer silently truncates your disk image on eject", not "flush now fails". Read the README's current test count at the time of writing rather than adjusting a remembered figure: the suite baseline is in flux independently of this work (the Dormann data was missing from some worktrees, so recent figures measured a suite doing less work, and a fix is in flight elsewhere)
- [ ] T050 Pre-merge gates: `scripts/RunTests.ps1 -Build` for x64 Debug **and** Release (different test sets — Release is not a substitute), `scripts/Build.ps1 -RunCodeAnalysis` clean, `scripts/CheckStyle.ps1` clean; merge to master with `--no-ff`. The gate is **all tests passing**, never a particular total — the suite baseline is moving for unrelated reasons, so a changed count is not by itself evidence of anything. The boot-gated tests (T033, T039, T042, T046) fail rather than skip when the cached master image is absent, so a green suite already proves they ran — no separate confirmation needed
- [ ] T051 Reference GH **#115** — already filed, OPEN, `bug` / `priority: high` / `impact: user` — from the Phase 2 commits: `Refs #115` on T002/T003, `Closes #115` on whichever commit lands T004 + T005 together, since the fix is not complete until the refusal has a recovery path. **Do not file a duplicate**; research R-002's evidence is already on the issue, and a second one splits the discussion

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
