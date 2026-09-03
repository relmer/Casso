#include "Pch.h"

#include "Config/CrtResolver.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CrtResolverTests
//
//  The layering rules, which used to be written four times and are now
//  written once. Three of those four copies lived where no test could reach
//  them, and two defects shipped out of that: a resize that changed
//  brightness, and a Restore Defaults that read a theme's base values rather
//  than the machine variant's.
//
//  The matrix below is the whole contract. Per field: preset, then the theme
//  where the theme declares that field's GROUP, then the user where the user
//  has set that FIELD.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CrtResolverTests)
{
public:

    // A preset with every value distinct from every default, so a field that
    // silently falls back to a struct default is visible rather than
    // coincidentally correct.
    static CrtValues  MakePreset()
    {
        CrtValues  preset;

        preset.brightness         = 1.05f;
        preset.contrast           = 0.90f;
        preset.gamma              = 1.80f;
        preset.scanlinesEnabled   = true;
        preset.scanlinesIntensity = 0.40f;
        preset.bloomEnabled       = true;
        preset.bloomRadius        = 3.00f;
        preset.bloomStrength      = 0.40f;
        preset.colorBleedEnabled  = true;
        preset.colorBleedWidth    = 2.00f;
        preset.persistence        = 0.55f;

        return preset;
    }


    // A theme declaring every group it can, with values distinct from the
    // preset's. Note it cannot declare gamma or persistence at all.
    static ThemeCrtDefaults  MakeFullTheme()
    {
        ThemeCrtDefaults  theme;

        theme.hasBrightness      = true;
        theme.brightness         = 1.10f;
        theme.hasContrast        = true;
        theme.contrast           = 1.00f;
        theme.hasScanlines       = true;
        theme.scanlinesEnabled   = true;
        theme.scanlinesIntensity = 0.85f;
        theme.hasBloom           = true;
        theme.bloomEnabled       = true;
        theme.bloomRadius        = 2.00f;
        theme.bloomStrength      = 0.70f;
        theme.hasColorBleed      = true;
        theme.colorBleedEnabled  = true;
        theme.colorBleedWidth    = 1.50f;

        return theme;
    }


    static CrtSource  SourceOf (const CrtResolved & r, CrtField f)
    {
        return r.source[(size_t) f];
    }


    //  Row 1 of the table: neither tier has an opinion.
    TEST_METHOD (NoThemeNoUser_EveryFieldComesFromThePreset)
    {
        CrtValues     preset = MakePreset();
        CrtOverrides  none;
        CrtResolved   r      = CrtResolver::Resolve (preset, nullptr, none);
        size_t        i      = 0;

        Assert::AreEqual (1.05f, r.values.brightness);
        Assert::AreEqual (0.90f, r.values.contrast);
        Assert::AreEqual (1.80f, r.values.gamma);
        Assert::IsTrue   (r.values.scanlinesEnabled);
        Assert::AreEqual (0.40f, r.values.scanlinesIntensity);
        Assert::AreEqual (3.00f, r.values.bloomRadius);
        Assert::AreEqual (2.00f, r.values.colorBleedWidth);
        Assert::AreEqual (0.55f, r.values.persistence);

        for (i = 0; i < (size_t) CrtField::Count; i++)
        {
            Assert::IsTrue (r.source[i] == CrtSource::Preset);
        }
    }


    //  Row 2: the theme declares a group, the user has not touched it.
    TEST_METHOD (ThemeOnly_DeclaredGroupsWinAndReportTheme)
    {
        CrtValues         preset = MakePreset();
        ThemeCrtDefaults  theme  = MakeFullTheme();
        CrtOverrides      none;
        CrtResolved       r      = CrtResolver::Resolve (preset, &theme, none);

        Assert::AreEqual (1.10f, r.values.brightness);
        Assert::AreEqual (0.85f, r.values.scanlinesIntensity);
        Assert::AreEqual (2.00f, r.values.bloomRadius);
        Assert::AreEqual (1.50f, r.values.colorBleedWidth);

        Assert::IsTrue (SourceOf (r, CrtField::Brightness)         == CrtSource::Theme);
        Assert::IsTrue (SourceOf (r, CrtField::Contrast)           == CrtSource::Theme);
        Assert::IsTrue (SourceOf (r, CrtField::ScanlinesIntensity) == CrtSource::Theme);
        Assert::IsTrue (SourceOf (r, CrtField::BloomRadius)        == CrtSource::Theme);
        Assert::IsTrue (SourceOf (r, CrtField::ColorBleedWidth)    == CrtSource::Theme);
    }


    //  A theme that omits a group leaves the preset alone. This is the
    //  defect the has* flags exist to prevent: without them a theme that
    //  says nothing about scanlines wipes the preset with struct zeroes.
    TEST_METHOD (ThemeOmittingAGroup_LeavesThatGroupOnThePreset)
    {
        CrtValues         preset = MakePreset();
        ThemeCrtDefaults  theme;
        CrtOverrides      none;
        CrtResolved       r;

        theme.hasBrightness = true;
        theme.brightness    = 1.10f;

        r = CrtResolver::Resolve (preset, &theme, none);

        Assert::AreEqual (1.10f, r.values.brightness);
        Assert::AreEqual (0.40f, r.values.scanlinesIntensity);
        Assert::IsTrue   (r.values.scanlinesEnabled);
        Assert::AreEqual (3.00f, r.values.bloomRadius);

        Assert::IsTrue (SourceOf (r, CrtField::Brightness)         == CrtSource::Theme);
        Assert::IsTrue (SourceOf (r, CrtField::ScanlinesIntensity) == CrtSource::Preset);
        Assert::IsTrue (SourceOf (r, CrtField::BloomRadius)        == CrtSource::Preset);
    }


    //  Row 3: no theme, the user has set one field.
    TEST_METHOD (UserOnly_OverriddenFieldWinsAndTheRestStayOnThePreset)
    {
        CrtValues     preset = MakePreset();
        CrtOverrides  user;
        CrtResolved   r;

        user.bloomStrength = 0.50f;

        r = CrtResolver::Resolve (preset, nullptr, user);

        Assert::AreEqual (0.50f, r.values.bloomStrength);
        Assert::AreEqual (3.00f, r.values.bloomRadius);
        Assert::IsTrue   (r.values.bloomEnabled);

        Assert::IsTrue (SourceOf (r, CrtField::BloomStrength) == CrtSource::User);
        Assert::IsTrue (SourceOf (r, CrtField::BloomRadius)   == CrtSource::Preset);
        Assert::IsTrue (SourceOf (r, CrtField::BloomEnabled)  == CrtSource::Preset);
    }


    //  Row 4, and the point of the whole feature: the user beats the theme
    //  for that ONE field, and the theme still wins for every sibling in the
    //  same group. Under the old whole-block flag the theme lost entirely.
    TEST_METHOD (ThemeAndUser_UserWinsPerFieldNotPerGroup)
    {
        CrtValues         preset = MakePreset();
        ThemeCrtDefaults  theme  = MakeFullTheme();
        CrtOverrides      user;
        CrtResolved       r;

        user.bloomStrength = 0.50f;

        r = CrtResolver::Resolve (preset, &theme, user);

        Assert::AreEqual (0.50f, r.values.bloomStrength);
        Assert::AreEqual (2.00f, r.values.bloomRadius);
        Assert::AreEqual (1.10f, r.values.brightness);
        Assert::AreEqual (1.50f, r.values.colorBleedWidth);

        Assert::IsTrue (SourceOf (r, CrtField::BloomStrength)   == CrtSource::User);
        Assert::IsTrue (SourceOf (r, CrtField::BloomRadius)     == CrtSource::Theme);
        Assert::IsTrue (SourceOf (r, CrtField::BloomEnabled)    == CrtSource::Theme);
        Assert::IsTrue (SourceOf (r, CrtField::Brightness)      == CrtSource::Theme);
        Assert::IsTrue (SourceOf (r, CrtField::ColorBleedWidth) == CrtSource::Theme);
    }


    //  No theme group carries gamma or persistence, so those two have only
    //  two rows rather than four. A page that offers a theme-default label
    //  for either is describing a tier that cannot exist.
    TEST_METHOD (GammaAndPersistence_NeverReportTheme)
    {
        CrtValues         preset = MakePreset();
        ThemeCrtDefaults  theme  = MakeFullTheme();
        CrtOverrides      user;
        CrtResolved       withTheme;
        CrtResolved       withUser;

        withTheme = CrtResolver::Resolve (preset, &theme, CrtOverrides {});

        Assert::AreEqual (1.80f, withTheme.values.gamma);
        Assert::AreEqual (0.55f, withTheme.values.persistence);
        Assert::IsTrue (SourceOf (withTheme, CrtField::Gamma)       == CrtSource::Preset);
        Assert::IsTrue (SourceOf (withTheme, CrtField::Persistence) == CrtSource::Preset);

        user.gamma       = 2.20f;
        user.persistence = 0.10f;
        withUser         = CrtResolver::Resolve (preset, &theme, user);

        Assert::AreEqual (2.20f, withUser.values.gamma);
        Assert::AreEqual (0.10f, withUser.values.persistence);
        Assert::IsTrue (SourceOf (withUser, CrtField::Gamma)       == CrtSource::User);
        Assert::IsTrue (SourceOf (withUser, CrtField::Persistence) == CrtSource::User);
    }


    //  Switching theme moves every field the user has not set, and moves
    //  none that they have. This is the scenario the README promises and the
    //  old model could not deliver.
    TEST_METHOD (ThemeChange_LeavesUserFieldsAndMovesTheRest)
    {
        CrtValues         preset = MakePreset();
        ThemeCrtDefaults  first  = MakeFullTheme();
        ThemeCrtDefaults  second;
        CrtOverrides      user;
        CrtResolved       before;
        CrtResolved       after;

        user.bloomStrength = 0.50f;

        second.hasBloom          = true;
        second.bloomEnabled      = true;
        second.bloomRadius       = 0.80f;
        second.bloomStrength     = 0.25f;
        second.hasColorBleed     = true;
        second.colorBleedEnabled = false;
        second.colorBleedWidth   = 0.00f;

        before = CrtResolver::Resolve (preset, &first,  user);
        after  = CrtResolver::Resolve (preset, &second, user);

        Assert::AreEqual (0.50f, before.values.bloomStrength);
        Assert::AreEqual (0.50f, after.values.bloomStrength);

        Assert::AreEqual (2.00f, before.values.bloomRadius);
        Assert::AreEqual (0.80f, after.values.bloomRadius);

        Assert::IsTrue   (before.values.colorBleedEnabled);
        Assert::IsFalse  (after.values.colorBleedEnabled);

        //  The second theme declares no brightness, so brightness returns to
        //  the preset rather than keeping the first theme's value.
        Assert::AreEqual (1.10f, before.values.brightness);
        Assert::AreEqual (1.05f, after.values.brightness);
    }


    //  Clearing a field falls back to the theme where the theme declares it
    //  and to the preset where it does not.
    TEST_METHOD (ClearingAField_FallsBackThemeThenPreset)
    {
        CrtValues         preset = MakePreset();
        ThemeCrtDefaults  theme;
        CrtOverrides      user;
        CrtResolved       r;

        theme.hasBloom       = true;
        theme.bloomEnabled   = true;
        theme.bloomRadius    = 2.00f;
        theme.bloomStrength  = 0.70f;

        user.bloomStrength = 0.50f;
        user.gamma         = 2.20f;

        user.bloomStrength.reset();
        r = CrtResolver::Resolve (preset, &theme, user);
        Assert::AreEqual (0.70f, r.values.bloomStrength);
        Assert::IsTrue (SourceOf (r, CrtField::BloomStrength) == CrtSource::Theme);

        user.gamma.reset();
        r = CrtResolver::Resolve (preset, &theme, user);
        Assert::AreEqual (1.80f, r.values.gamma);
        Assert::IsTrue (SourceOf (r, CrtField::Gamma) == CrtSource::Preset);
    }


    //  Provenance is what supplied the value, never a comparison of numbers.
    //  The old badge inferred it by comparing floats, so a value the user
    //  deliberately set to match the default read as a default.
    TEST_METHOD (OverrideEqualToTheDefault_StillReportsUser)
    {
        CrtValues     preset = MakePreset();
        CrtOverrides  user;
        CrtResolved   r;

        user.bloomRadius = 3.00f;

        r = CrtResolver::Resolve (preset, nullptr, user);

        Assert::AreEqual (3.00f, r.values.bloomRadius);
        Assert::IsTrue (SourceOf (r, CrtField::BloomRadius) == CrtSource::User);
    }


    TEST_METHOD (EmptyOverridesReportsEmptyAndOneFieldDoesNot)
    {
        CrtOverrides  o;

        Assert::IsTrue (o.IsEmpty());

        o.persistence = 0.2f;
        Assert::IsFalse (o.IsEmpty());

        o.persistence.reset();
        Assert::IsTrue (o.IsEmpty());
    }


    //  Cancel restores by assigning a snapshot map back, which compares the
    //  mapped type. Without the defaulted equality that does not compile.
    TEST_METHOD (OverridesCompareByValue)
    {
        CrtOverrides  a;
        CrtOverrides  b;

        Assert::IsTrue (a == b);

        a.brightness = 1.2f;
        Assert::IsFalse (a == b);

        b.brightness = 1.2f;
        Assert::IsTrue (a == b);
    }


    ////////////////////////////////////////////////////////////////////////
    //  Key construction. Per-monitor isolation is entirely a property of
    //  key distinctness, which is what makes it testable at all.
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (MakeKey_JoinsMonitorAndMode)
    {
        Assert::AreEqual (std::string ("AppleMonitorII/color"),  CrtResolver::MakeKey ("AppleMonitorII",  0));
        Assert::AreEqual (std::string ("AppleMonitorII/green"),  CrtResolver::MakeKey ("AppleMonitorII",  1));
        Assert::AreEqual (std::string ("AppleMonitorIIc/amber"), CrtResolver::MakeKey ("AppleMonitorIIc", 2));
        Assert::AreEqual (std::string ("AppleMonitorIIc/white"), CrtResolver::MakeKey ("AppleMonitorIIc", 3));
    }


    //  SC-002: adjusting one monitor and mode cannot disturb another.
    TEST_METHOD (MakeKey_EveryMonitorAndModePairIsDistinct)
    {
        const char *       monitors[] = { "AppleMonitorII", "AppleMonitorIIc" };
        std::set<std::string>  keys;
        size_t                 m     = 0;
        size_t                 mode  = 0;
        size_t                 total = 0;

        for (m = 0; m < _countof (monitors); m++)
        {
            for (mode = 0; mode < CrtResolver::kModeCount; mode++)
            {
                keys.insert (CrtResolver::MakeKey (monitors[m], mode));
                total++;
            }
        }

        Assert::AreEqual (total, keys.size());
    }


    //  Sorted order is not enum order. The tokens are declared in
    //  SettingsColorMode order, but a map sorts them alphabetically, so any
    //  expected-order assertion written from the enum would be wrong.
    TEST_METHOD (MakeKey_SortsAlphabeticallyNotByModeOrdinal)
    {
        std::set<std::string>            keys;
        std::vector<std::string>         ordered;
        size_t                           mode = 0;

        for (mode = 0; mode < CrtResolver::kModeCount; mode++)
        {
            keys.insert (CrtResolver::MakeKey ("AppleMonitorII", mode));
        }

        ordered.assign (keys.begin(), keys.end());

        Assert::AreEqual (std::string ("AppleMonitorII/amber"), ordered[0]);
        Assert::AreEqual (std::string ("AppleMonitorII/color"), ordered[1]);
        Assert::AreEqual (std::string ("AppleMonitorII/green"), ordered[2]);
        Assert::AreEqual (std::string ("AppleMonitorII/white"), ordered[3]);
    }


    //  The separator sits below every lowercase letter, so a shared prefix
    //  cannot interleave one monitor's keys into another's.
    TEST_METHOD (MakeKey_SharedPrefixDoesNotInterleave)
    {
        std::set<std::string>     keys;
        std::vector<std::string>  ordered;
        size_t                    mode = 0;

        for (mode = 0; mode < CrtResolver::kModeCount; mode++)
        {
            keys.insert (CrtResolver::MakeKey ("AppleMonitorII",  mode));
            keys.insert (CrtResolver::MakeKey ("AppleMonitorIIc", mode));
        }

        ordered.assign (keys.begin(), keys.end());

        Assert::AreEqual ((size_t) 8, ordered.size());

        for (size_t i = 0; i < 4; i++)
        {
            Assert::IsTrue (ordered[i].starts_with ("AppleMonitorII/"));
        }

        for (size_t i = 4; i < 8; i++)
        {
            Assert::IsTrue (ordered[i].starts_with ("AppleMonitorIIc/"));
        }
    }


    TEST_METHOD (ModeToken_OutOfRangeYieldsColor)
    {
        Assert::IsTrue (CrtResolver::ModeToken (99) == "color");
    }

};
