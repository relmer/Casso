#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TextInputPasteTests
//
//  A paste into a single line turns its line breaks and tabs into spaces,
//  unless SetPasteKeepsBreaks is on, which inserts them as pasted. A paste
//  is given its text here, as the clipboard would.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TextInputPasteTests)
{
public:

    static constexpr const wchar_t *  kpszBlock = L"Dane Bigham|Gene Portwood\r\nLauren\tElliott";

    TEST_METHOD (Paste_TurnsLineBreaksAndTabsIntoSpaces)
    {
        DxuiTextInput  input;

        input.SetFocused (true);
        input.PasteText  (kpszBlock);

        Assert::AreEqual (std::wstring (L"Dane Bigham|Gene Portwood  Lauren Elliott"), input.GetText());
    }


    TEST_METHOD (Paste_KeepsThem_WhenTheFieldDoes)
    {
        DxuiTextInput  input;

        input.SetFocused          (true);
        input.SetPasteKeepsBreaks (true);
        input.PasteText           (kpszBlock);

        Assert::AreEqual (std::wstring (kpszBlock), input.GetText());
    }
};