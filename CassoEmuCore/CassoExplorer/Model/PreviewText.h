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

    //  The text view's rows for a preview; given colors, an assembler's source
    //  has its syntax colored.
    static std::vector<DxuiTextView::Row>  BuildTextRows      (const PreviewContent & preview, bool lineAddresses, const SourceSyntax::Colors & colors = {});

    //  Visual Studio's code colors for the theme's background, dark or light,
    //  as Casso's debugger colors its source.
    static SourceSyntax::Colors            GetSourceColors    (const IDxuiTheme * theme);

    //  Where each line of a BASIC program starts in memory.
    static std::vector<Word>               GetLineAddresses   (const std::vector<Byte> & program, bool integerBasic);
    static std::vector<std::wstring>       SplitLineNumber    (const std::wstring & line);
    static std::wstring                    FormatPreviewError (const std::wstring & message);
};