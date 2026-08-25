#include "Pch.h"

#include "AppleDoubleHiResMode.h"
#include "AppleHiResMode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple //e Double Hi-Res 16-Color Palette (RGBA)
//
//  Sather UTAIIe Tab 8.5 DHR palette. 4-bit nibble (LSB first
//  per scanline bit order) selects one of 16 colors. Mirrors the lo-res
//  palette ordering so DHR with all-aux=0 main=0 produces black, and
//  all-on aux=$7F main=$7F produces white.
//
////////////////////////////////////////////////////////////////////////////////

// Apple //e DHGR 16-color palette in B8G8R8A8 byte layout (matches the
// DXGI_FORMAT_B8G8R8A8_UNORM swap chain). Same convention as
// AppleLoResMode::kLoResColors — see Video/PixelFormat.h for the
// project-wide byte-order convention.
static const uint32_t kDhrColors[16] =
{
    0xFF000000,   //  0: Black
    0xFFDD2266,   //  1: Magenta
    0xFF000099,   //  2: Dark Blue
    0xFFDD0044,   //  3: Purple
    0xFF002200,   //  4: Dark Green
    0xFF555555,   //  5: Gray 1
    0xFF0022CC,   //  6: Medium Blue
    0xFF66AAFF,   //  7: Light Blue
    0xFF885500,   //  8: Brown
    0xFFFF4400,   //  9: Orange
    0xFFAAAAAA,   // 10: Gray 2
    0xFFFF8888,   // 11: Pink
    0xFF00DD00,   // 12: Light Green
    0xFFFFFF00,   // 13: Yellow
    0xFF44FFDD,   // 14: Aqua
    0xFFFFFFFF,   // 15: White
};





////////////////////////////////////////////////////////////////////////////////
//
//  AppleDoubleHiResMode
//
////////////////////////////////////////////////////////////////////////////////

AppleDoubleHiResMode::AppleDoubleHiResMode (MemoryBus & bus)
    : m_bus (bus)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetActivePageAddress
//
////////////////////////////////////////////////////////////////////////////////

Word AppleDoubleHiResMode::GetActivePageAddress (bool page2) const
{
    return page2 ? static_cast<Word> (0x4000) : static_cast<Word> (0x2000);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadDhrByte
//
//  Helper: reads one byte of one half of a DHR byte pair.
//
//  `direct` is the bank this half belongs to -- the aux buffer for the aux
//  half, the main buffer for the main half. Both are wired straight to the
//  RAM the //e MMU owns, and the bus is only a fallback for a renderer with
//  no direct pointers.
//
//  Going straight to the banks is a CORRECTNESS requirement, not a shortcut.
//  DHR needs main and aux at the same instant, but the bus page table for
//  $2000-$3FFF follows live MMU banking: with 80STORE and HIRES set, PAGE2
//  alone points that whole range at aux (Apple2eMmu::ResolveHires20_3F), and
//  with 80STORE off it follows RAMRD. Reading the main half through the bus
//  therefore returned the AUX byte for any program that left the switches
//  pointing at aux while the frame was scanned, rendering aux into both
//  halves of every 14-dot group.
//
////////////////////////////////////////////////////////////////////////////////

static Byte ReadDhrByte (
    const Byte * direct,
    const Byte * videoRam,
    MemoryBus  & bus,
    Word         addr)
{
    Byte  value = 0;



    if      (direct != nullptr)   { value = direct[addr];       }
    else if (videoRam != nullptr) { value = videoRam[addr];     }
    else                          { value = bus.ReadByte (addr); }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Render
//
//  Apple //e Double Hi-Res 560x192. Each scanline is 80 bytes total —
//  40 from aux RAM and 40 from main RAM, interleaved aux-first per byte
//  position: aux[$2000], main[$2000], aux[$2001], main[$2001], ...
//  Each byte contributes 7 horizontal dots (bit 7 unused).
//
//  Pass 1 unpacks a scanline into its 560 dots. Pass 2 turns those dots
//  into pixels, and which pass 2 runs depends on the monitor:
//
//    color        4 consecutive dots form a nibble indexing the 16-color
//                 DHR palette, replicated across the cell's 4 dots
//                 (140 color cells across).
//    monochrome   each dot is its own pixel, lit or dark (560 across).
//
//  The color decode is lossy by nature -- it throws away which of the four
//  dots in a cell were lit and keeps only how many and in what arrangement.
//  That is correct for a color monitor, where NTSC does the same thing, but
//  it destroys art authored as 560x192 monochrome. So the monochrome path is
//  not a tint of the color path; it has to decode from the dots.
//
////////////////////////////////////////////////////////////////////////////////

void AppleDoubleHiResMode::Render (
    const Byte * videoRam,
    uint32_t   * framebuffer,
    int          fbWidth,
    int          fbHeight)
{
    static constexpr int      kDhrPixelsPerScanline = 560;
    static constexpr int      kDhrScanlines         = 192;
    static constexpr int      kBytesPerScanline     = 40;
    static constexpr int      kBitsPerByte          = 7;
    static constexpr uint32_t kMonoOn               = 0xFFFFFFFF;
    static constexpr uint32_t kMonoOff              = 0xFF000000;



    Word     pageBase                    = GetActivePageAddress (m_page2);
    bool     dots[kDhrPixelsPerScanline] = {};
    Byte     auxByte                     = 0;
    Byte     mainByte                    = 0;
    int      x                           = 0;
    int      paletteIdx                  = 0;
    uint32_t color                       = 0;
    int      fbX                         = 0;
    int      fbY                         = 0;



    for (int scanline = 0; scanline < kDhrScanlines; scanline++)
    {
        Word lineAddr = AppleHiResMode::ScanlineAddress (scanline, pageBase);

        // Pass 1: unpack 80 bytes (aux+main) into 560 dots in display order.
        for (int byteIdx = 0; byteIdx < kBytesPerScanline; byteIdx++)
        {
            Word addr = static_cast<Word> (lineAddr + byteIdx);

            auxByte  = ReadDhrByte (m_auxMem,  videoRam, m_bus, addr);
            mainByte = ReadDhrByte (m_mainMem, videoRam, m_bus, addr);

            for (int bit = 0; bit < kBitsPerByte; bit++)
            {
                x        = byteIdx * 14 + bit;
                dots[x]  = (auxByte & (1 << bit)) != 0;
            }

            for (int bit = 0; bit < kBitsPerByte; bit++)
            {
                x        = byteIdx * 14 + kBitsPerByte + bit;
                dots[x]  = (mainByte & (1 << bit)) != 0;
            }
        }

        // Scanlines are doubled vertically, so 192 emulated lines fill the
        // 384-line framebuffer and DHR matches the other modes' geometry.
        fbY = scanline * 2;

        if (m_monochrome)
        {
            // Pass 2, monochrome: one dot, one pixel. Lit dots are white so
            // the shell's phosphor tint reaches full brightness on green and
            // amber monitors as well as white.
            for (fbX = 0; fbX < kDhrPixelsPerScanline; fbX++)
            {
                color = dots[fbX] ? kMonoOn : kMonoOff;

                if (fbX < fbWidth && fbY + 1 < fbHeight)
                {
                    framebuffer[fbY       * fbWidth + fbX] = color;
                    framebuffer[(fbY + 1) * fbWidth + fbX] = color;
                }
            }

            continue;
        }

        // Pass 2, color: group 4 consecutive dots into a nibble that indexes
        // the 16-color palette. Each color cell is 4 dots wide; we
        // replicate the same color across all 4 dots in the cell so
        // the framebuffer renders true 16-color DHR (560 horizontal
        // dots, 140 color cells).
        for (int cell = 0; cell + 3 < kDhrPixelsPerScanline; cell += 4)
        {
            paletteIdx = (dots[cell + 0] ? 1 : 0)
                       | (dots[cell + 1] ? 2 : 0)
                       | (dots[cell + 2] ? 4 : 0)
                       | (dots[cell + 3] ? 8 : 0);

            color = kDhrColors[paletteIdx];

            for (int dotInCell = 0; dotInCell < 4; dotInCell++)
            {
                fbX = cell + dotInCell;

                if (fbX < fbWidth && fbY + 1 < fbHeight)
                {
                    framebuffer[fbY       * fbWidth + fbX] = color;
                    framebuffer[(fbY + 1) * fbWidth + fbX] = color;
                }
            }
        }
    }
}