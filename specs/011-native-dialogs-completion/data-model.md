# Phase 1 Data Model: Native DX Dialogs Completion

This feature adds two persistent / in-memory data structures and extends one
existing structure.

## 1. Disk MRU (`DiskMru`)

**Owner**: `Casso/Shell/GlobalUserPrefs.*` (persistence) + a small
`DiskMru` helper class with the pure list operations.

**Persisted shape** (in `GlobalUserPrefs` JSON):

```json
{
  "recentDisks": [
    "C:\\Users\\…\\Disks\\GameA.dsk",
    "C:\\Users\\…\\Disks\\GameB.dsk"
  ]
}
```

**In-memory shape**:

```cpp
class DiskMru
{
public:
    static constexpr size_t k_capacity = 16;

    void                              RecordMount     (const std::filesystem::path & path);
    std::vector<std::filesystem::path> Snapshot       () const;
    std::vector<std::filesystem::path> Prune          (const std::function<bool (const std::filesystem::path &)> & existsPredicate);

private:
    std::vector<std::filesystem::path> m_entries;  // index 0 = most recent
};
```

**Rules** (mirrors FR-003):

- Most-recent-first ordering.
- Cap = 16. New mount at cap evicts the oldest (index `k_capacity - 1`).
- Re-mounting an existing entry moves it to index 0 (no duplicate growth).
- Path comparison: lexical equality after `std::filesystem::weakly_canonical`
  where available, otherwise raw string equality (decide in implementation
  if a difference between two casings of the same path causes test failures).
- `Prune` takes an injected `existsPredicate` so unit tests can drive it
  without touching the real file system. Production callers pass
  `[] (const auto & p) { return std::filesystem::exists (p); }`.

**Validation**:

- All entries are non-empty absolute paths.
- On load, malformed entries (non-string, empty) are dropped silently
  (do not fail prefs load).
- On save, entries are written in current in-memory order.

**State transitions**:

| Event                                    | Effect                                                       |
| ---------------------------------------- | ------------------------------------------------------------ |
| Mount via any path (picker/drag/boot)    | `RecordMount` → moves or inserts at index 0, evicts oldest   |
| Boot picker render                       | `Prune` with real `exists` → re-persist if anything changed  |
| User clears prefs                        | List goes empty                                              |
| Eject                                    | No change — MRU remembers past mounts, not current state     |

## 2. Drive Widget State extension

**Owner**: `Casso/Ui/Chrome/DriveWidget.*` (state struct) +
`DriveWidgetController` (population).

**Existing shape** (snapshot — actual fields per current source):
roughly `{ driveIndex, isActive, … }`.

**Extension** (FR-007 / FR-008):

```cpp
struct DriveWidgetState
{
    // … existing fields …

    std::filesystem::path imagePath;       // empty when no disk mounted
    // (Alternative: std::string imageName already derived. Decision below.)
};
```

**Decision: store `imagePath`, derive basename at paint time.**

- Rationale: the controller already knows the full path at mount time.
  Storing the path keeps the option open for "show full path on hover"
  in a later spec without an additional plumbing pass. Basename
  derivation (`imagePath.filename()`) is cheap and deterministic.
- Edge case from spec: filenames with no extension or multiple dots
  display the literal `path.filename()` result; do not strip extensions.

**Truncation algorithm** (pure, testable — `DriveLabelTruncation.*`):

```cpp
std::wstring TruncateToWidth (
    std::wstring_view                                              basename,
    float                                                          maxWidthPx,
    const std::function<float (std::wstring_view)>               & measure);
```

- If `measure (basename) <= maxWidthPx` → return `basename`.
- Otherwise binary-search the longest prefix `p` such that
  `measure (p + L"…") <= maxWidthPx`; return that.
- The single-character ellipsis (`L'\u2026'`) is used, not three dots.
- The injected `measure` callback is `DxUiPainter::MeasureTextRunWidth`
  in production and a deterministic stub in tests.

**State transitions**:

| Event           | Effect                                |
| --------------- | ------------------------------------- |
| Disk mount      | `imagePath = newPath; repaint`        |
| Disk eject      | `imagePath.clear (); repaint`         |
| Theme change    | repaint only (path unchanged)         |
| DPI change      | re-truncate against new pixel width   |

## 3. Themed Dialog Definition (`DialogDefinition`)

**Owner**: `Casso/Ui/Dialog/DialogDefinition.h`.

Pure value type consumed by `DialogPrimitive::Show`. Details (fields,
button result codes, hyperlink representation) are documented in
[`contracts/dialog-primitive.md`](./contracts/dialog-primitive.md).

This is the only "new shape" used by every P1/P2 consumer
(unified-startup, boot-disk picker, About, Keymap, Machine Info,
SettingsPanel-stray). P3 consumers (Debug Console, Disk II Debug
Panel) host their own bespoke content inside a primitive-style
chrome rather than fitting into a simple `DialogDefinition`.

## 4. Startup Download Set (transient)

**Owner**: `Casso/Ui/Dialog/StartupDownloadDialog.*`.

Aggregates the asset-bootstrap discovery output into a single
collection the unified dialog enumerates:

```cpp
struct StartupAssetEntry
{
    std::wstring  displayLabel;     // e.g., "Apple //e Enhanced ROM"
    std::wstring  destinationPath;
    std::wstring  sourceUrl;
    uint64_t      expectedSizeBytes;
};

struct StartupDownloadSet
{
    std::vector<StartupAssetEntry> missing;   // empty => no dialog shown
};
```

Lifetime: built once during early boot from existing `AssetBootstrap`
discovery, consumed by the dialog, discarded after user decision.
Not persisted.
