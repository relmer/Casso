#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "CassoLink/FullSend/FullSendBootResults.h"
#include "CassoLink/FullSend/FullSendDisk.h"
#include "CassoLink/FullSend/FullSendLink.h"
#include "CassoLink/FullSend/FullSendManifest.h"
#include "CassoLink/FullSend/FullSendManifestFile.h"
#include "CassoLink/FullSend/FullSendProtection.h"
#include "CassoLink/FullSend/FullSendScreen.h"
#include "CassoLink/FullSend/FullSendSession.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/LinkProgram.h"
#include "CassoLink/Session/CassoLinkDiskTransfer.h"
#include "CassoLink/Session/CassoLinkFullSend.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "FullSendTestDisks.h"
#include "FullSendRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace FullSendTestDisks;
using namespace FullSendRig;





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendTests
//
//  FullSendSession, the session over a scripted Apple: each drive's disk
//  changes at scripted events, and each key in the script ends a pause.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FullSendTests)
{
public:

    TEST_METHOD (Session_NeverOverwritesAFile)
    {
        Rig  rig;

        rig.link.drives[0] = Other (1);
        rig.link.present[1] = false;
        rig.io.files["C:\\Box\\0001.dsk"] = { 1, 2, 3 };

        auto  session = rig.Run();

        Assert::IsTrue (rig.io.files["C:\\Box\\0001.dsk"] == Image ({ 1, 2, 3 }));
        Assert::IsTrue (rig.io.files["C:\\Box\\0001 (2).dsk"] == Other (1));
        Assert::AreEqual (std::string ("0001 (2).dsk"), session->GetManifest().GetRows()[0].file);
    }




    //  Drive order, pauses and ends.

    TEST_METHOD (Session_ReadsDriveOneThenTwo_WritingEachWithNoKey)
    {
        Rig  rig;

        rig.link.drives[0] = Dos ("GAME");
        rig.link.drives[1] = ProDos ("GAMES");

        auto  session = rig.Run();

        Assert::IsTrue (rig.link.diskReads == std::vector<int> ({ 1, 2 }));
        Assert::IsTrue (rig.io.files["C:\\Box\\0001 GAME.dsk"] == rig.link.drives[0]);
        Assert::IsTrue (rig.io.files.contains ("C:\\Box\\0002 GAMES.po"));
        Assert::AreEqual (1, rig.link.waits, L"the pause came only once neither drive had a new disk");
        Assert::AreEqual (2, session->GetScreen().sent);
    }

    TEST_METHOD (Session_AfterADisk_ChecksTheOtherDriveFirst)
    {
        Rig  rig;

        rig.link.drives[0] = Other (1);
        rig.link.drives[1] = Other (2);

        //  Drive 2 was read last, so drive 1 is the other drive at the pause.
        rig.link.beforeKey[1] = [] (ScriptedLink & link) { link.drives[0] = Other (3); link.drives[1] = Other (4); };
        rig.link.keys         = { FullSendKey::Space };

        rig.Run();

        Assert::IsTrue (rig.link.diskReads == std::vector<int> ({ 1, 2, 1, 2 }));
    }

    TEST_METHOD (Session_ASwapDuringTheOtherDrivesTransfer_IsSentWithNoKey)
    {
        Rig  rig;

        rig.link.drives[0]         = Other (1);
        rig.link.drives[1]         = Other (2);
        rig.link.duringDiskRead[2] = [] (ScriptedLink & link) { link.drives[0] = Other (3); };
        rig.link.duringDiskRead[3] = [] (ScriptedLink & link) { link.drives[1] = Other (4); };

        auto  session = rig.Run();

        Assert::IsTrue (rig.link.diskReads == std::vector<int> ({ 1, 2, 1, 2 }));
        Assert::AreEqual (1, rig.link.waits);
        Assert::AreEqual (4, session->GetScreen().sent);
    }

    TEST_METHOD (Session_WithNoNewDisk_PausesWithNoDriveRead_UntilSpace)
    {
        Rig  rig;
        int  tracksAtPause = -1;

        rig.link.drives[0]    = Other (1);
        rig.link.beforeKey[1] = [&] (ScriptedLink & link) { tracksAtPause = (int) link.trackDrives.size(); };

        rig.Run();

        Assert::AreEqual (1, rig.link.waits);
        Assert::AreEqual (tracksAtPause, (int) rig.link.trackDrives.size(), L"no track was read after the pause began");
    }

    TEST_METHOD (Session_Sending_PutsTheOtherDriveOnDeck)
    {
        Rig  rig;

        rig.link.drives[0] = Other (1);

        rig.Run();

        auto  sending = std::find_if (rig.link.screens.begin(), rig.link.screens.end(), [] (const FullSendScreen & screen) { return screen.drives[0] == FullSendDriveState::Sending; });

        Assert::IsTrue (sending != rig.link.screens.end());
        Assert::IsTrue (sending->drives[1] == FullSendDriveState::OnDeck);
        Assert::AreEqual (0, (int) sending->quip, L"the instruction, the first time");
        Assert::AreNotEqual (0, (int) rig.link.screens.back().quip, L"the pause has a prompt line");
    }

    TEST_METHOD (Session_PromptLines_ShowEveryLineBeforeAnyRepeats)
    {
        Rig                                   rig;
        std::vector<int>                      shown;
        const size_t                          count = FullSendScreen::kQuipCount;

        rig.link.keys.assign (count * 4, FullSendKey::Space);

        rig.Run();

        for (const FullSendScreen & screen : rig.link.screens)
        {
            if (screen.quip != 0)
            {
                shown.push_back (screen.quip);
            }
        }

        Assert::IsTrue (shown.size() > count * 3);

        for (size_t start = 0; start + count <= shown.size(); start += count)
        {
            std::set<int>  set (shown.begin() + (ptrdiff_t) start, shown.begin() + (ptrdiff_t) (start + count));

            Assert::AreEqual (count, set.size());
            Assert::AreEqual (1, *set.begin());
            Assert::AreEqual ((int) count, *set.rbegin());
        }

        for (size_t i = 1; i < shown.size(); i++)
        {
            Assert::AreNotEqual (shown[i - 1], shown[i]);
        }
    }

    TEST_METHOD (Screen_PromptLines_FitAfterOnDeck)
    {
        for (int quip = 1; quip <= FullSendScreen::kQuipCount; quip++)
        {
            Assert::IsTrue (strlen (FullSendScreen::GetQuipText ((Byte) quip)) <= 27, TextEncoding::NarrowToWide (FullSendScreen::GetQuipText ((Byte) quip)).c_str());
        }
    }

    TEST_METHOD (Screen_Rows_HaveTheCountsALineAndEachDrive)
    {
        FullSendScreen  screen;

        screen.drives = { FullSendDriveState::Sending, FullSendDriveState::OnDeck };
        screen.names  = { "13 GAMES DISK", "" };
        screen.sent   = 12;

        std::vector<std::string>  rows = screen.GetRows();

        Assert::AreEqual (std::string ("Full Send    Verify: off  Prefix: none"), rows[0]);
        Assert::AreEqual (std::string ("Sent 12   Dup 0   Bad 0"), rows[1]);
        Assert::AreEqual (std::string (40, '-'), rows[2]);
        Assert::AreEqual (std::string ("D1: Sending 13 GAMES DISK"), rows[3]);
        Assert::AreEqual (std::string ("D2: On deck--Insert a disk to send next"), rows[5]);

        screen.quip = 6;
        Assert::AreEqual (std::string ("D2: On deck--Feed me, Seymour!"), screen.GetRows()[5]);
    }

    TEST_METHOD (Session_ScreenName_HasTheImagesNumber_AndTheMapMarksABadTrack)
    {
        Rig  rig;

        rig.link.drives[0]  = Dos ("GAME");
        rig.link.badDisk    = rig.link.drives[0];
        rig.link.badSectors = { { 29, 3 } };
        rig.link.present[1] = false;

        auto  session = rig.Run();

        Assert::AreEqual (std::string ("0001 GAME"), session->GetScreen().names[0]);
        Assert::AreEqual ('X', session->GetScreen().maps[0][29]);
        Assert::AreEqual ('.', session->GetScreen().maps[0][28]);
        Assert::IsTrue (rig.io.files.contains ("C:\\Box\\0001 GAME.dsk"));
    }

    TEST_METHOD (Session_OnDeck_HasTheInstructionFirst_ThenAPromptLine)
    {
        Rig  rig;

        rig.link.drives[0]         = Other (1);
        rig.link.drives[1]         = Other (2);
        rig.link.duringDiskRead[2] = [] (ScriptedLink & link) { link.drives[0] = Other (3); };

        rig.Run();

        std::vector<int>  decks;

        for (const FullSendScreen & screen : rig.link.screens)
        {
            if (screen.drives[1] == FullSendDriveState::OnDeck)
            {
                decks.push_back (screen.quip);
            }
        }

        Assert::AreEqual (0, decks.front(), L"drive 2's first time on deck");
    }

    TEST_METHOD (Session_OneDrive_Sending_HasNoPromptLine)
    {
        Rig  rig;

        rig.link.drives[0]  = Other (1);
        rig.link.present[1] = false;

        rig.Run();

        for (const FullSendScreen & screen : rig.link.screens)
        {
            Assert::IsTrue (screen.drives[0] != FullSendDriveState::Sending || screen.quip == 0);
        }
    }

    TEST_METHOD (Session_OneDrive_LeavesTheMissingDriveOff)
    {
        Rig  rig;

        rig.link.drives[0]  = Other (1);
        rig.link.present[1] = false;

        rig.Run();

        for (const FullSendScreen & screen : rig.link.screens)
        {
            Assert::IsTrue (screen.drives[1] == FullSendDriveState::Off);
        }
    }

    TEST_METHOD (Session_VerifyKey_TogglesVerification_WithTheFlagOnTheScreen)
    {
        Rig  rig;

        rig.link.drives[0]  = Other (1);
        rig.link.present[1] = false;
        rig.link.keys       = { FullSendKey::Verify };

        rig.Run();

        Assert::IsTrue (rig.link.verifying);
        Assert::IsTrue (rig.link.screens.back().verify);
        Assert::AreEqual ((int) (unsigned char) ('V' | 0x80), (int) (unsigned char) rig.link.screens.back().GetRows()[0][13], L"inverse on the Apple");
    }

    TEST_METHOD (Session_Marks_AreOnTheMap_AndInTheManifestRow)
    {
        Rig  rig;

        rig.link.drives[0]     = Other (1);
        rig.link.present[1]    = false;
        rig.link.reportMarks   = { SectorMark { 9, 1, 'R' }, SectorMark { 9, 2, 'U' }, SectorMark { 12, 0, 'R' } };
        rig.link.reportVerified = 20;

        auto  session = rig.Run();

        const FullSendRow  & row = session->GetManifest().GetRows()[0];

        Assert::AreEqual ('U', session->GetScreen().maps[0][9],  L"the worst of the track");
        Assert::AreEqual ('R', session->GetScreen().maps[0][12]);
        Assert::AreEqual (2, row.recovered);
        Assert::AreEqual (1, row.unreliable);
        Assert::AreEqual (std::string (FullSendManifest::kVerifyPartial), row.verification);
        Assert::AreEqual (1, session->GetScreen().recovered);
        Assert::IsTrue (rig.io.files.contains ("C:\\Box\\0001.dsk.unreadable.txt"));
    }

    TEST_METHOD (Assess_FollowsTheFileSystem_ThenTheTracks)
    {
        Assert::AreEqual (std::string (FullSendManifest::kStatusProtected), std::string (FullSendDisk::AssessDamage (Other (1), 4, 64)));
        Assert::AreEqual (std::string (FullSendManifest::kStatusUnclear),   std::string (FullSendDisk::AssessDamage (Other (1), 0, 2)));
        Assert::AreEqual (std::string (FullSendManifest::kStatusUnclear),   std::string (FullSendDisk::AssessDamage (Dos ("GAME"), 4, 64)));
        Assert::AreEqual (std::string (FullSendManifest::kStatusDamaged),   std::string (FullSendDisk::AssessDamage (Dos ("GAME"), 0, 2)));
    }

    //  A formatted disk with no files is blank, whatever its file system. A
    //  DOS 3.3 disk whose one file is a BASIC program with no lines is blank
    //  once that program's track is read, and one with a line isn't. A disk
    //  with a file isn't, and neither is one of no file system.
    TEST_METHOD (Blank_IsAFormattedDiskWithNoFiles_OrOnlyAProgramWithNoLines)
    {
        std::string       system;
        std::vector<int>  all (NibbleFormat::kTrackCount);
        Image             empty   = DosProgram ({ 0x00, 0x00 });
        Image             written = DosProgram ({ 0x0A, 0x08, 0x0A, 0x00, 0xBA, 0x00, 0x00, 0x00 });

        std::iota (all.begin(), all.end(), 0);

        Assert::IsTrue (FullSendDisk::IsBlank (BlankDos(), {}, system));
        Assert::AreEqual (std::string ("DOS 3.3"), system);
        Assert::IsTrue (FullSendDisk::IsBlank (BlankProDos ("EMPTY"), {}, system));
        Assert::AreEqual (std::string ("ProDOS"), system);

        Assert::IsFalse (FullSendDisk::IsBlank (Dos ("GAME"), all, system), L"A disk with a file");
        Assert::IsFalse (FullSendDisk::IsBlank (ProDos ("FULL"), all, system), L"A ProDOS disk with a file");
        Assert::IsFalse (FullSendDisk::IsBlank (Other (1), all, system), L"No file system");

        Assert::IsTrue (FullSendDisk::FindLoneProgramTrack (empty) >= 0, L"The program's track is found");
        Assert::IsFalse (FullSendDisk::IsBlank (empty, {}, system), L"Not before the program's track is read");
        Assert::IsTrue (FullSendDisk::IsBlank (empty, all, system), L"A program with no lines");
        Assert::IsFalse (FullSendDisk::IsBlank (written, all, system), L"A program with a line");
    }

    //  A blank disk isn't sent: its drive's row and the status have it, with
    //  its file system, no image is written, and the totals count it. The
    //  drive's number on the Apple sends it.
    TEST_METHOD (Session_ABlankDisk_IsReportedAndNotSent_UntilItsDriveIsPressed)
    {
        Rig  rig;

        rig.link.drives[0]  = BlankDos();
        rig.link.present[1] = false;

        auto  session = rig.Run();

        Assert::IsTrue (rig.io.files.empty() || !rig.io.files.contains ("C:\\Box\\0001.dsk"), L"No image");
        Assert::IsTrue (session->GetManifest().GetRows().empty(), L"No row");
        Assert::IsTrue (session->GetScreen().drives[0] == FullSendDriveState::Blank);
        Assert::IsTrue (session->GetScreen().GetRows()[3].starts_with ("D1: DOS 3.3, blank"), TextEncoding::NarrowToWide (session->GetScreen().GetRows()[3]).c_str());

        FullSendSnapshot  snapshot = rig.status.GetSnapshot();

        Assert::AreEqual (1, snapshot.totals.blanks);
        Assert::AreEqual (0, snapshot.totals.sent);
    }

    //  A disk whose directory is empty isn't blank when it's write-protected,
    //  or when a probe track away from the directory has data, as an Infocom
    //  disk's track 0 passed for an empty Pascal directory. Each is sent.
    TEST_METHOD (Session_AnEmptyDirectory_WithDataPastIt_OrWriteProtected_IsSent)
    {
        for (bool isProtected : { false, true })
        {
            Rig  rig;

            rig.link.drives[0]  = BlankDos();
            rig.link.present[1] = false;

            if (isProtected)
            {
                rig.link.writeProtected[0] = true;
            }
            else
            {
                size_t  at = (size_t) FullSendDisk::kBlankProbeTracks[1] * NibbleFormat::kSectorsPerTrack * NibbleFormat::kSectorByteSize;

                rig.link.drives[0][at + 7] = 0x4C;
            }

            auto  session = rig.Run();

            Assert::IsTrue (session->GetScreen().drives[0] != FullSendDriveState::Blank, isProtected ? L"Write-protected" : L"Data on a probe track");
            Assert::AreEqual ((size_t) 1, session->GetManifest().GetRows().size(), L"The disk is sent");
            Assert::AreEqual (0, rig.status.GetSnapshot().totals.blanks);
        }
    }

    //  A drive whose track 0 has no sectors is probed raw. One with a disk is
    //  captured raw with no sector read, and left in its drive it's a repeat
    //  on the next Space. An empty drive is probed and has nothing captured.
    TEST_METHOD (Session_ADiskWithNoSectorsOnTrackZero_IsCapturedRaw_AndAnEmptyDriveIsNot)
    {
        Rig  rig;

        rig.link.drives[0]    = Other (1);
        rig.link.blankTracks  = { 0 };
        rig.link.rawTrack0[0] = Image (6000, 0x96);
        rig.link.keys         = { FullSendKey::Space };

        rig.link.rawTrack0[0][100] = 0xD5;

        auto  session = rig.Run();

        Assert::IsTrue (rig.link.captures == std::vector<int> { 1 }, L"Drive 1's disk is captured once");
        Assert::IsTrue (rig.link.diskReads.empty(), L"With no sector read");
        Assert::AreEqual ((size_t) 1, session->GetManifest().GetRows().size());
        Assert::AreEqual (std::string ("0001.woz"), session->GetManifest().GetRows()[0].file);
        Assert::IsTrue (std::count (rig.link.probes.begin(), rig.link.probes.end(), 2) >= 1, L"The empty drive 2 is probed");
        Assert::IsTrue (session->GetScreen().drives[1] == FullSendDriveState::Empty, L"And it's empty");
        Assert::IsTrue (session->GetScreen().drives[0] == FullSendDriveState::Repeat, L"Drive 1's disk is a repeat on the next Space");
    }

    //  A raw capture's reads, when the link returns them, are written beside
    //  the image with .reads added to its file.
    TEST_METHOD (Session_ARawCapturesReads_AreWrittenBesideItsImage)
    {
        Rig  rig;

        rig.link.drives[0]   = Other (1);
        rig.link.badDisk     = rig.link.drives[0];
        rig.link.badSectors  = { { 5, 0 }, { 6, 0 } };
        rig.link.reportEmpty = 3;
        rig.link.present[1]  = false;
        rig.link.rawReads    = { 'C', 'R', 'A', 'W', 3, 0 };
        rig.link.rawMarks    = "weak 3, 11";

        auto  session = rig.Run();

        Assert::IsTrue (rig.io.files["C:\\Box\\0001.woz"] == rig.link.rawWoz, L"The image is the capture");
        Assert::IsTrue (rig.io.files["C:\\Box\\0001.woz.reads"] == rig.link.rawReads, L"The reads are beside it");
        Assert::AreEqual ((size_t) 1, session->GetManifest().GetRows().size(), L"The reads have no row");
        Assert::AreEqual (std::string ("weak 3, 11"), session->GetManifest().GetRows()[0].marks, L"The row has the capture's marks");
        Assert::IsTrue (rig.link.noted == std::vector<std::string> { "C:\\Box\\0001.woz" }, L"The image is noted for the boot check");
    }

    //  A boot result arrives while the Apple is at a pause, for an image the
    //  manifest lacks: the image is on the Apple at the pause after that
    //  one, once, with no offer to read its disk again.
    TEST_METHOD (Session_AnImageThatDidNotBoot_IsListedAtTheNextPause)
    {
        Rig          rig;
        std::string  line = "0007 did not boot.";
        auto         has  = [&line] (const std::vector<std::string> & lines) { return std::find (lines.begin(), lines.end(), line) != lines.end(); };

        rig.link.drives[0]    = Other (1);
        rig.link.present[1]   = false;
        rig.link.beforeKey[1] = [&rig] (ScriptedLink &) { rig.status.SetBoot ("0007.dsk", FullSendManifest::kBootDidNotBoot); };
        rig.link.keys         = { FullSendKey::Space, FullSendKey::Space };

        rig.Run();

        Assert::AreEqual ((size_t) 3, rig.link.pauseLines.size());
        Assert::IsFalse (has (rig.link.pauseLines[0]), L"The result arrived after the first pause was drawn");
        Assert::IsTrue (has (rig.link.pauseLines[1]), L"The next pause has the image");
        Assert::IsFalse (has (rig.link.pauseLines[2]), L"And the one after it doesn't");
    }

    //  Whether the lines have line, with a line in inverse read as its text.
    static bool  HasLine (const std::vector<std::string> & lines, const std::string & line)
    {
        return std::any_of (lines.begin(), lines.end(), [&line] (const std::string & each)
        {
            std::string  text = each;

            std::transform (text.begin(), text.end(), text.begin(), [] (char c) { return (char) (c & 0x7F); });
            return text == line;
        });
    }

    //  A clean raw capture that doesn't boot fails the same way each time, so
    //  the pause lists it with no offer to capture it again.
    TEST_METHOD (Session_ACleanRawCaptureThatDidNotBoot_HasNoOffer)
    {
        Rig  rig;



        rig.link.drives[0]   = Other (1);
        rig.link.badDisk     = rig.link.drives[0];
        rig.link.badSectors  = { { 5, 0 }, { 6, 0 } };
        rig.link.reportEmpty = 3;
        rig.link.present[1]  = false;
        rig.link.onAwait     = [&rig] (const std::string & file) { rig.status.SetBoot (file, FullSendManifest::kBootDidNotBoot); };

        rig.Run();

        Assert::AreEqual ((size_t) 1, rig.link.captures.size(), L"Captured once");
        Assert::IsTrue  (HasLine (rig.link.pauseLines[0], "0001 did not boot."));
        Assert::IsFalse (HasLine (rig.link.pauseLines[0], "0001 did not boot. Capture it again"), L"No offer");
    }

    //  Several images that did not boot before a pause are one line, the
    //  first four by number and a count of the rest, so the pause fits the
    //  Apple's rows.
    TEST_METHOD (Session_ImagesThatDidNotBoot_AreOneLineAtThePause)
    {
        Rig  rig;



        rig.link.drives[0]    = Other (1);
        rig.link.present[1]   = false;
        rig.link.beforeKey[1] = [&rig] (ScriptedLink &)
        {
            for (int image = 7; image <= 12; image++)
            {
                rig.status.SetBoot (std::format ("{:04}.dsk", image), FullSendManifest::kBootDidNotBoot);
            }
        };
        rig.link.keys         = { FullSendKey::Space, FullSendKey::Space };

        rig.Run();

        Assert::AreEqual ((size_t) 3, rig.link.pauseLines.size());
        Assert::AreEqual (std::string ("0007, 0008, 0009, 0010 and 2 more did not boot."), rig.link.pauseLines[1][0]);
        Assert::IsTrue (rig.link.pauseLines[1].size() <= FullSendSession::s_kPauseRows);
    }

    //  A pause's lines are cut to the Apple's rows and request: blank rows
    //  first, then the quip, then the earliest notes; the instructions remain.
    TEST_METHOD (PauseLines_AreCutToTheApplesRows_KeepingWhatToDo)
    {
        std::vector<std::string>  lines;



        for (int note = 0; note < 7; note++)
        {
            lines.push_back (std::format ("Note {}", note));
        }

        lines.insert (lines.end(), { "", "Put in a new disk and press Space.", "", "The quip" });

        FullSendSession::FitPauseLines (lines);

        Assert::AreEqual (FullSendSession::s_kPauseRows, lines.size());
        Assert::AreEqual (std::string ("Note 0"), lines[0]);
        Assert::AreEqual (std::string ("Put in a new disk and press Space."), lines[7]);

        std::vector<std::string>  wide (6, std::string (60, 'x'));

        wide.insert (wide.end(), { "Put in a new disk and press Space.", "The quip" });
        FullSendSession::FitPauseLines (wide);

        size_t  bytes = 1;

        for (const std::string & line : wide)
        {
            bytes += line.size() + 1;
        }

        Assert::IsTrue (bytes <= FullSendSession::s_kPauseBytes, L"The request fits");
        Assert::AreEqual (std::string ("Put in a new disk and press Space."), wide.back());
    }

    //  The Apple's history line for a session that failed has the reason
    //  first, and fits the line's characters after ERR.
    TEST_METHOD (TheHistoryLine_OfAFailedSession_HasItsReasonFirst)
    {
        std::string  failed = CassoLinkFullSend::DescribeEnd (FullSendSession::EndReason::LinkFailed, 15, 0, 1);



        Assert::AreEqual (std::string ("Link failed: 15 sent, 0 bad"), failed);
        Assert::IsTrue   (failed.size() + 4 <= CassoLinkCodec::kLogLength, L"ERR and the line fit");
        Assert::AreEqual (std::string ("Full Send: 15 sent, 0 bad, 1 dup"), CassoLinkFullSend::DescribeEnd (FullSendSession::EndReason::Quit, 15, 0, 1));
    }

    //  An image that is booted is awaited with VALIDATING on the Apple, and
    //  one that did not boot is on the Apple at the pause that follows.
    TEST_METHOD (Session_AnImageBeingBooted_IsAwaited_AndItsResultIsAtThePauseThatFollows)
    {
        Rig   rig;
        bool  validating = false;

        rig.link.drives[0]  = Other (1);
        rig.link.present[1] = false;
        rig.link.rawMarks   = "weak 3";
        rig.link.onAwait    = [&rig, &validating] (const std::string & file)
        {
            validating = rig.link.screens.back().drives[0] == FullSendDriveState::Validating
                         && rig.status.GetSnapshot().activity == FullSendActivity::Validating;

            rig.status.SetBoot (file, FullSendManifest::kBootDidNotBoot);
        };

        rig.Run();

        Assert::IsTrue (validating, L"The Apple and the window have the wait");
        Assert::IsTrue (std::none_of (rig.link.screens.begin(), rig.link.screens.end(), [] (const FullSendScreen & screen) { return screen.tone == FullSendTone::DidNotBoot; }),
                        L"No tone for the result unless the Apple has them on");
        Assert::IsTrue (rig.link.screens.back().drives[0] != FullSendDriveState::Validating);
        Assert::AreEqual ((size_t) 1, rig.link.pauseLines.size());
        Assert::IsTrue (HasLine (rig.link.pauseLines[0], "0001 did not boot. Capture it again"));
    }

    //  Space with the disk that did not boot in its drive reads it again,
    //  once: a raw capture is built with the capture before it, the image
    //  is a second file, and a second failure has no offer.
    TEST_METHOD (Session_SpaceOnADiskThatDidNotBoot_ReadsItAgainOnce)
    {
        Rig   rig;
        auto  has = [] (const std::vector<std::string> & lines, const char * line) { return HasLine (lines, line); };

        rig.link.drives[0]   = Other (1);
        rig.link.badDisk     = rig.link.drives[0];
        rig.link.badSectors  = { { 5, 0 }, { 6, 0 } };
        rig.link.reportEmpty = 3;
        rig.link.present[1]  = false;
        rig.link.rawMarks    = "weak 3";
        rig.link.onAwait     = [&rig] (const std::string & file) { rig.status.SetBoot (file, FullSendManifest::kBootDidNotBoot); };
        rig.link.keys        = { FullSendKey::Space, FullSendKey::Space };

        auto  session = rig.Run();

        Assert::AreEqual ((size_t) 2, rig.link.captures.size(), L"Captured, and captured again");
        Assert::AreEqual (1, rig.link.capturesWithEarlier, L"The second with the reads of the first");
        Assert::AreEqual ((size_t) 3, rig.link.pauseLines.size());
        Assert::IsTrue (has (rig.link.pauseLines[0], "0001 did not boot. Capture it again"));
        Assert::IsTrue (has (rig.link.pauseLines[1], "0002 still does not boot."));
        Assert::IsFalse (has (rig.link.pauseLines[2], "0002 still does not boot."), L"Space after that is a repeat's");
        Assert::IsTrue (rig.io.files.contains ("C:\\Box\\0001.woz") && rig.io.files.contains ("C:\\Box\\0002.woz"), L"Each capture has its image");
        Assert::AreEqual ((size_t) 2, session->GetManifest().GetRows().size());
    }

    //  A likely copy-protected disk saved as its sectors, with raw capture
    //  off, is captured raw when its image doesn't boot and Space reads it
    //  again; verification stays off.
    TEST_METHOD (Session_SpaceOnAProtectedSectorCopyThatDidNotBoot_CapturesItRaw)
    {
        Rig  rig;

        rig.link.drives[0]        = Other (1);
        rig.link.badDisk          = rig.link.drives[0];
        rig.link.badSectors       = { { 5, 0 }, { 6, 0 } };
        rig.link.reportEmpty      = 3;
        rig.link.present[1]       = false;
        rig.link.onAwait          = [&rig] (const std::string & file) { rig.status.SetBoot (file, FullSendManifest::kBootDidNotBoot); };
        rig.link.keys             = { FullSendKey::Space };
        rig.protection.captureRaw = false;

        auto  session = rig.Run();

        Assert::IsTrue (rig.link.captures == std::vector<int> { 1 }, L"Only the second read is captured");
        Assert::IsTrue (rig.link.readsVerified == std::vector<bool> ({ false }), L"The raw capture comes with no second sector read");
        Assert::AreEqual (std::string (FullSendManifest::kStatusProtected),  session->GetManifest().GetRows()[0].status);
        Assert::AreEqual (std::string (FullSendManifest::kStatusRawCapture), session->GetManifest().GetRows()[1].status);
        Assert::AreEqual (session->GetManifest().GetRows()[0].hash,       session->GetManifest().GetRows()[1].hash);
        Assert::AreEqual (session->GetManifest().GetRows()[0].unreadable, session->GetManifest().GetRows()[1].unreadable);
        Assert::IsTrue (rig.io.files.contains ("C:\\Box\\0001.dsk") && rig.io.files.contains ("C:\\Box\\0002.woz"));
        Assert::IsFalse (rig.link.verifying);
    }

    //  A DOS 3.3 disk with whole tracks unreadable may be copy-protected: its
    //  status is unclear, and Space captures it raw.
    TEST_METHOD (Session_SpaceOnAnUnclearDiskThatDidNotBoot_CapturesItRaw)
    {
        Rig  rig;

        rig.link.drives[0]   = Dos ("GAME");
        rig.link.badDisk     = rig.link.drives[0];
        rig.link.badSectors  = { { 20, 3 } };
        rig.link.reportEmpty = 3;
        rig.link.present[1]  = false;
        rig.link.rawMarks    = "weak 3";
        rig.link.onAwait     = [&rig] (const std::string & file) { rig.status.SetBoot (file, FullSendManifest::kBootDidNotBoot); };
        rig.link.keys        = { FullSendKey::Space };

        auto  session = rig.Run();

        Assert::AreEqual (std::string (FullSendManifest::kStatusUnclear), session->GetManifest().GetRows()[0].status);
        Assert::IsTrue (rig.link.captures == std::vector<int> { 1 }, L"Only the second read is captured");
        Assert::IsTrue (rig.link.readsVerified == std::vector<bool> ({ false }), L"The raw capture comes with no second sector read");
        Assert::AreEqual (std::string (FullSendManifest::kStatusRawCapture), session->GetManifest().GetRows()[1].status);
    }

    //  A DOS 3.3 disk with a bad sector is damaged, not protected: Space reads
    //  it again with verification, and isn't a raw capture.
    TEST_METHOD (Session_SpaceOnADamagedDiskThatDidNotBoot_ReadsItAgainVerified)
    {
        Rig  rig;

        rig.link.drives[0]  = Dos ("GAME");
        rig.link.badDisk    = rig.link.drives[0];
        rig.link.badSectors = { { 20, 3 } };
        rig.link.present[1] = false;
        rig.link.onAwait    = [&rig] (const std::string & file) { rig.status.SetBoot (file, FullSendManifest::kBootDidNotBoot); };
        rig.link.keys       = { FullSendKey::Space };

        auto  session = rig.Run();

        Assert::IsTrue (rig.link.captures.empty(), L"Not captured raw");
        Assert::IsTrue (rig.link.readsVerified == std::vector<bool> ({ false, true }), L"The second read is verified");
        Assert::AreEqual ((size_t) 2, session->GetManifest().GetRows().size());
        Assert::IsFalse (rig.link.verifying, L"Verification is off again after it");
    }

    //  A write-protected disk's image is write-protected: a sector image by
    //  its read-only attribute, and its manifest row has the flag.
    TEST_METHOD (Session_AWriteProtectedDisk_HasAWriteProtectedImage)
    {
        Rig  rig;

        rig.link.drives[0]         = Other (1);
        rig.link.present[1]        = false;
        rig.link.writeProtected[0] = true;

        auto  session = rig.Run();

        Assert::AreEqual ((size_t) 1, session->GetManifest().GetRows().size());
        Assert::IsTrue (session->GetManifest().GetRows()[0].writeProtected);
        Assert::IsTrue (rig.io.readOnlyFiles.contains ("C:\\Box\\" + session->GetManifest().GetRows()[0].file), L"The read-only attribute");
        Assert::IsTrue (rig.io.readOnlyFiles["C:\\Box\\" + session->GetManifest().GetRows()[0].file]);
    }

    //  With WOZ chosen, each disk is measured and its image is built to the
    //  measured layouts, write-protected as its disk is; with another type,
    //  no disk is measured.
    TEST_METHOD (Session_WithWozChosen_TheImageIsBuiltToTheMeasuredLayouts)
    {
        Rig    rig;
        Rig    sectors;
        Image  built;

        rig.extension              = ".woz";
        rig.link.drives[0]         = Dos ("GAME");
        rig.link.present[1]        = false;
        rig.link.writeProtected[0] = true;

        for (int track = 0; track < NibbleFormat::kTrackCount; track++)
        {
            rig.link.layouts.push_back (TrackLayout { 254, 6337, 51244, (track * 30763LL + 4111) % 51244 });
        }

        auto  session = rig.Run();

        Assert::IsTrue (rig.link.measuring);
        Assert::AreEqual ((size_t) 1, session->GetManifest().GetRows().size());

        AssertSucceeded (TrackLayoutWriter::BuildWoz (rig.link.drives[0], rig.link.layouts, built));
        AssertSucceeded (WozLoader::SetWriteProtectFlag (built, true));

        Assert::IsTrue (rig.io.files["C:\\Box\\" + session->GetManifest().GetRows()[0].file] == built, L"The WOZ is built to the layouts, write-protected");

        sectors.extension       = ".dsk";
        sectors.link.drives[0]  = Dos ("GAME");
        sectors.link.present[1] = false;

        sectors.Run();

        Assert::IsFalse (sectors.link.measuring);
    }

    //  The layouts of a disk at 300 rpm whose address fields have `volume`.
    static std::vector<TrackLayout>  MakeLayouts (Byte volume)
    {
        std::vector<TrackLayout>  layouts;

        for (int track = 0; track < NibbleFormat::kTrackCount; track++)
        {
            layouts.push_back (TrackLayout { volume, 6330, TrackLayoutWriter::kDefaultBits, (track * 30763LL) % TrackLayoutWriter::kDefaultBits });
        }

        return layouts;
    }

    //  A disk whose track 0 has a volume its sector image can't reproduce is
    //  measured on every track and saved as a WOZ built to the layouts,
    //  whatever image type is chosen.
    TEST_METHOD (Session_ADiskWithAVolumeItsImageCantStore_IsSavedAsABuiltWoz)
    {
        Rig    rig;
        Image  built;

        rig.extension       = ".dsk";
        rig.link.drives[0]  = Dos ("GAME");
        rig.link.present[1] = false;
        rig.link.layouts    = MakeLayouts (0);

        auto  session = rig.Run();

        Assert::AreEqual ((size_t) 1, session->GetManifest().GetRows().size());
        Assert::IsTrue (session->GetManifest().GetRows()[0].file.ends_with (".woz"), L"A WOZ in place of the .dsk");
        Assert::IsTrue (rig.link.measures == std::vector<int> { 1, NibbleFormat::kTrackCount }, L"Track 0, then every track");

        AssertSucceeded (TrackLayoutWriter::BuildWoz (rig.link.drives[0], rig.link.layouts, built));
        Assert::IsTrue (rig.io.files["C:\\Box\\" + session->GetManifest().GetRows()[0].file] == built, L"The WOZ is built to the layouts");
        Assert::IsTrue (std::any_of (rig.link.reports.begin(), rig.link.reports.end(), [] (const std::string & line) { return line.find ("doesn't use volume 254") != std::string::npos && line.find ("its volume 0") != std::string::npos; }));
    }

    //  A disk with the volume its image is nibblized with, the DOS 3.3
    //  catalog's here, is a sector image after a measure of track 0; with
    //  the option off, no track is measured.
    TEST_METHOD (Session_ADiskWithItsImagesVolume_OrWithTheOptionOff_IsASectorImage)
    {
        Rig  rig;
        Rig  off;

        rig.extension       = ".dsk";
        rig.link.drives[0]  = Dos ("GAME");
        rig.link.present[1] = false;
        rig.link.layouts    = MakeLayouts (254);

        auto  session = rig.Run();

        Assert::IsTrue (session->GetManifest().GetRows()[0].file.ends_with (".dsk"));
        Assert::IsTrue (rig.link.measures == std::vector<int> { 1 });

        off.extension            = ".dsk";
        off.link.drives[0]       = Dos ("GAME");
        off.link.present[1]      = false;
        off.link.layouts         = MakeLayouts (0);
        off.protection.volumeWoz = false;

        auto  unmeasured = off.Run();

        Assert::IsTrue (unmeasured->GetManifest().GetRows()[0].file.ends_with (".dsk"));
        Assert::IsTrue (off.link.measures.empty());
    }

    //  A disk captured raw after its sector read took the read's time and the
    //  capture's, and both are active time.
    TEST_METHOD (Session_ADiskCapturedRawAfterItsRead_TookTheReadAndTheCapture)
    {
        Rig               rig;
        FullSendSnapshot  snapshot;

        rig.link.drives[0]      = Other (1);
        rig.link.badDisk        = rig.link.drives[0];
        rig.link.badSectors     = { { 5, 0 }, { 6, 0 } };
        rig.link.reportEmpty    = 3;
        rig.link.present[1]     = false;
        rig.link.readSeconds    = 30.0;
        rig.link.captureSeconds = 300.0;

        auto  session = rig.Run();

        snapshot = rig.status.GetSnapshot();

        Assert::IsTrue (std::any_of (rig.link.reports.begin(), rig.link.reports.end(), [] (const std::string & line) { return line.ends_with (". Took 5:30."); }), L"The disk's line has its time");
        Assert::IsTrue (session->GetMessage().find ("5:30 active, ") != std::string::npos, L"The session's message has its active time");
        Assert::AreEqual ((size_t) 1, rig.link.captures.size(), L"The disk is captured raw");
        Assert::AreEqual ((size_t) 1, snapshot.disks.size());
        Assert::AreEqual (330.0, snapshot.disks[0].seconds, 0.001, L"The disk's time");
        Assert::AreEqual (330.0, snapshot.totals.elapsed, 0.001, L"The session's active time");
    }

    TEST_METHOD (Session_WithBootTonesOnTheApple_TheResultsToneIsInPlaceOfTheDisks)
    {
        Rig  rig;

        rig.link.drives[0]  = Other (1);
        rig.link.present[1] = false;
        rig.link.bootTones  = true;
        rig.link.onAwait    = [&rig] (const std::string & file) { rig.status.SetBoot (file, FullSendManifest::kBootBooted); };

        rig.Run();

        Assert::IsTrue (std::any_of (rig.link.screens.begin(), rig.link.screens.end(), [] (const FullSendScreen & screen) { return screen.tone == FullSendTone::Booted; }));
        Assert::IsTrue (std::none_of (rig.link.screens.begin(), rig.link.screens.end(), [] (const FullSendScreen & screen) { return screen.tone == FullSendTone::Done; }));
    }

    //  An image with unreadable sectors is a failed capture when its boot is
    //  unclear. One that boots has the success's tone.
    TEST_METHOD (Session_WithBootTonesOnTheApple_AnImageWithBadSectors_HasTheFailureToneUnlessItBoots)
    {
        for (const char * boot : { FullSendManifest::kBootUnclear, FullSendManifest::kBootBooted })
        {
            Rig   rig;
            bool  isBooted = std::string (boot) == FullSendManifest::kBootBooted;

            rig.link.drives[0]  = Other (1);
            rig.link.badDisk    = rig.link.drives[0];
            rig.link.badSectors = { { 5, 0 } };
            rig.link.present[1] = false;
            rig.link.bootTones  = true;
            rig.link.onAwait    = [&rig, boot] (const std::string & file) { rig.status.SetBoot (file, boot); };

            rig.Run();

            Assert::AreEqual (!isBooted, std::any_of (rig.link.screens.begin(), rig.link.screens.end(), [] (const FullSendScreen & screen) { return screen.tone == FullSendTone::DidNotBoot; }));
            Assert::AreEqual (isBooted,  std::any_of (rig.link.screens.begin(), rig.link.screens.end(), [] (const FullSendScreen & screen) { return screen.tone == FullSendTone::Booted; }));
        }
    }

    //  An image with a bad file system that boots has the success's tone, as a
    //  crack's odd catalog reads as bad, and the pause offers to capture its
    //  disk again in raw mode. One whose boot is unclear has the failure's
    //  tone. An image with no standard file system isn't a failure.
    TEST_METHOD (Session_WithBootTonesOnTheApple_ABadFileSystem_FailsOnlyAnImageThatDidntBoot)
    {
        struct Case
        {
            const char *  boot;
            const char *  check;
            bool          isFailed;
        };

        for (const Case & each : { Case { FullSendManifest::kBootBooted,  FullSendManifest::kCheckBad,  false },
                                   Case { FullSendManifest::kBootUnclear, FullSendManifest::kCheckBad,  true },
                                   Case { FullSendManifest::kBootBooted,  FullSendManifest::kCheckNone, false } })
        {
            Rig   rig;
            bool  isOffered = false;

            rig.link.drives[0]  = Other (1);
            rig.link.present[1] = false;
            rig.link.bootTones  = true;
            rig.link.onAwait    = [&rig, each] (const std::string & file) { rig.status.SetBoot (file, each.boot, std::string(), each.check); };

            rig.Run();

            for (const std::string & line : rig.link.pauseLines.back())
            {
                isOffered = isOffered || line.find ("booted, but its catalog is odd") != std::string::npos;
            }

            Assert::AreEqual (each.isFailed,  std::any_of (rig.link.screens.begin(), rig.link.screens.end(), [] (const FullSendScreen & screen) { return screen.tone == FullSendTone::DidNotBoot; }));
            Assert::AreEqual (!each.isFailed, std::any_of (rig.link.screens.begin(), rig.link.screens.end(), [] (const FullSendScreen & screen) { return screen.tone == FullSendTone::Booted; }));
            Assert::AreEqual (std::string (each.boot) == FullSendManifest::kBootBooted && std::string (each.check) == FullSendManifest::kCheckBad, isOffered, L"The raw capture is offered");
        }
    }

    //  The thread that booted an image writes its verdict to the manifest's
    //  file. The session's manifest takes it too, so the session's next write
    //  doesn't put back the row as it was before the boot.
    TEST_METHOD (BootResults_SetTheFileSystemVerdictInTheSessionsManifest)
    {
        Rig                  rig;
        FullSendManifest     manifest;
        FullSendRow          row;
        FullSendBootResults  boots (rig.status, manifest);

        row.sequence = 1;
        row.file     = "0001.dsk";
        row.status   = FullSendManifest::kStatusSent;
        manifest.Add (row);

        rig.status.SetBoot ("0001.dsk", FullSendManifest::kBootBooted, std::string(), FullSendManifest::kCheckBad);
        boots.Apply();

        Assert::AreEqual (std::string (FullSendManifest::kBootBooted), manifest.GetRows()[0].boot);
        Assert::AreEqual (std::string (FullSendManifest::kCheckBad),   manifest.GetRows()[0].fsCheck);
        Assert::AreEqual (std::string (FullSendManifest::kCheckBad),   boots.GetLatestCheck());

        boots.ClearLatest();
        Assert::IsTrue (boots.GetLatestCheck().empty());
    }

    //  A likely copy-protected disk is captured raw: its image is the WOZ, its
    //  row is a raw capture, and the session goes on. A damaged disk with a
    //  file system isn't captured.
    TEST_METHOD (Session_ALikelyProtectedDisk_IsSavedAsARawCapture_AndTheSessionGoesOn)
    {
        Rig  rig;

        rig.link.drives[0]   = Other (1);
        rig.link.badDisk     = rig.link.drives[0];
        rig.link.badSectors  = { { 5, 0 }, { 6, 0 } };
        rig.link.reportEmpty = 3;
        rig.link.present[1]  = false;

        auto  session = rig.Run();

        Assert::IsTrue (rig.link.captures == std::vector<int> { 1 }, L"Drive 1 is captured");
        Assert::IsTrue (rig.io.files["C:\\Box\\0001.woz"] == rig.link.rawWoz, L"The image is the capture");
        Assert::IsFalse (rig.io.files.contains ("C:\\Box\\0001.dsk"), L"No sector image");
        Assert::AreEqual (std::string (FullSendManifest::kStatusRawCapture), session->GetManifest().GetRows()[0].status);
        Assert::AreEqual (std::string ("0001.woz"), session->GetManifest().GetRows()[0].file);
        Assert::AreEqual (1, session->GetScreen().prot);
        Assert::AreEqual (0, session->GetScreen().bad);
        Assert::IsTrue (session->GetScreen().drives[0] == FullSendDriveState::Protected);
        Assert::IsTrue (std::any_of (rig.link.screens.begin(), rig.link.screens.end(), [] (const FullSendScreen & s) { return s.tone == FullSendTone::Protected; }));
        Assert::IsTrue (std::any_of (rig.link.screens.begin(), rig.link.screens.end(), [] (const FullSendScreen & s) { return s.drives[0] == FullSendDriveState::Capturing; }),
                        L"The Apple's screen has the capture while it runs");
        Assert::IsTrue (session->GetScreen().maps[0].empty(), L"No map of bad sectors");
        Assert::IsFalse (rig.io.files.contains ("C:\\Box\\0001.woz.unreadable.txt"), L"No list of unreadable sectors");
        Assert::IsFalse (rig.io.files.contains ("C:\\Box\\0001.woz.reads"), L"No reads file when the link has no reads");

        FullSendSnapshot  snapshot = rig.status.GetSnapshot();

        Assert::AreEqual (1, snapshot.totals.rawCaptures);
        Assert::AreEqual (std::string ("0001.woz"), snapshot.disks[0].row.file);
        Assert::IsFalse (snapshot.drives[0].positions.empty(), L"The drive's panel has the capture's positions");
        Assert::IsTrue (snapshot.drives[0].tracks[5] == FullSendTrackMark::Unreadable);

        Rig  damaged;

        damaged.link.drives[0]  = Dos ("GAME");
        damaged.link.badDisk    = damaged.link.drives[0];
        damaged.link.badSectors = { { 20, 3 } };
        damaged.link.present[1] = false;

        auto  other = damaged.Run();

        Assert::IsTrue (damaged.link.captures.empty(), L"A damaged disk isn't captured");
        Assert::AreEqual (std::string (FullSendManifest::kStatusDamaged), other->GetManifest().GetRows()[0].status);
    }

    //  A disk that reads cleanly with a program that reads the disk drive's
    //  data register itself, as Wizardry I's boot disk does, is captured raw;
    //  saved as its sector copy with the likely copy-protected status when
    //  the choice is to save it; and sent as any disk when its condition is
    //  off.
    TEST_METHOD (Session_ADiskWithDiskCode_IsCapturedRaw_OrMarked_ByTheChoices)
    {
        Image        buffer (NibbleFormat::kImageByteSize, 0);
        Image        disk;
        FilePayload  payload;
        Rig          capture;
        Rig          marked;
        Rig          off;



        payload.type           = Dos33Volume::kTypeBinary;
        payload.loadAddress    = 0x8A00;
        payload.hasLoadAddress = true;

        for (int i = 0; i < FullSendProtection::kDiskCodeReads; i++)
        {
            payload.bytes.insert (payload.bytes.end(), { 0xBD, 0x8C, 0xC0, 0x10, 0xFB, 0xEA, 0xEA });
        }

        AssertSucceeded (Dos33Skeleton::Write (buffer, 254));
        AssertSucceeded (Dos33Volume (buffer).Write (FilePath::Parse ("GAME"), payload, disk));

        for (Rig * rig : { &capture, &marked, &off })
        {
            rig->link.drives[0]  = disk;
            rig->link.present[1] = false;
        }

        marked.protection.captureRaw = false;
        off.protection.diskCode      = false;

        auto  captured = capture.Run();
        auto  saved    = marked.Run();
        auto  sent     = off.Run();

        Assert::IsTrue  (capture.link.captures == std::vector<int> { 1 }, L"Captured raw");
        Assert::AreEqual (std::string (FullSendManifest::kStatusRawCapture), captured->GetManifest().GetRows()[0].status);
        Assert::IsTrue  (std::any_of (capture.link.reports.begin(), capture.link.reports.end(), [] (const std::string & text) { return text.starts_with ("GAME on the disk in drive 1 accesses the disk drive directly"); }));

        Assert::IsTrue  (marked.link.captures.empty(), L"Not captured when the choice is to save the sector copy");
        Assert::AreEqual (std::string (FullSendManifest::kStatusProtected), saved->GetManifest().GetRows()[0].status);
        Assert::AreEqual (std::string ("0001 GAME.dsk"), saved->GetManifest().GetRows()[0].file);
        Assert::AreEqual (1, saved->GetScreen().prot);
        Assert::AreEqual (std::string ("D1: Protected, sent 0001 GAME"), saved->GetScreen().GetRows()[3], L"Not captured raw");
        Assert::AreEqual (std::string ("D1: Protected, captured raw 0001 GAME"), captured->GetScreen().GetRows()[3]);

        Assert::IsTrue  (off.link.captures.empty(), L"Not captured with the condition off");
        Assert::AreEqual (std::string (FullSendManifest::kStatusSent), sent->GetManifest().GetRows()[0].status);
    }

    //  The Full Send window's model after a disk: its row, title, file
    //  system and tracks, the totals, the session's end and the lines.
    TEST_METHOD (Session_ADiskSent_IsInTheStatus_WithItsTitleTracksAndRow)
    {
        Rig  rig;

        rig.link.drives[0]  = Dos ("GAME");
        rig.link.present[1] = false;

        auto              session  = rig.Run();
        FullSendSnapshot  snapshot = rig.status.GetSnapshot();

        Assert::IsTrue (snapshot.activity == FullSendActivity::Ended);
        Assert::IsFalse (snapshot.reason.empty());
        Assert::AreEqual ((size_t) 1, snapshot.disks.size());
        Assert::AreEqual (session->GetManifest().GetRows()[0].file, snapshot.disks[0].row.file);
        Assert::AreEqual (std::string ("DOS 3.3"), snapshot.disks[0].fileSystem);
        Assert::AreEqual (1, snapshot.totals.sent);
        Assert::AreEqual ((uint64_t) NibbleFormat::kImageByteSize, snapshot.totals.bytes);
        Assert::IsTrue (snapshot.drives[0].state == FullSendDiskState::Sent);
        Assert::AreEqual (std::string ("0001 GAME"), snapshot.drives[0].title);
        Assert::IsTrue (std::all_of (snapshot.drives[0].tracks.begin(), snapshot.drives[0].tracks.end(), [] (FullSendTrackMark mark) { return mark == FullSendTrackMark::Read; }));
        Assert::IsTrue (std::any_of (snapshot.lines.begin(), snapshot.lines.end(), [] (const FullSendLine & line) { return line.text.find ("0001 GAME.dsk") != std::string::npos; }));
    }

    TEST_METHOD (Session_DoesNotReadADisk_UntilSpaceBegins)
    {
        Rig  rig;

        rig.link.drives[0] = Other (1);
        rig.link.beginKey  = FullSendKey::Quit;

        rig.Run (IFullSendLink::kQuit);

        Assert::IsTrue (rig.link.trackDrives.empty());
        Assert::IsTrue (rig.link.diskReads.empty());
    }

    TEST_METHOD (Session_Begin_AsksToRemoveTheCassoLinkDisk_UnlessBootstrapped)
    {
        Rig  booted;
        Rig  bootstrapped;

        bootstrapped.link.bootstrapped = true;

        booted.Run();
        bootstrapped.Run();

        Assert::AreEqual (std::string ("Remove the CassoLink disk"), booted.link.entryLines[4]);
        Assert::AreEqual (std::string ("Insert disks to be sent into the drives"), bootstrapped.link.entryLines[4]);
    }

    //  Space with the disk just sent still in the drive changes nothing.
    TEST_METHOD (Session_Space_OnAnAlreadySentDisk_DoesNotReadIt_OrCountADuplicate)
    {
        Rig  rig;

        rig.link.drives[0]  = Other (1);
        rig.link.present[1] = false;
        rig.link.keys       = { FullSendKey::Space };

        auto  session = rig.Run();

        Assert::IsTrue   (rig.link.diskReads == std::vector<int> ({ 1 }));
        Assert::AreEqual (0, session->GetScreen().duplicates);
        Assert::AreEqual (std::string ("D1 may already have been sent."), rig.link.pauseLines.back()[0]);
        Assert::AreEqual (std::string ("or press 1 to send D1 anyway."), rig.link.pauseLines.back()[4]);
    }

    //  The drive of a disk already sent has the image it matches, not "No disk".
    TEST_METHOD (Session_ADiskAlreadySent_HasTheImageItMatchesInTheStatus)
    {
        Rig  rig;

        rig.link.drives[0]  = Other (1);
        rig.link.present[1] = false;
        rig.link.keys       = { FullSendKey::Space };

        auto              session  = rig.Run();
        FullSendSnapshot  snapshot = rig.status.GetSnapshot();

        Assert::IsTrue   (snapshot.drives[0].state == FullSendDiskState::Duplicate);
        Assert::AreEqual (session->GetManifest().GetRows()[0].file, snapshot.drives[0].title);
    }

    //  Two Apples sending to one folder at once: the second session runs from
    //  start to end while the first is between reading its disk and writing it.
    TEST_METHOD (Session_TwoAtOnceInOneFolder_KeepEveryRow_AndNumberEachDisk)
    {
        Rig                                 first;
        Rig                                 second;
        std::unique_ptr<FullSendSession>    other;
        FullSendManifest                    manifest;



        first.link.drives[0]    = Dos ("GAME");
        first.link.present[1]   = false;
        second.link.drives[0]   = Dos ("WORLD", 2);
        second.link.present[1]  = false;

        first.link.afterDiskRead[1] = [&] (ScriptedLink &)
        {
            other = std::make_unique<FullSendSession> (second.link, first.io, [] () { return std::string ("2026-10-30T12:00:00Z"); }, kFolder, "", "");
            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_CANCELLED), other->Run());
        };

        first.Run();

        AssertSucceeded (manifest.Parse (first.io.files["C:\\Box\\manifest.fullsend"]));
        Assert::AreEqual ((size_t) 2, manifest.GetRows().size());
        Assert::AreEqual (std::string ("0001 WORLD.dsk"), manifest.GetRows()[0].file);
        Assert::AreEqual (std::string ("0002 GAME.dsk"), manifest.GetRows()[1].file);
    }

    TEST_METHOD (Session_ADiskAnotherSessionSent_IsARepeat)
    {
        Rig  first;
        Rig  second;



        first.link.drives[0]    = Dos ("GAME");
        first.link.present[1]   = false;
        second.link.drives[0]   = Image();
        second.link.present[1]  = false;
        second.link.keys        = { FullSendKey::Space };

        //  The second session started first, and its drive gets the disk the
        //  first sent.
        second.link.beforeKey[1] = [&] (ScriptedLink & link)
        {
            FullSendSession  session (first.link, second.io, [] () { return std::string ("2026-10-30T12:00:00Z"); }, kFolder, "", "");

            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_CANCELLED), session.Run());
            link.drives[0] = Dos ("GAME");
        };

        second.Run();

        Assert::IsTrue   (second.link.diskReads.empty(), L"The disk the first session sent isn't read again");
        Assert::AreEqual (std::string ("D1 may already have been sent."), second.link.pauseLines.back()[0]);
    }

    //  A protected disk whose catalog track has no sector that reads: the read
    //  after the disk's is of track 0, so the disk isn't taken for a swap.
    TEST_METHOD (Session_ADiskWithABlankCatalogTrack_IsSent)
    {
        Rig  rig;

        rig.link.drives[0]   = Other (4);
        rig.link.present[1]  = false;
        rig.link.blankTracks = { FullSendDisk::kDosCatalogTrack };

        auto  session = rig.Run();

        Assert::IsTrue   (rig.link.diskReads == std::vector<int> ({ 1 }));
        Assert::AreEqual ((size_t) 1, session->GetManifest().GetRows().size());
        Assert::IsTrue   (std::none_of (rig.link.reports.begin(), rig.link.reports.end(), [] (const std::string & line) { return line.find ("changed while it was read") != std::string::npos; }));
    }

    TEST_METHOD (Session_DriveKey_SendsTheListedDisk_EvenAsAnExactCopy)
    {
        Rig  rig;

        rig.link.drives[0]  = Other (1);
        rig.link.present[1] = false;
        rig.link.keys       = { FullSendKey::Space, FullSendKey::Drive1 };

        auto  session = rig.Run();

        Assert::IsTrue   (rig.link.diskReads == std::vector<int> ({ 1, 1 }));
        Assert::IsTrue   (rig.Files() == std::vector<std::string> ({ "0001.dsk", "0002.dsk", "manifest.fullsend" }));
        Assert::AreEqual (0, session->GetScreen().duplicates);
        Assert::AreEqual (std::string ("D1: Sent 0002"), session->GetScreen().GetRows()[3]);
    }

    //  A blank disk sent with its drive's key is captured raw too, as what
    //  reads as blank may have more than its sectors.
    TEST_METHOD (Session_DriveKey_CapturesABlankDiskRaw)
    {
        Rig  rig;

        rig.link.drives[0]  = BlankDos();
        rig.link.present[1] = false;
        rig.link.keys       = { FullSendKey::Drive1 };

        rig.Run();

        Assert::IsTrue (rig.link.captures == std::vector<int> { 1 });
    }

    //  A raw capture started on the PC skips the sector read, whatever the
    //  check found.
    TEST_METHOD (Session_RawDriveKey_CapturesTheDiskRawOnly)
    {
        Rig  rig;

        rig.link.drives[0]  = BlankDos();
        rig.link.present[1] = false;
        rig.link.keys       = { FullSendKey::RawDrive1 };

        rig.Run();

        Assert::IsTrue (rig.link.captures == std::vector<int> { 1 });
        Assert::IsTrue (rig.link.diskReads.empty(), L"no sector read");
    }

    TEST_METHOD (Session_DriveKey_WithNoDiskListed_DoesNothing)
    {
        Rig  rig;

        rig.link.drives[0]  = Other (1);
        rig.link.present[1] = false;
        rig.link.keys       = { FullSendKey::Drive1 };

        rig.Run();

        Assert::IsTrue (rig.link.diskReads == std::vector<int> ({ 1 }));
    }

    TEST_METHOD (Session_Space_WithARepeatInBothDrives_ListsBoth)
    {
        Rig  rig;

        rig.link.drives[0] = Other (1);
        rig.link.drives[1] = Other (2);
        rig.link.keys      = { FullSendKey::Space, FullSendKey::Drive2 };

        rig.Run();

        //  The drive just sent isn't checked again before the first pause.
        Assert::AreEqual (std::string ("D1 may already have been sent."), rig.link.pauseLines[0][0]);
        Assert::AreEqual (std::string ("D1: Dupe of 0001.dsk?"), rig.link.screens[rig.link.screens.size() - 1].GetRows()[3]);
        Assert::AreEqual (std::string ("D1 and D2 may already have been sent."), rig.link.pauseLines[1][0]);
        Assert::AreEqual (std::string ("or press 1 or 2 to send D1 or D2 anyway."), rig.link.pauseLines[1][4]);
        Assert::IsTrue   (rig.link.diskReads == std::vector<int> ({ 1, 2, 2 }));
    }
};
