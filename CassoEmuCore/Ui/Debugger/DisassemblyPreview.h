#pragma once

#include "Pch.h"

#include "Ui/Debugger/DebuggerTextColors.h"
#include "Ui/Debugger/DisassemblyOptions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyPreview
//
//  A short sample program drawn as a disassembly view draws it, with the
//  options the Disassembly tab has checked: source lines with their numbers,
//  symbols, an indirect load with its pointer and operand value, and the PC
//  on a taken branch with its arrow. It is for looking at only; it takes no
//  clicks or keys.
//
////////////////////////////////////////////////////////////////////////////////

class DisassemblyPreview : public DxuiListView
{
public:
    static constexpr int  kMinWidthDip = 560;
    static constexpr int  kHeightDip   = 236;

    DisassemblyPreview ();

    void  SetOptions (const DisassemblyOptions & options);

    void  Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override  { UNREFERENCED_PARAMETER (ev); return false; }
    bool  OnKey   (const DxuiKeyEvent & ev) override    { UNREFERENCED_PARAMETER (ev); return false; }

    //  The rows as last built, for tests.
    const std::vector<DisassemblyOptions::Row> &  GetSampleRows () const { return m_rows; }

    static std::vector<DebuggerViewSnapshot::CodeLine>  MakeSampleLines ();
    static std::wstring                                 GetSampleSource (int sourceLine);

private:
    void  BuildRows (const DebuggerTextColors::Set & colors, uint32_t numberArgb, uint32_t pcMarkerArgb);

    std::vector<DebuggerViewSnapshot::CodeLine>  m_lines;
    std::vector<DisassemblyOptions::Row>         m_rows;
    DisassemblyOptions                           m_options;
    DebuggerTextColors::Cache                    m_colors;
};
