#pragma once

#include "Pch.h"

#include "Core/IDxuiControl.h"
#include "Devices/Printer/PrinterStatusModel.h"   // PrinterStatus




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterIndicator
//
//  Casso-specific chrome control: a compact printer glyph with a status LED,
//  anchored in the command-bar dead space to the right of the centred drive
//  widgets (FR-019). It shows, at a glance, whether the emulated printer is
//  idle, receiving a print, holding a finished page, or in error -- the
//  discoverability cue the printing feature was missing.
//
//  This first cut is a status display only. The eventual click-to-toggle of
//  the printer panel (FR-020) is wired once that panel exists; until then the
//  shell does not register a hit rect for it.
//
//  Layout honours the caller-supplied rect (the shell sizes it to the command-
//  bar dead space and bottom-aligns it to the drive shelf). Paint draws nothing
//  when hidden (a machine with no printer card shows no printer UI).
//
////////////////////////////////////////////////////////////////////////////////

class PrinterIndicator : public IDxuiControl
{
public:
    PrinterIndicator  () = default;
    ~PrinterIndicator () override = default;

    void           SetStatus (PrinterStatus status) { m_status = status; }
    PrinterStatus  Status    () const               { return m_status; }

    // A machine with no printer card shows nothing: zero the rect and latch
    // hidden so Paint early-outs. Layout clears the latch.
    void           Hide ()
    {
        m_bodyRect = {};
        m_hidden   = true;
    }
    bool           Hidden () const { return m_hidden; }

    void           Paint  (IDxuiPainter        & painter,
                           IDxuiTextRenderer   & text,
                           const IDxuiTheme    & theme) override;

    void           Layout (const RECT          & boundsDip,
                           const DxuiDpiScaler & scaler) override;

    RECT           OuterRect () const { return m_bodyRect; }

private:
    // Maps the current status to its LED core colour.
    static uint32_t  StatusCore (PrinterStatus status);

    PrinterStatus  m_status   = PrinterStatus::Idle;
    RECT           m_bodyRect = {};
    UINT           m_dpi      = 96;
    bool           m_hidden   = false;
};
