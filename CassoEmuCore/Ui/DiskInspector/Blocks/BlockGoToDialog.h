#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockGoToDialog
//
//  Block mode's Go to: a block number in decimal, or in hex after '$', from 0
//  to the image's last block. OK is enabled only for a number in range.
//
////////////////////////////////////////////////////////////////////////////////

class BlockGoToDialog : public DxuiDialogWindow
{
public:
    static constexpr SIZE  kSizeDip = { 420, 190 };

    //  The dialog, modal over `owner`. True with the block chosen.
    static bool  Choose (HWND owner, const IDxuiTheme * theme, uint32_t blockCount, uint32_t current, uint32_t & outBlock);

protected:
    void  OnCreate () override;

private:
    class Body;

    void  Validate ();

    const IDxuiTheme  * m_theme      = nullptr;
    uint32_t            m_blockCount = 0;
    uint32_t            m_current    = 0;
    uint32_t            m_block      = 0;
    bool                m_isValid    = false;
    Body              * m_body       = nullptr;
    DxuiLabel           m_label;
    DxuiTextInput       m_input;
    DxuiLabel           m_error;
    DxuiButton        * m_ok         = nullptr;
};
