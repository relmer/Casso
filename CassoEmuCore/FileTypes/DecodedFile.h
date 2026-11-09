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
    IigsFont, WizardryScenario, InfocomStory,
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
//  DecodeOptions
//
//  Chosen in a file's preview while its tab is open: the record length
//  that replaces the one found, 0 for the one found, and the character set,
//  Default for Mac OS Roman in a IIgs format and Apple text otherwise.
//
////////////////////////////////////////////////////////////////////////////////

struct DecodeOptions
{
    size_t   recordLength = 0;
    Charset  charset      = Charset::Default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  Run, Paragraph and Grid
//
//  A document's text in styled runs, a tab a character of the text, and a
//  grid's headings and rows of cell strings.
//
////////////////////////////////////////////////////////////////////////////////

struct Run
{
    std::wstring  text;
    bool          bold      = false;
    bool          italic    = false;
    bool          underline = false;

    bool  operator== (const Run &) const = default;
};



enum class Alignment { Left, Center, Right, Justified };



struct Paragraph
{
    Alignment         alignment = Alignment::Left;
    std::vector<Run>  runs;

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
//  later version or the missing lines of a short picture.
//
////////////////////////////////////////////////////////////////////////////////

struct DecodedFile
{
    enum class Kind { Lines, Document, Grid, Picture };

    FileFormat                 format = FileFormat::None;
    Kind                       kind   = Kind::Lines;
    std::vector<std::wstring>  lines;
    std::vector<Paragraph>     paragraphs;
    Grid                       grid;
    std::vector<uint32_t>      bgra;
    int                        width  = 0;
    int                        height = 0;
    std::vector<std::wstring>  notes;
};
