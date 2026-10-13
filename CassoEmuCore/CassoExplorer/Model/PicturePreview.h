#pragma once

#include "Pch.h"


class MemoryBus;





////////////////////////////////////////////////////////////////////////////////
//
//  PicturePreview
//
//  A binary that is the size and address of a graphics buffer, drawn the way
//  the machine would show it.
//
//  THE RULE IS THE SPEC'S AND NOTHING SMARTER: hi-res at $2000 or $4000 with
//  8192 or $1FF8 bytes, double hi-res at $2000 with 16384 bytes, lo-res at
//  $400 or $800 with 1024 bytes. A double hi-res file named .A2FM is the
//  monochrome form, its bits drawn one pixel each in white on black, as the
//  A2FC/A2FM naming has it, and one named .A2FC is color. One named neither
//  way is color when its dots repeat every four far more than at nearby
//  spacings, as color's four-dot cells make them, and monochrome otherwise.
//  A file that misses by a byte is a hex dump,
//  which is the honest answer for something nobody can prove is a picture.
//
//  The video modes render from a caller-supplied span and touch their bus
//  only when the span is null, so the bus handed in is never read; a test
//  proves that with a bus that fails on any read.
//
////////////////////////////////////////////////////////////////////////////////

class PicturePreview
{
public:
    enum class Mode { None, HiRes, DoubleHiRes, DoubleHiResMono, LoRes };

    //  The monitor a picture is drawn for, in the order of Casso's own
    //  monitor list, then a custom one in a color the user picks. A
    //  monochrome one draws hi-res and double hi-res as their bits, as Casso
    //  does, then tints the whole picture its color.
    enum class Monitor { Color, Green, Amber, White, Custom };
    static constexpr int  kMonitorCount = 5;

    //  The custom monitor's color until the user picks one.
    static constexpr uint32_t  kDefaultCustomArgb = 0xFF66CCFF;

    //  The Casso monitor whose effects a monitor starts from: the custom one
    //  starts from white's, the neutral phosphor.
    static constexpr size_t  GetPresetIndex (Monitor monitor) { return (size_t) ((monitor == Monitor::Custom) ? Monitor::White : monitor); }

    static Mode  Choose (Word address, size_t length, std::string_view name = {}, std::span<const Byte> bytes = {});

    //  How strongly a double hi-res file's rows repeat every four dots, as a
    //  multiple of how they repeat at nearby spacings: color art scores well
    //  above kColorPeriodicity, monochrome art well below it.
    static double  MeasurePeriodFour (std::span<const Byte> bytes);

    //  Measured on 34 pictures: color 10 and up, monochrome 4 and under.
    static constexpr double  kColorPeriodicity = 6.0;

    static HRESULT  Render (Mode                     mode,
                            std::span<const Byte>    bytes,
                            Word                     address,
                            MemoryBus              & bus,
                            std::vector<uint32_t>  & outBgra,
                            int                    & outWidth,
                            int                    & outHeight,
                            Monitor                  monitor    = Monitor::Color,
                            uint32_t                 customArgb = kDefaultCustomArgb);

    //  Each pixel in the monochrome monitor's color by its brightness; the
    //  color monitor makes no change. `customArgb` is the custom monitor's.
    static void  Tint (std::vector<uint32_t> & bgra, Monitor monitor, uint32_t customArgb = kDefaultCustomArgb);

    static constexpr int  kWidth  = 560;
    static constexpr int  kHeight = 384;

    static constexpr Word    kHiResPage1        = 0x2000;
    static constexpr Word    kHiResPage2        = 0x4000;
    static constexpr Word    kLoResPage1        = 0x0400;
    static constexpr Word    kLoResPage2        = 0x0800;
    static constexpr size_t  kHiResLength       = 8192;
    static constexpr size_t  kHiResShort        = 0x1FF8;
    static constexpr size_t  kDoubleHiResLength = 16384;
    static constexpr size_t  kLoResLength       = 1024;

private:
    static constexpr size_t  kAddressSpace = 0x10000;

    static bool  IsMonochromeName (std::string_view name);
    static bool  IsColorName      (std::string_view name);
    static bool  HasSuffix        (std::string_view name, std::string_view suffix);
    static void  RenderMonochrome (std::span<const Byte> bytes, Word address, std::vector<uint32_t> & outBgra);
};
