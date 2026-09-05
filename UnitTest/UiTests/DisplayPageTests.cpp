#include "Pch.h"

#include "Core/DxuiDpiScaler.h"
#include "Ui/Settings/DisplayPage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPageTests
//
//  The badge a row shows for each tier.
//
//  The page used to work this out by comparing the widget's value against a
//  resolved default with an epsilon, which got the interesting case wrong: a
//  value the user deliberately set to match a default was labeled as that
//  default. It now reads the tier that supplied the value, so the mapping is
//  a lookup and can be pinned without a device or a window.
//
//  Only defaults are labeled. A row the user adjusted carries no badge, and
//  the empty indicator column is what marks it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DisplayPageTests)
{
public:

    TEST_METHOD (LabelForSource_GivesTheTextForEachDefault)
    {
        Assert::AreEqual (L"(monitor default)", DisplayPage::LabelForSource (CrtSource::Preset));
        Assert::AreEqual (L"(theme default)",   DisplayPage::LabelForSource (CrtSource::Theme));
    }


    // The absence of a badge is the signal, so an adjusted row must produce
    // no text at all rather than a third label.
    TEST_METHOD (LabelForSource_AnAdjustedRowHasNoBadge)
    {
        Assert::IsNull (DisplayPage::LabelForSource (CrtSource::User));
    }


    TEST_METHOD (LabelForSource_TheTwoDefaultsAreDistinct)
    {
        const wchar_t *  preset = DisplayPage::LabelForSource (CrtSource::Preset);
        const wchar_t *  theme  = DisplayPage::LabelForSource (CrtSource::Theme);

        Assert::IsNotNull   (preset);
        Assert::IsNotNull   (theme);
        Assert::AreNotEqual (preset, theme);
    }


    // A default-constructed hint reports every field as a monitor default,
    // which is what a page with no prefs bound has to show.
    TEST_METHOD (DefaultHint_ReportsEveryFieldAsAMonitorDefault)
    {
        DisplayDefaultsHint  hint;
        size_t               i = 0;

        for (i = 0; i < (size_t) CrtField::Count; i++)
        {
            Assert::IsTrue (hint.source[i] == CrtSource::Preset);
            Assert::AreEqual (L"(monitor default)", DisplayPage::LabelForSource (hint.source[i]));
        }
    }


    // Gamma and persistence have no theme group, so a page that offered them
    // a theme-default label would describe a tier that cannot supply them.
    // The resolver enforces that; this pins that the two field slots exist
    // and are addressable, since the page indexes its rows by them.
    TEST_METHOD (GammaAndPersistenceAreAddressableFields)
    {
        DisplayDefaultsHint  hint;

        hint.source[(size_t) CrtField::Gamma]       = CrtSource::User;
        hint.source[(size_t) CrtField::Persistence] = CrtSource::User;

        Assert::IsNull (DisplayPage::LabelForSource (hint.source[(size_t) CrtField::Gamma]));
        Assert::IsNull (DisplayPage::LabelForSource (hint.source[(size_t) CrtField::Persistence]));
        Assert::IsTrue (hint.source[(size_t) CrtField::Brightness] == CrtSource::Preset);
    }

};





////////////////////////////////////////////////////////////////////////////////
//
//  DisplaySliderResolutionTests
//
//  That a slider can hold the value the tiers resolved for it.
//
//  A slider quantizes on the way in, so one whose granularity is coarser than
//  the values a theme or preset may declare silently shows a different number
//  than the one behind the picture -- and, because a user's adjustment is
//  dropped when it matches the resolved default, can never be dragged back to
//  where it started. The color bleed width row did both: it rounded all four
//  shipped theme widths to the same 1.
//
//  Layout is where the granularity is set, and it needs no device, so the
//  round trip pins here.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DisplaySliderResolutionTests)
{
public:

    // Every width the shipped themes declare, plus the color preset's 3.0.
    TEST_METHOD (ColorBleedWidth_HoldsTheWidthsTheTiersDeclare)
    {
        const float  widths[] = { 1.2f, 1.5f, 1.7f, 1.8f, 3.0f };
        DisplayPage  page;
        size_t       i        = 0;

        LayOut (page);

        for (i = 0; i < _countof (widths); i++)
        {
            page.GetColorBleedSlider().SetValue (widths[i]);

            Assert::AreEqual (widths[i], page.GetColorBleedSlider().GetValue(), 0.0001f);
        }
    }


    // Dragging a control away and back is what returns a row to its default
    // badge, so the value has to survive the excursion exactly.
    TEST_METHOD (ColorBleedWidth_SurvivesAnExcursionAndBack)
    {
        DisplayPage  page;

        LayOut (page);

        page.GetColorBleedSlider().SetValue (1.2f);
        page.GetColorBleedSlider().SetValue (4.0f);
        page.GetColorBleedSlider().SetValue (1.2f);

        Assert::AreEqual (1.2f, page.GetColorBleedSlider().GetValue(), 0.0001f);
    }


    // Bloom radius already resolved at a tenth; this guards the two from
    // drifting apart, since they carry the same kind of value.
    TEST_METHOD (BloomRadius_HoldsTheRadiiTheTiersDeclare)
    {
        const float  radii[] = { 0.6f, 0.8f, 1.8f, 2.4f };
        DisplayPage  page;
        size_t       i       = 0;

        LayOut (page);

        for (i = 0; i < _countof (radii); i++)
        {
            page.GetBloomRadiusSlider().SetValue (radii[i]);

            Assert::AreEqual (radii[i], page.GetBloomRadiusSlider().GetValue(), 0.0001f);
        }
    }


private:

    // Slider ranges and granularity are set in Layout, so nothing below is
    // meaningful until it has run once.
    static void  LayOut (DisplayPage & page)
    {
        DxuiDpiScaler  scaler;
        RECT           rect = { 0, 0, 600, 900 };

        page.Layout (rect, scaler);
    }

};
