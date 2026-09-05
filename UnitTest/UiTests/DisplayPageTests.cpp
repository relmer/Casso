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
//  resolved default with an epsilon, which got the interesting case wrong:
//  a value the user deliberately set to match a default reported itself as
//  a default. It now reads the tier that supplied the value, so the mapping
//  is a lookup and can be pinned without a device or a window.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DisplayPageTests)
{
public:

    TEST_METHOD (LabelForSource_GivesTheTextForEachTier)
    {
        Assert::AreEqual (L"(monitor default)", DisplayPage::LabelForSource (CrtSource::Preset));
        Assert::AreEqual (L"(theme default)",   DisplayPage::LabelForSource (CrtSource::Theme));
        Assert::AreEqual (L"(custom)",          DisplayPage::LabelForSource (CrtSource::User));
    }


    // Every row carries a badge now. Under the old comparison a row whose
    // value differed from the default drew nothing at all, which is how an
    // override stayed invisible.
    TEST_METHOD (LabelForSource_EveryTierHasText)
    {
        const CrtSource  tiers[] = { CrtSource::Preset, CrtSource::Theme, CrtSource::User };

        for (const CrtSource & tier : tiers)
        {
            const wchar_t *  label = DisplayPage::LabelForSource (tier);

            Assert::IsNotNull (label);
            Assert::IsTrue    (wcslen (label) > 0);
        }
    }


    TEST_METHOD (LabelForSource_TiersAreDistinct)
    {
        const wchar_t *  preset = DisplayPage::LabelForSource (CrtSource::Preset);
        const wchar_t *  theme  = DisplayPage::LabelForSource (CrtSource::Theme);
        const wchar_t *  user   = DisplayPage::LabelForSource (CrtSource::User);

        Assert::AreNotEqual (preset, theme);
        Assert::AreNotEqual (theme,  user);
        Assert::AreNotEqual (preset, user);
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
    // The resolver enforces that; this pins the two field slots exist and are
    // addressable, since the page indexes its rows by them.
    TEST_METHOD (GammaAndPersistenceAreAddressableFields)
    {
        DisplayDefaultsHint  hint;

        hint.source[(size_t) CrtField::Gamma]       = CrtSource::User;
        hint.source[(size_t) CrtField::Persistence] = CrtSource::User;

        Assert::AreEqual (L"(custom)", DisplayPage::LabelForSource (hint.source[(size_t) CrtField::Gamma]));
        Assert::AreEqual (L"(custom)", DisplayPage::LabelForSource (hint.source[(size_t) CrtField::Persistence]));
        Assert::IsTrue   (hint.source[(size_t) CrtField::Brightness] == CrtSource::Preset);
    }

};
