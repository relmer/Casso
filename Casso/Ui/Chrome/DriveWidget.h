#pragma once

#include "Pch.h"

#include "ChromeTheme.h"
#include "LedIndicator.h"
#include "../DriveWidgetState.h"
#include "../DwriteTextRenderer.h"
#include "../DxUiPainter.h"





class IDriveCommandSink;


enum class DriveWidgetRegion
{
    None,
    Body,
    Eject,
};


class DriveWidget
{
public:
    DriveWidget  () = default;
    ~DriveWidget () = default;

    void               Initialize      (int slot, int drive, IDriveCommandSink * pSink);
    void               Layout          (int x, int y, UINT dpi);
    void               SyncFromState   (const DriveWidgetState & state);
    void               Paint           (DxUiPainter & painter,
                                         DwriteTextRenderer & text,
                                         const ChromeVisualState & visual,
                                         const ChromeTheme & theme);
    DriveWidgetRegion  HitTest         (int x, int y) const;
    HRESULT            OnDrop          (const std::wstring & path);
    RECT               BodyRect        () const { return m_bodyRect; }
    RECT               EjectRect       () const { return m_ejectRect; }
    LedState           Led             () const { return m_led.GetState(); }
    int                Drive           () const { return m_drive; }

private:
    int                 m_slot      = 6;
    int                 m_drive     = 0;
    IDriveCommandSink * m_sink      = nullptr;
    RECT                m_bodyRect  = {};
    RECT                m_slotRect  = {};
    RECT                m_ejectRect = {};
    LedIndicator        m_led;
    DriveWidgetState    m_state;
};
