#include "Pch.h"

#include "CppUnitTest.h"

#include "../Casso/Ui/DpiScaler.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace UiTests
{

TEST_CLASS (DpiScalerTests)
{
public:

    TEST_METHOD (DefaultIsBaseDpi)
    {
        DpiScaler  scaler;

        Assert::AreEqual ((UINT) 96, scaler.Dpi());
        Assert::AreEqual (16,        scaler.Px (16));
        Assert::AreEqual (10.0f,     scaler.Pxf (10.0f));
    }


    TEST_METHOD (SetDpi_100Percent_IsIdentity)
    {
        DpiScaler  scaler;

        scaler.SetDpi (96);
        Assert::AreEqual (16,    scaler.Px (16));
        Assert::AreEqual (24,    scaler.Px (24));
        Assert::AreEqual (13.0f, scaler.Pxf (13.0f));
    }


    TEST_METHOD (SetDpi_150Percent_ScalesUp)
    {
        DpiScaler  scaler;

        scaler.SetDpi (144);
        Assert::AreEqual (24,                  scaler.Px (16));
        Assert::AreEqual (216,                 scaler.Px (144));
        Assert::AreEqual (19.5f,               scaler.Pxf (13.0f));
    }


    TEST_METHOD (SetDpi_200Percent_DoublesValues)
    {
        DpiScaler  scaler;

        scaler.SetDpi (192);
        Assert::AreEqual (32,    scaler.Px (16));
        Assert::AreEqual (26.0f, scaler.Pxf (13.0f));
    }


    TEST_METHOD (SetDpi_Zero_FallsBackToBaseDpi)
    {
        DpiScaler  scaler;

        scaler.SetDpi (240);
        scaler.SetDpi (0);
        Assert::AreEqual ((UINT) 96, scaler.Dpi());
        Assert::AreEqual (16,        scaler.Px (16));
    }
};

}   // namespace UiTests
