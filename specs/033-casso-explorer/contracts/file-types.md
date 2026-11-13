# Contract: File types (User Story 18)

FR-156 to FR-171, research R35. `FileFormatTable` matches a file to one
row below, then runs the row's content check. A file matching no row, or
failing its check, falls to `PreviewDecoder`'s existing rules.

## Identification and outputs

Cells were filled from the CiderPress II note in the last column (T477).
Each is checked against the File Type Note or manual the note cites when
its decoder is written. The table is the test list for
`FileFormatTableTests`.

| Format | ProDOS type/aux | DOS 3.3 and Pascal | Preview | Out | Import | Note |
|---|---|---|---|---|---|---|
| Pascal textfile | $03 | Pascal kind 3 | text | `NAME.Pascal text.txt` | yes | Pascal |
| Random-access text | $04, aux = record length | T, entries at fixed spacing | grid | `NAME.Random text.csv` | no | DOS, ProDOS |
| Merlin source | $04, name ending `.S` | B, name starting `T.` | columns | `NAME.Merlin.txt` | yes | MerlinAsm |
| S-C Assembler | none, no ProDOS version was released | I, name often starting `S.`; each line a length byte, a line number and bytes from $20 to $C0, ending in $00 | listing | `NAME.S-C Assembler.txt` | no | SCAsm |
| LISA v2, v3, v4-5 | INT ($FA): aux $1000-$3FFF for v3, $4000-$5FFF for v4-5, often ending `.A` | alternate B for v2, first word usually $1800 | listing | `NAME.LISA.txt` | no | LisaAsm |
| Business BASIC | $09 | none | listing | `NAME.Business BASIC.txt` | no | BASIC |
| Pascal codefile | $02 | Pascal kind 2 | listing | `NAME.Pascal code.txt` | no | Pascal |
| OMF | $B1 to $BE | none | listing | `NAME.OMF.txt` | no | OMF |
| AppleWorks WP | $1A | none | document | `NAME.AppleWorks WP.rtf` | no | AppleWorks |
| AppleWorks DB | $19 | none | grid | `NAME.AppleWorks DB.csv` | no | AppleWorks |
| AppleWorks SS | $1B | none | grid | `NAME.AppleWorks SS.csv` | no | AppleWorks |
| AppleWorks GS WP | $50/$8010 | none | document | `NAME.AppleWorks GS WP.rtf` | no | AWGS |
| Teach | $50/$5445 | none | document | `NAME.Teach.rtf` | no | Teach |
| VisiCalc | $04, name ending `.VC` | T, name ending `.VC` | grid, recalculated | `NAME.VisiCalc.csv` | no | R35, R57 |
| Gutenberg | Gutenberg volume | Gutenberg volume | text | `NAME.Gutenberg.txt` | no | Gutenberg, GutenbergWP |
| Magic Window | none | B, name ending `.MW`, a 256-byte header starting $8D | text | `NAME.Magic Window.txt` | no | MagicWindow |
| Super hi-res | $C1/$0000 | none | picture | `NAME.Super Hi-Res.png` | no | SuperHiRes |
| Packed super hi-res | $C0/$0001, $C0/$0002 (Apple Preferred) | none | picture | `NAME.Super Hi-Res.png` | no | SuperHiRes |
| Paintworks | $C0/$0000 | none | picture | `NAME.Super Hi-Res.png` | no | SuperHiRes |
| 3200-color | $C1/$0002 | none | picture | `NAME.3200 Color.png` | no | SuperHiRes |
| DreamGrafix | $C0/$8005 | none | picture | `NAME.3200 Color.png` | no | SuperHiRes |
| 3201 | BIN, name ending `.3201`, starting `APP` $00 or `NRL` $00 | none | picture | `NAME.3200 Color.png` | no | SuperHiRes |
| IIgs animation | $C2/$0000 | none | picture, first frame | `NAME.Animation.png` | no | PaintworksAnim |
| LZ4FH | $08/$8066 | none | picture | `NAME.LZ4FH.png` | no | HiRes, fhpack |
| Print Shop graphic | generic types, by size and load address | B at $4800, $5800, $6800 or $7800, 572 or 576 bytes | picture | `NAME.Print Shop.png` | no | PrintShop |
| Print Shop GS clip art | $F8/$C323 color, $F8/$C313 monochrome | none | picture | `NAME.Print Shop.png` | no | PrintShop |
| Applesoft shape table | BIN ($06): a shape count, then offsets inside the file, each shape ending in $00 | B, the same check | sheet | `NAME.Shape table.png` | no | ShapeTable |
| IIgs icons | $CA | none | picture | `NAME.IIgs Icons.png` | no | GSFinderIcon |
| Apple II hi-res font | $07/$0000, or BIN, 768 bytes | B, 768 bytes | sheet | `NAME.Font.png` | no | HiRes |
| Apple /// hi-res font | $07/$0000, or BIN, 1024 bytes; the SOS file type is not in the notes and is taken from the CustomFONT Manual | none | sheet, 128 glyphs | `NAME.Font.png` | no | HiRes |
| Fontrix font | none | B, name starting `SET.`, $90 $F7 $B2 at $18, or $6F $08 $4D in an edited font | sheet | `NAME.Font.png` | no | Fontrix |
| Print Shop font | $F8/$C316 for Print Shop GS | B at $6000 or $5FF4, more than 576 bytes | sheet | `NAME.Font.png` | no | PrintShop |
| IIgs bitmap font | $C8 | none | sheet | `NAME.Font.png` | no | BitmapFont |
| Wizardry scenario | Pascal file `SCENARIO.DATA` on a Wizardry I to III disk | Pascal kind 5, the same name | lists, maps | `NAME.Wizardry.txt`, `NAME.Level N.png` | no | R56 |
| Infocom story | BIN, or a host `.z1` to `.z8` or `.dat`, with a Z-machine header: version 1 to 8 at $00, the high memory, dictionary and object table addresses inside the file | the story's sectors on an Infocom disk with no catalog, from track 3 on, opened as one file | listing | `NAME.Infocom.txt` | no | Z-Machine Standards Document 1.1 |

Import rows: a host file whose suffix is the row's Out suffix is put as
that format with no dialog, and the suffix is stripped from the catalog
name, as FR-018 does for the existing suffixes. The put dialog's type
choice lists the import rows the destination file system has.

## Preview kinds

| Kind | `PreviewContent` | Drawn by |
|---|---|---|
| text, listing | `lines` | `DxuiTextView`, as now |
| columns | `lines`, one `DxuiTextView` cell per field | `DxuiTextView` |
| document | `paragraphs` of styled runs | `DxuiTextView` with run styles and row alignment |
| grid | `gridColumns`, `gridRows` | `DxuiListView` in Details |
| picture, sheet | `bgra`, `width`, `height` | `DxuiFramebufferView`, as now |

## Interface

```cpp
enum class FileFormat { None, PascalText, RandomText, Merlin, ScAssembler, Lisa,
                        BusinessBasic, PascalCode, Omf, AppleWorksWp, AppleWorksDb,
                        AppleWorksSs, AppleWorksGsWp, Teach, VisiCalc, Gutenberg,
                        MagicWindow, SuperHiRes, PackedSuperHiRes, Paintworks,
                        SuperHiRes3200, DreamGrafix, SuperHiRes3201, IigsAnimation,
                        Lz4fh, PrintShopGraphic, PrintShopGsClipArt, ShapeTable,
                        IigsIcons, HiResFont, Apple3Font, FontrixFont, PrintShopFont,
                        IigsFont, WizardryScenario, InfocomStory };

class FileFormatTable
{
public:
    static FileFormat  Identify  (const FileEntry & entry, VolumeKind volume, std::span<const Byte> bytes);
    static HRESULT     Decode    (FileFormat format, std::span<const Byte> bytes, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);
    static HRESULT     Export    (const DecodedFile & file, std::vector<Byte> & outBytes);
    static HRESULT     Import    (FileFormat format, VolumeKind volume, std::span<const Byte> hostBytes, FilePayload & outPayload, std::wstring & outError);
    static const wchar_t *  GetSuffix (FileFormat format);
};
```

`Decode` takes a `DecodeOptions`: the record length that replaces the one found (FR-160) and the character set text is read in (FR-173).

`Decode` fails with `ERROR_INVALID_DATA` and a reason in words for a file
that fails its check, which FR-169's preview and copy report. `Export`
writes RTF, CSV, text or PNG by the `DecodedFile`'s kind.

## BASIC tools (Story 37, R55)

| Command | Input | Output |
|---|---|---|
| `casso basic tokenize --applesoft\|--integer IN OUT` | a listing in UTF-8 or Apple text | the program BASIC saves, with the load address Applesoft's SAVE writes |
| `casso basic list IN OUT` | a tokenized program | its listing, as Get writes it |
| `casso basic renumber --start N --step N IN OUT` | a tokenized program | the renumbered program; fails with the line when a number would pass 63999 or 32767 |
| `casso basic xref IN` | a tokenized program | the cross-reference, as the preview lists it |

Cross-reference listing: one section each for variables, line references, and CALL, PEEK and POKE addresses, each entry followed by the lines it is on, in ascending order.

## Gutenberg volumes (Story 34)

Worked out from disks Gutenberg, Jr. formatted and wrote in Casso on a //e:
a disk FORMAT had just made, the same disk after one short document, after
a long document, after a delete, and after enough documents to fill the
first directory sector. AVAILABLE SPACE reported 480, 479 and 440 empty
sectors for the first three. The sector and directory layout is from
CiderPress II's Gutenberg notes.

**The map** is track 17, sector 6. It covers tracks 3 to 34, eight bytes a
track, one byte for each pair of sectors. Bit 7 is set when the even sector
is in use, and bit 0 when the odd one is. The byte for track `t`, sector `s`
is at `(t - 3) * 8 + s / 2`. Tracks 0 to 2 aren't in the map, and Gutenberg
never allocates them on a data disk. A free sector is one whose bit is
clear, and the free count is the number of clear bits.

| Sectors | FORMAT writes | Meaning |
|---|---|---|
| Track 17, sectors 0 and 1 | `$FF` | reserved |
| Track 17, sectors 2 to 7 | `$81` | in use: the map, the directory, and four reserved for it |
| Track 33, sector 8 to track 34, sector 15 | `$FF` | reserved |
| Everything else | `$00` | free |

Gutenberg's master disks have other bit patterns in some bytes, such as
`$95` and `$9F`. Only bits 7 and 0 are read, and a write doesn't change the
other bits.

**Allocation** takes the lowest free sector in map order, from track 3,
sector 0. A deleted file's sectors are reused by the next write.

**A sector header** is the previous, current and next track and sector. A
track or sector of 0 is stored as `$00` in a header and as `$40` in a
directory entry. A link to the file's first sector has bit 7 set in its
sector byte, except the last sector's next link, which points to the first
sector with bit 7 set in its track byte instead. A one-sector file at track
3, sector 0 has the header `03 80 03 80 83 00`.

**A document** is high ASCII with `$8D` line ends, and ends at its first
`$00`. The rest of its last sector is `$00`.

**The directory** grows by one sector, from the map like any other, when a
new file has no empty slot. The new sector repeats the volume name and
`$8D` markers, and is allocated before the file's data. The old last
sector's next link points to it, and the new sector's next link points back to
track 17, sector 7 with the end flag. Gutenberg doesn't change the first
sector's previous link.

**A delete** clears the file's map bits and fills the entry's first 15
bytes with spaces (`$A0`). The data sectors are left as they were.

**Casso's rules.** `GutenbergVolume` writes, renames, deletes, locks and
unlocks as above. A name is 1 to 12 printable characters with no space or
slash. A locked document (`L`), DIR among them, can't be replaced,
renamed or deleted. A type Gutenberg doesn't store becomes a document, a
document without a `$00` gains one, and text copied from another file system
is converted to high ASCII with `$8D` line ends. The integrity check counts
tracks 0 to 2, `$FF` bytes, and the in-use sectors of track 17, sectors 0
to 7, as the volume's.
