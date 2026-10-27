#pragma once

#include "Pch.h"

#include "Debugger/Reverse/HistoryTimelineScrub.h"

class ShellHistory;
struct HistoryThumbnailCell;





////////////////////////////////////////////////////////////////////////////////
//
//  RewindBar
//
//  The emulator window's history timeline: a toolbar with a strip of
//  history's pictures over the bottom of the emulated screen, with where
//  history begins before them and Live after them. It opens when the pointer
//  enters a band kOpeningBandDip tall, scaled for the DPI, along the screen's
//  bottom edge, and closes when the pointer moves off it. It never opens
//  while the pointer is captured or history is off, and covers nothing while
//  closed.
//
//  A click on the strip seeks there and a drag of its playhead line scrubs,
//  through HistoryTimelineClick and HistoryTimelineScrub as the debugger's
//  timeline does; a running machine is paused first, and the machine is left
//  paused where it moved, for Run to replay from there. Every pointer event
//  over the bar goes to the bar, and none to the guest mouse.
//
//  All on the UI thread.
//
////////////////////////////////////////////////////////////////////////////////

class RewindBar
{
public:
    static constexpr float  kOpeningBandDip = 8.0f;
    static constexpr float  kLabelRoomDp    = 20.0f;
    static constexpr int    kLabelRooms     = 2;        // above the pictures and below them

    //  Whether the pointer opens the bar: in the bottom kOpeningBandDip of
    //  the screen at that DPI, with the pointer free and history on.
    static bool  IsInOpeningBand (POINT pointer, const RECT & screen, UINT dpi, bool isCaptured, bool isHistoryOn);

                 RewindBar  ();

    void  Configure    (ShellHistory & history, IDxuiTextRenderer * text);
    void  SetPopupHost (DxuiHwndSource * host);
    void  Adopt        (DxuiPanel & root);

    //  The screen the bar lies along the bottom of.
    void  Layout       (const RECT & screen, const DxuiDpiScaler & scaler);

    //  A pointer event, true when the bar takes it. A move into the opening
    //  band opens the bar and a move off it closes it; a press on it, and the
    //  drag and release that follow, go to the bar.
    bool  OfferMouse   (DxuiMouseEventKind kind, int x, int y, bool isCaptured, int64_t nowMs);
    void  Close        ();

    //  Once a frame: the pictures drawn only while the bar is open, the
    //  strip's preview, and the seek a drag is due.
    void  Sync         ();

    //  Hidden from a capture paint, as the other overlays about the
    //  application rather than the machine are.
    void  SetHidden    (bool isHidden);

    bool           IsOpen     () const { return m_isOpen; }
    RECT           GetBounds  () const { return m_bar.GetBounds(); }
    DxuiTooltip &  GetTooltip ()       { return m_tooltip; }
    bool           WantsTick  () const { return m_tooltip.WantsTick(); }

private:
    void  Open          ();
    void  OnSeek        (const HistoryThumbnailCell & cell);
    void  OnScrub       (uint64_t cycle, bool isFinal);
    void  ApplyScrub    (const HistoryTimelineScrubStep & step);
    void  ShowState     ();

    ShellHistory                  * m_history    = nullptr;
    DxuiToolbar                     m_bar;
    DxuiImageStrip                  m_strip;
    std::shared_ptr<DxuiCommand>    m_command;
    DxuiTooltip                     m_tooltip;
    HistoryTimelineScrub            m_scrub;
    RECT                            m_screen     = {};
    DxuiDpiScaler                   m_scaler;
    bool                            m_isOpen     = false;
    bool                            m_isPressed  = false;   // a press on the bar, whose drag and release go to the bar too
    bool                            m_isHidden   = false;
};
