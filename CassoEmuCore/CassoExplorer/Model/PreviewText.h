#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/PreviewDecoder.h"
#include "Ui/Debugger/SourceSyntax.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewText
//
//  A preview's content as the text view's rows: a BASIC listing's line
//  numbers and, on request, the address each line loads at; an assembler's
//  source with its line numbers and its syntax colored; a file's details as
//  labels and values; and the message for a preview that failed.
//
////////////////////////////////////////////////////////////////////////////////

class PreviewText
{
public:
    //  Where DOS 3.3 on a 48K machine leaves HIMEM, against which Integer BASIC
    //  keeps its program.
    static constexpr int       kIntegerBasicHimem = 0x9600;

    //  Half of the largest luminance GetSourceColors weighs, 255 times 1000.
    static constexpr uint32_t  s_kDarkLumaLimit   = 127500;

    //  The columns of Disassembler::FormatLine: four address digits, two
    //  spaces, the bytes padded to nine, a space, then the instruction.
    static constexpr size_t    s_kAddressDigits     = 4;
    static constexpr size_t    s_kBytesColumn       = 6;
    static constexpr size_t    s_kInstructionColumn = 16;

    //  The text view's rows for a preview; given colors, an assembler's source
    //  and a disassembly have their syntax colored.
    static std::vector<DxuiTextView::Row>  BuildTextRows      (const PreviewContent & preview, bool lineAddresses, const SourceSyntax::Colors & colors = {});
    static std::vector<DxuiTextView::Row>  BuildDetailRows    (const std::vector<std::pair<std::wstring, std::wstring>> & details);
    static std::vector<DxuiTextView::Row>  BuildDocumentRows  (const std::vector<Paragraph> & paragraphs);

    //  A BASIC program's cross-reference, a row a line of it. Any other
    //  preview, and a program that cannot be cross-referenced, has the rows
    //  of BuildTextRows.
    static std::vector<DxuiTextView::Row>  BuildCrossReferenceRows (const PreviewContent & preview, bool lineAddresses, const SourceSyntax::Colors & colors = {});

    //  Visual Studio's code colors for the theme's background, dark or light,
    //  as Casso's debugger colors its source.
    static SourceSyntax::Colors            GetSourceColors    (const IDxuiTheme * theme);

    //  Where each line of a BASIC program starts in memory.
    static std::vector<Word>               GetLineAddresses   (const std::vector<Byte> & program, bool integerBasic);
    static std::vector<std::wstring>       SplitLineNumber    (const std::wstring & line);
    static std::wstring                    FormatPreviewError (const std::wstring & message);

    //  An NMOS 6502 undocumented instruction's mnemonic, which a disassembly
    //  colors as an instruction.
    static bool  IsUndocumentedMnemonic (const std::wstring & word);

    //  A BASIC listing's coloring: a row's address and line number cells, and
    //  its statements' keywords, variables, strings, numbers and REM.
    static void                            AddBasicSpans     (DxuiTextView::Row & ioRow, const SourceSyntax::Colors & colors);
    static std::vector<SourceSyntax::Run>  GetBasicRuns      (const std::wstring & statements);
    static size_t                          MatchBasicKeyword (const std::wstring & text, size_t at);
};
