#include "Pch.h"

#include "Devices/Printer/ImageWriterInterpreter.h"
#include "Devices/Printer/PrintRaster.h"




// Control codes (exact -- ASCII).
static constexpr Byte   s_kCR  = 0x0D;
static constexpr Byte   s_kLF  = 0x0A;
static constexpr Byte   s_kFF  = 0x0C;
static constexpr Byte   s_kEsc = 0x1B;

static constexpr Byte   s_kPrintableLo = 0x20;
static constexpr Byte   s_kPrintableHi = 0x7E;

// ESC command bytes. PROVISIONAL (pending T011 capture) except where noted;
// isolated here so the grammar is a one-line change once the capture lands.
static constexpr Byte   s_kCmdGraphics   = 'G';   // bit-image, 4 ASCII-digit count
static constexpr Byte   s_kCmdLineSpace  = 'T';   // n/144" line feed, 2 ASCII digits
static constexpr Byte   s_kCmd6Lpi       = 'A';   // 1/6" line feed
static constexpr Byte   s_kCmd8Lpi       = 'B';   // 1/8" line feed
static constexpr Byte   s_kCmdColor      = 'K';   // seven-colour select (acted on in US2)
static constexpr Byte   s_kCmdReset      = 'c';   // software reset

// Recognized-but-inert preamble Print Shop sends before every graphics pass.
// ESC '>' / ESC '<' select uni/bidirectional print direction (moot for our
// deterministic column placement) and ESC 'P' selects the graphics pitch that
// already matches the 160-dpi native grid. Consumed so they don't surface as
// UnknownCommand -- confirmed against the Print Shop Color capture, which
// renders correctly treating them as no-ops.
static constexpr Byte   s_kCmdPrintDirFwd = '>';
static constexpr Byte   s_kCmdPrintDirRev = '<';
static constexpr Byte   s_kCmdGfxPitch    = 'P';

static constexpr int    s_kGraphicsDigits   = 4;
static constexpr int    s_kLineSpaceDigits   = 2;
static constexpr int    s_kColorParams       = 1;

// Defaults (data-model). 6 lpi over the 144 rows/inch native grid.
static constexpr int    s_kDefaultLineFeedRows  = PrinterGrid::kRowsPerInch / 6;   // 24
static constexpr int    s_kDefaultPitchDots      = PrinterGrid::kDotsPerInchH / 10;  // ~10 cpi (provisional)
static constexpr int    s_kGraphicsPins          = 8;
static constexpr int    s_kGraphicsRowsPerPin    = PrinterGrid::kRowsPerInch / 72;  // 2: pins sit 1/72" apart on the 144-row grid




static int DecodeAsciiDigits (const Byte * digits, int count)
{
    int   value = 0;
    int   i     = 0;

    for (i = 0; i < count; i++)
    {
        Byte   d = digits[i];

        if (d >= '0' && d <= '9')
        {
            value = value * 10 + (d - '0');
        }
        else
        {
            value = value * 10;   // tolerate stray bytes as zero
        }
    }

    return value;
}




static int PitchForCommand (Byte cmd)
{
    // PROVISIONAL pitch selections (unused until the US6 draft font renders
    // text; stored only so the commands are consumed, not flagged unknown).
    switch (cmd)
    {
    case 'n':   return PrinterGrid::kDotsPerInchH / 9;
    case 'N':   return PrinterGrid::kDotsPerInchH / 10;
    case 'E':   return PrinterGrid::kDotsPerInchH / 12;
    case 'e':   return PrinterGrid::kDotsPerInchH / 13;
    case 'q':   return PrinterGrid::kDotsPerInchH / 15;
    case 'Q':   return PrinterGrid::kDotsPerInchH / 17;
    default:    return 0;
    }
}




static bool IsPitchCommand (Byte cmd)
{
    return cmd == 'n' || cmd == 'N' || cmd == 'E' || cmd == 'e' || cmd == 'q' || cmd == 'Q';
}




// ESC K colour select: one ASCII digit 0..6 per the ImageWriter II ribbon
// table (0 black, 1 yellow, 2 red, 3 blue, 4 orange, 5 green, 6 purple). The
// composites are the OR of their primaries, matching how two overprinted
// passes accumulate in a cell -- Print Shop Color drives only the 1/2/3
// primaries and lets overlap form the composites itself.
static InkPrimary ColorForCode (Byte digit)
{
    Byte   yellow = (Byte) InkPrimary::Yellow;
    Byte   red    = (Byte) InkPrimary::Red;
    Byte   blue   = (Byte) InkPrimary::Blue;

    switch (digit)
    {
    case '0':   return InkPrimary::Black;
    case '1':   return InkPrimary::Yellow;
    case '2':   return InkPrimary::Red;
    case '3':   return InkPrimary::Blue;
    case '4':   return (InkPrimary) (yellow | red);    // orange
    case '5':   return (InkPrimary) (yellow | blue);   // green
    case '6':   return (InkPrimary) (red | blue);      // purple
    default:    return InkPrimary::Black;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void ImageWriterInterpreter::Reset ()
{
    m_headColumnDots  = 0;
    m_lineFeedRows     = s_kDefaultLineFeedRows;
    m_pitchDotsPerChar = s_kDefaultPitchDots;
    m_color            = InkPrimary::Black;

    m_state        = EscState::Idle;
    m_cmd          = 0;
    m_paramsNeeded = 0;
    m_paramsGot    = 0;

    m_gfxRemaining = 0;
    m_burstFromDot = -1;
    m_burstToDot   = -1;
    m_burstRow     = 0;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Consume
//
////////////////////////////////////////////////////////////////////////////////

void ImageWriterInterpreter::Consume (
    const Byte *            data,
    size_t                  count,
    PrintRaster &           raster,
    vector<PrinterEvent> &  outEvents)
{
    size_t   i = 0;

    for (i = 0; i < count; i++)
    {
        Byte   b = data[i];

        switch (m_state)
        {
        case EscState::Idle:
            ConsumeIdle (b, raster, outEvents);
            break;

        case EscState::Esc:
            ConsumeEsc (b, raster, outEvents);
            break;

        case EscState::Param:
            m_params[m_paramsGot++] = b;
            if (m_paramsGot >= m_paramsNeeded)
            {
                ExecuteParamCommand (raster, outEvents);
            }
            break;

        case EscState::GraphicsData:
            ConsumeGraphicsByte (b, raster, outEvents);
            break;
        }
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  ConsumeIdle
//
//  Ground state: control codes act; ESC opens a command; printable ASCII is
//  consumed but not rendered (the draft font arrives with US6); other control
//  bytes are ignored.
//
////////////////////////////////////////////////////////////////////////////////

void ImageWriterInterpreter::ConsumeIdle (Byte b, PrintRaster & raster, vector<PrinterEvent> & events)
{
    if (b == s_kCR)
    {
        m_headColumnDots = 0;
    }
    else if (b == s_kLF)
    {
        PrinterEvent   ev;

        raster.AdvanceRows (m_lineFeedRows);
        ev.type = PrinterEventType::LineFeed;
        ev.rows = m_lineFeedRows;
        events.push_back (ev);
    }
    else if (b == s_kFF)
    {
        PrinterEvent   ev;

        raster.MarkFormFeed ();
        m_headColumnDots = 0;
        ev.type = PrinterEventType::FormFeed;
        events.push_back (ev);
    }
    else if (b == s_kEsc)
    {
        m_state = EscState::Esc;
    }
    else if (b >= s_kPrintableLo && b <= s_kPrintableHi)
    {
        // Consumed-not-rendered until the US6 draft font.
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  ConsumeEsc
//
//  Dispatches the byte after ESC. Parametered commands arm the Param collector;
//  zero-parameter commands act immediately; anything unrecognised is consumed
//  and reported (FR-009).
//
////////////////////////////////////////////////////////////////////////////////

void ImageWriterInterpreter::ConsumeEsc (Byte b, PrintRaster & raster, vector<PrinterEvent> & events)
{
    UNREFERENCED_PARAMETER (raster);

    m_cmd       = b;
    m_paramsGot = 0;

    if (b == s_kCmdGraphics)
    {
        m_paramsNeeded = s_kGraphicsDigits;
        m_state        = EscState::Param;
    }
    else if (b == s_kCmdLineSpace)
    {
        m_paramsNeeded = s_kLineSpaceDigits;
        m_state        = EscState::Param;
    }
    else if (b == s_kCmdColor)
    {
        m_paramsNeeded = s_kColorParams;
        m_state        = EscState::Param;
    }
    else if (b == s_kCmd6Lpi)
    {
        m_lineFeedRows = PrinterGrid::kRowsPerInch / 6;
        m_state        = EscState::Idle;
    }
    else if (b == s_kCmd8Lpi)
    {
        m_lineFeedRows = PrinterGrid::kRowsPerInch / 8;
        m_state        = EscState::Idle;
    }
    else if (b == s_kCmdReset)
    {
        EmitReset (events);
        m_state = EscState::Idle;
    }
    else if (IsPitchCommand (b))
    {
        m_pitchDotsPerChar = PitchForCommand (b);
        m_state            = EscState::Idle;
    }
    else if (b == s_kCmdPrintDirFwd || b == s_kCmdPrintDirRev || b == s_kCmdGfxPitch)
    {
        m_state = EscState::Idle;   // recognized no-op preamble (see command table)
    }
    else
    {
        PrinterEvent   ev;

        ev.type     = PrinterEventType::UnknownCommand;
        ev.leadByte = b;
        events.push_back (ev);
        m_state = EscState::Idle;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  ExecuteParamCommand
//
////////////////////////////////////////////////////////////////////////////////

void ImageWriterInterpreter::ExecuteParamCommand (PrintRaster & raster, vector<PrinterEvent> & events)
{
    UNREFERENCED_PARAMETER (raster);

    if (m_cmd == s_kCmdGraphics)
    {
        int   dataCount = DecodeAsciiDigits (m_params, s_kGraphicsDigits);

        m_gfxRemaining = dataCount;
        m_burstFromDot = -1;
        m_burstToDot   = -1;
        m_state        = dataCount > 0 ? EscState::GraphicsData : EscState::Idle;
    }
    else if (m_cmd == s_kCmdLineSpace)
    {
        int   n = DecodeAsciiDigits (m_params, s_kLineSpaceDigits);

        if (n > 0)
        {
            m_lineFeedRows = n;   // n/144" -> n native rows
        }
        m_state = EscState::Idle;
    }
    else if (m_cmd == s_kCmdColor)
    {
        PrinterEvent   ev;

        m_color  = ColorForCode (m_params[0]);
        ev.type  = PrinterEventType::ColorChange;
        ev.color = m_color;
        events.push_back (ev);
        m_state  = EscState::Idle;
    }
    else
    {
        m_state = EscState::Idle;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  ConsumeGraphicsByte
//
//  One bit-image column: eight vertical dots at the current head column.
//  Bit order confirmed against a real Print Shop Color capture: the LSB (bit 0)
//  is the TOP pin. The ImageWriter II's pins are spaced 1/72" apart, i.e. two
//  rows on the 144 rows/inch native grid, so each pin fills a 2-row-tall dot
//  (an 8-pin column spans 16 rows). Print Shop feeds ~14 rows between passes,
//  slightly overlapping the bands so there are no white gaps between lines; the
//  older 1-row-per-pin model left a 6-row blank stripe every line. A HeadBurst
//  summarising the run is emitted when it ends.
//
////////////////////////////////////////////////////////////////////////////////

void ImageWriterInterpreter::ConsumeGraphicsByte (Byte b, PrintRaster & raster, vector<PrinterEvent> & events)
{
    int   row = raster.PaperRow ();
    int   bit = 0;

    for (bit = 0; bit < s_kGraphicsPins; bit++)
    {
        if ((b & (0x01 << bit)) != 0)
        {
            int   top = row + bit * s_kGraphicsRowsPerPin;

            for (int r = 0; r < s_kGraphicsRowsPerPin; r++)
            {
                raster.Strike (m_headColumnDots, top + r, m_color);
            }
        }
    }

    if (m_burstFromDot < 0)
    {
        m_burstFromDot = m_headColumnDots;
        m_burstRow     = row;
    }
    m_burstToDot = m_headColumnDots;

    m_headColumnDots++;
    m_gfxRemaining--;

    if (m_gfxRemaining <= 0)
    {
        PrinterEvent   ev;

        ev.type    = PrinterEventType::HeadBurst;
        ev.fromDot = m_burstFromDot;
        ev.toDot   = m_burstToDot;
        ev.row     = m_burstRow;
        events.push_back (ev);
        m_state = EscState::Idle;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  EmitReset
//
//  Software reset returns the interpreter to power-on defaults (FR-010). The
//  paper strip is deliberately untouched -- reset is not an eject.
//
////////////////////////////////////////////////////////////////////////////////

void ImageWriterInterpreter::EmitReset (vector<PrinterEvent> & events)
{
    PrinterEvent   ev;

    Reset ();
    ev.type = PrinterEventType::ResetSeen;
    events.push_back (ev);
}
