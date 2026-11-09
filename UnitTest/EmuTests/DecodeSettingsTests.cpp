#include "Pch.h"

#include "Devices/Disk/Inspector/DecodeSettings.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeSettingsTests
//
//  Custom marks and checks per range of whole tracks (FR-019, FR-020): custom
//  marks come first, the standard ones follow unless they are turned off, and
//  a later range overrides an earlier one where they overlap.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DecodeSettingsTests)
{
public:

    static DiskMarkPattern Parse (std::string_view text)
    {
        DiskMarkPattern  pattern;
        std::string      error;



        Assert::IsTrue (DiskMarkPattern::TryParse (text, pattern, error));

        return pattern;
    }



    TEST_METHOD (TheStandardSettingsMatchTheStandardMarksEverywhere)
    {
        DecodeSettings  settings = DecodeSettings::MakeStandard();
        FieldMarks      standard = FieldMarks::MakeStandard();



        Assert::IsTrue (settings.IsStandard());
        Assert::IsTrue (settings.GetMarksForTrack (0).address16  == standard.address16);
        Assert::IsTrue (settings.GetMarksForTrack (39).data      == standard.data);
        Assert::IsTrue (settings.GetChecksForTrack (17).isDataChecksumOn);
    }



    TEST_METHOD (CustomMarksComeFirstAndOnlyInTheirRange)
    {
        DecodeSettings  settings;
        DecodeRange     range;
        FieldMarks      marks;



        range.firstTrack = 3;
        range.lastTrack  = 5;
        range.customMarks.address16.push_back (Parse ("D4 AA ??"));
        settings.AddRange (range);

        marks = settings.GetMarksForTrack (4);
        Assert::IsFalse (settings.IsStandard());
        Assert::IsTrue (marks.address16.front() == Parse ("D4 AA ??"));
        Assert::IsTrue (marks.address16.back()  == FieldMarks::MakeStandard().address16.back(), L"the standard marks follow");
        Assert::IsTrue (settings.GetMarksForTrack (6).address16 == FieldMarks::MakeStandard().address16);
    }



    TEST_METHOD (StandardMarksCanBeTurnedOff)
    {
        DecodeSettings  settings;
        DecodeRange     range;
        FieldMarks      marks;



        range.customMarks.address16.push_back (Parse ("D4 AA 96"));
        range.matchStandardToo = false;
        settings.AddRange (range);

        marks = settings.GetMarksForTrack (0);
        Assert::AreEqual (1, static_cast<int> (marks.address16.size()));
        Assert::IsTrue (marks.address16[0] == Parse ("D4 AA 96"));
    }



    TEST_METHOD (ALaterRangeOverridesAnEarlierOne)
    {
        DecodeSettings  settings;
        DecodeRange     all;
        DecodeRange     some;



        all.checks.isEpilogueOn = false;
        settings.AddRange (all);

        some.firstTrack              = 10;
        some.lastTrack               = 12;
        some.checks.isDataChecksumOn = false;
        settings.AddRange (some);

        Assert::IsFalse (settings.GetChecksForTrack (0).isEpilogueOn);
        Assert::IsTrue  (settings.GetChecksForTrack (0).isDataChecksumOn);
        Assert::IsTrue  (settings.GetChecksForTrack (11).isEpilogueOn);
        Assert::IsFalse (settings.GetChecksForTrack (11).isDataChecksumOn);
    }



    TEST_METHOD (ResetReturnsToStandard)
    {
        DecodeSettings  settings;
        DecodeRange     range;



        range.checks.isAddressChecksumOn = false;
        settings.AddRange (range);
        Assert::IsFalse (settings.IsStandard());

        settings = DecodeSettings::MakeStandard();
        Assert::IsTrue (settings.IsStandard());
        Assert::IsTrue (settings.GetChecksForTrack (0).isAddressChecksumOn);
    }
};
