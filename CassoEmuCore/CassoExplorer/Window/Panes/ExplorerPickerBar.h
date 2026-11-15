#pragma once

#include "Pch.h"

struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerBar
//
//  A picker's bar along the bottom of the window: the receiving drive and
//  the images listed for it, the filter entries as a choice when there are
//  several, the create command when the request has one, and the confirm
//  and cancel buttons. In a window that is not a picker it has no controls
//  and its band has no height.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerPickerBar
{
public:
    explicit ExplorerPickerBar (ExplorerContext & context);

    static constexpr int  kBandDip       = 52;
    static constexpr int  kButtonDip     = 32;
    static constexpr int  kEdgePadDip    = 12;
    static constexpr int  kGapDip        = 8;
    static constexpr int  kFilterMaxDip  = 320;

    //  The band's height in the window: kBandDip in a picker, 0 otherwise.
    int   GetBandDip () const;

    void  Create     ();
    void  Layout     (const RECT & band);

    //  The confirm button follows the selection, and the filter button the
    //  active entry.
    void  Update     ();

    bool  IsConfirmEnabled () const;

private:
    void  ShowFilterMenu ();

    ExplorerContext  & m_context;
    DxuiLabel        * m_header  = nullptr;
    DxuiButton       * m_filter  = nullptr;
    DxuiButton       * m_create  = nullptr;
    DxuiButton       * m_confirm = nullptr;
    DxuiButton       * m_cancel  = nullptr;
};
