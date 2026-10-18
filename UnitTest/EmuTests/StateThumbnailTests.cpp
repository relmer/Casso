#include "Pch.h"

#include "Devices/Printer/PngCodec.h"
#include "Devices/Printer/RgbaImage.h"
#include "Shell/StateThumbnail.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  StateThumbnailTests
//
//  A machine state's thumbnail is the emulator's 560 by 384 picture halved
//  to 280 by 192, opaque, as PNG.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (StateThumbnailTests)
{
public:

    TEST_METHOD_INITIALIZE (InitCom)
    {
        HRESULT  hr = CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED);



        m_ownsCom = (hr == S_OK || hr == S_FALSE);
    }


    TEST_METHOD_CLEANUP (UninitCom)
    {
        if (m_ownsCom)
        {
            CoUninitialize();
        }
    }


    TEST_METHOD (ThePictureIsHalvedAndEncodedOpaque)
    {
        constexpr int          kFrameWidth  = 560;
        constexpr int          kFrameHeight = 384;
        constexpr uint32_t     kGreen       = 0x0033CC66;    // alpha zero, as a framebuffer may have it
        constexpr Byte         kRed         = 0x33;
        constexpr Byte         kGreenByte   = 0xCC;
        constexpr Byte         kBlue        = 0x66;
        constexpr Byte         kOpaque      = 0xFF;
        std::vector<uint32_t>  frame ((size_t) kFrameWidth * kFrameHeight, kGreen);
        std::vector<Byte>      png;
        RgbaImage              decoded;
        HRESULT                hr           = S_OK;



        hr = StateThumbnail::Encode (frame, kFrameWidth, kFrameHeight, png);
        AssertSucceeded (hr, L"Encode");

        hr = PngCodec::DecodeRgba (png, decoded);
        AssertSucceeded (hr, L"the thumbnail is a PNG");

        Assert::AreEqual (StateThumbnail::kWidth,  decoded.width,  L"width");
        Assert::AreEqual (StateThumbnail::kHeight, decoded.height, L"height");
        Assert::AreEqual (kRed,       decoded.rgba[0], L"red");
        Assert::AreEqual (kGreenByte, decoded.rgba[1], L"green");
        Assert::AreEqual (kBlue,      decoded.rgba[2], L"blue");
        Assert::AreEqual (kOpaque,    decoded.rgba[3], L"opaque");
    }


private:

    bool  m_ownsCom = false;
};





