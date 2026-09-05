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



    TEST_METHOD (EveryMessageGivesTheFileAndTheDrive)
    {
        std::vector<ChangePrompt>  prompts;



        prompts.push_back (ChangePrompt::Compose ("C:\\work\\Loader.dsk", 1, ChangeAction::Ask,
                                                  "C:\\work\\Loader.20260830-014233-01.dsk"));
        prompts.push_back (ChangePrompt::Compose ("C:\\work\\Loader.dsk", 1, ChangeAction::Unusable));
        prompts.push_back (ChangePrompt::Compose ("C:\\work\\Loader.dsk", 1, ChangeAction::Deleted));
        prompts.push_back (ChangePrompt::ComposePickUpReport ("C:\\work\\Loader.dsk", 1, false,
                                                              "Apple //e"));
        prompts.push_back (ChangePrompt::ComposeConflictReport ("C:\\work\\Loader.dsk", 1,
                                                                "C:\\work\\Loader.x.dsk"));
        prompts.push_back (ChangePrompt::ComposeSaveFailure ("C:\\work\\Loader.dsk", 1,
                                                             "C:\\work\\Loader.x.dsk",
                                                             E_ACCESSDENIED,
                                                             SaveFailureCause::ExternalChange));

        for (const ChangePrompt & prompt : prompts)
        {
            std::wstring  whole = prompt.title + L" " + prompt.message;

            Assert::IsTrue (prompt.IsAsked(), L"every one of these is acted on or dismissed");
            Assert::IsTrue (whole.find (L"Loader.dsk") != std::wstring::npos,
                            L"a user with two disks mounted cannot act on a message "
                            L"that does not give which");
            Assert::IsTrue (whole.find (L"Drive 2") != std::wstring::npos,
                            L"the drive is written as the number on the machine, so a "
                            L"zero-based bay index reads as Drive 2");
            Assert::IsFalse (prompt.title.empty());
        }
    }



    //  A TITLE IS A CONDITION AND A BODY IS AN INSTANCE. The title used to
    //  repeat the body's first line almost word for word, which spent a line
    //  saying nothing twice.
    TEST_METHOD (TitlesDescribeTheConditionRatherThanTheFile)
    {
        const ChangePrompt  prompts[] = {
            ChangePrompt::Compose ("C:\\work\\Loader.dsk", 0, ChangeAction::Ask),
            ChangePrompt::Compose ("C:\\work\\Loader.dsk", 0, ChangeAction::Deleted),
            ChangePrompt::Compose ("C:\\work\\Loader.dsk", 0, ChangeAction::Unusable),
            ChangePrompt::ComposeSaveFailure ("C:\\work\\Loader.dsk", 0, "C:\\x.dsk",
                                              E_FAIL, SaveFailureCause::FileLost),
        };



        for (const ChangePrompt & prompt : prompts)
        {
            Assert::IsTrue (prompt.title.find (L"Loader") == std::wstring::npos,
                            (L"the title carries an instance: " + prompt.title).c_str());
            Assert::IsTrue (prompt.title.find (L".dsk") == std::wstring::npos,
                            (L"the title carries a filename: " + prompt.title).c_str());
        }
    }



    TEST_METHOD (TheGoneAndUnreadableTitlesAreNotInterchangeable)
    {
        ChangePrompt  deleted    = ChangePrompt::Compose ("Work.dsk", 0, ChangeAction::Deleted);
        ChangePrompt  unreadable = ChangePrompt::Compose ("Work.dsk", 0, ChangeAction::Unusable);



        //  A user whose file was deleted needs to be told it is gone; one whose
        //  share dropped needs to be told it cannot be read.
        Assert::IsTrue (deleted.title.find (L"removed") != std::wstring::npos);
        Assert::IsTrue (unreadable.title.find (L"read") != std::wstring::npos);
        Assert::IsTrue (deleted.title != unreadable.title);

        //  Both offer the same two things, because both end the same way.
        Assert::AreEqual ((size_t) 2, deleted.answers.size());
        Assert::AreEqual ((size_t) 2, unreadable.answers.size());
        Assert::IsTrue (deleted.answers[0].action == ChangeAction::PreserveCopy);
        Assert::IsTrue (deleted.answers[1].action == ChangeAction::KeepHeld);

        //  The path leads on its own line, then what the screen will not show
        //  for itself: the disk is still in memory and can be saved or
        //  discarded. Nothing explains Discard; people know what it means.
        for (const ChangePrompt * p : { &deleted, &unreadable })
        {
            size_t  gap = p->message.find (L"\n\n");

            Assert::IsTrue (gap != std::wstring::npos && gap > 0,
                            L"the path is the first line, set off by a blank one");
            Assert::IsTrue (p->message.find (L"still has the disk's contents in memory")
                                != std::wstring::npos);
            Assert::IsTrue (p->message.find (L"save it to a new file or discard it")
                                != std::wstring::npos);
            Assert::IsTrue (p->message.find (L"the disk is gone") == std::wstring::npos,
                            L"Discard is not glossed");
        }
    }



    TEST_METHOD (TheConflictReportGivesWhereTheGuestsVersionWent)
    {
        ChangePrompt  report = ChangePrompt::ComposeConflictReport (
                                   "C:\\work\\Loader.dsk", 0,
                                   "C:\\work\\Loader.20260830-014233-01.dsk");



        //  "There was a conflict" helps nobody. The file holding the other
        //  version is the thing the user can act on.
        Assert::IsTrue (report.message.find (L"Loader.20260830-014233-01.dsk")
                            != std::wstring::npos);

        //  The folder is not repeated into the sentence; the file is enough,
        //  and the path buries it.
        Assert::IsTrue (report.message.find (L"C:\\work") == std::wstring::npos);

        //  It is a report: one action, and that action is dismissal.
        Assert::AreEqual ((size_t) 1, report.answers.size());
        Assert::IsTrue (report.answers[0].action == ChangeAction::Ignore);
    }



    //  THE FAILURE IS THE ONE MESSAGE THAT PRINTS A WHOLE PATH. Where it tried
    //  to write is the actionable part when the reason is permissions, and the
    //  folder is what the user has to go and look at.
    TEST_METHOD (ASaveFailureGivesThePathTheCodeAndAWayOut)
    {
        ChangePrompt  prompt = ChangePrompt::ComposeSaveFailure (
                                   "C:\\work\\Loader.dsk", 0,
                                   "C:\\work\\Loader.20260830-014233-01.dsk",
                                   E_ACCESSDENIED,
                                   SaveFailureCause::ExternalChange);



        Assert::IsTrue (prompt.message.find (L"C:\\work\\Loader.20260830-014233-01.dsk")
                            != std::wstring::npos,
                        L"the whole path it tried");

        Assert::IsTrue (prompt.message.find (L"0x80070005") != std::wstring::npos,
                        L"and the code, which is the part that can be searched for");

        //  The user's question at that moment is whether they have lost
        //  anything, and the answer is no -- precisely because the copy could
        //  not be written.
        Assert::IsTrue (prompt.message.find (L"still in Drive 1") != std::wstring::npos);

        //  Somewhere else to put it beats advice about freeing space.
        Assert::AreEqual ((size_t) 2, prompt.answers.size());
        Assert::IsTrue (prompt.answers[0].action == ChangeAction::PreserveCopy);
        Assert::IsTrue (prompt.answers[1].action == ChangeAction::Ignore);
    }



    //  THE TWO CAUSES USED TO SHARE ONE MESSAGE, so a file that had been
    //  deleted was reported with "another program modified it" -- which is not
    //  what happened, and sends the reader looking for a program that does not
    //  exist.
    TEST_METHOD (ASaveFailureAfterALostFileDoesNotBlameAnotherProgram)
    {
        ChangePrompt  lost = ChangePrompt::ComposeSaveFailure ("Loader.dsk", 0, "x.dsk",
                                                               E_FAIL,
                                                               SaveFailureCause::FileLost);
        ChangePrompt  changed = ChangePrompt::ComposeSaveFailure ("Loader.dsk", 0, "x.dsk",
                                                                  E_FAIL,
                                                                  SaveFailureCause::ExternalChange);



        Assert::IsTrue (lost.message.find (L"Another program") == std::wstring::npos,
                        L"nothing modified a file that is gone");
        Assert::IsTrue (changed.message.find (L"Another program") != std::wstring::npos);
        Assert::IsTrue (lost.message != changed.message);
    }



    TEST_METHOD (TheQuestionOffersInsertingOrKeepingAndNothingElse)
    {
        ChangePrompt  prompt = ChangePrompt::Compose ("C:\\work\\Loader.dsk", 0,
                                                      ChangeAction::Ask,
                                                      "C:\\work\\Loader.x.dsk");



        Assert::AreEqual ((size_t) 2, prompt.answers.size(),
                          L"two answers: put the modified disk in, or keep the one in there");

        Assert::IsTrue (prompt.answers[0].action == ChangeAction::TakeUpInPlace);
        Assert::IsTrue (prompt.answers[1].action == ChangeAction::KeepHeld);

        //  THE LABELS STATE THE ACTION. They were "Accept the changes" and
        //  "Ignore the changes", which described the external edit twice and
        //  the disk in the drive not at all -- and "ignore" reads as throwing
        //  the new file away, which is the one thing it does not do.
        Assert::IsTrue (prompt.answers[0].label.find (L"Insert") != std::wstring::npos);
        Assert::IsTrue (prompt.answers[1].label.find (L"Keep") != std::wstring::npos);

        //  AND THEY DO NOT CARRY THE FILENAME. The insert button used to read
        //  "Insert the modified <file>", which set the dialog's width from the
        //  length of a name the body has already given on a line of its own.
        for (const PromptAnswer & answer : prompt.answers)
        {
            Assert::IsTrue (answer.label.find (L".dsk") == std::wstring::npos,
                            (L"a button carries a filename: " + answer.label).c_str());
        }

        //  The reboot is not an answer here. It is something the user may do
        //  afterwards, from the toolbar.
        for (const PromptAnswer & answer : prompt.answers)
        {
            Assert::IsTrue (answer.action != ChangeAction::Restart,
                            L"the toolbar carries the reboot");
        }
    }



    //  SIX PRINTINGS OF ONE NAME. Every sentence used to carry the file and the
    //  drive, so a dialog about "mockingboard-speech-demo-dhgr.dsk" said it in
    //  the first line, in both offers, in the rename, and on a button, and the
    //  sentences around it could not be read. The name and the drive now lead
    //  on a line of their own, drive in parentheses after the file, and the
    //  prose says "this disk" and "it".
    TEST_METHOD (TheQuestionNamesTheFileOnceAndTheDriveOnce)
    {
        ChangePrompt  prompt = ChangePrompt::Compose ("C:\\work\\Loader.dsk", 0,
                                                      ChangeAction::Ask,
                                                      "C:\\work\\Loader.20260830-014233-01.dsk");
        std::wstring  whole  = prompt.message;
        size_t        files  = 0;
        size_t        drives = 0;
        size_t        at     = 0;



        for (at = whole.find (L"Loader.dsk"); at != std::wstring::npos;
             at = whole.find (L"Loader.dsk", at + 1))
        {
            files++;
        }

        for (at = whole.find (L"Drive 1"); at != std::wstring::npos;
             at = whole.find (L"Drive 1", at + 1))
        {
            drives++;
        }

        Assert::AreEqual ((size_t) 1, files,  L"the file is named once");
        Assert::AreEqual ((size_t) 1, drives, L"the drive is named once");

        //  Together, on their own line, where the eye finds them without
        //  reading a sentence.
        Assert::IsTrue (whole.find (L"\n\nLoader.dsk (Drive 1)") != std::wstring::npos,
                        (L"the identifying line is missing: " + whole).c_str());

        //  The copy is the one other name here, and it appears once.
        Assert::IsTrue (whole.find (L"Loader.20260830-014233-01.dsk") != std::wstring::npos);
    }



    //  KEEPING IS A SAVE-AS AND THE MESSAGE HAS TO SAY WHICH TENSE. A conflict
    //  writes the copy before anything is put to the user, so by then it
    //  already exists; with no conflict there is nothing on disk yet.
    TEST_METHOD (TheQuestionSaysWhetherTheCopyExistsYet)
    {
        ChangePrompt  before = ChangePrompt::Compose ("Loader.dsk", 0, ChangeAction::Ask,
                                                      "Loader.20260830-014233-01.dsk", false);
        ChangePrompt  after  = ChangePrompt::Compose ("Loader.dsk", 0, ChangeAction::Ask,
                                                      "Loader.20260830-014233-01.dsk", true);



        Assert::IsTrue (before.message.find (L"We'll rename it") != std::wstring::npos);
        Assert::IsTrue (after.message.find  (L"already saved as") != std::wstring::npos);

        //  And the one that already happened says so up front, because the
        //  reader's first question is whether their work survived.
        Assert::IsTrue (after.message.find (L"hadn't been saved yet") != std::wstring::npos);
        Assert::IsTrue (before.message.find (L"hadn't been saved yet") == std::wstring::npos);
    }



    //  "The Apple" is not what is in front of them.
    TEST_METHOD (TheRebootNoticeGivesTheMachineTheUserHas)
    {
        ChangePrompt  enhanced = ChangePrompt::ComposePickUpReport ("Game.dsk", 0, true,
                                                                    "Apple //e Enhanced");
        ChangePrompt  unnamed  = ChangePrompt::ComposePickUpReport ("Game.dsk", 0, true, "");



        Assert::IsTrue (enhanced.message.find (L"Apple //e Enhanced") != std::wstring::npos);

        //  A machine with no name still produces a sentence.
        Assert::IsTrue (unnamed.message.find (L"the machine") != std::wstring::npos);
    }



    //  THE CAVEAT IS GONE. It explained that a running program may misread a
    //  swapped disk, and read as alarming for something working exactly as
    //  asked. The audience for this feature knows what a swapped disk does.
    TEST_METHOD (ThePickUpNoticeDoesNotLectureAboutRebooting)
    {
        ChangePrompt  running = ChangePrompt::ComposePickUpReport ("Game.dsk", 0, false,
                                                                   "Apple //e");



        Assert::IsTrue (running.message.find (L"misbehav") == std::wstring::npos);
        Assert::IsTrue (running.message.find (L"directory") == std::wstring::npos);
        Assert::IsTrue (running.message.find (L"CassoCli") != std::wstring::npos,
                        L"this notice is unreachable except through the message channel, "
                        L"so it can attribute the write");
    }



    //  ONLY THE NOTICE ABOUT SOMETHING THE USER ASKED FOR CLOSES ITSELF. They
    //  configured the pick-up with a switch, so making them dismiss a notice
    //  about it charges them twice for the same decision. The others report
    //  something they did not ask for and stand until read.
    TEST_METHOD (OnlyThePickUpNoticeClosesItself)
    {
        ChangePrompt  pickUp   = ChangePrompt::ComposePickUpReport ("Game.dsk", 0, false,
                                                                    "Apple //e");
        ChangePrompt  conflict = ChangePrompt::ComposeConflictReport ("Game.dsk", 0, "Game.x.dsk");
        ChangePrompt  failed   = ChangePrompt::ComposeSaveFailure ("Game.dsk", 0, "Game.x.dsk",
                                                                   E_FAIL,
                                                                   SaveFailureCause::ExternalChange);
        ChangePrompt  asked    = ChangePrompt::Compose ("Game.dsk", 0, ChangeAction::Ask);



        Assert::IsTrue (pickUp.selfDismisses);

        Assert::IsFalse (conflict.selfDismisses,
                         L"a copy was written and the user has to learn its name");
        Assert::IsFalse (failed.selfDismisses,
                         L"this is the only trace that the writes are still only in memory");
        Assert::IsFalse (asked.selfDismisses, L"a question cannot answer itself");
    }



    TEST_METHOD (ThePickUpReportCarriesOnlyItsOwnDismissal)
    {
        ChangePrompt  running  = ChangePrompt::ComposePickUpReport ("Game.dsk", 0, false,
                                                                    "Apple //e");
        ChangePrompt  rebooted = ChangePrompt::ComposePickUpReport ("Game.dsk", 0, true,
                                                                    "Apple //e");



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
        //  needs the copy's path this one has no way to supply.
        const ChangeAction  notQuestions[] = { ChangeAction::Ignore,
                                               ChangeAction::TakeUpInPlace,
                                               ChangeAction::Restart,
                                               ChangeAction::Defer,
                                               ChangeAction::Conflict,
                                               ChangeAction::KeepHeld };



        for (ChangeAction action : notQuestions)
        {
            ChangePrompt  prompt = ChangePrompt::Compose ("Work.dsk", 0, action);

            Assert::IsFalse (prompt.IsAsked(), L"nothing to put, so no blank dialog");
        }
    }



    //  THE FILE IS REPRODUCED EXACTLY AS IT IS ON DISK, never re-cased to make
    //  a sentence read better. A message that changes the spelling of the file
    //  sends the user looking for something that is not there.
    TEST_METHOD (EveryMessageSpellsTheFileTheWayDiskDoes)
    {
        std::vector<ChangePrompt>  prompts;



        prompts.push_back (ChangePrompt::Compose ("C:\\work\\loader.dsk", 0, ChangeAction::Ask,
                                                  "C:\\work\\loader.x.dsk"));
        prompts.push_back (ChangePrompt::Compose ("C:\\work\\loader.dsk", 0, ChangeAction::Deleted));
        prompts.push_back (ChangePrompt::Compose ("C:\\work\\loader.dsk", 0, ChangeAction::Unusable));
        prompts.push_back (ChangePrompt::ComposePickUpReport ("C:\\work\\loader.dsk", 0, false,
                                                              "Apple //e"));
        prompts.push_back (ChangePrompt::ComposePickUpReport ("C:\\work\\loader.dsk", 0, true,
                                                              "Apple //e"));
        prompts.push_back (ChangePrompt::ComposeConflictReport ("C:\\work\\loader.dsk", 0,
                                                                "C:\\work\\loader.x.dsk"));
        prompts.push_back (ChangePrompt::ComposeSaveFailure ("C:\\work\\loader.dsk", 0,
                                                             "C:\\work\\loader.x.dsk", E_FAIL,
                                                             SaveFailureCause::ExternalChange));

        for (const ChangePrompt & prompt : prompts)
        {
            Assert::IsFalse (prompt.message.empty());
            Assert::IsTrue (prompt.message.find (L"loader.dsk") != std::wstring::npos,
                            (L"the file was re-cased: " + prompt.message).c_str());
            Assert::IsTrue (prompt.message.find (L"Loader.dsk") == std::wstring::npos);
        }
    }



    //  The whole path appears only where it is the thing to act on: the save
    //  failure (where to look) and the lost file (which file vanished), each on
    //  a line of its own. A question about a disk in a drive names the file
    //  alone.
    TEST_METHOD (TheWholePathAppearsOnlyWhereItIsTheActionablePart)
    {
        ChangePrompt  asked = ChangePrompt::Compose ("C:\\deep\\folder\\loader.dsk", 0,
                                                     ChangeAction::Ask,
                                                     "C:\\deep\\folder\\loader.x.dsk");
        ChangePrompt  failed = ChangePrompt::ComposeSaveFailure (
                                   "C:\\deep\\folder\\loader.dsk", 0,
                                   "C:\\deep\\folder\\loader.x.dsk", E_FAIL,
                                   SaveFailureCause::ExternalChange);
        ChangePrompt  lost   = ChangePrompt::ComposeLostFile ("C:\\deep\\folder\\loader.dsk", 0,
                                                              ChangeAction::Deleted);



        Assert::IsTrue (asked.message.find (L"deep") == std::wstring::npos,
                        L"a folder in the middle of a sentence buries the file");
        Assert::IsTrue (failed.message.find (L"deep") != std::wstring::npos,
                        L"the failure prints where to look");
        Assert::IsTrue (lost.message.rfind (L"C:\\deep\\folder\\loader.dsk", 0) == 0,
                        L"the lost-file notice leads with the whole path, on its own line");
    }

};
