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
    DoubleHiResScreen, LoResScreen, MacPaint, Disassembly,
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
//  in a fixed color in a copy out. A paragraph with a picture is the
//  picture alone, in 32-bit BGRA pixels.
//
////////////////////////////////////////////////////////////////////////////////

enum class RunColor { Text, Good, Bad, Muted, Keyword, String, Comment };



struct Run
{
    std::wstring  text;
    bool          bold      = false;
    bool          italic    = false;
    bool          underline = false;
    RunColor      color     = RunColor::Text;

    bool  operator== (const Run &) const = default;
};



enum class Alignment { Left, Center, Right, Justified };



struct Paragraph
{
    Alignment              alignment = Alignment::Left;
    std::vector<Run>       runs;
    std::vector<uint32_t>  picture;
    int                    width     = 0;
    int                    height    = 0;

    bool  operator== (const Paragraph &) const = default;
};



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
