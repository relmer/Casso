#include "Pch.h"

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
