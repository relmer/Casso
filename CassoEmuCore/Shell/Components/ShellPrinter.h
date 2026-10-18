#pragma once

#include "Pch.h"

#include "Devices/Printer/PrinterStatusModel.h"
#include "Print/PrinterWorker.h"
#include "Shell/ModernPrintDialog.h"
#include "Ui/Chrome/PrinterStatusLed.h"



class EmulatorShell;
class IPrintDialog;
class PrinterPanel;
struct MachineBuildServices;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellPrinter
//
//  The emulated ImageWriter as the emulator presents it: the worker that
//  drains the printer card's ring into a raster, the print preview window,
//  the toolbar's status light, and the system print dialog. The card itself
//  is the machine's.
//
////////////////////////////////////////////////////////////////////////////////

class ShellPrinter
{
public:
    explicit ShellPrinter (EmulatorShell & shell);
    ~ShellPrinter();

    PrinterWorker     & GetWorker      ()          { return m_printerWorker; }
    PrinterStatusLed  & GetLed         ()          { return m_printerLed; }
    IPrintDialog      & GetPrintDialog () noexcept { return m_printDialog; }

    // Points the machine builder at the worker it starts on the new card, and
    // at the activity count the auto-open compares against.
    void  BindBuildServices (MachineBuildServices & services);

    // Open (creating if needed) the printer panel / print preview window, and
    // push it a fresh snapshot of the current strip. `activate` false shows it
    // without stealing focus from the guest (used by the auto-open path).
    void    ShowPrinterPanel     (bool activate = true);

    // Closes the preview window. It holds no machine sinks.
    void    ClosePrinterPanel    ();

    // Force-refresh the printer panel from the drain worker (race-free, without
    // stopping it): the panel snapshots and renders only its visible ~1-page
    // viewport span. Non-destructive: the live interpreter keeps running, so
    // refreshing mid-print can never disturb the job's state or the output.
    void    SnapshotStripToPanel ();

    // Per-frame: the status light, the preview's auto-open and live refresh,
    // and the preview window's own frame.
    void    UpdatePrinterStatus  ();
    void    UpdatePrinterPreview ();
    HRESULT RenderPanelFrame     ();

    // Stops the drain thread before the card it reads is freed, and keeps the
    // pending strip for the machine's next run, or clears a stale one.
    void    StopAndSavePendingStrip ();

    // Delivery outcome -> the printer status LED: failed=true lights the red
    // error state until a success / discard clears it or the guest prints
    // something new. Called from the delivery paths (WindowCommandManager).
    void    NotePrinterDeliveryResult (bool failed);

    // Owner HWND for printer confirmation / notice message boxes: the preview
    // panel when it is open and visible (so the box centers on the dialog the
    // user is acting in), otherwise the main window.
    HWND    GetPrinterDialogOwner () const;

    // One-line printer summary for the Settings > Printing info banner: what
    // printer this machine emulates and how it connects, or that it has none.
    std::wstring  GetPrinterBannerMessage () const;

private:
    EmulatorShell                 & m_shell;

    // The toolbar's printer button carries this status light.
    PrinterStatusLed                m_printerLed;

    // The pure model deriving the printer LED state from the worker's live
    // signals, plus the last state pushed to the toolbar so a transition
    // repaints exactly once.
    PrinterStatusModel              m_printerStatus;
    PrinterStatus                   m_printerStatusShown = PrinterStatus::Idle;

    // Delivery-failure latch feeding the status model's error input (the
    // toolbar LED's red). Set by the delivery paths in WindowCommandManager;
    // cleared by a successful delivery, a discard, or fresh guest print
    // activity (the user has moved on -- red must not mask the new print).
    bool                            m_printerDeliveryError = false;
    uint64_t                        m_printerErrorActivity = 0;

    // Background printer drain (ring -> interpreter -> raster). The shell
    // declares this component after the machine, so the thread is joined
    // before the card it drains is torn down.
    PrinterWorker                   m_printerWorker;

    std::unique_ptr<PrinterPanel>   m_printerPanel;

    // Live-preview bookkeeping (UpdatePrinterPreview). Auto-open fires once when a
    // *new* print begins -- activity resuming after an idle gap -- so it opens even
    // when a prior pending strip is still loaded, yet a mid-print manual close does
    // not fight a re-open (activity never goes idle mid-print). Refresh pacing and
    // change detection live in the panel's viewport (PrinterPanel::RefreshLive).
    bool                            m_printerAutoOpenArmed    = true;
    uint64_t                        m_printerAutoOpenActivity = 0;
    int64_t                         m_printerActiveLastMs     = 0;

    ModernPrintDialog               m_printDialog;
};
