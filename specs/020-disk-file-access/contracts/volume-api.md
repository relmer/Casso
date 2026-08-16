# Contract: Volume API (core)

`CassoEmuCore/Devices/Disk/` — pure static/instance API over a flat sector
buffer, EHM conventions, no host dependencies. Every type is constructible from
bytes and assertable in `UnitTest`, per Constitution Principle VI.

## The seam

```cpp
//  IVolume.h -- the filesystem seam. Two implementations, almost no shared
//  structure below this surface, so the interface stays narrow.
class IVolume
{
public:
    virtual ~IVolume () = default;

    virtual HRESULT  Enumerate   (VolumeListing & outListing) const = 0;
    virtual HRESULT  Read        (const FilePath & path, FilePayload & outPayload) const = 0;

    //  Add or replace. Produces a COMPLETE new sector buffer; never mutates
    //  the input. This is what makes FR-013 structural rather than careful.
    virtual HRESULT  Write       (const FilePath     & path,
                                  const FilePayload  & payload,
                                  vector<Byte>       & outBuffer) const = 0;

    virtual HRESULT  Delete      (const FilePath & path, vector<Byte> & outBuffer) const = 0;

    virtual HRESULT  BuildIntegrityReport (VolumeIntegrityReport & outReport) const = 0;

    //  Mechanism differs entirely per filesystem -- deliberately not unified.
    virtual HRESULT  SetStartupProgram    (const FilePath & path, vector<Byte> & outBuffer) const = 0;
};
```

Implementations: `Dos33Volume`, `ProDosVolume` — each its own `.h`/`.cpp` pair
per the one-class-per-pair rule, not nested inside the skeleton headers the way
the existing readers and writers are.

## Guarantees

- **Nothing mutates in place.** Every mutating call takes the current buffer and
  yields a new one. A failed call leaves the caller's buffer untouched (FR-013).
- **Replace is computed whole** (FR-012) — never a delete applied to the target
  followed by a write, which would lose the file outright if it failed between
  the two.
- **Delete frees only uniquely-owned units** (FR-011), reports the rest as
  leaked, and remains available for a file whose chain is damaged so a bad file
  cannot strand the volume.
- **Every `Write` and `Delete` runs the integrity pass over its own computed
  result and refuses to return a result that fails it** (FR-039).
- **Buffers are always the flat 143,360-byte sector buffer** in DOS 3.3 logical
  order, matching every existing skeleton. Format ordering is the track layer's
  business.

## Integrity pass

```cpp
//  VolumeIntegrityReport.h -- one mechanism, four consumers: delete, listing,
//  allocation, and the pre-commit check on every computed write.
struct VolumeIntegrityReport
{
    //  Empty, one, or several claimants per addressable unit.
    vector<vector<uint16_t>>  claimedBy;
    vector<uint16_t>          crossLinked;
    vector<uint16_t>          allocatedButUnclaimed;
    vector<uint16_t>          claimedButFree;
    vector<uint16_t>          unfollowableChains;
    bool                      catalogFullyParsed = false;
    bool                      isClean            = false;
};
```

**Termination is part of the contract** (FR-038). Traversal is bounded by a
visited set and a ceiling derived from the volume's capacity; a chain that hits
the bound is recorded in `unfollowableChains` rather than followed. This pass
runs by design on volumes selected for being damaged, so an unbounded walk is a
hang, not an edge case.

`allocatedButUnclaimed` is **never** freed by any operation. It is either
already-leaked space or an invisible file's data; refusing to guess is correct.

## Track layer

```cpp
//  NibblizationLayer.h -- extended. The existing three-argument Denibblize
//  keeps its signature and behavior for callers that do not need the report.
static HRESULT  Denibblize (const DiskImage      & img,
                            DiskFormat             fmt,
                            vector<Byte>         & out,
                            SectorDecodeReport   & outReport);

//  Re-encode ONLY the listed tracks; every other track's packed bits are left
//  exactly as they are (FR-017).
static HRESULT  RenibblizeTracks (const vector<Byte>  & sectors,
                                  DiskFormat            fmt,
                                  std::span<const int>  tracks,
                                  DiskImage           & inOutImage);
```

**The decode loop must continue past a failed sector, not break.** The present
`break` is why one undecodable sector currently zeroes every later sector in scan
order on that track (research R-002). Sectors that do not decode are marked
unrecovered in the report; they are never presented as zero bytes.

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
| Illegal name for the filesystem | `E_INVALIDARG`, naming why |
| Track not writable | `E_INVALIDARG`, naming the track and the reason |
| Computed result failed its own integrity check | `E_UNEXPECTED` — this is a bug in the writer, not user error, and asserts accordingly |

## Testability

No test may touch a real file. Volumes are built from synthetic buffers —
`BlankDiskBuilder` already produces formatted ones — and damaged volumes are
constructed by deliberately corrupting a good buffer, which is also how SC-010's
cyclic-chain termination cases are made.
