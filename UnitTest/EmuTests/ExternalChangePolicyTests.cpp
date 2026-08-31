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



    TEST_METHOD (EveryIntentOnACleanImage)
    {
        struct Row
        {
            PickUpIntent  intent;
            ChangeAction  expected;
        };

        //  Three intents, three rows, no second axis. The stored fallback that
        //  used to multiply this by three is gone: a writer that can speak says
        //  what it meant, and one that cannot leaves the question to a person.
        const Row  rows[] =
        {
            { PickUpIntent::TakeUpInPlace, ChangeAction::TakeUpInPlace },
            { PickUpIntent::Restart,       ChangeAction::Restart },
            { PickUpIntent::Unstated,      ChangeAction::Ask },
        };



        for (const Row & row : rows)
        {
            Situation  situation = Seen();

            situation.intent = row.intent;

            AssertDecides (situation, row.expected);
        }
    }



    TEST_METHOD (AConflictOutranksEveryStatedIntent)
    {
        //  The rule the whole feature exists for: an intent says how the guest
        //  carries on, and never that work may be discarded. If this row ever
        //  moves below the intent test, `--on-change reload` silently throws
        //  away the guest's unsaved writes.
        const PickUpIntent  intents[] = { PickUpIntent::Unstated,
                                          PickUpIntent::TakeUpInPlace,
                                          PickUpIntent::Restart };



        for (PickUpIntent intent : intents)
        {
            Situation  situation = Seen();

            situation.guestDirty = true;
            situation.intent     = intent;

            AssertDecides (situation, ChangeAction::Conflict);
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

        situation             = Seen();
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



    TEST_METHOD (EveryMessageNamesTheImageAndTheDrive)
    {
        const ChangeAction  questions[] = { ChangeAction::Ask,
                                            ChangeAction::Conflict,
                                            ChangeAction::Unusable };



        for (ChangeAction question : questions)
        {
            ChangePrompt  prompt = ChangePrompt::Compose ("C:\\work\\Loader.dsk", 1, question);

            Assert::IsTrue (prompt.IsAsked(), L"a question has answers");
            Assert::IsTrue (prompt.message.find (L"Loader.dsk") != std::wstring::npos,
                            L"a user with two disks mounted cannot act on a message "
                            L"that does not say which");
            Assert::IsTrue (prompt.message.find (L"Drive 2") != std::wstring::npos,
                            L"the drive is written as the number on the machine, so a "
                            L"zero-based bay index reads as Drive 2");
            Assert::IsFalse (prompt.title.empty());
        }
    }



    TEST_METHOD (TheAskDialogOffersAcceptingOrIgnoringAndNothingElse)
    {
        ChangePrompt  prompt = ChangePrompt::Compose ("C:\\work\\Loader.dsk", 0,
                                                      ChangeAction::Ask);



        Assert::AreEqual ((size_t) 2, prompt.answers.size(),
                          L"two answers: take the changes, or leave them alone");

        Assert::IsTrue (prompt.answers[0].action == ChangeAction::TakeUpInPlace);
        Assert::IsTrue (prompt.answers[1].action == ChangeAction::KeepHeld);

        //  The reboot is not an answer here. It is a thing the user may do
        //  afterwards, from the toolbar, which is why the message says why
        //  they might want to rather than offering a third button.
        for (const PromptAnswer & answer : prompt.answers)
        {
            Assert::IsTrue (answer.action != ChangeAction::Restart,
                            L"the toolbar carries the reboot");
        }
    }



    TEST_METHOD (EveryMessageAboutARunningMachineExplainsWhyARebootMayBeNeeded)
    {
        ChangePrompt  asked   = ChangePrompt::Compose ("Loader.dsk", 0, ChangeAction::Ask);
        ChangePrompt  running = ChangePrompt::ComposePickUpReport ("Loader.dsk", 0, false);
        ChangePrompt  rebooted = ChangePrompt::ComposePickUpReport ("Loader.dsk", 0, true);
        std::wstring  warning = ChangePrompt::StaleDirectoryWarning();



        //  The hazard is the same whether the user was asked or merely told,
        //  so the sentence is the same and comes from one place.
        Assert::IsTrue (asked.message.find (warning)   != std::wstring::npos);
        Assert::IsTrue (running.message.find (warning) != std::wstring::npos);

        //  A machine that has just rebooted has already done the thing the
        //  warning advises, so repeating it would be noise.
        Assert::IsTrue (rebooted.message.find (warning) == std::wstring::npos);
    }



    TEST_METHOD (ThePickUpReportCarriesOnlyItsOwnDismissal)
    {
        ChangePrompt  running  = ChangePrompt::ComposePickUpReport ("Game.dsk", 0, false);
        ChangePrompt  rebooted = ChangePrompt::ComposePickUpReport ("Game.dsk", 0, true);



        //  A notice that stands until closed needs something to close it, and
        //  that is the only action it gets: the reboot lives on the toolbar,
        //  and a duplicate button is one more thing to dismiss.
        Assert::AreEqual ((size_t) 1, running.answers.size());
        Assert::AreEqual ((size_t) 1, rebooted.answers.size());

        Assert::IsTrue (running.answers[0].action  == ChangeAction::Ignore);
        Assert::IsTrue (rebooted.answers[0].action == ChangeAction::Ignore);
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
            ChangePrompt  prompt = ChangePrompt::Compose ("Work.dsk", 0, action);

            Assert::IsFalse (prompt.IsAsked(), L"nothing to ask, so no blank dialog");
        }
    }



    TEST_METHOD (AnUnnamedImageStillProducesAReadableMessage)
    {
        ChangePrompt  prompt = ChangePrompt::Compose ("", 0, ChangeAction::Ask);



        Assert::IsTrue (prompt.IsAsked());
        Assert::IsFalse (prompt.message.empty(), L"a question with no text is no question");
        Assert::IsTrue (prompt.message.find (L"Drive 1") != std::wstring::npos,
                        L"the drive is still named even when the file cannot be");
    }
};
