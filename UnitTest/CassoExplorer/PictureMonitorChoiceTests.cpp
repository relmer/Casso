#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoExplorer/Browser/Preview/PictureMonitorChoice.h"
#include "CassoExplorer/Model/PreviewDecoder.h"
#include "Core/MemoryBus.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PictureMonitorChoiceTests
//
//  The monitor a picture preview is drawn on, and the toggle between a double
//  hi-res picture's color form and its bits.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PictureMonitorChoiceTests)
{
public:

    static void  MakeDoubleHiResPicture (PreviewContent & preview)
    {
        preview.kind          = PreviewContent::Kind::Picture;
        preview.pictureChosen = PicturePreview::Mode::DoubleHiRes;
        preview.pictureMode   = PicturePreview::Mode::DoubleHiRes;
        preview.bytes.assign (PicturePreview::kDoubleHiResLength, 0);
        preview.origin        = PicturePreview::kHiResPage1;
    }


    //  A double hi-res picture's monitor follows its form: color on the color
    //  monitor, its bits on the chosen monochrome one, or the white one when
    //  the color monitor is chosen. Any other picture is on the chosen one.
    TEST_METHOD (DoubleHiResForm_PicksTheMonitor)
    {
        using Mode    = PicturePreview::Mode;
        using Monitor = PicturePreview::Monitor;

        Assert::IsTrue (PictureMonitorChoice::ChooseMonitor (Monitor::Green,  Mode::DoubleHiRes)     == Monitor::Color);
        Assert::IsTrue (PictureMonitorChoice::ChooseMonitor (Monitor::Color,  Mode::DoubleHiRes)     == Monitor::Color);
        Assert::IsTrue (PictureMonitorChoice::ChooseMonitor (Monitor::Amber,  Mode::DoubleHiResMono) == Monitor::Amber);
        Assert::IsTrue (PictureMonitorChoice::ChooseMonitor (Monitor::Custom, Mode::DoubleHiResMono) == Monitor::Custom);
        Assert::IsTrue (PictureMonitorChoice::ChooseMonitor (Monitor::Color,  Mode::DoubleHiResMono) == Monitor::White);
        Assert::IsTrue (PictureMonitorChoice::ChooseMonitor (Monitor::Green,  Mode::HiRes)           == Monitor::Green);
        Assert::IsTrue (PictureMonitorChoice::ChooseMonitor (Monitor::Color,  Mode::LoRes)           == Monitor::Color);
    }


    TEST_METHOD (Toggle_IsRefusedForAPictureThatIsNotDoubleHiRes)
    {
        PreviewContent        preview;
        MemoryBus             bus;
        PictureMonitorChoice  choice (preview, bus);

        preview.kind          = PreviewContent::Kind::Picture;
        preview.pictureChosen = PicturePreview::Mode::HiRes;
        preview.pictureMode   = PicturePreview::Mode::HiRes;

        Assert::IsFalse (choice.CanTogglePictureColor());
        Assert::IsFalse (choice.TogglePictureColor());
        Assert::IsTrue  (preview.pictureMode == PicturePreview::Mode::HiRes, L"The picture is as it was");
    }


    TEST_METHOD (Toggle_IsRefusedForAPreviewThatIsNotAPicture)
    {
        PreviewContent        preview;
        MemoryBus             bus;
        PictureMonitorChoice  choice (preview, bus);

        MakeDoubleHiResPicture (preview);
        preview.kind = PreviewContent::Kind::Text;

        Assert::IsFalse (choice.CanTogglePictureColor());
        Assert::IsFalse (choice.TogglePictureColor());
    }


    TEST_METHOD (Toggle_SwitchesDoubleHiResAndItsBitsBothWays)
    {
        PreviewContent        preview;
        MemoryBus             bus;
        PictureMonitorChoice  choice (preview, bus);

        MakeDoubleHiResPicture (preview);
        Assert::IsTrue (choice.CanTogglePictureColor());
        Assert::IsTrue (choice.GetPictureMonitor() == PicturePreview::Monitor::Color);

        Assert::IsTrue (choice.TogglePictureColor());
        Assert::IsTrue (preview.pictureMode == PicturePreview::Mode::DoubleHiResMono);
        Assert::IsTrue (choice.GetPictureMonitor() == PicturePreview::Monitor::White, L"The bits are drawn on the white monitor when the color one is chosen");
        Assert::IsFalse (preview.bgra.empty(), L"The picture is drawn again");

        Assert::IsTrue (choice.TogglePictureColor());
        Assert::IsTrue (preview.pictureMode == PicturePreview::Mode::DoubleHiRes);
        Assert::IsTrue (choice.GetPictureMonitor() == PicturePreview::Monitor::Color);
    }


    TEST_METHOD (SetPreviewMonitor_ToTheSameMonitor_DrawsNothingAgain)
    {
        PreviewContent        preview;
        MemoryBus             bus;
        PictureMonitorChoice  choice (preview, bus);

        preview.kind        = PreviewContent::Kind::Picture;
        preview.pictureMode = PicturePreview::Mode::HiRes;
        preview.bytes.assign (PicturePreview::kHiResLength, 0);
        preview.origin      = PicturePreview::kHiResPage1;

        choice.SetPreviewMonitor (PicturePreview::Monitor::Color, PicturePreview::kDefaultCustomArgb);
        Assert::IsTrue (preview.bgra.empty(), L"The color monitor with the default color is already chosen");

        choice.SetPreviewMonitor (PicturePreview::Monitor::Green);
        Assert::IsTrue (choice.GetPreviewMonitor() == PicturePreview::Monitor::Green);
        Assert::IsFalse (preview.bgra.empty(), L"A different monitor draws the picture again");
    }


    TEST_METHOD (SetPreviewMonitor_ChangesTheCustomColor)
    {
        PreviewContent        preview;
        MemoryBus             bus;
        PictureMonitorChoice  choice (preview, bus);

        choice.SetPreviewMonitor (PicturePreview::Monitor::Custom, 0xFF112233);

        Assert::IsTrue (choice.GetPreviewMonitor() == PicturePreview::Monitor::Custom);
        Assert::AreEqual ((uint32_t) 0xFF112233, choice.GetCustomArgb());
    }


    TEST_METHOD (RedrawForForm_DrawsAgainOnlyWhenTheFormChangesTheMonitor)
    {
        PreviewContent        preview;
        MemoryBus             bus;
        PictureMonitorChoice  choice (preview, bus);
        HRESULT               hr = S_OK;

        MakeDoubleHiResPicture (preview);
        preview.pictureMode = PicturePreview::Mode::DoubleHiResMono;

        hr = choice.RedrawForForm();
        Assert::IsTrue (SUCCEEDED (hr));
        Assert::IsFalse (preview.bgra.empty(), L"Its bits on the color monitor go to the white one");

        preview.bgra.clear();
        preview.pictureMode   = PicturePreview::Mode::HiRes;
        preview.pictureChosen = PicturePreview::Mode::HiRes;

        hr = choice.RedrawForForm();
        Assert::IsTrue (SUCCEEDED (hr));
        Assert::IsTrue (preview.bgra.empty(), L"Any other picture is on the chosen monitor already");
    }
};
