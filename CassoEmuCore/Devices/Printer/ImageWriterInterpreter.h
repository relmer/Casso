#pragma once

#include "Pch.h"

#include "Devices/Printer/PrinterTypes.h"

class PrintRaster;




////////////////////////////////////////////////////////////////////////////////
//
//  ImageWriterInterpreter
//
//  Pure byte-stream-in, raster-strikes-out interpreter for the ImageWriter II
//  command subset (FR-005, research R-003). No system dependencies (FR-017):
//  bytes come in via Consume, cells are struck into the caller's PrintRaster,
//  and an ordered PrinterEvent stream is appended for the presenter.
//
//  Parser: a small state machine (Idle / Esc / Param / GraphicsData). Control
//  codes CR/LF/FF and the ESC dispatch framework are exact; the specific
//  ESC-command byte assignments and bit-image geometry (marked PROVISIONAL
//  below) are pinned by the ImageWriter II Technical Reference and confirmed
//  against captured Print Shop streams (checkpoint T011) -- the same
//  provisional stance as the slot firmware. Unknown commands are consumed and
//  surfaced as UnknownCommand events (FR-009) rather than desynchronising the
//  stream.
//
////////////////////////////////////////////////////////////////////////////////

class ImageWriterInterpreter
{
public:
    ImageWriterInterpreter () { Reset (); }

    void    Reset   ();                                          // FR-010 power-on defaults
    void    Consume (const Byte *            data,
                     size_t                  count,
                     PrintRaster &           raster,
                     vector<PrinterEvent> &  outEvents);

    // The print head's current dot column, for the panel's left-to-right ink
    // reveal (FR-034). Observing it never alters interpretation or the raster.
    int     HeadColumnDots () const { return m_headColumnDots; }

private:
    enum class EscState
    {
        Idle,
        Esc,
        Param,          // collecting m_paramsNeeded raw bytes for m_cmd
        GraphicsData,   // consuming m_gfxRemaining bit-image data bytes
    };

    void    ConsumeIdle         (Byte b, PrintRaster & raster, vector<PrinterEvent> & events);
    void    ConsumeEsc          (Byte b, PrintRaster & raster, vector<PrinterEvent> & events);
    void    ExecuteParamCommand (PrintRaster & raster, vector<PrinterEvent> & events);
    void    ConsumeGraphicsByte (Byte b, PrintRaster & raster, vector<PrinterEvent> & events);
    void    EmitReset           (vector<PrinterEvent> & events);

    // Interpreter state (data-model: resets on printer-reset and machine start).
    int         m_headColumnDots  = 0;
    int         m_lineFeedRows     = 0;
    int         m_pitchDotsPerChar = 0;
    InkPrimary  m_color            = InkPrimary::Black;

    // Parser working state.
    EscState    m_state         = EscState::Idle;
    Byte        m_cmd           = 0;
    int         m_paramsNeeded  = 0;
    int         m_paramsGot     = 0;
    Byte        m_params[4]     = { 0, 0, 0, 0 };

    // Active bit-image run. m_gfxMsbTop selects the ESC L conventions (both
    // from the T011 capture): TOP pin in bit 7 (ESC G uses bit 0) and
    // 120-dpi columns laid from m_gfxStartDot by m_gfxColIndex (ESC G is
    // native 1:1).
    int         m_gfxRemaining  = 0;
    bool        m_gfxMsbTop     = false;
    int         m_gfxColIndex   = 0;
    int         m_gfxStartDot   = 0;
    int         m_burstFromDot  = -1;
    int         m_burstToDot    = -1;
    int         m_burstRow      = 0;
};
