#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/ExternalChangePolicy.h"
#include "Devices/Disk/ChangePrompt.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ExternalChangePolicyTests
//
//  The whole decision surface, swept in both directions.
//
//  BOTH DIRECTIONS IS THE POINT AND NOT A FLOURISH. Sweeping the inputs and
//  checking each outcome proves every row present is right; sweeping the
//  OUTCOMES and checking which inputs produce them is what catches a row that
//  is missing. A table swept one way hides exactly the case nobody wrote.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ExternalChangePolicyTests)
{
public:

    using Situation = ExternalChangePolicy::Situation;

    static Situation  Seen()
    {
        Situation  situation;

        situation.changeSeen = true;

        return situation;
    }



    static const wchar_t *  NameOf (ChangeAction action)
    {
        switch (action)
        {
        case ChangeAction::Ignore:        return L"Ignore";
        case ChangeAction::TakeUpInPlace: return L"TakeUpInPlace";
        case ChangeAction::Restart:       return L"Restart";
        case ChangeAction::Ask:           return L"Ask";
        case ChangeAction::Conflict:      return L"Conflict";
        case ChangeAction::Unusable:      return L"Unusable";
        case ChangeAction::Defer:         return L"Defer";
        case ChangeAction::KeepHeld:      return L"KeepHeld";
        case ChangeAction::PreserveCopy:  return L"PreserveCopy";
        }

        return L"(unnamed)";
    }



    static void  AssertDecides (const Situation & situation, ChangeAction expected)
    {
        ChangeAction  actual = ExternalChangePolicy::Decide (situation);

        Assert::IsTrue (actual == expected,
                        (std::wstring (L"expected ") + NameOf (expected)
                                                     + L", got " + NameOf (actual)).c_str());
    }



    TEST_METHOD (NoChangeSeenDecidesNothing)
    {
        Situation  situation;



        //  Every other field is at its default and none of them matter: with
        //  nothing seen there is no question to answer.
        AssertDecides (situation, ChangeAction::Ignore);
    }



    TEST_METHOD (EveryCombinationOfIntentAndFallbackOnACleanImage)
    {
        //  The sweep in the forward direction: three intents by three
        //  fallbacks, all nine rows named.
        struct Row
        {
            PickUpIntent    intent;
            FallbackAnswer  fallback;
            ChangeAction    expected;
        };

        const Row  rows[] =
        {
            //  A stated intent is obeyed whatever the fallback says: the
            //  writer knew what they changed, and the fallback is for writers
            //  who could not say.
            { PickUpIntent::TakeUpInPlace, FallbackAnswer::Ask,           ChangeAction::TakeUpInPlace },
            { PickUpIntent::TakeUpInPlace, FallbackAnswer::TakeUpInPlace, ChangeAction::TakeUpInPlace },
            { PickUpIntent::TakeUpInPlace, FallbackAnswer::Restart,       ChangeAction::TakeUpInPlace },

            { PickUpIntent::Restart,       FallbackAnswer::Ask,           ChangeAction::Restart },
            { PickUpIntent::Restart,       FallbackAnswer::TakeUpInPlace, ChangeAction::Restart },
            { PickUpIntent::Restart,       FallbackAnswer::Restart,       ChangeAction::Restart },

            //  Nothing stated: the user's declared answer decides, and its
            //  default is to ask.
            { PickUpIntent::Unstated,      FallbackAnswer::Ask,           ChangeAction::Ask },
            { PickUpIntent::Unstated,      FallbackAnswer::TakeUpInPlace, ChangeAction::TakeUpInPlace },
            { PickUpIntent::Unstated,      FallbackAnswer::Restart,       ChangeAction::Restart },
        };



        for (const Row & row : rows)
        {
            Situation  situation = Seen();

            situation.intent   = row.intent;
            situation.fallback = row.fallback;

            AssertDecides (situation, row.expected);
        }
    }



    TEST_METHOD (AConflictOutranksEveryStatedIntentAndEveryFallback)
    {
        //  The rule the whole feature exists for: an intent says how the guest
        //  carries on, and never that work may be discarded. If this row ever
        //  moves below the intent test, `--on-change reload` silently throws
        //  away the guest's unsaved writes.
        const PickUpIntent    intents[]   = { PickUpIntent::Unstated,
                                              PickUpIntent::TakeUpInPlace,
                                              PickUpIntent::Restart };
        const FallbackAnswer  fallbacks[] = { FallbackAnswer::Ask,
                                              FallbackAnswer::TakeUpInPlace,
                                              FallbackAnswer::Restart };



        for (PickUpIntent intent : intents)
        {
            for (FallbackAnswer fallback : fallbacks)
            {
                Situation  situation = Seen();

                situation.guestDirty = true;
                situation.intent     = intent;
                situation.fallback   = fallback;

                AssertDecides (situation, ChangeAction::Conflict);
            }
        }
    }



    TEST_METHOD (UnusableOutranksTheConflictAndEverythingUnderIt)
    {
        const PickUpIntent  intents[] = { PickUpIntent::Unstated,
                                          PickUpIntent::TakeUpInPlace,
                                          PickUpIntent::Restart };



        for (PickUpIntent intent : intents)
        {
            Situation  situation = Seen();

            situation.usable     = false;
            situation.guestDirty = true;
            situation.intent     = intent;
            situation.fallback   = FallbackAnswer::TakeUpInPlace;

            //  There is nothing to take up, so what the guest has written is
            //  not yet the question.
            AssertDecides (situation, ChangeAction::Unusable);
        }
    }



    TEST_METHOD (AFileSomebodyElseHoldsOutranksEverything)
    {
        Situation  situation = Seen();



        situation.heldByOther = true;
        situation.usable      = false;
        situation.guestDirty  = true;
        situation.intent      = PickUpIntent::Restart;
        situation.fallback    = FallbackAnswer::Restart;

        //  Acting on a file still being written would read a half-written
        //  disk, and everything below this test is a judgement about contents
        //  that are not final yet.
        AssertDecides (situation, ChangeAction::Defer);
    }



    TEST_METHOD (EveryOutcomeThePolicyCanReachIsReachedByAKnownSituation)
    {
        //  The reverse sweep. For each outcome the policy is capable of
        //  producing, name the situation that produces it -- which is what
        //  catches a row nobody wrote, as opposed to a row written wrongly.
        struct Reach
        {
            ChangeAction  outcome;
            Situation     situation;
        };

        std::vector<Reach>  reaches;
        Situation           situation;



        reaches.push_back (Reach { ChangeAction::Ignore, Situation() });

        situation            = Seen();
        situation.heldByOther = true;
        reaches.push_back (Reach { ChangeAction::Defer, situation });

        situation        = Seen();
        situation.usable = false;
        reaches.push_back (Reach { ChangeAction::Unusable, situation });

        situation            = Seen();
        situation.guestDirty = true;
        reaches.push_back (Reach { ChangeAction::Conflict, situation });

        situation        = Seen();
        situation.intent = PickUpIntent::TakeUpInPlace;
        reaches.push_back (Reach { ChangeAction::TakeUpInPlace, situation });

        situation        = Seen();
        situation.intent = PickUpIntent::Restart;
        reaches.push_back (Reach { ChangeAction::Restart, situation });

        situation = Seen();
        reaches.push_back (Reach { ChangeAction::Ask, situation });

        for (const Reach & reach : reaches)
        {
            AssertDecides (reach.situation, reach.outcome);
        }

        //  KeepHeld and PreserveCopy are answers a person gives, never
        //  outcomes the policy reaches on its own. Asserted so that making
        //  either one reachable from a rule has to be a deliberate edit here.
        for (const Reach & reach : reaches)
        {
            Assert::IsTrue (reach.outcome != ChangeAction::KeepHeld
                         && reach.outcome != ChangeAction::PreserveCopy,
                            L"these are answers, not decisions");
        }
    }



    TEST_METHOD (OnlyTheThreeQuestionsNeedAnAnswer)
    {
        Assert::IsTrue  (ExternalChangePolicy::NeedsAnAnswer (ChangeAction::Ask));
        Assert::IsTrue  (ExternalChangePolicy::NeedsAnAnswer (ChangeAction::Conflict));
        Assert::IsTrue  (ExternalChangePolicy::NeedsAnAnswer (ChangeAction::Unusable));

        Assert::IsFalse (ExternalChangePolicy::NeedsAnAnswer (ChangeAction::Ignore));
        Assert::IsFalse (ExternalChangePolicy::NeedsAnAnswer (ChangeAction::TakeUpInPlace));
        Assert::IsFalse (ExternalChangePolicy::NeedsAnAnswer (ChangeAction::Restart));
        Assert::IsFalse (ExternalChangePolicy::NeedsAnAnswer (ChangeAction::Defer));
    }



    TEST_METHOD (TheStoredAnswerRoundTripsAndAnUnknownOneAsks)
    {
        const FallbackAnswer  answers[] = { FallbackAnswer::Ask,
                                            FallbackAnswer::TakeUpInPlace,
                                            FallbackAnswer::Restart };



        for (FallbackAnswer answer : answers)
        {
            std::string  spelled = ExternalChangePolicy::SpellFallbackAnswer (answer);

            Assert::IsTrue (ExternalChangePolicy::ParseFallbackAnswer (spelled) == answer,
                            L"an answer written down and read back is the same answer");
        }

        //  A file edited by hand, or written by a later version, must not
        //  silently pick the answer that discards the most.
        Assert::IsTrue (ExternalChangePolicy::ParseFallbackAnswer ("")         == FallbackAnswer::Ask);
        Assert::IsTrue (ExternalChangePolicy::ParseFallbackAnswer ("RELOAD")   == FallbackAnswer::Ask);
        Assert::IsTrue (ExternalChangePolicy::ParseFallbackAnswer ("whatever") == FallbackAnswer::Ask);
    }



    TEST_METHOD (EveryQuestionNamesTheImageItIsAbout)
    {
        const ChangeAction  questions[] = { ChangeAction::Ask,
                                            ChangeAction::Conflict,
                                            ChangeAction::Unusable };



        for (ChangeAction question : questions)
        {
            ChangePrompt  prompt = ChangePrompt::Compose ("C:\\work\\Loader.dsk", question);

            Assert::IsTrue (prompt.IsAsked(), L"a question has answers");
            Assert::IsTrue (prompt.message.find (L"Loader.dsk") != std::wstring::npos,
                            L"a user with several disks mounted cannot act on a "
                            L"question that does not say which");
            Assert::IsFalse (prompt.title.empty());
        }
    }



    TEST_METHOD (AnActionThatIsNotAQuestionComposesNothing)
    {
        const ChangeAction  notQuestions[] = { ChangeAction::Ignore,
                                               ChangeAction::TakeUpInPlace,
                                               ChangeAction::Restart,
                                               ChangeAction::Defer,
                                               ChangeAction::KeepHeld };



        for (ChangeAction action : notQuestions)
        {
            ChangePrompt  prompt = ChangePrompt::Compose ("Work.dsk", action);

            Assert::IsFalse (prompt.IsAsked(), L"nothing to ask, so no blank dialog");
        }
    }



    TEST_METHOD (ThePickUpReportCarriesTheRestartWhenTheMachineKeptRunning)
    {
        ChangePrompt  running = ChangePrompt::ComposePickUpReport ("C:\\work\\Game.dsk", false);
        ChangePrompt  restarted = ChangePrompt::ComposePickUpReport ("C:\\work\\Game.dsk", true);



        Assert::AreEqual ((size_t) 1, running.answers.size(),
                          L"the swap may have been the wrong call, and the recovery "
                          L"is the action the user was not offered");
        Assert::IsTrue (running.answers[0].action == ChangeAction::Restart);
        Assert::IsTrue (running.message.find (L"Game.dsk") != std::wstring::npos);

        Assert::AreEqual ((size_t) 0, restarted.answers.size(),
                          L"a machine that has just restarted has nothing to offer");
    }



    TEST_METHOD (AnUnnamedImageStillProducesAReadableQuestion)
    {
        ChangePrompt  prompt = ChangePrompt::Compose ("", ChangeAction::Ask);



        Assert::IsTrue (prompt.IsAsked());
        Assert::IsFalse (prompt.message.empty(), L"a question with no text is no question");
    }


    TEST_METHOD (TheOfferedRowsAndTheStoredAnswersAgreeInBothDirections)
    {
        const FallbackAnswer  answers[] = { FallbackAnswer::Ask,
                                            FallbackAnswer::TakeUpInPlace,
                                            FallbackAnswer::Restart };
        int                   index     = 0;



        //  The list a settings control offers is ordered here rather than in
        //  the page, so a reordering is one edit and cannot silently change
        //  what an existing stored preference means.
        for (FallbackAnswer answer : answers)
        {
            index = ExternalChangePolicy::IndexOfFallbackAnswer (answer);

            Assert::IsTrue (index >= 0 && index < ExternalChangePolicy::kFallbackAnswerCount);
            Assert::IsTrue (ExternalChangePolicy::FallbackAnswerAtIndex (index) == answer,
                            L"a row selected and read back is the same answer");
        }

        //  Asking is row zero: a control that cannot resolve its selection
        //  lands on the answer that acts on nothing.
        Assert::AreEqual (0, ExternalChangePolicy::IndexOfFallbackAnswer (FallbackAnswer::Ask));
        Assert::IsTrue (ExternalChangePolicy::FallbackAnswerAtIndex (-1) == FallbackAnswer::Ask);
        Assert::IsTrue (ExternalChangePolicy::FallbackAnswerAtIndex (99) == FallbackAnswer::Ask);
    }
};
