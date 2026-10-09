#include "Pch.h"

#include "Ui/DiskInspector/DecodeSettingsForm.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeSettingsFormTests
//
//  The "Decode settings" dialog's fields (FR-019): marks with ?? for any
//  byte, an empty field for none, a track range checked before it is used,
//  and the dialog opening on the settings for the selected track.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DecodeSettingsFormTests)
{
public:

    TEST_METHOD (AFormMakesTheRangeItDescribes)
    {
        DecodeSettingsForm  form;
        DecodeRange         range;
        std::wstring        error;



        form.firstTrack       = L"3";
        form.lastTrack        = L"9";
        form.address16        = L"D4 AA ??";
        form.matchStandardToo = false;
        form.checkData        = false;

        Assert::IsTrue (form.TryBuildRange (range, error), error.c_str());
        Assert::AreEqual (3, range.firstTrack);
        Assert::AreEqual (9, range.lastTrack);
        Assert::AreEqual (static_cast<size_t> (1), range.customMarks.address16.size());
        Assert::IsTrue   (range.customMarks.address16[0].IsAny (2));
        Assert::IsTrue   (range.customMarks.address13.empty(), L"an empty field adds no mark");
        Assert::IsFalse  (range.matchStandardToo);
        Assert::IsFalse  (range.checks.isDataChecksumOn);
        Assert::IsTrue   (range.checks.isAddressChecksumOn);
    }



    TEST_METHOD (ABadFieldSaysWhichAndWhy)
    {
        DecodeSettingsForm  form;
        DecodeRange         range;
        std::wstring        error;



        form.data = L"D5 XX";
        Assert::IsFalse (form.TryBuildRange (range, error));
        Assert::IsTrue  (error.starts_with (L"Data prologue: "), error.c_str());

        form           = DecodeSettingsForm();
        form.firstTrack = L"12";
        form.lastTrack  = L"4";
        Assert::IsFalse (form.TryBuildRange (range, error), L"the first track after the last");

        form.lastTrack = L"40";
        Assert::IsFalse (form.TryBuildRange (range, error), L"past track 39");
    }



    TEST_METHOD (TheDialogOpensOnTheSettingsForTheTrack)
    {
        DecodeSettings      settings;
        DecodeRange         range;
        DecodeSettingsForm  form;
        DiskMarkPattern     pattern;
        std::string         error;



        Assert::IsTrue (DiskMarkPattern::TryParse ("D5 AA ??", pattern, error));
        range.firstTrack = 10;
        range.lastTrack  = 12;
        range.customMarks.data.push_back (pattern);
        range.checks.isEpilogueOn = false;
        settings.AddRange (range);

        form = DecodeSettingsForm::MakeFrom (settings, 11);
        Assert::AreEqual (std::wstring (L"10"), form.firstTrack);
        Assert::AreEqual (std::wstring (L"D5 AA ??"), form.data);
        Assert::IsFalse  (form.checkEpilogues);

        form = DecodeSettingsForm::MakeFrom (settings, 2);
        Assert::AreEqual (std::wstring (L"0"), form.firstTrack, L"a track no range covers opens on the standard form");
        Assert::IsTrue   (form.data.empty());
    }
};
