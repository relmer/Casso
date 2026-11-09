# Contract: Volumes, containers and archives (Stories 19 to 21, 30 to 34)

Research R36 to R39 and R49 to R52. Every format here is implemented from
the source in its row, by the rule FR-171 sets.

## Containers and image files

| Container | Extensions | Detected by | Payload | Source |
|---|---|---|---|---|
| Raw blocks | `.po`, `.hdv`, `.img`, `.iso` | size a multiple of 512 and a ProDOS, HFS, SOS or partition signature | `Blocks` | ProDOS 8 Technical Reference |
| 140K sector images | `.dsk`, `.do`, `.po` at 143,360 bytes | size and the file system's checks, as today | `Dos140K` | as today |
| 13-sector | `.d13` at 116,480 bytes; `.nib` and `.woz` with D5 AA B5 address prologs | size, or the prolog | `Dos13` | DOS 3.2 manual, CiderPress II's DOS notes |
| 2IMG | `.2mg`, `.2img` | `2IMG` at 0 | DOS order, ProDOS order or nibbles, by the header | 2IMG format notes |
| Disk Copy 4.2 | `.dc`, `.dc42`, `.image` | $0100 at $52 and sizes that match the file | 400K or 800K `Blocks`, plus tags | Disk Copy 4.2 format notes |
| gzip | `.gz` | $1F $8B | the inner file | RFC 1952 |
| NuFX | `.shk`, `.sdk`, `.bxy`, `.sea` | `NuFile` at 0, or Binary II before it, or a self-extracting header | records | File Type Note $E0/8002, CiderPress II's NuFX notes |
| Binary II | `.bny`, `.bqy`, `.bxy` | $0A $47 $4C at 0 and $02 at $12 | files | File Type Note $E0/0000 |
| AppleSingle, AppleDouble | `.as`, `._NAME` | $00051600 or $00051607 at 0 | one file with its forks | AppleSingle/AppleDouble Formats, 1990 |
| Tape recording | `.wav`, `.mp3`, `.ogg` | a header tone and a sync bit | files, read-only | Apple II Reference Manual's cassette format |

A container that fails its checks opens as the host file it is, as a file
that fails a format check falls to hex today.

## NuFX

- **Read**: every thread format: 0 uncompressed, 1 Squeeze, 2 LZW/1, 3
  LZW/2, 4 LZC 12, 5 LZC 16, 6 Deflate, 7 bzip2. A thread's CRC is checked
  after it is expanded: CRC-16/XMODEM for LZW/1's, and the record's data CRC
  where the master header's version has it.
- **Write**: new and changed records are LZW/2. A record that isn't changed
  is copied with its threads as they are, compressed bytes included.
- **Disk records**: the volume's layout follows from a disk thread's block count and size: 280 blocks is a 140K disk, converted to `Dos140K`; anything else is
  `Blocks`.
- **Names**: the record's file name thread, with its file system separator,
  is the path inside the archive.

## Extended files (Story 20)

| On | Data fork | Resource fork | Finder information |
|---|---|---|---|
| ProDOS | storage type 5 key block, data mini-entry | resource mini-entry | the key block's two 18-byte entries |
| AppleSingle | entry 1 | entry 2 | entry 9, and entry 11 for ProDOS's access, type and aux type |
| AppleDouble | the `NAME` file | `._NAME`, entry 2 | `._NAME`, entries 9 and 11 |
| HFS | data fork | resource fork | the catalog record's FInfo and FXInfo |
| NuFX | data thread | resource thread | the record's option list |

## Partition layouts (Story 31)

| Layout | Detected by | Partitions | Source |
|---|---|---|---|
| CFFA | a ProDOS volume directory at block 2 of each consecutive 32 MB slice | slices of 65,536 blocks, of which ProDOS addresses 65,535; after the fourth, the HFS partitions of the 1.2 GS/OS driver | CFFA Manual 2.11 |
| FocusDrive | its partition map in block 0 | the map's entries | CiderPress II's FocusDrive notes |
| MicroDrive | its partition map in block 0 | the map's entries | CiderPress II's MicroDrive notes |
| DOS.MASTER | a ProDOS volume whose last blocks have DOS 3.3 VTOCs at the spacing DOS.MASTER sets | each DOS 3.3 volume | CiderPress II's DOS.MASTER notes |
| AmDOS, OzDOS, UniDOS | an 800K image with two DOS 3.3 VTOCs at the layout's offsets | two 400K volumes, 50 tracks of 32 sectors | CiderPress II's DOS hybrid notes |
| DOS hybrid | a 140K disk with a valid VTOC and a valid ProDOS or Pascal directory | two file systems on one buffer | CiderPress II's DOS hybrid notes |
| Pascal ProFile Manager | its volume table in block 0 | the table's volumes | CiderPress II's PPM notes |

## File systems

| Kind | Read | Write | Create | Source |
|---|---|---|---|---|
| DOS 3.2 | yes | yes | 13-sector, 35 tracks | DOS 3.2 manual |
| DOS 3.3, 40, 50, 80 tracks, 32 sectors | yes | yes | as today, plus 40 and 50 tracks | CiderPress II's DOS notes |
| HFS | yes | yes | 800K and hard disk sizes | Inside Macintosh: Files |
| SOS | yes | yes | 140K, ProFile 5 MB | Apple /// SOS Reference Manual |
| Gutenberg | yes | after the map is worked out (R52) | no | CiderPress II's Gutenberg notes, disks saved in Casso |
| RDOS 2.1, 3.2, 3.3 | yes | no | no | CiderPress II's RDOS notes |

HFS to ProDOS types: the HFS type `p` followed by the ProDOS type byte and
aux type, creator `pdos`, and the pairs Apple's File Type Note on HFS lists
(`TEXT` to $04, `BINA` to $06 and others). A ProDOS file copied to HFS is
written with the `p` form unless its type is one of the pairs.

## Interface

```cpp
enum class VolumeLayoutKind { Dos140K, Dos13, Blocks };

struct VolumeLayout
{
    VolumeLayoutKind  kind       = VolumeLayoutKind::Dos140K;
    uint32_t          blockCount = 280;
};

class IBlockStore
{
public:
    virtual             ~IBlockStore () = default;
    virtual uint32_t     GetBlockCount () const = 0;
    virtual HRESULT      ReadBlocks    (uint32_t first, uint32_t count, std::span<Byte> outBytes) = 0;
    virtual HRESULT      WriteBlocks   (uint32_t first, std::span<const Byte> bytes) = 0;
    virtual HRESULT      Commit        () = 0;
};

class ContainerPath
{
public:
    static HRESULT  Open    (const ContainerPath & path, IFileSystem & fs, OpenedContainer & outOpened);
    static HRESULT  Replace (const ContainerPath & path, IFileSystem & fs, std::span<const Byte> innerBytes);
};
```

`ContainerPath::Replace` rebuilds each container from the innermost step out
and replaces the host file last, through a temporary file.
