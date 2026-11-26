#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoExplorer/Browser/Preview/IPreviewRefresh.h"
#include "CassoExplorer/Browser/Preview/PreviewViewState.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewViewStateTests
//
//  The choices made for the files previewed, and that a change of one makes
//  the preview again.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PreviewViewStateTests)
{
public:

    class FakePreviewRefresh : public IPreviewRefresh
    {
    public:
        void  UpdatePreview() override { updates++; }

        int  updates = 0;
    };


    TEST_METHOD (HexForm_AppliesToTheFilePreviewedNow)
    {
        FakePreviewRefresh  refresh;
        PreviewViewState    state (refresh);

        Assert::IsTrue (state.GetHexForm (L"a.dsk|ONE") == PreviewDecoder::HexForm::Auto);

        state.SetHexForm (PreviewDecoder::HexForm::Disassembly);
        Assert::AreEqual (1, refresh.updates, L"A new form makes the preview again");
        Assert::IsTrue (state.GetHexForm (L"a.dsk|ONE") == PreviewDecoder::HexForm::Disassembly);

        Assert::IsTrue (state.GetHexForm (L"a.dsk|TWO") == PreviewDecoder::HexForm::Auto, L"Another file opens in its default form");
        Assert::IsTrue (state.GetHexForm (L"a.dsk|ONE") == PreviewDecoder::HexForm::Auto, L"and the first file is not given it back");
    }


    TEST_METHOD (ViewAs_AppliesToTheFilePreviewedNow)
    {
        FakePreviewRefresh  refresh;
        PreviewViewState    state (refresh);

        Assert::IsTrue (state.GetViewAs() == ViewChoice::Default);

        state.GetHexForm (L"a.dsk|ONE");
        state.SetViewAs (ViewChoice::Hex);
        Assert::AreEqual (1, refresh.updates);
        Assert::IsTrue (state.GetViewAs() == ViewChoice::Hex);

        state.GetHexForm (L"a.dsk|TWO");
        Assert::IsTrue (state.GetViewAs() == ViewChoice::Default, L"Another file opens in its default view");
    }


    TEST_METHOD (Charset_BeforeAnyFileIsPreviewed_ChangesNothing)
    {
        FakePreviewRefresh  refresh;
        PreviewViewState    state (refresh);

        state.SetCharset (Charset::MacOsRoman);
        state.SetRecordLength (64);
        state.SetFormulas (true);

        Assert::AreEqual (0, refresh.updates, L"There is no file to apply them to");
        Assert::IsTrue (state.GetDecodeOptions (L"", "").charset == Charset::Default);
    }


    TEST_METHOD (FileOptions_AreSetForTheFilePreviewedNow)
    {
        FakePreviewRefresh  refresh;
        PreviewViewState    state (refresh);
        DecodeOptions       one;
        DecodeOptions       other;

        state.GetHexForm (L"a.dsk|ONE");
        state.SetCharset (Charset::MacOsRoman);
        state.SetRecordLength (64);
        state.SetFormulas (true);
        Assert::AreEqual (3, refresh.updates, L"Each choice makes the preview again");

        one   = state.GetDecodeOptions (L"a.dsk", "ONE");
        other = state.GetDecodeOptions (L"a.dsk", "TWO");

        Assert::IsTrue (one.charset == Charset::MacOsRoman);
        Assert::AreEqual ((size_t) 64, one.recordLength);
        Assert::IsTrue (one.formulas);

        Assert::IsTrue (other.charset == Charset::Default, L"Another file has the defaults");
        Assert::AreEqual ((size_t) 0, other.recordLength);
        Assert::IsFalse (other.formulas);
    }


    TEST_METHOD (DefaultOptions_ReplaceEachFilesOptionsButItsRecordLength)
    {
        FakePreviewRefresh  refresh;
        PreviewViewState    state (refresh);
        DecodeOptions       defaults;
        DecodeOptions       one;
        DecodeOptions       other;

        state.GetHexForm (L"a.dsk|ONE");
        state.SetCharset (Charset::MacOsRoman);
        state.SetRecordLength (64);

        defaults.charset      = Charset::Iso8859_1;
        defaults.formulas     = true;
        defaults.recordLength = 99;
        state.SetDefaultDecodeOptions (defaults);

        one   = state.GetDecodeOptions (L"a.dsk", "ONE");
        other = state.GetDecodeOptions (L"a.dsk", "TWO");

        Assert::IsTrue (one.charset == Charset::Iso8859_1);
        Assert::IsTrue (one.formulas);
        Assert::AreEqual ((size_t) 64, one.recordLength, L"The record length of the file is as it was");

        Assert::IsTrue (other.charset == Charset::Iso8859_1, L"A file with no options has the defaults");
        Assert::AreEqual ((size_t) 0, other.recordLength, L"but never a default record length");
        Assert::AreEqual ((size_t) 0, state.GetDefaultDecodeOptions().recordLength);
    }


    TEST_METHOD (SavedGameShown_IsOnUntilChanged_AndMakesThePreviewAgain)
    {
        FakePreviewRefresh  refresh;
        PreviewViewState    state (refresh);

        Assert::IsTrue (state.IsSavedGameShown());

        state.SetSavedGameShown (false);
        Assert::IsFalse (state.IsSavedGameShown());
        Assert::AreEqual (1, refresh.updates);

        state.SetSavedGameShown (true);
        Assert::IsTrue (state.IsSavedGameShown());
        Assert::AreEqual (2, refresh.updates);
    }
};
