#include "Pch.h"

#include "CppUnitTest.h"
using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace UiTests
{





////////////////////////////////////////////////////////////////////////////////
//
//  DpiScalerTests
//
//  DIP-to-pixel conversion at the DPIs Windows actually uses.
//
//  Tested at the real scale factors -- 100, 125, 150, 200 percent -- rather
//  than at convenient round numbers, because 125% is the one that produces
//  fractional results and where a truncation-versus-rounding choice becomes
//  visible as controls a pixel out of alignment.
//
//  A ZERO DPI is covered because callers legitimately have one: chrome is
//  positioned before the window's DPI is known, and the scaler must fall back
//  to 96 rather than collapse everything to nothing.
//
//  The float and integer conversions are both asserted, since layouts use
//  whichever suits and a disagreement between them puts a control's rect and
//  its painted content half a pixel apart.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DpiScalerTests)
{
public:

    TEST_METHOD (DefaultIsBaseDpi)
    {
        DxuiDpiScaler  scaler;

        Assert::AreEqual ((UINT) 96, scaler.Dpi());
        Assert::AreEqual (16,        scaler.Px (16));
        Assert::AreEqual (10.0f,     scaler.Pxf (10.0f));
    }


    TEST_METHOD (SetDpi_100Percent_IsIdentity)
    {
        DxuiDpiScaler  scaler;

        scaler.SetDpi (96);
        Assert::AreEqual (16,    scaler.Px (16));
        Assert::AreEqual (24,    scaler.Px (24));
        Assert::AreEqual (13.0f, scaler.Pxf (13.0f));
    }


    TEST_METHOD (SetDpi_150Percent_ScalesUp)
    {
        DxuiDpiScaler  scaler;

        scaler.SetDpi (144);
        Assert::AreEqual (24,                  scaler.Px (16));
        Assert::AreEqual (216,                 scaler.Px (144));
        Assert::AreEqual (19.5f,               scaler.Pxf (13.0f));
    }


    TEST_METHOD (SetDpi_200Percent_DoublesValues)
    {
        DxuiDpiScaler  scaler;

        scaler.SetDpi (192);
        Assert::AreEqual (32,    scaler.Px (16));
        Assert::AreEqual (26.0f, scaler.Pxf (13.0f));
    }


    TEST_METHOD (SetDpi_Zero_FallsBackToBaseDpi)
    {
        DxuiDpiScaler  scaler;

        scaler.SetDpi (240);
        scaler.SetDpi (0);
        Assert::AreEqual ((UINT) 96, scaler.Dpi());
        Assert::AreEqual (16,        scaler.Px (16));
    }
};

}   // namespace UiTests
