#pragma once

#include "Pch.h"

#include "Ui/Debugger/DisassemblyOptions.h"
#include "Ui/Debugger/DisassemblyPreview.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyOptionsPage
//
//  The Disassembly tab of the debugger's Options: a check box for each
//  DisassemblyOptions option, the disassembly toolbar's five in the left
//  column and the four the toolbar does not have in the right, the kind of
//  row to dim while source is visible under them, then a preview of a
//  disassembly view with the options as set.
//
////////////////////////////////////////////////////////////////////////////////

class DisassemblyOptionsPage : public DxuiPropertyPage
{
public:
    explicit DisassemblyOptionsPage (std::wstring title);

    void                SetOptions (const DisassemblyOptions & options);
    DisassemblyOptions  GetOptions () const;

    void  Layout (const RECT & rect, const DxuiDpiScaler & scaler) override;

    // Test accessors.
    DxuiCheckbox        & GetCheckbox (DisassemblyOptions::Option option) { return m_checks[(size_t) option]; }
    DisassemblyPreview  & GetPreview  ()                                  { return m_preview; }
    DxuiRadioGroup      & GetDimRadio ()                                  { return m_dim; }

private:
    static constexpr int  kPagePadDp    = 16;
    static constexpr int  kRowHeightDp  = 28;
    static constexpr int  kRowGapDp     = 6;
    static constexpr int  kSectionGapDp = 18;
    static constexpr int  kColumnGapDp  = 24;
    static constexpr int  kDimOptionDp  = 170;

    static RECT  MakeRect (int l, int t, int w, int h);

    std::array<DxuiCheckbox, DisassemblyOptions::kOptionCount>  m_checks;
    DxuiLabel                                                   m_dimLabel;
    DxuiRadioGroup                                              m_dim;
    DxuiLabel                                                   m_previewLabel;
    DisassemblyPreview                                          m_preview;
};
