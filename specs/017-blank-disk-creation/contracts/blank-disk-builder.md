# Contract: BlankDiskBuilder (core)

`CassoEmuCore/Devices/Disk/BlankDiskBuilder.h`, pure static API, EHM
conventions, no host dependencies beyond caller-supplied payload bytes.

```cpp
enum class BlankDiskContents { Unformatted, Dos33, ProDos };

struct BlankDiskSpec
{
    DiskFormat         format       = DiskFormat::Woz;
    BlankDiskContents  contents     = BlankDiskContents::Dos33;
    bool               bootable     = false;
    Byte               volumeNumber = 254;          // DOS 3.3
    std::string        volumeName   = "NEWDISK";    // ProDOS
};

// Optional boot payload, pre-loaded by the SHELL from the downloaded master
// images. Builder never touches the filesystem.
struct BootPayload
{
    std::vector<Byte>  dosMasterSectors;   // whole 143,360 B System Master (.dsk), empty if absent
    std::vector<Byte>  proDosUsersDisk;    // whole 143,360 B Users Disk (.dsk),  empty if absent
};

class BlankDiskBuilder
{
public:
    //  Validate the spec's format/contents/bootable combination (FR-010).
    //  E_INVALIDARG with no output on an invalid pairing.
    static HRESULT  ValidateSpec (const BlankDiskSpec & spec);

    //  Produce the complete on-disk bytes for the new image. All-or-nothing:
    //  on failure outBytes is untouched (FR-011). Bootable specs REQUIRE the
    //  matching payload member to be non-empty (E_INVALIDARG otherwise --
    //  availability is the shell's problem, enforced before calling).
    static HRESULT  Build (const BlankDiskSpec & spec,
                           const BootPayload   & payload,
                           std::vector<Byte>   & outBytes);
};
```

## Guarantees

- `Build` output for `format == Woz` is bit-identical to what
  `WozLoader::Serialize` emits for the equivalently built `DiskImage` (same
  CRC discipline; INFO write-protect byte = 0 per FR-012).
- `Dsk`/`Po` outputs are exactly 143,360 bytes.
- Formatted outputs satisfy the skeleton invariants in data-model.md
  (VTOC/catalog for DOS 3.3; directory/bitmap for ProDOS), unit tests assert
  them structurally, and mount-level tests assert `CATALOG`/`CAT` cleanliness.
- Bootable DOS output boots to the Applesoft prompt via the copied DOS tracks
  + generated `HELLO` (SC-006 gate exercises this with the real CPU).
- Deterministic: identical spec + payload ⇒ identical bytes (no clocks, no
  randomness), keeps golden-byte tests stable.

## Sub-components (internal, individually unit-tested)

- `Dos33Skeleton::Write (buffer, volumeNumber)` / `Dos33Skeleton::InstallDos
  (buffer, masterSectors)` / `Dos33FileWriter::WriteHello (buffer)`.
- `ProDosSkeleton::Write (buffer, volumeName)` / `ProDosSkeleton::InstallBoot
  (buffer, usersDisk)`, which uses `ProDosReader::ExtractFile (usersDisk, name,
  outBytes, outFileType, outAuxType)` and `ProDosFileWriter::WriteFile
  (buffer, name, fileType, auxType, bytes)`.
- `ProDosReader` / `ProDosFileWriter` maintain bitmap/directory consistency:
  every allocated block is marked used exactly once; file count and entry
  chains stay coherent (property asserted in tests).
