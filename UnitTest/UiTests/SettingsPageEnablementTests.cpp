#include "Pch.h"

#include "Config/GlobalUserPrefs.h"
#include "Ui/Settings/DisplayPage.h"
#include "Ui/Settings/ScreenshotsPage.h"
#include "Ui/Settings/SettingsPanelState.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsPageEnablementTests
//
//  One control governing another, and a page reporting what the user did.
//
//  A settings page is a set of widgets by value on a panel that needs no
//  window, so the whole thing can be built on the stack, driven by the same
//  key handlers a keyboard reaches, and read back through its accessors.
//  Nothing is painted: enablement is state, and the state is what the paint
//  would have shown.
//
//  Two kinds of rule are pinned here. A GOVERNED control follows its
//  governor -- a scanline slider means nothing with scanlines off, and dims.
//  An EMITTED change is the page telling its owner what happened, through the
//  callback the owner installed, with the value the user arrived at.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SettingsPageEnablementTests)
{
public:

    TEST_METHOD (TheScanlineSliderFollowsItsToggle)
    {
        SettingsPanelState          state;
        DisplayPage                 page;
        GlobalUserPrefsCrtSnapshot  snap;
        std::vector<bool>           told;

        snap.scanlinesEnabled = false;
        page.SetState (&state);
        page.SetInitialCrt (snap);
        page.SetOnScanlinesEnChange ([&told] (bool on) { told.push_back (on); });

        Assert::IsFalse (page.GetScanlinesSlider().IsEnabled(),
                         L"with scanlines off there is no intensity to set");

        Press (page.GetScanlinesToggle());

        Assert::IsTrue (page.GetScanlinesSlider().IsEnabled(), L"turning them on frees the slider");
        Assert::AreEqual ((size_t) 1, told.size(), L"and the owner hears about it once");
        Assert::IsTrue (told[0], L"as an enable");

        Press (page.GetScanlinesToggle());

        Assert::IsFalse (page.GetScanlinesSlider().IsEnabled(), L"and off dims it again");
        Assert::AreEqual ((size_t) 2, told.size());
        Assert::IsFalse (told[1]);
    }


    TEST_METHOD (BothBloomSlidersFollowTheOneBloomToggle)
    {
        SettingsPanelState          state;
        DisplayPage                 page;
        GlobalUserPrefsCrtSnapshot  snap;

        snap.bloomEnabled = true;
        page.SetState (&state);
        page.SetInitialCrt (snap);

        Assert::IsTrue (page.GetBloomRadiusSlider().IsEnabled());
        Assert::IsTrue (page.GetBloomStrengthSlider().IsEnabled());

        Press (page.GetBloomToggle());

        //  Two sliders, one governor. A page that remembered to dim one and
        //  not the other would leave a live strength control on a dead effect.
        Assert::IsFalse (page.GetBloomRadiusSlider().IsEnabled(),   L"radius follows the toggle");
        Assert::IsFalse (page.GetBloomStrengthSlider().IsEnabled(), L"and so does strength");
    }


    TEST_METHOD (TheColorBleedSliderFollowsItsToggle)
    {
        SettingsPanelState          state;
        DisplayPage                 page;
        GlobalUserPrefsCrtSnapshot  snap;

        snap.colorBleedEnabled = false;
        page.SetState (&state);
        page.SetInitialCrt (snap);

        Assert::IsFalse (page.GetColorBleedSlider().IsEnabled());

        Press (page.GetColorBleedToggle());

        Assert::IsTrue (page.GetColorBleedSlider().IsEnabled());
    }


    TEST_METHOD (TheFolderControlsFollowSaveToFile)
    {
        GlobalUserPrefs  prefs;
        ScreenshotsPage  page;

        prefs.screenshotSaveFile = false;
        page.SetPrefs (&prefs);

        //  With nothing being saved, a folder is a setting with no effect.
        Assert::IsFalse (page.GetBrowseFolderButton().IsEnabled(), L"browse dims");
        Assert::IsFalse (page.GetFolderLink().IsEnabled(),         L"and so does the folder link");
        Assert::IsFalse (page.IsDirty(), L"loading a page is not a change");

        Press (page.GetSaveFileCheckbox());

        Assert::IsTrue (prefs.screenshotSaveFile,                 L"the checkbox wrote the preference");
        Assert::IsTrue (page.GetBrowseFolderButton().IsEnabled(), L"and the folder row woke up");
        Assert::IsTrue (page.GetFolderLink().IsEnabled());
        Assert::IsTrue (page.IsDirty(),                           L"which is a change to apply");
    }


    TEST_METHOD (ASliderKeystrokeEmitsTheValueArrivedAt)
    {
        SettingsPanelState  state;
        DisplayPage         page;
        std::vector<float>  brightness;
        std::vector<float>  contrast;

        page.SetState (&state);
        page.SetOnBrightnessChange ([&brightness] (float v) { brightness.push_back (v); });
        page.SetOnContrastChange   ([&contrast]   (float v) { contrast.push_back (v);   });

        //  A slider takes its range and step from the layout pass, the same
        //  one a sheet runs before the page can be seen, so it runs here too.
        page.Layout (RECT { 0, 0, 640, 900 }, DxuiDpiScaler());

        //  The slider steps by ten, so one key to the right from 100 is 110,
        //  and the page hands its owner that number and nothing rounded.
        page.GetBrightnessSlider().SetValue (100.0f);
        page.GetBrightnessSlider().SetFocused (true);
        page.GetBrightnessSlider().OnKey (VK_RIGHT);

        Assert::AreEqual ((size_t) 1, brightness.size(), L"one keystroke, one report");
        Assert::AreEqual (110.0f, brightness[0], L"with the value the slider now holds");
        Assert::AreEqual (110.0f, page.GetBrightnessSlider().GetValue());
        Assert::IsTrue (contrast.empty(), L"and the contrast owner hears nothing");
    }


    TEST_METHOD (ATogglePressEmitsTheStateArrivedAt)
    {
        SettingsPanelState          state;
        DisplayPage                 page;
        GlobalUserPrefsCrtSnapshot  snap;
        std::vector<bool>           bloom;
        std::vector<bool>           bleed;

        snap.bloomEnabled      = false;
        snap.colorBleedEnabled = true;
        page.SetState (&state);
        page.SetInitialCrt (snap);
        page.SetOnBloomEnChange      ([&bloom] (bool on) { bloom.push_back (on); });
        page.SetOnColorBleedEnChange ([&bleed] (bool on) { bleed.push_back (on); });

        Press (page.GetBloomToggle());
        Press (page.GetColorBleedToggle());

        Assert::AreEqual ((size_t) 1, bloom.size());
        Assert::IsTrue  (bloom[0], L"bloom went from off to on");
        Assert::AreEqual ((size_t) 1, bleed.size());
        Assert::IsFalse (bleed[0], L"color bleed went from on to off");
    }


private:

    //  What the keyboard does to a focused control: the space bar flips it.
    //  Focus is given here because a real sheet gives it before any key can
    //  arrive, and a control refuses keys it does not own.
    template <typename Control>
    static void Press (Control & control)
    {
        control.SetFocused (true);
        Assert::IsTrue (control.OnKey (VK_SPACE), L"a focused, enabled control takes the space bar");
    }
};
