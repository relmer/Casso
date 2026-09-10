#include "Pch.h"

#include "GoldenImage.h"

#include "Devices/Printer/PngCodec.h"
#include "Devices/Printer/RgbaImage.h"
#include "FixtureProvider.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AssertMatches
//
////////////////////////////////////////////////////////////////////////////////

void GoldenImage::AssertMatches (const std::vector<uint32_t> &  bgra,
                                 int                            width,
                                 int                            height,
                                 const wchar_t *                goldenName)
{
    std::filesystem::path  goldenPath = GoldenPath (goldenName);
    std::filesystem::path  actualPath = ActualPath (goldenName);
    std::vector<Byte>      png;
    RgbaImage              golden;
    HRESULT                hr         = S_OK;
    size_t                 differing  = 0;
    int                    firstX     = -1;
    int                    firstY     = -1;
    uint32_t               firstWant  = 0;
    uint32_t               firstGot   = 0;



    Assert::AreEqual ((size_t) width * height, bgra.size(), L"the picture must be width * height words");

    if (!std::filesystem::exists (goldenPath))
    {
        hr = WritePng (actualPath, bgra, width, height);
        Assert::Fail (std::format (L"no golden at {}; the picture produced was written to {} "
                                   L"for a person to look at and, if it is right, copy into place (write: {:08X})",
                                   goldenPath.wstring(), actualPath.wstring(), (unsigned) hr).c_str());
    }

    {
        std::ifstream  in (goldenPath, std::ios::binary);

        png.assign (std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>());
    }

    hr = PngCodec::DecodeRgba (png, golden);
    Assert::IsTrue (SUCCEEDED (hr), std::format (L"the golden at {} must decode", goldenPath.wstring()).c_str());

    if (golden.width != width || golden.height != height)
    {
        WritePng (actualPath, bgra, width, height);
        Assert::Fail (std::format (L"golden {} is {}x{} but the picture is {}x{}; the picture was written to {}",
                                   goldenName, golden.width, golden.height, width, height,
                                   actualPath.wstring()).c_str());
    }

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            const Byte *  g    = golden.rgba.data() + ((size_t) y * width + x) * 4;
            uint32_t      want = ((uint32_t) g[3] << 24) | ((uint32_t) g[0] << 16)
                               | ((uint32_t) g[1] <<  8) |  (uint32_t) g[2];
            uint32_t      got  = bgra[(size_t) y * width + x];

            if (want != got)
            {
                if (differing == 0)
                {
                    firstX    = x;
                    firstY    = y;
                    firstWant = want;
                    firstGot  = got;
                }

                differing++;
            }
        }
    }

    if (differing != 0)
    {
        WritePng (actualPath, bgra, width, height);
        Assert::Fail (std::format (L"{} pixel(s) differ from golden {}; first at ({},{}): golden {:08X}, "
                                   L"picture {:08X}. The picture was written to {}",
                                   differing, goldenName, firstX, firstY, firstWant, firstGot,
                                   actualPath.wstring()).c_str());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GoldenPath / ActualPath
//
//  The fixtures root is the one every other fixture resolves against, so a
//  golden is found the same way a ROM or a disk image is.
//
////////////////////////////////////////////////////////////////////////////////

std::filesystem::path GoldenImage::GoldenPath (const wchar_t * goldenName)
{
    FixtureProvider  fixtures;



    return std::filesystem::path (fixtures.GetRoot()) / L"golden" / (std::wstring (goldenName) + L".png");
}


std::filesystem::path GoldenImage::ActualPath (const wchar_t * goldenName)
{
    return std::filesystem::temp_directory_path() / L"Casso-golden" / (std::wstring (goldenName) + L".actual.png");
}





////////////////////////////////////////////////////////////////////////////////
//
//  WritePng
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GoldenImage::WritePng (const std::filesystem::path &  path,
                               const std::vector<uint32_t> &  bgra,
                               int                            width,
                               int                            height)
{
    HRESULT            hr = S_OK;
    RgbaImage          rgba;
    std::vector<Byte>  png;
    std::error_code    ec;



    rgba.width  = width;
    rgba.height = height;
    rgba.rgba.resize ((size_t) width * height * 4);

    for (size_t i = 0; i < bgra.size(); i++)
    {
        uint32_t  p = bgra[i];

        rgba.rgba[i * 4 + 0] = (Byte) ((p >> 16) & 0xFF);
        rgba.rgba[i * 4 + 1] = (Byte) ((p >>  8) & 0xFF);
        rgba.rgba[i * 4 + 2] = (Byte) ( p        & 0xFF);
        rgba.rgba[i * 4 + 3] = (Byte) ((p >> 24) & 0xFF);
    }

    hr = PngCodec::EncodeRgba (rgba, 0, png);
    CHR (hr);

    std::filesystem::create_directories (path.parent_path(), ec);

    {
        std::ofstream  out (path, std::ios::binary | std::ios::trunc);
        bool           opened = out.is_open();

        CBREx (opened, HRESULT_FROM_WIN32 (ERROR_CANNOT_MAKE));
        out.write (reinterpret_cast<const char *> (png.data()), (std::streamsize) png.size());
    }

Error:
    return hr;
}
