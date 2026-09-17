#include "Pch.h"

#include "Ui/Chrome/PrinterStatusLed.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ChromeToolbarPartsTests
//
//  The emulator parts the toolbar hosts, driven on their own.
//
//  The input cluster's segment tests went with the cluster: what drives the
//  paddle axes is the command bar's picker now (PaddleSourceRowsTests), and
//  mouse mode is a plain toggle beside it rather than a segment.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ChromeToolbarPartsTests)
{
public:

    TEST_METHOD (PrinterLed_ColorPerStatus_NothingWhileIdle)
    {
        Assert::AreEqual (0u,          PrinterStatusLed::GetStatusCoreColor (PrinterStatus::Idle));
        Assert::AreEqual (0xFF4CE96Au, PrinterStatusLed::GetStatusCoreColor (PrinterStatus::Receiving));
        Assert::AreEqual (0xFFFFB938u, PrinterStatusLed::GetStatusCoreColor (PrinterStatus::Pending));
        Assert::AreEqual (0xFFFF5257u, PrinterStatusLed::GetStatusCoreColor (PrinterStatus::Error));
    }
};
