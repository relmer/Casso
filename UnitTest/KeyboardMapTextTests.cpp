#include "Pch.h"

#include "CppUnitTest.h"

#include "Ui/Dialogs/KeyboardMapText.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  KeyboardMapTextTests
//
//  The dialog must describe the machine in front of the user and no other.
//  A row naming a key the hardware lacks sends the reader hunting for a fault
//  in the emulator when the key was never there, so absence is asserted as
//  hard as presence.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (KeyboardMapTextTests)
{
public:

    static KeyboardMapText::Machine MakeTwoPlus()
    {
        KeyboardMapText::Machine  m;

        m.hasAppleKeys = false;
        m.hasTwoeKeys  = false;
        m.hasGamePort  = true;    // the ][+ has a game port

        return m;
    }


    static KeyboardMapText::Machine MakeTwoe()
    {
        KeyboardMapText::Machine  m;

        m.hasAppleKeys = true;
        m.hasTwoeKeys  = true;
        m.hasGamePort  = true;

        return m;
    }


    // True when any row of the body mentions `needle`, on either side.
    static bool Mentions (const std::vector<DialogTextRun> & body, const wchar_t * needle)
    {
        for (const DialogTextRun & run : body)
        {
            if (run.text.find (needle) != std::wstring::npos ||
                run.rightText.find (needle) != std::wstring::npos)
            {
                return true;
            }
        }

        return false;
    }


    static size_t CountColumnRows (const std::vector<DialogTextRun> & body)
    {
        size_t  rows = 0;

        for (const DialogTextRun & run : body)
        {
            if (run.IsColumnRow())
            {
                rows++;
            }
        }

        return rows;
    }


    TEST_METHOD (TwoPlus_NamesNoKeyItLacks)
    {
        std::vector<DialogTextRun>  body = KeyboardMapText::BuildBody (MakeTwoPlus());

        Assert::IsFalse (Mentions (body, L"Open Apple"),
            L"the ][+ has no Open Apple key, so no row may name one");
        Assert::IsFalse (Mentions (body, L"Closed Apple"),
            L"the ][+ has no Closed Apple key, so no row may name one");
        Assert::IsFalse (Mentions (body, L"Delete"),
            L"DELETE arrived with the //e; the ][+ row must not exist");
    }

    TEST_METHOD (TwoPlus_StillNamesWhatItHas)
    {
        std::vector<DialogTextRun>  body = KeyboardMapText::BuildBody (MakeTwoPlus());

        Assert::IsTrue (Mentions (body, L"Backspace"),
            L"every model reaches the left arrow with Backspace");
        Assert::IsTrue (Mentions (body, L"X / Z"),
            L"a ][+ has a game port, so the fire buttons apply");
    }

    TEST_METHOD (Twoe_NamesEveryKeyItAdds)
    {
        std::vector<DialogTextRun>  body = KeyboardMapText::BuildBody (MakeTwoe());

        Assert::IsTrue (Mentions (body, L"Open Apple"),   L"//e has Open Apple");
        Assert::IsTrue (Mentions (body, L"Closed Apple"), L"//e has Closed Apple");
        Assert::IsTrue (Mentions (body, L"Delete"),       L"//e has DELETE");
    }

    //  The dialog sizes itself from the rows, so a machine with fewer keys
    //  must actually produce fewer rows rather than blank ones.
    TEST_METHOD (FewerKeys_ProduceFewerRows)
    {
        std::vector<DialogTextRun>  twoPlus = KeyboardMapText::BuildBody (MakeTwoPlus());
        std::vector<DialogTextRun>  twoe    = KeyboardMapText::BuildBody (MakeTwoe());

        Assert::IsTrue (CountColumnRows (twoPlus) < CountColumnRows (twoe),
            L"the ][+ body must be shorter than the //e body, not padded to match");
    }

    //  No game port means the joystick mapping moves nothing, so it is not
    //  described. Guards the same rule as the key rows.
    TEST_METHOD (NoGamePort_DropsTheJoystickRows)
    {
        KeyboardMapText::Machine  m = MakeTwoe();

        m.hasGamePort = false;

        std::vector<DialogTextRun>  body = KeyboardMapText::BuildBody (m);

        Assert::IsFalse (Mentions (body, L"Joystick"),
            L"with no game port there is nothing for the arrows to drive");
        Assert::IsTrue (Mentions (body, L"Open Apple"),
            L"dropping the joystick rows must not disturb the key rows");
    }

    //  Every key row is a column row, because the arrows have to line up.
    TEST_METHOD (KeyRows_AreColumnRows)
    {
        std::vector<DialogTextRun>  body = KeyboardMapText::BuildBody (MakeTwoe());

        Assert::IsTrue (CountColumnRows (body) >= 4,
            L"Open Apple, Closed Apple, Backspace and Delete are all column rows");

        for (const DialogTextRun & run : body)
        {
            if (run.IsColumnRow())
            {
                Assert::IsFalse (run.text.empty(),
                    L"a column row needs a left cell as well as a right one");
            }
        }
    }
};
