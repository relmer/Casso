#pragma once

#include "Pch.h"

#include "ChromeTheme.h"
#include "Core/IDxuiControl.h"
#include "LedIndicator.h"
#include "../DriveWidgetState.h"





class IDriveCommandSink;


enum class DriveWidgetRegion
{
    None,
    Body,
    Eject,
};


//
//  DriveWidget is Casso-specific (skeuomorphic Apple Disk II). The
//  IDxuiControl Paint signature uses an IDxuiTheme, but the widget
//  expects that theme is actually a ChromeTheme and static_casts to
//  read drive-body / bezel / label palette fields. A debug
//  dynamic_cast guard in Paint pins the contract.
//
class DriveWidget : public IDxuiControl
{
public:
    DriveWidget  ();
    ~DriveWidget () override = default;

    void               Initialize      (int slot, int drive, IDriveCommandSink * pSink);
    void               Hide            ()
    {
        m_bodyRect  = {};
        m_faceRect  = {};
        m_slotRect  = {};
        m_ejectRect = {};
        m_labelRect = {};
    }
    void               SetPerspectiveSkewPx (int skewPx) { m_perspectiveSkewPx = skewPx; }
    void               SetCompact      (bool compact)    { m_compact = compact; }
    bool               IsCompact       () const          { return m_compact; }
    void               SetFocused      (bool focused)    { m_focused = focused; }
    bool               IsFocused       () const          { return m_focused; }
    void               SyncFromState   (const DriveWidgetState & state);

    void               Paint           (IDxuiPainter        & painter,
                                        IDxuiTextRenderer   & text,
                                        const IDxuiTheme    & theme) override;

    //
    //  IDxuiControl::Layout — treats boundsDip.left / boundsDip.top as
    //  the widget's anchor and computes its own body / face / slot /
    //  eject / label rects from per-DPI metrics scaled off
    //  scaler.Dpi(). boundsDip.right / bottom are ignored (the widget
    //  has an intrinsic size; SetBounds is overwritten with the
    //  computed OuterRect at the end).
    //
    void               Layout          (const RECT          & boundsDip,
                                        const DxuiDpiScaler & scaler) override;

    DriveWidgetRegion  HitTest         (int x, int y) const;
    HRESULT            OnDrop          (const std::wstring & path);
    RECT               BodyRect        () const { return m_bodyRect; }
    RECT               OuterRect       () const
    {
        RECT  r = m_bodyRect;
        if (m_labelRect.bottom > r.bottom) { r.bottom = m_labelRect.bottom; }
        return r;
    }
    RECT               EjectRect       () const { return m_ejectRect; }
    LedState           Led             () const { return m_led.GetState(); }
    int                Drive           () const { return m_drive; }

private:
    void                PaintBasenameLabel (IDxuiTextRenderer & text,
                                            const ChromeTheme & theme,
                                            UINT                dpi);

    int                 m_slot      = 6;
    int                 m_drive     = 0;
    IDriveCommandSink * m_sink      = nullptr;
    RECT                m_bodyRect  = {};
    RECT                m_faceRect  = {};
    RECT                m_slotRect  = {};
    RECT                m_ejectRect = {};
    RECT                m_labelRect = {};
    LedIndicator        m_led;
    DriveWidgetState    m_state;
    UINT                m_dpi               = 96;
    int                 m_perspectiveSkewPx = 0;
    bool                m_compact           = false;
    bool                m_focused           = false;
};
