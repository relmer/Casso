#include "Pch.h"
#include "Devices/Disk/DeferredChangePrompts.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DeferredChangePromptsTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DeferredChangePromptsTests)
{
public:

    using Kind = DeferredChangePrompts::Kind;

    static ChangePrompt  MakePrompt (const wchar_t * message)
    {
        ChangePrompt  prompt;

        prompt.message = message;

        return prompt;
    }



    TEST_METHOD (TakeReturnsThePromptsInTheOrderTheyArrivedAndEmptiesTheList)
    {
        DeferredChangePrompts                       prompts;
        std::vector<DeferredChangePrompts::Entry>   taken;



        Assert::IsTrue (prompts.IsEmpty());

        prompts.Defer (Kind::Question, 6, 0, MakePrompt (L"first"));
        prompts.Defer (Kind::Report,   6, 1, MakePrompt (L"second"));

        taken = prompts.Take();

        Assert::AreEqual ((size_t) 2,                    taken.size());
        Assert::AreEqual (std::wstring (L"first"),       taken[0].prompt.message);
        Assert::IsTrue   (taken[0].kind == Kind::Question);
        Assert::AreEqual (1,                             taken[1].drive);
        Assert::IsTrue   (prompts.IsEmpty());
        Assert::IsTrue   (prompts.Take().empty());
    }



    TEST_METHOD (ALaterPromptOfTheSameKindForTheSameBayReplacesTheEarlierOne)
    {
        DeferredChangePrompts                       prompts;
        std::vector<DeferredChangePrompts::Entry>   taken;



        prompts.Defer (Kind::Report,   6, 0, MakePrompt (L"old report"));
        prompts.Defer (Kind::Question, 6, 0, MakePrompt (L"question"));
        prompts.Defer (Kind::Report,   5, 0, MakePrompt (L"other slot"));
        prompts.Defer (Kind::Report,   6, 0, MakePrompt (L"new report"));

        taken = prompts.Take();

        Assert::AreEqual ((size_t) 3,                    taken.size());
        Assert::AreEqual (std::wstring (L"question"),    taken[0].prompt.message);
        Assert::AreEqual (std::wstring (L"other slot"),  taken[1].prompt.message);
        Assert::AreEqual (std::wstring (L"new report"),  taken[2].prompt.message);
    }
};
