#include "Pch.h"

#include "CassoLinkCaptures.h"
#include "InMemoryCassoLinkFolder.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "CassoLink/CassoLinkFolder.h"
#include "Core/TextEncoding.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/CassoLinkHost.h"
#include "Seams/InProcessByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHostTests
//
//  The host against ADTPro's own client: each capture's client side is sent
//  to the host, and every reply must be the bytes ADTPro's server sent. The
//  folder has the files ADTPro's had -- the image a receive pulled is rebuilt
//  from the capture itself -- so the replies can match exactly.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkHostTests)
{
public:

    //  The host on its own thread, serving until the test is done.
    struct Session
    {
        std::shared_ptr<InProcessByteStream>  hostEnd;
        std::shared_ptr<InProcessByteStream>  clientEnd;
        std::unique_ptr<CassoLinkHost>        host;
        std::jthread                          thread;
        std::atomic<HRESULT>                  lastFailure { S_OK };

        Session (ICassoLinkFolder & folder, CassoLinkHost::ClockFn clock = nullptr)
        {
            InProcessByteStream::CreatePair (hostEnd, clientEnd);

            host   = std::make_unique<CassoLinkHost> (hostEnd, folder, clock ? clock : [] () { return std::array<Byte, 4> {}; });
            thread = std::jthread ([this] (std::stop_token stop)
            {
                while (!stop.stop_requested())
                {
                    HRESULT  hr = host->ServeOne (20);

                    if (FAILED (hr))
                    {
                        lastFailure = hr;
                        break;
                    }
                }
            });
        }

        ~Session()
        {
            thread.request_stop();
            thread.join();
        }

        void  Send (std::span<const Byte> bytes)
        {
            Assert::AreEqual (S_OK, clientEnd->Write (bytes.data(), bytes.size()));
        }

        std::vector<Byte>  Receive (size_t count, DWORD timeoutMs = 5000)
        {
            std::vector<Byte>  bytes;
            Byte               chunk[4096];
            size_t             got   = 0;
            auto               until = std::chrono::steady_clock::now() + std::chrono::milliseconds (timeoutMs);

            while (bytes.size() < count && std::chrono::steady_clock::now() < until)
            {
                Assert::AreEqual (S_OK, clientEnd->Read (chunk, std::min (sizeof (chunk), count - bytes.size()), 100, got));
                bytes.insert (bytes.end(), chunk, chunk + got);
            }

            return bytes;
        }

        //  Nothing more from the host within the wait.
        bool  IsQuiet (DWORD waitMs = 200)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (waitMs));
            return clientEnd->GetReadableCount() == 0;
        }
    };

    static void  Replay (const std::string & fixture, ICassoLinkFolder & folder, CassoLinkHost::ClockFn clock = nullptr)
    {
        Session  session (folder, clock);
        size_t   index = 0;

        for (const CassoLinkRun & run : CassoLinkCaptures::LoadRuns (fixture))
        {
            if (run.direction == '>')
            {
                session.Send (run.bytes);
            }
            else
            {
                std::vector<Byte>  got = session.Receive (run.bytes.size());

                Assert::IsTrue (got == run.bytes, std::format (L"{}: the host's reply in run {} differs", TextEncoding::NarrowToWide (fixture), index).c_str());
            }

            index++;
        }

        Assert::IsTrue (session.IsQuiet(), L"and nothing more");
    }

    static std::vector<Byte>  Packet (Byte command, std::initializer_list<Byte> payload)
    {
        std::vector<Byte>  wire;

        AssertSucceeded (CassoLinkCodec::Encode (command, std::vector<Byte> (payload), wire));
        return wire;
    }

    static std::vector<Byte>  Name (const std::string & name)
    {
        std::vector<Byte>  bytes;

        for (char c : name) { bytes.push_back ((Byte) (c | 0x80)); }
        bytes.push_back (0);
        return bytes;
    }


    TEST_METHOD (DirectoryMatchesAdtProsServer)
    {
        InMemoryCassoLinkFolder  folder;

        folder.path = "C:\\Users\\relmer\\Downloads\\ADTPro-2.1.0\\disks\\";

        for (const char * name : { "ADTPRO-2.1.0.DSK", "ADTPRO-2.1.0.PO", "ADTPROSOS-2.1.0.DSK", "D2.dsk", "VDRIVE-2.1.0.DSK", "Virtual.po", "Virtual2.po" })
        {
            folder.files[name] = std::vector<Byte> (512, 0);
        }

        Replay ("adtpro-directory.txt", folder);
    }


    //  The client sends a whole disk; the host's replies are ADTPro's, and it
    //  writes the image once, after the summary.
    TEST_METHOD (SendMatchesAdtProsServerAndWritesTheImage)
    {
        InMemoryCassoLinkFolder  folder;

        Replay ("adtpro-send-disk.txt", folder);

        Assert::AreEqual (1, folder.writes);
        std::vector<Byte>  dosOrder;

        CassoLinkHost::ToFileOrder ("CASSOCAP.DSK", CassoLinkCaptures::RebuildImage ("adtpro-send-disk.txt", '>', 280), dosOrder);
        Assert::IsTrue (folder.files["CASSOCAP.DSK"] == dosOrder, L"the .dsk is written in DOS 3.3 order");
    }


    TEST_METHOD (ReceiveMatchesAdtProsServer)
    {
        InMemoryCassoLinkFolder  folder;

        CassoLinkHost::ToFileOrder ("VDRIVE-2.1.0.DSK", CassoLinkCaptures::RebuildImage ("adtpro-receive-disk.txt", '<', 280), folder.files["VDRIVE-2.1.0.DSK"]);

        Replay ("adtpro-receive-disk.txt", folder);

        Assert::AreEqual (0, folder.writes);
    }


    //  The virtual drive: Read3 with the server's clock, and writes. The
    //  image starts as the blocks the server sent and the clock replays the
    //  times it sent with them.
    TEST_METHOD (VirtualDriveMatchesAdtProsServer)
    {
        InMemoryCassoLinkFolder              folder;
        std::vector<Byte>                    host   = CassoLinkCaptures::LoadDirection ("adtpro-vsdrive.txt", '<');
        std::vector<Byte>                    image (1600 * 512, 0);
        std::set<size_t>                     seen;
        std::deque<std::array<Byte, 4>>      stamps;
        size_t                               pos    = 0;

        while (pos + 9 + 513 <= host.size())
        {
            bool  read3 = host[pos] == 0xC5 && host[pos + 1] == 0x03 && CassoLinkCodec::ComputeEor (std::span<const Byte> (host).subspan (pos, 8)) == host[pos + 8];

            if (!read3)
            {
                pos += host[pos] == 0xC5 && host[pos + 1] == 0x02 ? 5 : 1;
                continue;
            }

            size_t  block = (size_t) host[pos + 2] | ((size_t) host[pos + 3] << 8);

            stamps.push_back ({ host[pos + 4], host[pos + 5], host[pos + 6], host[pos + 7] });

            if (seen.insert (block).second)
            {
                std::copy (host.begin() + (ptrdiff_t) (pos + 9), host.begin() + (ptrdiff_t) (pos + 9 + 512), image.begin() + (ptrdiff_t) (block * 512));
            }

            pos += 9 + 513;
        }

        folder.files["Virtual.po"] = image;

        Replay ("adtpro-vsdrive.txt", folder, [&stamps] ()
        {
            std::array<Byte, 4>  stamp = stamps.front();

            stamps.pop_front();
            return stamp;
        });

        Assert::AreEqual (10, folder.writes, L"each block write is saved");
    }


    TEST_METHOD (SizeReportsMissingAndNonImageFiles)
    {
        InMemoryCassoLinkFolder  folder;
        std::vector<Byte>        name;

        folder.files["ODD.BIN"] = std::vector<Byte> (700, 1);
        Session  session (folder);

        session.Send (Packet (CassoLinkCodec::kSize, {}));
        Assert::IsTrue (session.Receive (3) == std::vector<Byte> ({ 0x00, 0x00, CassoLinkHost::kReplyNotFound }));

        name = Name ("ODD.BIN");
        {
            std::vector<Byte>  wire;

            AssertSucceeded (CassoLinkCodec::Encode (CassoLinkCodec::kSize, name, wire));
            session.Send (wire);
        }

        Assert::IsTrue (session.Receive (3) == std::vector<Byte> ({ 0x01, 0x00, CassoLinkHost::kReplyNotAnImage }));
    }


    TEST_METHOD (WithNoFolderOpenRequestsAreRefused)
    {
        InMemoryCassoLinkFolder  folder;
        std::vector<Byte>        put = Name ("NEW.PO");
        std::vector<Byte>        wire;

        folder.open = false;
        Session  session (folder);

        put.push_back (0x18);
        put.push_back (0x01);
        AssertSucceeded (CassoLinkCodec::Encode (CassoLinkCodec::kPut, put, wire));
        session.Send (wire);
        Assert::IsTrue (session.Receive (1) == std::vector<Byte> ({ CassoLinkHost::kReplyNotFound }));

        wire.clear();
        AssertSucceeded (CassoLinkCodec::Encode (CassoLinkCodec::kCd, Name ("GAMES"), wire));
        session.Send (wire);
        Assert::IsTrue (session.Receive (1) == std::vector<Byte> ({ CassoLinkHost::kReplyUnableToCd }));
    }


    TEST_METHOD (ChangeDirectoryReportsTheFolderResult)
    {
        InMemoryCassoLinkFolder  folder;
        std::vector<Byte>        wire;
        Session                  session (folder);

        AssertSucceeded (CassoLinkCodec::Encode (CassoLinkCodec::kCd, Name ("GAMES"), wire));
        session.Send (wire);
        Assert::IsTrue (session.Receive (1) == std::vector<Byte> ({ CassoLinkHost::kReplyOk }));

        wire.clear();
        AssertSucceeded (CassoLinkCodec::Encode (CassoLinkCodec::kCd, Name ("MISSING"), wire));
        session.Send (wire);
        Assert::IsTrue (session.Receive (1) == std::vector<Byte> ({ CassoLinkHost::kReplyUnableToCd }));

        Assert::IsTrue (folder.changes == std::vector<std::string> ({ "GAMES" }));
    }


    //  ADTPro's Nibble send: each track's capture, an ACK for each packet and
    //  one for the track, then the Summary; the image is a .nib of one
    //  revolution per track, from its first address field.
    TEST_METHOD (ANibbleSend_IsWrittenAsANibImage)
    {
        InMemoryCassoLinkFolder  folder;
        Session                  session (folder);
        std::vector<Byte>        capture (CassoLinkHost::kNibbleCapture, 0xFF);
        std::vector<Byte>        wire;
        static constexpr size_t  s_kPeriod = 6400;

        //  A revolution of 6,400 nibbles, its address field 100 nibbles in,
        //  captured twice over.
        for (size_t i = 0; i < capture.size(); i++)
        {
            capture[i] = (Byte) (0x96 + (i % s_kPeriod) % 0x60);
        }

        for (size_t start = 100; start < capture.size(); start += s_kPeriod)
        {
            capture[start] = 0xD5; capture[start + 1] = 0xAA; capture[start + 2] = 0x96;
        }

        session.Send (Packet (CassoLinkCodec::kNibble, { 0xC7, 0xC1, 0xCD, 0xC5, 0x00, 0x00, 0x00 }));
        Assert::IsTrue (session.Receive (1) == std::vector<Byte> ({ CassoLinkHost::kReplyOk }));

        for (size_t track = 0; track < CassoLinkHost::kNibbleTracks; track++)
        {
            wire.clear();
            AssertSucceeded (CassoLinkCodec::EncodeImageData (0, capture, wire));
            session.Send (wire);
            Assert::IsTrue (session.Receive (2) == std::vector<Byte> ({ CassoLinkCodec::kAck, CassoLinkCodec::kAck }), L"the packet's, then the track's");
        }

        session.Send (Packet (CassoLinkCodec::kSummary, { 0x00 }));
        Assert::IsTrue (session.IsQuiet (500));

        Assert::AreEqual ((size_t) CassoLinkHost::kNibbleTracks * CassoLinkHost::kNibbleTrackSize, folder.files["GAME.nib"].size());
        Assert::IsTrue (std::equal (capture.begin() + 100, capture.begin() + 100 + s_kPeriod, folder.files["GAME.nib"].begin()), L"one revolution from the address field");
        Assert::AreEqual<Byte> (0xFF, folder.files["GAME.nib"][s_kPeriod], L"then sync bytes to the track's length");
    }



    TEST_METHOD (Home_HasNoReply)
    {
        InMemoryCassoLinkFolder  folder;
        Session                  session (folder);

        session.Send (Packet (CassoLinkCodec::kHome, {}));
        Assert::IsTrue (session.IsQuiet (500), L"ADTPro's server sends nothing back");
    }


    TEST_METHOD (DirectoryPagesHoldEighteenNames_AndFlagTheNextAfterTheText)
    {
        std::vector<std::string>  names;

        for (int i = 0; i < 20; i++) { names.push_back (std::format ("DISK{:02}.DSK", i)); }

        std::vector<Byte>  first   = CassoLinkHost::BuildDirectoryPage ("C:\\X\\", names, 0);
        std::vector<Byte>  second  = CassoLinkHost::BuildDirectoryPage ("C:\\X\\", names, 1);
        std::string        text    (first.begin(), std::find (first.begin(), first.end(), (Byte) 0));
        std::string        later   (second.begin(), std::find (second.begin(), second.end(), (Byte) 0));

        Assert::IsTrue (text.starts_with ("DIRECTORY OF: C:\\X\\\r\r" + std::string (40, '-')), L"a one-line header has a second return, so the names start on row 3");
        Assert::IsTrue (text.find ("DISK17.DSK") != std::string::npos && text.find ("DISK18.DSK") == std::string::npos);
        Assert::AreEqual<Byte> (1, first[text.size() + 1], L"another page follows, in the byte after the text's 0");
        Assert::IsTrue (later.find ("DISK18.DSK") != std::string::npos);
        Assert::AreEqual<Byte> (0, second[later.size() + 1]);
    }



    TEST_METHOD (DirectoryPages_ShortenLongNames_AndListNoFiles)
    {
        std::string        longName (45, 'A');
        std::vector<Byte>  page  = CassoLinkHost::BuildDirectoryPage ("C:\\X\\", { longName }, 0);
        std::vector<Byte>  empty = CassoLinkHost::BuildDirectoryPage ("C:\\X\\", {}, 0);
        std::string        text  (page.begin(), std::find (page.begin(), page.end(), (Byte) 0));
        std::string        none  (empty.begin(), std::find (empty.begin(), empty.end(), (Byte) 0));

        Assert::IsTrue (text.ends_with (std::string (38, 'A') + ".."), L"cut to fill its line, so no return follows");
        Assert::IsTrue (none.ends_with ("NO FILES"));
    }



    TEST_METHOD (Filespec_MatchesStarAndQuestionMarkWithoutCase)
    {
        Assert::IsTrue  (CassoLinkHost::MatchesFilespec ("GAMES.DSK", ""));
        Assert::IsTrue  (CassoLinkHost::MatchesFilespec ("GAMES.DSK", "*.dsk"));
        Assert::IsTrue  (CassoLinkHost::MatchesFilespec ("GAMES.DSK", "G?MES*"));
        Assert::IsFalse (CassoLinkHost::MatchesFilespec ("GAMES.PO",  "*.DSK"));
    }



    //  Receive's file list: Return on a name sends its page and row in place
    //  of the name, from the last listing, which a DIR's filespec narrowed.
    TEST_METHOD (TheFileChoosersToken_PicksFromTheLastListing)
    {
        InMemoryCassoLinkFolder  folder;
        Session                  session (folder);

        folder.path = "C:\\Disks\\";
        folder.files["A.DSK"]  = std::vector<Byte> (512, 0);
        folder.files["B.PO"]   = std::vector<Byte> (1024, 0);
        folder.files["C.PO"]   = std::vector<Byte> (2048, 0);

        //  DIR *.PO, page 0: B.PO on row 3, C.PO on row 4.
        session.Send (Packet (CassoLinkCodec::kDirectory, { 0xAA, 0xAE, 0xD0, 0xCF, 0x00, 0x00 }));
        Assert::IsFalse (session.Receive (2000, 1000).empty());

        session.Send (Packet (CassoLinkCodec::kSize, { 0x01, 0x01, 0x04, 0x00 }));
        Assert::IsTrue (session.Receive (3) == std::vector<Byte> ({ 0x04, 0x00, CassoLinkHost::kReplyOk }), L"C.PO's 4 blocks");
    }



    TEST_METHOD (Batch_NamesCountOnFromThePrefix_WithTheImagesKind)
    {
        InMemoryCassoLinkFolder  folder;
        std::vector<Byte>        image (CassoLinkHost::kDosImageSize, 0);
        std::vector<Byte>        wire;
        Session                  session (folder);

        folder.files["DISK0001.dsk"] = image;

        session.Send (Packet (CassoLinkCodec::kBatch, { 0xC4, 0xC9, 0xD3, 0xCB, 0x00, 0x18, 0x01 }));
        Assert::IsTrue (session.Receive (1) == std::vector<Byte> ({ CassoLinkHost::kReplyOk }));

        for (size_t block = 0; block < image.size() / 512; block += 2)
        {
            wire.clear();
            AssertSucceeded (CassoLinkCodec::EncodeImageData ((Word) block, std::span<const Byte> (image).subspan (block * 512, 1024), wire));
            session.Send (wire);
            Assert::IsTrue (session.Receive (1) == std::vector<Byte> ({ CassoLinkCodec::kAck }));
        }

        session.Send (Packet (CassoLinkCodec::kSummary, { 0x00 }));
        Assert::IsTrue (session.IsQuiet (500));
        Assert::IsTrue (folder.files.contains ("DISK0002.dsk"), L"the next free number, then .dsk for a 140 KB disk");
    }


    TEST_METHOD (ProDosDateTimeIsMinuteHourThenDate)
    {
        SYSTEMTIME  time = {};

        time.wYear   = 2026;
        time.wMonth  = 10;
        time.wDay    = 15;
        time.wHour   = 19;
        time.wMinute = 46;

        //  As ADTPro's server sent it in the virtual-drive capture.
        Assert::IsTrue (CassoLinkHost::GetProDosDateTime (time) == std::array<Byte, 4> { 0x2E, 0x13, 0x4F, 0x35 });
    }


    //  Put a whole image: the command, its packets two blocks at a time, and
    //  the summary, with the host's replies.
    static void  PutImage (Session & session, const std::string & name, const std::vector<Byte> & image)
    {
        std::vector<Byte>  payload = Name (name);
        std::vector<Byte>  wire;
        size_t             blocks  = image.size() / 512;

        payload.push_back ((Byte) (blocks & 0xFF));
        payload.push_back ((Byte) (blocks >> 8));
        AssertSucceeded (CassoLinkCodec::Encode (CassoLinkCodec::kPut, payload, wire));
        session.Send (wire);
        Assert::IsTrue (session.Receive (1) == std::vector<Byte> ({ CassoLinkHost::kReplyOk }));

        for (size_t block = 0; block < blocks; block += 2)
        {
            wire.clear();
            AssertSucceeded (CassoLinkCodec::EncodeImageData ((Word) block, std::span<const Byte> (image).subspan (block * 512, std::min<size_t> (2, blocks - block) * 512), wire));
            session.Send (wire);
            Assert::IsTrue (session.Receive (1) == std::vector<Byte> ({ CassoLinkCodec::kAck }));
        }

        session.Send (Packet (CassoLinkCodec::kSummary, { 0x00 }));
    }

    static std::vector<Byte>  TestImage (size_t blocks)
    {
        std::vector<Byte>  image (blocks * 512);

        for (size_t i = 0; i < image.size(); i++) { image[i] = (Byte) (i * 31 + i / 512); }
        return image;
    }


    TEST_METHOD (TheFolderFollowsTheTab)
    {
        Assert::AreEqual (std::wstring (L"C:\\Disks"), CassoLinkFolder::GetFolderFor (Location::MakeHostFolder (L"C:\\Disks")));
        Assert::AreEqual (std::wstring (L"C:\\Disks"), CassoLinkFolder::GetFolderFor (Location::MakeDiskImage (L"C:\\Disks\\Game.dsk")));
        Assert::AreEqual (std::wstring (L"C:\\Disks"), CassoLinkFolder::GetFolderFor (Location::MakeDiskDirectory (L"C:\\Disks\\Work.po", "SUB")));
        Assert::IsTrue (CassoLinkFolder::GetFolderFor (Location::MakeRoot (L"thispc")).empty(), L"This PC has no folder");
        Assert::IsTrue (CassoLinkFolder::GetFolderFor (Location::MakeRecycleBin()).empty());
        Assert::IsTrue (CassoLinkFolder::GetFolderFor (Location::MakeShellFolder (L"::{GALLERY}", L"Gallery")).empty());
    }


    TEST_METHOD (WithThisPcInTheTabTheHostIsUnable)
    {
        InMemoryFileSystem  fileSystem;
        FakeDiskFileIo      fileIo;
        CassoLinkFolder     folder (fileSystem, fileIo);
        std::vector<Byte>   wire;

        folder.SetLocation (Location::MakeRoot (L"thispc"));
        Session  session (folder);

        AssertSucceeded (CassoLinkCodec::Encode (CassoLinkCodec::kGet, Name ("GAME.DSK"), wire));
        session.Send (wire);
        Assert::IsTrue (session.Receive (1) == std::vector<Byte> ({ CassoLinkHost::kReplyNotFound }));
    }


    //  A tab inside a disk image serves the folder the image is in.
    TEST_METHOD (ATabInsideAnImageServesTheImagesFolder)
    {
        InMemoryFileSystem  fileSystem;
        FakeDiskFileIo      fileIo;
        CassoLinkFolder     folder (fileSystem, fileIo);
        std::vector<Byte>   wire;

        fileIo.files["C:\\Disks\\GAME.DSK"] = std::vector<Byte> (143360, 0);
        AssertSucceeded (fileSystem.WriteAllText (L"C:\\Disks\\GAME.DSK", ""));
        folder.SetLocation (Location::MakeDiskDirectory (L"C:\\Disks\\Work.po", "SUB"));
        Session  session (folder);

        AssertSucceeded (CassoLinkCodec::Encode (CassoLinkCodec::kSize, Name ("GAME.DSK"), wire));
        session.Send (wire);
        Assert::IsTrue (session.Receive (3) == std::vector<Byte> ({ 0x18, 0x01, CassoLinkHost::kReplyOk }));

        Assert::AreEqual (std::string ("C:\\Disks\\"), folder.GetPath());
    }


    //  The image lands in one step, with no temporary file left beside it.
    TEST_METHOD (APutImageIsWrittenDurably)
    {
        InMemoryFileSystem  fileSystem;
        FakeDiskFileIo      fileIo;
        CassoLinkFolder     folder (fileSystem, fileIo);
        std::vector<Byte>   image = TestImage (8);

        folder.SetLocation (Location::MakeHostFolder (L"C:\\Disks"));

        {
            Session  session (folder);

            PutImage (session, "NEW.PO", image);
            Assert::IsTrue (session.IsQuiet());
        }

        Assert::IsTrue (fileIo.files["C:\\Disks\\NEW.PO"] == image);
        Assert::AreEqual<size_t> (1, fileIo.files.size(), L"no temporary left behind");
    }


    TEST_METHOD (ADamagedPacketIsNakedAndItsResendAccepted)
    {
        InMemoryCassoLinkFolder  folder;
        std::vector<Byte>        image   = TestImage (2);
        std::vector<Byte>        payload = Name ("NEW.PO");
        std::vector<Byte>        wire;
        Session                  session (folder);

        payload.push_back (2);
        payload.push_back (0);
        AssertSucceeded (CassoLinkCodec::Encode (CassoLinkCodec::kPut, payload, wire));
        session.Send (wire);
        Assert::IsTrue (session.Receive (1) == std::vector<Byte> ({ CassoLinkHost::kReplyOk }));

        wire.clear();
        AssertSucceeded (CassoLinkCodec::EncodeImageData (0, image, wire));
        wire[wire.size() - 1] ^= 0xFF;
        session.Send (wire);
        Assert::IsTrue (session.Receive (1) == std::vector<Byte> ({ CassoLinkCodec::kNak }), L"a bad CRC is NAKed");

        wire[wire.size() - 1] ^= 0xFF;
        session.Send (wire);
        Assert::IsTrue (session.Receive (1) == std::vector<Byte> ({ CassoLinkCodec::kAck }));

        session.Send (Packet (CassoLinkCodec::kSummary, { 0x00 }));
        Assert::IsTrue (session.IsQuiet());
        Assert::IsTrue (folder.files["NEW.PO"] == image);
    }


    //  The client NAKs block 0 past the limit: the packet is sent once and
    //  resent kMaxResends times, then the host sends Home and ends the transfer.
    TEST_METHOD (GetGivesUpAfterTheRetryLimitWithHome)
    {
        InMemoryCassoLinkFolder  folder;
        std::vector<Byte>        payload = Name ("GAME.DSK");
        std::vector<Byte>        wire;
        std::vector<Byte>        packet;
        Session                  session (folder);

        folder.files["GAME.DSK"] = TestImage (4);
        AssertSucceeded (CassoLinkCodec::EncodeImageData (0, std::span<const Byte> (folder.files["GAME.DSK"]).first (1024), packet));

        payload.push_back (2);
        AssertSucceeded (CassoLinkCodec::Encode (CassoLinkCodec::kGet, payload, wire));
        session.Send (wire);
        Assert::IsTrue (session.Receive (1) == std::vector<Byte> ({ CassoLinkHost::kReplyOk }));

        for (int attempt = 0; attempt <= CassoLinkHost::kMaxResends; attempt++)
        {
            wire.clear();
            AssertSucceeded (CassoLinkCodec::EncodeAcknowledge (attempt == 0 ? CassoLinkCodec::kAck : CassoLinkCodec::kNak, 0, wire));
            session.Send (wire);
            Assert::IsTrue (session.Receive (packet.size()) == packet, L"block 0 again");
        }

        wire.clear();
        AssertSucceeded (CassoLinkCodec::EncodeAcknowledge (CassoLinkCodec::kNak, 0, wire));
        session.Send (wire);
        Assert::IsTrue (session.Receive (5) == std::vector<Byte> ({ 0xC1, 0x00, 0x00, 0xD8, 0x19 }));
        Assert::IsTrue (session.IsQuiet());
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_RETRY), session.host->GetLastResult());
    }


    //  ADTPro's client writes each 40 blocks to its disk before it requests
    //  the next, which at the Apple's own speed takes longer than a reply's
    //  timeout; the transfer waits for it.
    TEST_METHOD (AGetWaitsForTheClientBetweenChunks)
    {
        InMemoryCassoLinkFolder  folder;
        std::vector<Byte>        payload = Name ("GAME.PO");
        std::vector<Byte>        wire;
        std::vector<Byte>        first;
        std::vector<Byte>        second;
        Session                  session (folder);

        session.host->SetReplyTimeout (100);
        folder.files["GAME.PO"] = TestImage (4);
        AssertSucceeded (CassoLinkCodec::EncodeImageData (0, std::span<const Byte> (folder.files["GAME.PO"]).first (1024), first));
        AssertSucceeded (CassoLinkCodec::EncodeImageData (2, std::span<const Byte> (folder.files["GAME.PO"]).subspan (1024, 1024), second));

        payload.push_back (2);
        AssertSucceeded (CassoLinkCodec::Encode (CassoLinkCodec::kGet, payload, wire));
        session.Send (wire);
        Assert::IsTrue (session.Receive (1) == std::vector<Byte> ({ CassoLinkHost::kReplyOk }));

        wire.clear();
        AssertSucceeded (CassoLinkCodec::EncodeAcknowledge (CassoLinkCodec::kAck, 0, wire));
        session.Send (wire);
        Assert::IsTrue (session.Receive (first.size()) == first);

        std::this_thread::sleep_for (std::chrono::milliseconds (500));

        wire.clear();
        AssertSucceeded (CassoLinkCodec::EncodeAcknowledge (CassoLinkCodec::kAck, 2, wire));
        session.Send (wire);
        Assert::IsTrue (session.Receive (second.size()) == second, L"blocks 2 and 3, after the pause");
    }


    //  The client stops partway through a send: after the timeout nothing is
    //  written, and the next send, with neither end restarted, succeeds.
    TEST_METHOD (AStalledTransferWritesNothingAndTheNextSucceeds)
    {
        InMemoryCassoLinkFolder  folder;
        std::vector<Byte>        image   = TestImage (8);
        std::vector<Byte>        payload = Name ("FIRST.PO");
        std::vector<Byte>        wire;
        Session                  session (folder);

        session.host->SetOperationTimeout (200);

        payload.push_back (8);
        payload.push_back (0);
        AssertSucceeded (CassoLinkCodec::Encode (CassoLinkCodec::kPut, payload, wire));
        session.Send (wire);
        Assert::IsTrue (session.Receive (1) == std::vector<Byte> ({ CassoLinkHost::kReplyOk }));

        wire.clear();
        AssertSucceeded (CassoLinkCodec::EncodeImageData (0, std::span<const Byte> (image).first (1024), wire));
        session.Send (wire);
        Assert::IsTrue (session.Receive (1) == std::vector<Byte> ({ CassoLinkCodec::kAck }));

        std::this_thread::sleep_for (std::chrono::milliseconds (500));

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_TIMEOUT), session.host->GetLastResult());
        Assert::AreEqual (0, folder.writes, L"a failed transfer writes nothing");

        PutImage (session, "SECOND.PO", image);
        Assert::IsTrue (session.IsQuiet());
        Assert::IsTrue (folder.files["SECOND.PO"] == image);
        Assert::IsFalse (folder.files.contains ("FIRST.PO"));
    }


    TEST_METHOD (ASummaryBeforeEveryBlockWritesNothing)
    {
        InMemoryCassoLinkFolder  folder;
        std::vector<Byte>        image   = TestImage (4);
        std::vector<Byte>        payload = Name ("PART.DSK");
        std::vector<Byte>        wire;
        Session                  session (folder);

        payload.push_back (4);
        payload.push_back (0);
        AssertSucceeded (CassoLinkCodec::Encode (CassoLinkCodec::kPut, payload, wire));
        session.Send (wire);
        Assert::IsTrue (session.Receive (1) == std::vector<Byte> ({ CassoLinkHost::kReplyOk }));

        wire.clear();
        AssertSucceeded (CassoLinkCodec::EncodeImageData (0, std::span<const Byte> (image).first (1024), wire));
        session.Send (wire);
        Assert::IsTrue (session.Receive (1) == std::vector<Byte> ({ CassoLinkCodec::kAck }));

        session.Send (Packet (CassoLinkCodec::kSummary, { 0x00 }));
        Assert::IsTrue (session.IsQuiet());
        Assert::AreEqual (0, folder.writes);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), session.host->GetLastResult());
    }


    //  A resend under a block already received is acknowledged and its data
    //  dropped, as ADTPro's client sends one when an ACK is dropped.
    TEST_METHOD (APacketForBlocksAlreadyReceivedIsAcknowledgedAndDropped)
    {
        InMemoryCassoLinkFolder  folder;
        std::vector<Byte>        image   = TestImage (4);
        std::vector<Byte>        wrong (1024, 0xEE);
        std::vector<Byte>        payload = Name ("DUP.PO");
        std::vector<Byte>        wire;
        Session                  session (folder);

        payload.push_back (4);
        payload.push_back (0);
        AssertSucceeded (CassoLinkCodec::Encode (CassoLinkCodec::kPut, payload, wire));
        session.Send (wire);
        Assert::IsTrue (session.Receive (1) == std::vector<Byte> ({ CassoLinkHost::kReplyOk }));

        for (const std::vector<Byte> * data : { &image, &wrong })
        {
            wire.clear();
            AssertSucceeded (CassoLinkCodec::EncodeImageData (0, std::span<const Byte> (*data).first (1024), wire));
            session.Send (wire);
            Assert::IsTrue (session.Receive (1) == std::vector<Byte> ({ CassoLinkCodec::kAck }));
        }

        wire.clear();
        AssertSucceeded (CassoLinkCodec::EncodeImageData (2, std::span<const Byte> (image).subspan (1024), wire));
        session.Send (wire);
        Assert::IsTrue (session.Receive (1) == std::vector<Byte> ({ CassoLinkCodec::kAck }));

        session.Send (Packet (CassoLinkCodec::kSummary, { 0x00 }));
        Assert::IsTrue (session.IsQuiet());
        Assert::IsTrue (folder.files["DUP.PO"] == image);
    }


    //  Block 0 of a .dsk is DOS 3.3 sectors 0 and 14 of track 0; block 1 is 13
    //  and 12. A .po is in block order already, and so is a .dsk of another size.
    TEST_METHOD (ADskIsServedInBlockOrder)
    {
        std::vector<Byte>  file (143360);
        std::vector<Byte>  blocks;
        std::vector<Byte>  back;

        for (size_t i = 0; i < file.size(); i++) { file[i] = (Byte) (i / 256); }

        CassoLinkHost::ToBlockOrder ("GAME.DSK", file, blocks);
        Assert::AreEqual<Byte> (0,  blocks[0]);
        Assert::AreEqual<Byte> (14, blocks[256]);
        Assert::AreEqual<Byte> (13, blocks[512]);
        Assert::AreEqual<Byte> (12, blocks[768]);
        Assert::AreEqual<Byte> (16 + 15, blocks[(8 * 2 - 1) * 256 + 16 * 256], L"track 1's last block ends with its sector 15");

        CassoLinkHost::ToFileOrder ("GAME.DSK", blocks, back);
        Assert::IsTrue (back == file);

        CassoLinkHost::ToBlockOrder ("GAME.PO", file, blocks);
        Assert::IsTrue (blocks == file);
        Assert::IsFalse (CassoLinkHost::IsDosOrder ("BIG.DSK", 819200));
    }


    //  A program that does not assemble is reported with its first error, or
    //  as too large when there is no error text.
    TEST_METHOD (AnAssemblyFailureIsDescribedByItsFirstError)
    {
        Assert::AreEqual (std::string ("Casso's Apple program did not assemble: Program.s(12): branch out of range"),
                          CassoLinkHost::DescribeAssemblyFailure ("Program.s(12): branch out of range\nProgram.s(40): undefined label"));
        Assert::AreEqual (std::string ("Casso's Apple program does not fit below the shim"),
                          CassoLinkHost::DescribeAssemblyFailure (""));
    }
};
