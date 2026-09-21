#pragma once

#include "Window/DxuiDialogWindow.h"
#include "Widgets/DxuiCheckbox.h"
#include "Core/DxuiPanel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FlagsDialogPanel
//
//  One checkbox a flag, top to bottom, filling the dialog's content area.
//
////////////////////////////////////////////////////////////////////////////////

class FlagsDialogPanel : public DxuiPanel
{
public:
    void  Init   (std::span<DxuiCheckbox> boxes);
    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;

    static constexpr int  kRowDip = 26;

private:
    std::span<DxuiCheckbox>  m_boxes;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FlagsDialog
//
//  Edits the P register a flag at a time. Bit 5, which the 6502 does not
//  use, keeps whatever value it had.
//
////////////////////////////////////////////////////////////////////////////////

class FlagsDialog : public DxuiDialogWindow
{
public:
    struct Flag
    {
        wchar_t          letter = 0;
        Byte             bit    = 0;
        const wchar_t  * name   = nullptr;
    };

    //  N V B D I Z C, from bit 7 down, with the bit each is and what it means.
    static const std::array<Flag, 7> &  GetFlags ();

    //  Each flag on its own line, "N  Negative  1", for a tooltip.
    static std::wstring  Describe (Byte p);

    //  Runs the dialog modally over `owner`. False when it was cancelled.
    static bool  Ask (HWND owner, const IDxuiTheme * theme, Byte p, Byte & outP);

protected:
    void  OnCreate() override;

private:
    const IDxuiTheme             * m_theme     = nullptr;
    Byte                           m_p         = 0;
    bool                           m_confirmed = false;
    std::array<DxuiCheckbox, 7>    m_boxes;
    FlagsDialogPanel             * m_body      = nullptr;
};
