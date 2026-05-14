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
    0xFF555555,   //  5: Grey 1
    0xFF0022CC,   //  6: Medium Blue
    0xFF66AAFF,   //  7: Light Blue
    0xFF885500,   //  8: Brown
    0xFFFF4400,   //  9: Orange
    0xFFAAAAAA,   // 10: Grey 2
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
//  Helper: reads one byte from either aux memory (if useAux and aux is
//  bound) or main memory (videoRam pointer or memory bus).
//
////////////////////////////////////////////////////////////////////////////////

static Byte ReadDhrByte (
    bool         useAux,
    const Byte * auxMem,
    const Byte * videoRam,
    MemoryBus  & bus,
    Word         addr)
{
    if (useAux && auxMem != nullptr)
    {
        return auxMem[addr];
    }

    if (videoRam != nullptr)
    {
        return videoRam[addr];
    }

    return bus.ReadByte (addr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Render
//
//  Apple //e Double Hi-Res 560x192. Each scanline is 80 bytes total —
//  40 from aux RAM and 40 from main RAM, interleaved aux-first per byte
//  position: aux[$2000], main[$2000], aux[$2001], main[$2001], ...
//  Each byte contributes 7 horizontal dots (bit 7 unused). 4 consecutive
//  dots form a 4-bit nibble that indexes the 16-color DHR palette
//  (FR-019, audit M8 closure).
//
////////////////////////////////////////////////////////////////////////////////

void AppleDoubleHiResMode::Render (
    const Byte * videoRam,
    uint32_t   * framebuffer,
    int          fbWidth,
    int          fbHeight)
{
    static constexpr int kDhrPixelsPerScanline = 560;
    static constexpr int kDhrScanlines         = 192;
    static constexpr int kBytesPerScanline     = 40;
    static constexpr int kBitsPerByte          = 7;

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

            auxByte  = ReadDhrByte (true,  m_auxMem, videoRam, m_bus, addr);
            mainByte = ReadDhrByte (false, m_auxMem, videoRam, m_bus, addr);

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

        // Pass 2: group 4 consecutive dots into a nibble that indexes
        // the 16-color palette. Each color cell is 4 dots wide; we
        // replicate the same color across all 4 dots in the cell so
        // the framebuffer renders true 16-color DHR (560 horizontal
        // dots, 140 color cells).
        fbY = scanline * 2;

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