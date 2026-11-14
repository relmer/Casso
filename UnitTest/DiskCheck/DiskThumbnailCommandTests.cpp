#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "CassoExplorer/Model/PicturePreview.h"
#include "CommandLineParser.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "DiskCheck/ThumbnailStore.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskThumbnailCommandTests
//
//  The disk tool's `thumbnail` over an image and a cache in a folder of the
//  test's in the temporary folder.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskThumbnailCommandTests)
{
public:

    struct Rig
    {
        std::filesystem::path  root;
        std::filesystem::path  image;
        std::filesystem::path  cache;
        HRESULT                hrCom = S_OK;

        Rig()
        {
            static int  s_next = 0;

            hrCom = ::CoInitializeEx (nullptr, COINIT_MULTITHREADED);
            root  = std::filesystem::temp_directory_path() / std::format (L"CassoThumbCmd{}_{}", ::GetCurrentProcessId(), s_next++);
            image = root / L"0001.dsk";
            cache = root / L"cache";

            std::filesystem::remove_all (root);
            std::filesystem::create_directories (root);

            Write (image, std::vector<Byte> (4096, 0x5A));
        }

        ~Rig()
        {
            std::error_code  error;

            std::filesystem::remove_all (root, error);

            if (SUCCEEDED (hrCom))
            {
                ::CoUninitialize();
            }
        }

        static void  Write (const std::filesystem::path & path, const std::vector<Byte> & bytes)
        {
            std::ofstream  out (path, std::ios::binary | std::ios::trunc);

            out.write ((const char *) bytes.data(), (std::streamsize) bytes.size());
        }

        static std::vector<Byte>  Read (const std::filesystem::path & path)
        {
            std::ifstream  in (path, std::ios::binary);

            return std::vector<Byte> (std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>());
        }

        static std::string  Narrow (const std::filesystem::path & path)
        {
            return path.string();
        }

        DiskCommandResult  Run (std::vector<std::string> args)
        {
            FakeDiskFileIo             io;
            DiskCommandRunner          runner (io);
            std::vector<char *>        argv;
            CommandLineOptions         options;



            args.insert (args.begin(), { "CassoCli", "disk", "thumbnail" });

            for (std::string & arg : args)
            {
                argv.push_back (arg.data());
            }

            options = CommandLineParser::Parse ((int) argv.size(), argv.data(), [] (const std::string &) { return false; });

            runner.SetThumbnailFolder (cache);

            return runner.Run (options);
        }

        std::vector<Byte>  MakePicture (uint32_t bgra) const
        {
            std::vector<Byte>  png;
            HRESULT            hr = PicturePreview::EncodePng (std::vector<uint32_t> ((size_t) 4 * 3, bgra), 4, 3, png);



            AssertSucceeded (hr);

            return png;
        }
    };

    //  A picture set from the command line is the image's thumbnail, marked
    //  as set by the user, and --out writes it; --remove restores the one
    //  made by booting.
    TEST_METHOD (APicture_IsSetWrittenAndRemoved)
    {
        Rig                    rig;
        std::filesystem::path  picture = rig.root / L"title.png";
        std::filesystem::path  written = rig.root / L"out.png";
        std::vector<Byte>  booted    = rig.MakePicture (0xFF0000FF);
        std::vector<Byte>  found;
        ThumbnailRecord    record;
        bool               isUserSet = false;
        DiskCommandResult  result;



        Rig::Write (picture, rig.MakePicture (0xFF00FF00));

        {
            ThumbnailStore  store (rig.cache);

            AssertSucceeded (store.Store (rig.image, booted, ThumbnailRecord()));
        }

        result = rig.Run ({ Rig::Narrow (rig.image), Rig::Narrow (picture) });
        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, TextEncoding::NarrowToWide (result.diagnostics).c_str());

        {
            ThumbnailStore  store (rig.cache);

            AssertSucceeded (store.Find (rig.image, found, record, isUserSet));
            Assert::IsTrue (isUserSet, L"Marked as set by the user");
            Assert::IsFalse (found == booted);
        }

        result = rig.Run ({ Rig::Narrow (rig.image), "--out", Rig::Narrow (written) });
        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (Rig::Read (written) == found, L"--out writes the thumbnail returned");

        result = rig.Run ({ Rig::Narrow (rig.image), "--remove" });
        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);

        {
            ThumbnailStore  store (rig.cache);

            AssertSucceeded (store.Find (rig.image, found, record, isUserSet));
            Assert::IsFalse (isUserSet);
            Assert::IsTrue (found == booted, L"The booted one again");
        }

        result = rig.Run ({ Rig::Narrow (rig.image), "--remove" });
        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus, L"Nothing set by the user to remove");
    }

    //  A file that isn't a picture fails and sets nothing; --out fails for an
    //  image with no thumbnail; no action, or two, fails.
    TEST_METHOD (ABadPictureOrActionCount_IsRefused)
    {
        Rig                    rig;
        std::filesystem::path  text    = rig.root / L"notes.txt";
        std::filesystem::path  picture = rig.root / L"title.png";
        DiskCommandResult      result;



        Rig::Write (text, std::vector<Byte> (100, 'A'));
        Rig::Write (picture, rig.MakePicture (0xFF00FF00));

        result = rig.Run ({ Rig::Narrow (rig.image), Rig::Narrow (text) });
        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::AreEqual (0, ThumbnailStore (rig.cache).CountUserSet());

        result = rig.Run ({ Rig::Narrow (rig.image), "--out", Rig::Narrow (rig.root / L"out.png") });
        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus, L"No thumbnail to write");
        Assert::IsFalse (std::filesystem::exists (rig.root / L"out.png"));

        result = rig.Run ({ Rig::Narrow (rig.image) });
        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus, L"No action");

        result = rig.Run ({ Rig::Narrow (rig.image), Rig::Narrow (picture), "--remove" });
        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus, L"Two actions");
        Assert::AreEqual (0, ThumbnailStore (rig.cache).CountUserSet());

        result = rig.Run ({ Rig::Narrow (rig.root / L"missing.dsk"), Rig::Narrow (picture) });
        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus, L"No such image");
    }
};
