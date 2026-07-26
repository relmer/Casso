#pragma once

#include "Pch.h"




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterStatus / PrinterStatusModel
//
//  The printer's user-facing status for the chrome indicator + panel (FR-019 /
//  FR-020). Pure and clock-injected so the state machine is unit-tested; the
//  shell samples the live printer signals each frame and feeds them in.
//
//  Signals (all observable without touching the drain thread's raster):
//   - activityCount: a monotonic counter the worker advances as it drains guest
//     bytes. A change since the last sample means the guest is actively
//     printing; "Receiving" latches for receivingWindowMs after the last change
//     so brief gaps between strikes don't flicker the indicator.
//   - hasContent: the strip currently holds printed output awaiting a finish.
//   - hasError: the last delivery/op failed (sticky until the shell clears it).
//
//  Priority: Error > Receiving > Pending > Idle.
//
////////////////////////////////////////////////////////////////////////////////

enum class PrinterStatus
{
    Idle,        // no strip, nothing arriving
    Receiving,   // bytes arriving now (a print is in progress)
    Pending,     // a strip is waiting to be finished / copied / discarded
    Error        // last operation failed
};


class PrinterStatusModel
{
public:
    struct Config
    {
        double  receivingWindowMs = 400.0;   // "Receiving" latch after last byte
    };

    explicit PrinterStatusModel (const Config & cfg = Config ());

    // Sample the live signals at monotonic time nowMs and recompute the status.
    void           Update (uint64_t activityCount, double nowMs, bool hasContent, bool hasError);

    PrinterStatus  Status () const;

    // Forget history (e.g. on machine switch): next Update re-primes the
    // activity baseline without reporting a spurious "Receiving".
    void           Reset ();

private:
    Config         m_cfg;
    uint64_t       m_lastActivity   = 0;
    double         m_lastActivityMs = 0.0;
    bool           m_primed         = false;
    bool           m_sawActivity    = false;
    PrinterStatus  m_status         = PrinterStatus::Idle;
};
