# Contract: Volume API (core)

`CassoEmuCore/Devices/Disk/`, pure static/instance API over a flat sector
buffer, EHM conventions, no host dependencies. Every type is constructible from
bytes and assertable in `UnitTest`, per Constitution Principle VI.

## The seam

```cpp
//  IVolume.h -- the filesystem seam. Two implementations, almost no shared
//  structure below this surface, so the interface stays narrow.
//  The volume holds the current sector buffer as an immutable const reference
//  supplied at construction. Every mutating call therefore reads that buffer
//  and produces a COMPLETE new one in outBuffer; none writes through to the
//  input. That is what makes FR-013's all-or-nothing guarantee structural
//  rather than a discipline someone has to remember.
class IVolume
{
public:
    virtual ~IVolume () = default;

    virtual HRESULT  Enumerate   (VolumeListing & outListing) const = 0;
    virtual HRESULT  Read        (const FilePath & path, FilePayload & outPayload) const = 0;

    //  Add or replace.
    virtual HRESULT  Write       (const FilePath     & path,
                                  const FilePayload  & payload,
                                  vector<Byte>       & outBuffer) const = 0;

    virtual HRESULT  Delete      (const FilePath & path, vector<Byte> & outBuffer) const = 0;

    //  The same removal, plus what it freed, what it declined to free, and the
    //  conditions that bound the answer.
    virtual HRESULT  Delete      (const FilePath  & path,
                                  vector<Byte>    & outBuffer,
                                  DeleteOutcome   & outOutcome) const = 0;

    virtual HRESULT  BuildIntegrityReport (VolumeIntegrityReport & outReport) const = 0;

    //  Mechanism differs entirely per filesystem -- deliberately not unified.
    virtual HRESULT  SetStartupProgram    (const FilePath & path, vector<Byte> & outBuffer) const = 0;
};
```

Implementations: `Dos33Volume`, `ProDosVolume`, each its own `.h`/`.cpp` pair
per the one-class-per-pair rule, not nested inside the skeleton headers the way
the existing readers and writers are.

## Guarantees

- **Nothing mutates in place.** Every mutating call takes the current buffer and
  yields a new one. A failed call leaves the caller's buffer untouched (FR-013).
- **Replace is computed whole** (FR-012); never a delete applied to the target
  followed by a write, which would lose the file outright if it failed between
  the two.
- **Delete frees only uniquely-owned units** (FR-011), reports the rest as
  leaked, and remains available for a file whose chain is damaged so a bad file
  cannot strand the volume.
- **The account of what a delete declined to do is part of the seam**, added
  here after the two implementations had carried it privately for a while. It
  is what any caller reporting to a user needs, and such a caller holds only an
  `IVolume &`; leaving the pair concrete makes every one of them re-derive which
  filesystem it holds, which is the branch this seam exists to remove. The
  two-argument form remains for callers that genuinely do not want the account.
- **Every `Write` and `Delete` runs the integrity pass over its own computed
  result and refuses to return a result that fails it** (FR-039).
- **Buffers are always the flat 143,360-byte sector buffer** in DOS 3.3 logical
  order, matching every existing skeleton. Format ordering is the track layer's
  business.

## Integrity pass

```cpp
//  VolumeIntegrityReport -- one mechanism, four consumers: delete, listing,
//  allocation, and the pre-commit check on every computed write.
//
//  Claims are held BOTH ways. The requirement asks which units each file
//  claims AND which units have several claimants, and neither slicing derives
//  the other without re-walking every chain the pass already followed.
vector<vector<uint16_t>>  claimantsByUnit;   // who claims each unit, by name
vector<vector<uint32_t>>  claimsByOwner;     // which units each entry claims
vector<bool>              allocatedInFreeMap;
vector<uint32_t>          crossLinked;
vector<uint32_t>          allocatedButUnclaimed;
vector<uint32_t>          claimedButFree;
vector<uint16_t>          unfollowableChains;
bool                      catalogFullyParsed;
bool                      isClean;
```

**Cost, settled once so it is not re-litigated.** Holding claims both ways is
not a space concern at any size this project can encounter. The total is bounded
by allocated units rather than by how they are indexed, and the ceiling is the
*format*, not any drive: ProDOS block pointers are 16 bits, so 65,535 blocks, 
**32 MB**, is the largest volume that can exist. Fully allocated at that
maximum, both slices together are a few hundred kilobytes, transiently. Larger
media in later features do not change this.

The axis worth watching is **time**. The pass is O(volume) and FR-039 runs it
before every computed write; at a few tens of thousands of units that is
microseconds per command-line invocation, so it is a non-issue here. It becomes
interesting only if a caller runs it on something frequent (a live UI refresh in
the disk manager, say) and that is a caching question, not a structural one.
Noted so spec 021 inherits the analysis rather than the surprise.

**Termination is part of the contract** (FR-038). Traversal is bounded by a
visited set and a ceiling derived from the volume's capacity; a chain that hits
the bound is recorded in `unfollowableChains` rather than followed. This pass
runs by design on volumes selected for being damaged, so an unbounded walk is a
hang, not an edge case.

`allocatedButUnclaimed` is **never** freed by any operation. It is either
already-leaked space or an invisible file's data; refusing to guess is correct.

## Track layer

```cpp
//  NibblizationLayer.h -- extended.

//  Reporting form. Succeeds whenever it could decode at all; the report
//  carries the SHAPE of what it found, so a caller that can present partial
//  results (list, extract) decides for itself.
static HRESULT  Denibblize (const DiskImage      & img,
                            DiskFormat             fmt,
                            vector<Byte>         & out,
                            SectorDecodeReport   & outReport);

//  Existing signature, existing callers -- but NOT a bypass. Forwards to the
//  reporting form and FAILS when the report shows data loss, succeeding only
//  when every track is Complete or Unformatted.
static HRESULT  Denibblize (const DiskImage & img, DiskFormat fmt, vector<Byte> & out);

//  Re-encode ONLY the listed tracks; every other track's packed bits are left
//  exactly as they are (FR-017).
static HRESULT  RenibblizeTracks (const vector<Byte>  & sectors,
                                  DiskFormat            fmt,
                                  std::span<const int>  tracks,
                                  DiskImage           & inOutImage);
```

**A track's outcome is decided by coverage, not by how the loop exited.** The
decoder maintains a 16-bit mask of which logical sectors were filled. `Complete`
iff every bit is set and each was set exactly once.

The "exactly once" half is deliberately stronger than today's loop requires, the
sixteen-iteration bound already makes a full mask imply it. It is specified that
way so the invariant does not depend on that bound, which is a plausible thing to
change: scanning until the bit stream wraps is the more general algorithm, and
under it duplicates can coexist with full coverage. One extra test now, correct
under both.

This matters because the existing loop has **three** ways to leave a logical
sector zeroed and only one of them fails: `break` on a decode failure
(NibblizationLayer.cpp:766), `continue` on an out-of-range sector number (771),
and two physical sectors claiming the same number so one logical slot goes
unclaimed. The loop is bounded at sixteen iterations, so the latter two consume an
iteration without filling a distinct slot, no failure, nothing reported. A fix
aimed only at `break` leaves both live. Coverage catches all three, and anything
similar nobody anticipated.

The loop must also continue past a failed sector and resynchronize on the next
address prologue rather than abandoning the track.

**The three-argument form must fail rather than bypass.** Keeping it purely for
source compatibility would preserve the defect's reachability in the one place
that matters: `DiskImage::Serialize`, the emulator's flush path and the sole
production caller, would keep calling the reportless form and keep silently
truncating. Forwarding-and-failing instead means:

- all twelve existing test call sites keep compiling unchanged;
- the unformatted-track test keeps passing, because `Unformatted` is benign;
- **no caller can obtain a silently truncated buffer, whichever overload it
  picks**;
- `Serialize` is fixed whether or not anyone remembers to migrate it.

The point is to remove the attractive nuisance, not to document around it. A
simpler-looking overload that quietly loses data will eventually be chosen by
someone who did not read this file.

## Writability

```cpp
struct TrackWritability
{
    //  Set when the WHOLE image is unwritable -- checked before any track is
    //  decoded, because both signals are free.
    std::string   imageRefusalReason;
    vector<bool>  trackWritable;
};
```

Order of evaluation, cheapest first (FR-019):

1. Quarter-track map resolving any position off its whole track → refuse image.
2. Image metadata declaring timing-sensitive capture → refuse image.
3. Per-track: sixteen distinct valid standard sectors → that track is writable.

Positive proof only. Never a protection-scheme heuristic.

## Error mapping

| Condition | Result |
|---|---|
| Path not found | `HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND)` |
| Volume full | `HRESULT_FROM_WIN32 (ERROR_DISK_FULL)`, naming the shortfall |
| File locked | `HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED)` |
| Illegal name for the filesystem | `HRESULT_FROM_WIN32 (ERROR_INVALID_NAME)`, naming why |
| Track not writable | `HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED)`, naming the track and the reason |
| Name already present, on a path that does not replace | `HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS)` |
| Computed result failed its own integrity check | `E_UNEXPECTED`: this is a bug in the writer, not user error, and asserts accordingly |

**Not `E_INVALIDARG`, and the first two rows above used to say otherwise.**
`E_INVALIDARG` marks a *coding* error in this codebase and asserts on the spot;
an illegal name is something a user typed and an unwritable track is a property
of the disk they handed us. Neither is a bug, so neither may assert, end-user
input earns a verdict, never an assertion. Only the last row is a defect in our
own code, which is why it alone keeps an asserting code. Corrected after the
DOS 3.3 writer followed the rule rather than the table; the ProDOS side should
match it.

## Testability

No test may touch a real file. Volumes are built from synthetic buffers, 
`BlankDiskBuilder` already produces formatted ones, and damaged volumes are
constructed by deliberately corrupting a good buffer, which is also how SC-010's
cyclic-chain termination cases are made.
