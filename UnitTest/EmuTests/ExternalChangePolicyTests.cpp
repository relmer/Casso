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



    TEST_METHOD (AConflictIsNoLongerAQuestion)
    {
        //  It was one. Both versions survive whatever happens now, so there is
        //  no wrong answer to protect the user from -- only a fact to report.
        Assert::IsFalse (ExternalChangePolicy::NeedsAnAnswer (ChangeAction::Conflict));

        Assert::IsTrue  (ExternalChangePolicy::NeedsAnAnswer (ChangeAction::Ask));
        Assert::IsTrue  (ExternalChangePolicy::NeedsAnAnswer (ChangeAction::Unusable));
        Assert::IsTrue  (ExternalChangePolicy::NeedsAnAnswer (ChangeAction::Deleted));

        Assert::IsFalse (ExternalChangePolicy::NeedsAnAnswer (ChangeAction::Ignore));
        Assert::IsFalse (ExternalChangePolicy::NeedsAnAnswer (ChangeAction::TakeUpInPlace));
        Assert::IsFalse (ExternalChangePolicy::NeedsAnAnswer (ChangeAction::Restart));
        Assert::IsFalse (ExternalChangePolicy::NeedsAnAnswer (ChangeAction::Defer));
    }



    TEST_METHOD (AGoneFileAndAnUnreadableOneAreBothLostAndNothingElseIs)
    {
        Assert::IsTrue  (ExternalChangePolicy::IsFileLost (ChangeAction::Deleted));
        Assert::IsTrue  (ExternalChangePolicy::IsFileLost (ChangeAction::Unusable));

        Assert::IsFalse (ExternalChangePolicy::IsFileLost (ChangeAction::Conflict));
        Assert::IsFalse (ExternalChangePolicy::IsFileLost (ChangeAction::Ask));
        Assert::IsFalse (ExternalChangePolicy::IsFileLost (ChangeAction::TakeUpInPlace));
    }



    TEST_METHOD (EveryMessageNamesTheImageAndTheDrive)
    {
        std::vector<ChangePrompt>  prompts;



        prompts.push_back (ChangePrompt::Compose ("C:\\work\\Loader.dsk", 1, ChangeAction::Ask));
        prompts.push_back (ChangePrompt::Compose ("C:\\work\\Loader.dsk", 1, ChangeAction::Unusable));
        prompts.push_back (ChangePrompt::Compose ("C:\\work\\Loader.dsk", 1, ChangeAction::Deleted));
        prompts.push_back (ChangePrompt::ComposePickUpReport ("C:\\work\\Loader.dsk", 1, false));
        prompts.push_back (ChangePrompt::ComposeConflictReport ("C:\\work\\Loader.dsk", 1,
                                                                "C:\\work\\Loader.x.dsk", false));
        prompts.push_back (ChangePrompt::ComposePreserveFailure ("C:\\work\\Loader.dsk", 1));

        for (const ChangePrompt & prompt : prompts)
        {
            std::wstring  whole = prompt.title + L" " + prompt.message;

            Assert::IsTrue (prompt.IsAsked(), L"every one of these is acted on or dismissed");
            Assert::IsTrue (whole.find (L"Loader.dsk") != std::wstring::npos,
                            L"a user with two disks mounted cannot act on a message "
                            L"that does not say which");
            Assert::IsTrue (whole.find (L"Drive 2") != std::wstring::npos,
                            L"the drive is written as the number on the machine, so a "
                            L"zero-based bay index reads as Drive 2");
            Assert::IsFalse (prompt.title.empty());
        }
    }



    TEST_METHOD (TheGoneAndUnreadableTitlesAreNotInterchangeable)
    {
        ChangePrompt  deleted    = ChangePrompt::Compose ("Work.dsk", 0, ChangeAction::Deleted);
        ChangePrompt  unreadable = ChangePrompt::Compose ("Work.dsk", 0, ChangeAction::Unusable);



        //  A user who deleted the file needs to be told it is deleted; one
        //  whose share dropped needs to be told it cannot be reached.
        Assert::IsTrue (deleted.title.find (L"deleted") != std::wstring::npos);
        Assert::IsTrue (unreadable.title.find (L"no longer accessible") != std::wstring::npos);

        //  Both offer the same two things, because both end the same way.
        Assert::AreEqual ((size_t) 2, deleted.answers.size());
        Assert::AreEqual ((size_t) 2, unreadable.answers.size());
        Assert::IsTrue (deleted.answers[0].action == ChangeAction::PreserveCopy);
        Assert::IsTrue (deleted.answers[1].action == ChangeAction::KeepHeld);
    }



    TEST_METHOD (TheConflictReportNamesWhereTheOtherVersionWent)
    {
        ChangePrompt  keptFile  = ChangePrompt::ComposeConflictReport (
                                      "C:\\work\\Loader.dsk", 0,
                                      "C:\\work\\Loader.20260830-014233.dsk", false);
        ChangePrompt  keptGuest = ChangePrompt::ComposeConflictReport (
                                      "C:\\work\\Loader.dsk", 0,
                                      "C:\\work\\Loader.20260830-014233.dsk", true);



        //  "There was a conflict" helps nobody. The name of the file holding
        //  the other version is the thing the user can act on.
        Assert::IsTrue (keptFile.message.find (L"Loader.20260830-014233.dsk")
                            != std::wstring::npos);
        Assert::IsTrue (keptGuest.message.find (L"Loader.20260830-014233.dsk")
                            != std::wstring::npos);

        //  The two directions read differently, because different things
        //  happened.
        Assert::IsTrue (keptFile.message != keptGuest.message);

        //  It is a report: one action, and that action is dismissal.
        Assert::AreEqual ((size_t) 1, keptFile.answers.size());
        Assert::IsTrue (keptFile.answers[0].action == ChangeAction::Ignore);
    }



    TEST_METHOD (APreserveFailureSaysWhatDidNotHappen)
    {
        ChangePrompt  prompt = ChangePrompt::ComposePreserveFailure ("Loader.dsk", 0);



        //  The user's question at that moment is whether they have lost
        //  anything, and the answer is no -- precisely because the copy could
        //  not be made.
        Assert::IsTrue (prompt.message.find (L"Neither version has been touched")
                            != std::wstring::npos);
        Assert::AreEqual ((size_t) 1, prompt.answers.size());
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
        //  Conflict is here now: it composes through its own report, which
        //  needs the preserved path this one has no way to supply.
        const ChangeAction  notQuestions[] = { ChangeAction::Ignore,
                                               ChangeAction::TakeUpInPlace,
                                               ChangeAction::Restart,
                                               ChangeAction::Defer,
                                               ChangeAction::Conflict,
                                               ChangeAction::KeepHeld };



        for (ChangeAction action : notQuestions)
        {
            ChangePrompt  prompt = ChangePrompt::Compose ("Work.dsk", 0, action);

            Assert::IsFalse (prompt.IsAsked(), L"nothing to ask, so no blank dialog");
        }
    }



    TEST_METHOD (EveryMessageStartsItsSentenceWithACapital)
    {
        std::vector<ChangePrompt>  prompts;



        prompts.push_back (ChangePrompt::Compose ("C:\work\loader.dsk", 0, ChangeAction::Ask));
        prompts.push_back (ChangePrompt::Compose ("C:\work\loader.dsk", 0, ChangeAction::Deleted));
        prompts.push_back (ChangePrompt::Compose ("C:\work\loader.dsk", 0, ChangeAction::Unusable));
        prompts.push_back (ChangePrompt::ComposePickUpReport ("C:\work\loader.dsk", 0, false));
        prompts.push_back (ChangePrompt::ComposePickUpReport ("C:\work\loader.dsk", 0, true));
        prompts.push_back (ChangePrompt::ComposeConflictReport ("C:\work\loader.dsk", 0,
                                                                "C:\work\loader.x.dsk", false));
        prompts.push_back (ChangePrompt::ComposePreserveFailure ("C:\work\loader.dsk", 0));

        //  THE FILENAME IS LOWER CASE ON PURPOSE. A sentence may not open with
        //  it: capitalizing would misspell the file, and leaving it would open
        //  every notice with a small letter. The article carries the capital.
        for (const ChangePrompt & prompt : prompts)
        {
            Assert::IsFalse (prompt.message.empty());
            Assert::IsTrue (iswupper (prompt.message[0]) != 0,
                            (L"sentence starts lower case: " + prompt.message).c_str());

            //  And the name is still spelled the way it is on disk.
            Assert::IsTrue (prompt.message.find (L"loader.dsk") != std::wstring::npos
                         || prompt.title.find   (L"loader.dsk") != std::wstring::npos);
        }
    }



    TEST_METHOD (AnUnnamedImageDoesNotDoubleTheArticle)
    {
        std::wstring  subject = ChangePrompt::SentenceSubject ("", 0);



        //  Building the sentence form by prefixing the mid-sentence one would
        //  read "The disk Drive 1" here.
        Assert::IsTrue (subject == std::wstring (L"The disk in Drive 1"), subject.c_str());
        Assert::IsTrue (ChangePrompt::NameInDrive ("", 0) == std::wstring (L"Drive 1"));
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
