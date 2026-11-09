# Contract: File types (User Story 18)

FR-156 to FR-171, research R35. `FileFormatTable` matches a file to one
row below, then runs the row's content check. A file matching no row, or
failing its check, falls to `PreviewDecoder`'s existing rules.

## Identification and outputs

A cell marked *notes* is filled from the CiderPress II note in the last
column, against the File Type Note it cites, by the first task of the
phase. The table is the test list for `FileFormatTableTests`.

| Format | ProDOS type/aux | DOS 3.3 and Pascal | Preview | Out | Import | Note |
|---|---|---|---|---|---|---|
| Pascal textfile | $03 | Pascal kind 3 | text | `NAME.Pascal text.txt` | yes | Pascal |
| Random-access text | $04, aux = record length | T, entries at fixed spacing | grid | `NAME.Random text.csv` | no | DOS, ProDOS |
| Merlin source | $04, name ending `.S` | B, name starting `T.` | columns | `NAME.Merlin.txt` | yes | MerlinAsm |
| S-C Assembler | *notes* | I, content check | listing | `NAME.S-C Assembler.txt` | no | SCAsm |
| LISA v2, v3, v4-5 | *notes* | *notes* | listing | `NAME.LISA.txt` | no | LisaAsm |
| Business BASIC | $09 | none | listing | `NAME.Business BASIC.txt` | no | BASIC |
| Pascal codefile | $02 | Pascal kind 2 | listing | `NAME.Pascal code.txt` | no | Pascal |
| OMF | $B1 to $BE | none | listing | `NAME.OMF.txt` | no | OMF |
| AppleWorks WP | $1A | none | document | `NAME.AppleWorks WP.rtf` | no | AppleWorks |
| AppleWorks DB | $19 | none | grid | `NAME.AppleWorks DB.csv` | no | AppleWorks |
| AppleWorks SS | $1B | none | grid | `NAME.AppleWorks SS.csv` | no | AppleWorks |
| AppleWorks GS WP | $50/$8010 | none | document | `NAME.AppleWorks GS WP.rtf` | no | AWGS |
| Teach | $50/$5445 | none | document | `NAME.Teach.rtf` | no | Teach |
| VisiCalc | $04, name ending `.VC` | T, name ending `.VC` | grid | `NAME.VisiCalc.csv` | no | R35 |
| Gutenberg | Gutenberg volume | Gutenberg volume | text | `NAME.Gutenberg.txt` | no | Gutenberg, GutenbergWP |
| Magic Window | *notes* | *notes* | text | `NAME.Magic Window.txt` | no | MagicWindow |
| Super hi-res | $C1/$0000 | none | picture | `NAME.Super Hi-Res.png` | no | SuperHiRes |
| Packed super hi-res | $C0/$0001, $C0/$0002 (Apple Preferred) | none | picture | `NAME.Super Hi-Res.png` | no | SuperHiRes |
| Paintworks | $C0/$0000 | none | picture | `NAME.Super Hi-Res.png` | no | SuperHiRes |
| 3200-color | $C1/$0002 | none | picture | `NAME.3200 Color.png` | no | SuperHiRes |
| DreamGrafix | $C0/$8005 | none | picture | `NAME.3200 Color.png` | no | SuperHiRes |
| 3201 | name ending `.3201`, *notes* | none | picture | `NAME.3200 Color.png` | no | SuperHiRes |
| IIgs animation | $C2/$0000 | none | picture, first frame | `NAME.Animation.png` | no | PaintworksAnim |
| LZ4FH | $08/$8066 | none | picture | `NAME.LZ4FH.png` | no | HiRes, fhpack |
| Print Shop graphic | *notes* | B, *notes* | picture | `NAME.Print Shop.png` | no | PrintShop |
| Print Shop GS clip art | *notes* | none | picture, color | `NAME.Print Shop.png` | no | PrintShop |
| Applesoft shape table | $06 BIN, content check | B, content check | sheet | `NAME.Shape table.png` | no | ShapeTable |
| IIgs icons | $CA | none | picture | `NAME.IIgs Icons.png` | no | GSFinderIcon |
| Apple II hi-res font | none | B, 768 bytes | sheet | `NAME.Font.png` | no | *notes* |
| Apple /// hi-res font | $07, *notes* | none | sheet, 128 glyphs | `NAME.Font.png` | no | *notes* |
| Fontrix font | *notes* | B, *notes* | sheet | `NAME.Font.png` | no | Fontrix |
| Print Shop font | *notes* | B, *notes* | sheet | `NAME.Font.png` | no | PrintShop |
| IIgs bitmap font | $C8 | none | sheet | `NAME.Font.png` | no | BitmapFont |

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
                        IigsFont };

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
