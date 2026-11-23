#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FileFormat
//
//  The rows of contracts/file-types.md, each a format FileFormatTable
//  identifies and decodes.
//
////////////////////////////////////////////////////////////////////////////////

enum class FileFormat
{
    None, PascalText, RandomText, Merlin, ScAssembler, Lisa,
    BusinessBasic, PascalCode, Omf, AppleWorksWp, AppleWorksDb,
    AppleWorksSs, AppleWorksGsWp, Teach, VisiCalc, Gutenberg,
    MagicWindow, SuperHiRes, PackedSuperHiRes, Paintworks,
    SuperHiRes3200, DreamGrafix, SuperHiRes3201, IigsAnimation,
    Lz4fh, PrintShopGraphic, PrintShopGsClipArt, ShapeTable,
    IigsIcons, HiResFont, Apple3Font, FontrixFont, PrintShopFont,
    IigsFont, WizardryScenario, InfocomStory, HiResScreen,
    DoubleHiResScreen, LoResScreen, MacPaint, Disassembly, UltimaSave,
};





////////////////////////////////////////////////////////////////////////////////
//
//  Charset
//
//  The character set text is read in: Apple text, ASCII with the high bit
//  set or clear; Mac OS Roman, as the IIgs writes; or ISO 8859-1.
//
////////////////////////////////////////////////////////////////////////////////

enum class Charset { Default, AppleText, MacOsRoman, Iso8859_1 };





////////////////////////////////////////////////////////////////////////////////
//
//  SheetForm
//
//  A spreadsheet's cells as the values the program draws or as the formulas
//  typed, None for a file that has no such choice.
//
////////////////////////////////////////////////////////////////////////////////

enum class SheetForm { None, Values, Formulas };





////////////////////////////////////////////////////////////////////////////////
//
//  DoubleHiResForm
//
//  How a double hi-res picture is drawn. Auto is black and white for a
//  picture whose name ends .A2FM or whose dots are drawn as black and white,
//  and simple color otherwise. Simple color
//  is each four dots one of the sixteen colors, as Casso draws the screen;
//  windowed color is each dot the color of the four dots that end at it.
//
////////////////////////////////////////////////////////////////////////////////

enum class DoubleHiResForm { Auto, BlackAndWhite, SimpleColor, WindowedColor, LatchedColor };





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyCpu
//
//  The processor code is disassembled for on a copy, None for a code file
//  copied as its bytes: the 6502's documented opcodes, the 6502's with its
//  undocumented ones, the 65C02, or the 65816.
//
////////////////////////////////////////////////////////////////////////////////

enum class DisassemblyCpu { None, Mos6502, Mos6502Undocumented, Wdc65C02, Wdc65816 };





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeOptions
//
//  Chosen in a file's preview while its tab is open: the record length
//  that replaces the one found, 0 for the one found, and the character set,
//  Default for Mac OS Roman in a IIgs format and Apple text otherwise.
//
////////////////////////////////////////////////////////////////////////////////

struct DecodeOptions
{
    size_t           recordLength     = 0;
    Charset          charset          = Charset::Default;
    bool             formulas         = false;                    // a spreadsheet's formulas as typed, not their values
    bool             hiResMono        = false;                    // a hi-res picture's dots in black and white
    DoubleHiResForm  doubleHiRes      = DoubleHiResForm::Auto;
    bool             basicColor       = false;                    // a BASIC listing as RTF, its keywords, strings and remarks colored
    bool             basicRawControls = false;                    // an Applesoft listing's control characters as the bytes, not ^D
    DisassemblyCpu   disassembly      = DisassemblyCpu::None;     // a code file copied as a disassembly for this processor
    bool             longRegisters    = true;                     // 65816 code starts with 16-bit registers
    bool             followFlags      = true;                     // 65816 register widths follow REP and SEP
};





////////////////////////////////////////////////////////////////////////////////
//
//  Run, Paragraph and Grid
//
//  A document's text in styled runs, a tab a character of the text, and a
//  grid's headings and rows of cell strings.
//
//  A run's color is a role, drawn in the theme's color for it on screen and
//  in a fixed color in a copy out. A run with a swatch, an ARGB color, has a
//  square of that color before its text, as a map's legend has. A paragraph with a picture is the
//  picture alone, in 32-bit BGRA pixels.
//
////////////////////////////////////////////////////////////////////////////////

enum class RunColor { Text, Good, Bad, Muted, Keyword, String, Comment, Number, Label };



//  A swatch drawn as dark and light diagonal stripes, as a map marks what
//  couldn't be read, for a Run's swatch or a PictureTip line. Its alpha is
//  0, so it can't be a real color.
inline constexpr uint32_t  kHatchedSwatch = 0x00FFFFFF;



struct Run
{
    std::wstring  text;
    bool          bold      = false;
    bool          italic    = false;
    bool          underline = false;
    RunColor      color     = RunColor::Text;
    uint32_t      swatch    = 0;

    bool  operator== (const Run &) const = default;
};



enum class Alignment { Left, Center, Right, Justified };



//  What a cell of a picture is, as tooltip lines, each with the color it is
//  drawn in, or 0 for a line with no color.
struct PictureTip
{
    std::vector<std::pair<uint32_t, std::wstring>>  lines;

    bool  operator== (const PictureTip &) const = default;
};



struct RecordTable;



//  A picture's tips are a grid of tipCell-pixel cells from (tipLeft, tipTop),
//  tipColumns wide, row by row. A paragraph with a table is the table alone.
struct Paragraph
{
    Alignment                           alignment  = Alignment::Left;
    std::vector<Run>                    runs;
    std::vector<uint32_t>               picture;
    int                                 width      = 0;
    int                                 height     = 0;
    std::vector<PictureTip>             tips;
    int                                 tipCell    = 0;
    int                                 tipLeft    = 0;
    int                                 tipTop     = 0;
    int                                 tipColumns = 0;
    int                                 pingX      = -1;      // a picture's ping center, in its pixels; -1 for none
    int                                 pingY      = -1;
    bool                                canZoom    = false;   // the wheel zooms the picture and a drag pans it
    bool                                isLegend   = false;   // each run is an entry, laid out in columns as many as fit
    std::shared_ptr<const RecordTable>  table;

    bool  operator== (const Paragraph &) const = default;

    //  The tip of the cell at a pixel of the picture, or nullptr.
    const PictureTip * GetTipAt (int x, int y) const
    {
        int  column = (tipCell > 0 && x >= tipLeft) ? (x - tipLeft) / tipCell : -1;
        int  row    = (tipCell > 0 && y >= tipTop)  ? (y - tipTop)  / tipCell : -1;
        int  index  = row * tipColumns + column;

        return (column >= 0 && column < tipColumns && row >= 0 && index < (int) tips.size()) ? &tips[(size_t) index] : nullptr;
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  RecordTable
//
//  Records, such as a game's characters, drawn as a table with a heading
//  row and a row a record where the view is wide enough, and as each
//  record's card where it isn't. A record has a run for each column, the
//  lines of its card, and lines under it in either form, such as a
//  character's inventory. The title, when there is one, is a bold heading
//  above either form, with a rule under it above the cards.
//
////////////////////////////////////////////////////////////////////////////////

struct RecordTable
{
    struct Record
    {
        std::vector<Run>        cells;
        std::vector<Paragraph>  card;
        std::vector<Paragraph>  details;

        bool  operator== (const Record &) const = default;
    };

    std::wstring               title;
    std::vector<std::wstring>  headings;
    std::vector<Record>        records;

    bool  operator== (const RecordTable &) const = default;
};



//  The paragraphs with each table as its title in bold and its records'
//  cards and details, a blank line between records: the form a copy out
//  uses.
inline std::vector<Paragraph> FlattenRecordTables (const std::vector<Paragraph> & paragraphs)
{
    std::vector<Paragraph>  flat;

    for (const Paragraph & paragraph : paragraphs)
    {
        if (paragraph.table == nullptr)
        {
            flat.push_back (paragraph);
            continue;
        }

        if (!paragraph.table->title.empty())
        {
            Paragraph  title;

            title.runs = { Run { paragraph.table->title, true } };
            flat.push_back (title);
        }

        for (size_t i = 0; i < paragraph.table->records.size(); i++)
        {
            const RecordTable::Record &  record = paragraph.table->records[i];

            if (i > 0)
            {
                flat.push_back (Paragraph());
            }

            flat.insert (flat.end(), record.card.begin(),    record.card.end());
            flat.insert (flat.end(), record.details.begin(), record.details.end());
        }
    }

    return flat;
}



struct Grid
{
    std::vector<std::wstring>               columns;
    std::vector<std::vector<std::wstring>>  rows;

    bool  operator== (const Grid &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DecodedFile
//
//  A file decoded whole, as lines, a document, a grid or a picture. The
//  notes list what the decoder couldn't represent, such as a feature of a
//  later version or the missing lines of a short picture. The character set
//  is the one the text was read in, Default for a format that has one fixed
//  set, and the record length the one a random-access text file was split
//  by. The sheet form is the one a spreadsheet with formulas was decoded
//  in.
//
////////////////////////////////////////////////////////////////////////////////

struct DecodedFile
{
    enum class Kind { Lines, Document, Grid, Picture };

    FileFormat                 format       = FileFormat::None;
    Kind                       kind         = Kind::Lines;
    std::vector<std::wstring>  lines;
    std::vector<Paragraph>     paragraphs;
    Grid                       grid;
    std::vector<uint32_t>      bgra;
    int                        width        = 0;
    int                        height       = 0;
    std::vector<std::wstring>  notes;
    Charset                    charset      = Charset::Default;
    size_t                     recordLength = 0;
    SheetForm                  sheetForm    = SheetForm::None;
};
