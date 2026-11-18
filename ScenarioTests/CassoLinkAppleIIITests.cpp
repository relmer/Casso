#include "Pch.h"

#include "EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "GuestSession.h"
#include "CassoLink/CassoLinkSession.h"
#include "CassoLink/CassoLinkSosDisk.h"
#include "CassoLink/InMemoryCassoLinkFolder.h"
#include "CassoLink/Session/CassoLinkBlockDevices.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Seams/Win32ByteStreamFactory.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkAppleIIITests
//
//  Story 14's operations on an Apple /// in MAME (SC-059): the CassoLink SOS
//  disk made from the user's SOS disk boots in MAME's apple3, headless, with
//  its serial port on a TCP socket to a Wi-Fi modem connection, which
//  listens. A blank ProDOS disk is in the external drive. The Apple /// is
//  listed with both drives, a file is copied each way, a file renamed and
//  deleted and a folder made, and each disk read to an image and an image
//  written to the external drive.
//
//  The SOS disk is Apple's System Demo disk, the stock disk whose SOS.DRIVER
//  has .RS232. Run only when CASSO_MAME_APPLE3 is set and MAME is installed
//  with its apple3 ROMs; it passes with nothing done otherwise. The link runs
//  at 9,600 baud, as MAME's null modem does by default, and each whole disk
//  takes about two and a half minutes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkAppleIIITests)
{
public:

    static constexpr uint16_t      kPort        = 19801;
    static constexpr DWORD         kBaud        = 9600;
    static constexpr DWORD         kConnectMs   = 120'000;
    static constexpr DWORD         kWholeDiskMs = 600'000;
    static constexpr DWORD         kFileMs      = 180'000;
    static constexpr size_t        kSector      = 256;
    static constexpr size_t        kBlock       = 512;
    static constexpr size_t        kBlocks      = 280;
    static constexpr size_t        kTrackBlocks = 8;
    static constexpr int           kDosSector[] = { 0, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 15 };
    static constexpr const char *  kD1Image     = "C:\\a3\\d1.po";
    static constexpr const char *  kD2Image     = "C:\\a3\\d2.po";
    static constexpr const char *  kD2Written   = "C:\\a3\\d2-written.po";



    struct SessionSink : ICassoLinkSessionSink
    {
        std::mutex                                    lock;
        std::condition_variable                       changed;
        std::vector<std::shared_ptr<LinkOperation>>   done;
        std::vector<LinkState>                        states;

        void  OnStateChanged (LinkState state) override
        {
            std::lock_guard<std::mutex>  guard (lock);

            states.push_back (state);
            changed.notify_all();
        }

        void  OnProgress (const std::shared_ptr<LinkOperation> &) override
        {
        }

        void  OnDone (const std::shared_ptr<LinkOperation> & operation) override
        {
            std::lock_guard<std::mutex>  guard (lock);

            done.push_back (operation);
            changed.notify_all();
        }

        bool  WaitForDone (const std::shared_ptr<LinkOperation> & operation, DWORD timeoutMs)
        {
            std::unique_lock<std::mutex>  guard (lock);

            return changed.wait_for (guard, std::chrono::milliseconds (timeoutMs), [&] () { return std::find (done.begin(), done.end(), operation) != done.end(); });
        }

        bool  WaitForState (LinkState state, DWORD timeoutMs)
        {
            std::unique_lock<std::mutex>  guard (lock);

            return changed.wait_for (guard, std::chrono::milliseconds (timeoutMs), [&] () { return !states.empty() && states.back() == state; });
        }
    };


    //  MAME, headless, with no window to take the focus, ended when the test
    //  ends.
    struct Mame
    {
        PROCESS_INFORMATION  process = {};

        ~Mame()
        {
            if (process.hProcess != nullptr)
            {
                TerminateProcess (process.hProcess, 0);
                WaitForSingleObject (process.hProcess, INFINITE);
                CloseHandle (process.hProcess);
                CloseHandle (process.hThread);
            }
        }

        void  Start (const std::filesystem::path & mame, const std::filesystem::path & d1, const std::filesystem::path & d2)
        {
            STARTUPINFOW  startup = { sizeof (startup) };
            std::wstring  command = std::format (L"\"{}\" apple3 -flop1 \"{}\" -flop2 \"{}\" -rs232 null_modem -bitb socket.127.0.0.1:{} "
                                                 L"-video none -sound none -skip_gameinfo -seconds_to_run 3600",
                                                 mame.wstring(), d1.wstring(), d2.wstring(), kPort);
            BOOL          started = FALSE;



            started = CreateProcessW (nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                                      mame.parent_path().c_str(), &startup, &process);
            Assert::IsTrue (started != FALSE, L"MAME starts");
        }
    };


    static std::string  GetSetting (const char * name)
    {
        char         * value  = nullptr;
        size_t         length = 0;
        std::string    text;



        if (_dupenv_s (&value, &length, name) == 0 && value != nullptr)
        {
            text = value;
            free (value);
        }

        return text;
    }


    static void  WriteHostFile (const std::filesystem::path & path, std::span<const Byte> bytes)
    {
        std::ofstream  out (path, std::ios::binary);



        out.write ((const char *) bytes.data(), (std::streamsize) bytes.size());
    }


    //  A 140K disk's sectors in DOS order, as a .dsk has them, in ProDOS
    //  block order, as a block device's image has them.
    static std::vector<Byte>  ToBlockOrder (const std::vector<Byte> & dosOrder)
    {
        std::vector<Byte>  out (kBlocks * kBlock, 0);



        for (size_t block = 0; block < kBlocks; block++)
        {
            size_t  track = block / kTrackBlocks;

            for (size_t half = 0; half < 2; half++)
            {
                size_t  from = (track * 16 + (size_t) kDosSector[(block % kTrackBlocks) * 2 + half]) * kSector;

                std::copy_n (dosOrder.begin() + (ptrdiff_t) from, kSector, out.begin() + (ptrdiff_t) ((block * 2 + half) * kSector));
            }
        }

        return out;
    }


    static std::vector<std::string>  GetNames (const VolumeListing & listing)
    {
        std::vector<std::string>  files;



        for (const FileEntry & entry : listing.entries)
        {
            files.push_back (entry.name);
        }

        return files;
    }


    static std::shared_ptr<LinkOperation>  Finish (SessionSink & sink, std::shared_ptr<LinkOperation> operation, DWORD timeoutMs)
    {
        Assert::IsTrue (sink.WaitForDone (operation, timeoutMs), L"the operation finishes");
        Assert::IsTrue (SUCCEEDED (operation->result), std::format (L"{:08X} {}", (uint32_t) operation->result, TextEncoding::NarrowToWide (operation->message)).c_str());

        return operation;
    }


    static FilePayload  MakeBinary (size_t size, Byte seed)
    {
        FilePayload  payload;



        payload.bytes.resize (size);

        for (size_t i = 0; i < size; i++)
        {
            payload.bytes[i] = (Byte) (seed + i * 13);
        }

        payload.type           = ProDosVolume::kTypeBinary;
        payload.loadAddress    = 0x2000;
        payload.hasLoadAddress = true;

        return payload;
    }




    TEST_METHOD (AnAppleIIIInMame_RunsStory14sOperations)
    {
        std::string                        run     = GetSetting ("CASSO_MAME_APPLE3");
        std::filesystem::path              mame    = std::filesystem::path (GetSetting ("LOCALAPPDATA")) / "Programs" / "MAME" / "mame.exe";
        std::filesystem::path              folder  = std::filesystem::temp_directory_path() / "CassoAppleIII";
        std::vector<Byte>                  sosDisk;
        std::vector<Byte>                  linkDisk;
        std::vector<Byte>                  blank (kBlocks * kBlock, 0);
        std::vector<Byte>                  toWrite (kBlocks * kBlock, 0);
        std::vector<Byte>                  next;
        std::string                        error;
        FilePayload                        kernel;
        FilePayload                        copied  = MakeBinary (3000, 5);
        FilePayload                        onDisk  = MakeBinary (1500, 9);
        Win32ByteStreamFactory             factory;
        InMemoryCassoLinkFolder            linkFolder;
        FakeDiskFileIo                     fileIo;
        SessionSink                        sink;
        std::unique_ptr<CassoLinkSession>  session;
        CassoLinkSettings                  settings;
        Mame                               emulator;
        int                                d1      = CassoLinkBlockDevices::kFirstDrive;
        int                                d2      = CassoLinkBlockDevices::kFirstDrive + 1;



        if (run.empty())
        {
            Logger::WriteMessage ("NOT RUN: set CASSO_MAME_APPLE3 to run the Apple /// in MAME\n");
            return;
        }

        Assert::IsTrue (std::filesystem::exists (mame), L"MAME is installed");

        //  The link disk, a blank external disk, and the image written to
        //  the external drive: a ProDOS volume with a file.
        sosDisk = GuestSession::RequireSosDemoDisk();
        AssertSucceeded (CassoLinkSosDisk::Make (sosDisk, "sos-demo.dsk", kBaud, linkDisk, error));
        AssertSucceeded (ProDosSkeleton::Write (blank, "EXTERNAL"));
        AssertSucceeded (ProDosSkeleton::Write (toWrite, "WRITTEN"));
        AssertSucceeded (ProDosVolume (toWrite).Write (FilePath::Parse ("ONDISK"), onDisk, next));
        toWrite = std::move (next);
        AssertSucceeded (ProDosVolume (linkDisk).Read (FilePath::Parse ("SOS.KERNEL"), kernel));

        std::filesystem::create_directories (folder);
        WriteHostFile (folder / "d1.dsk", linkDisk);
        WriteHostFile (folder / "d2.dsk", blank);

        fileIo.files[kD2Written]  = ToBlockOrder (toWrite);
        fileIo.stamps[kD2Written] = FileStamp { toWrite.size(), 1 };

        //  The connection listens; MAME connects to it once it starts.
        settings.name      = "Apple3";
        settings.transport = LinkTransport::WifiModem;
        settings.address   = std::format ("127.0.0.1:{}", kPort);
        settings.baud      = kBaud;

        session = std::make_unique<CassoLinkSession> (factory, linkFolder, fileIo, sink);
        session->SetConnectTimeout (kConnectMs);
        AssertSucceeded (session->Start (settings));

        emulator.Start (mame, folder / "d1.dsk", folder / "d2.dsk");

        Assert::IsTrue (sink.WaitForState (LinkState::Ready, kConnectMs), L"the Apple /// connects");
        Assert::AreEqual (std::wstring (L"Apple ///"), session->GetModelName());

        //  The root: the built-in and external drives.
        Finish (sink, session->ListBlockDevices(), kFileMs);

        std::vector<CassoLinkBlockDevice>  devices = session->GetBlockDevices();

        Assert::IsTrue (devices.size() >= 2, L"the built-in and external drives");
        Assert::AreEqual (std::string (".D1"),       devices[0].device);
        Assert::AreEqual (std::string ("CASSOLINK"), devices[0].volume);
        Assert::AreEqual (std::string (".D2"),       devices[1].device);
        Assert::AreEqual (std::string ("EXTERNAL"),  devices[1].volume);
        Assert::AreEqual ((uint32_t) kBlocks,        devices[0].blocks);

        //  Browse, and a file copied from the Apple.
        auto  catalog = Finish (sink, session->Enumerate (d1, FilePath()), kFileMs);
        auto  files   = GetNames (catalog->listing);

        for (const std::string & file : files)
        {
            Logger::WriteMessage (("on .D1: " + file + "\n").c_str());
        }

        Assert::IsTrue (std::find (files.begin(), files.end(), "SOS.KERNEL") != files.end(), TextEncoding::NarrowToWide (".D1 has: " + std::accumulate (files.begin(), files.end(), std::string(), [] (std::string all, const std::string & file) { return all + file + " "; })).c_str());
        Assert::IsTrue (std::find (files.begin(), files.end(), "SOS.INTERP") != files.end(), L"and SOS.INTERP");

        auto  read = Finish (sink, session->ReadFile (d1, FilePath::Parse ("SOS.KERNEL")), kFileMs);

        Assert::IsTrue (read->file.bytes == kernel.bytes, L"SOS.KERNEL copied from the Apple is the disk's");

        //  A file copied to the Apple, read back, renamed and deleted, and a
        //  folder made.
        Finish (sink, session->WriteFile (d2, FilePath::Parse ("COPIED"), copied), kFileMs);

        auto  back = Finish (sink, session->ReadFile (d2, FilePath::Parse ("COPIED")), kFileMs);

        Assert::IsTrue (back->file.bytes == copied.bytes, L"the file copied to the Apple is read back");

        Finish (sink, session->Rename (d2, FilePath::Parse ("COPIED"), "RENAMED"), kFileMs);
        Finish (sink, session->MakeDirectory (d2, FilePath::Parse ("FOLDER")), kFileMs);
        Finish (sink, session->Delete (d2, FilePath::Parse ("RENAMED")), kFileMs);

        catalog = Finish (sink, session->Enumerate (d2, FilePath()), kFileMs);
        Assert::IsTrue (GetNames (catalog->listing) == std::vector<std::string> { "FOLDER" }, L".D2 has the folder alone");

        //  Each disk read to an image, and an image written to the external
        //  drive and read back.
        Finish (sink, session->ReadBlockDevice (d1, kD1Image, false), kWholeDiskMs);
        Assert::IsTrue (fileIo.files[kD1Image] == ToBlockOrder (linkDisk), L"the built-in drive's image is the link disk");

        Finish (sink, session->WriteBlockDevice (d2, kD2Written, false), kWholeDiskMs);
        Finish (sink, session->ReadBlockDevice (d2, kD2Image, false), kWholeDiskMs);
        Assert::IsTrue (fileIo.files[kD2Image] == fileIo.files[kD2Written], L"the external drive has the image written to it");

        session.reset();
    }
};
