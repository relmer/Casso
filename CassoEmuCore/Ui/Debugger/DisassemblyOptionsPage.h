#pragma once

#include "Pch.h"

#include "Ui/Debugger/DisassemblyOptions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyOptionsPage
//
//  The Disassembly tab of the debugger's Options: a check box for each
//  DisassemblyOptions option, the disassembly toolbar's five first, then the
//  four the toolbar does not have.
//
////////////////////////////////////////////////////////////////////////////////

class DisassemblyOptionsPage : public DxuiPropertyPage
{
public:
    explicit DisassemblyOptionsPage (std::wstring title);

    void                SetOptions (const DisassemblyOptions & options);
    DisassemblyOptions  GetOptions () const;

    void  Layout (const RECT & rect, const DxuiDpiScaler & scaler) override;

    // Test accessor.
    DxuiCheckbox  & GetCheckbox (DisassemblyOptions::Option option) { return m_checks[(size_t) option]; }

private:
    static constexpr int  kPagePadDp    = 16;
    static constexpr int  kRowHeightDp  = 28;
    static constexpr int  kRowGapDp     = 6;
    static constexpr int  kSectionGapDp = 18;

    static RECT  MakeRect (int l, int t, int w, int h);

    std::array<DxuiCheckbox, DisassemblyOptions::kOptionCount>  m_checks;
};
