#include "Pch.h"
#include "../EhmTestHelper.h"

#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/Dos33Catalog.h"
#include "Machines/Apple2/Common/Dos33SectorAccess.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33StartupProgram.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibbleFormat.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33StartupProgramTests
//
//  The greeting file name in the DOS image, over an in-memory volume with a
//  nonzero placeholder byte on the DOS tracks.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Dos33StartupProgramTests)
{
public:

    static constexpr Byte    kVolumeNumber   = 254;
    static constexpr int     kGreetingTrack  = 1;
    static constexpr int     kGreetingSector = 9;
    static constexpr size_t  kGreetingOffset = 0x75;
    static constexpr size_t  kNameBytes      = 30;
    static constexpr Byte    kDosPlaceholder = 0x4C;

    static vector<Byte> MakeVolumeWithHello (bool withDos)
    {
        vector<Byte>  buffer (NibbleFormat::kImageByteSize, 0);

        AssertSucceeded (Dos33Skeleton::Write (buffer, kVolumeNumber));
        AssertSucceeded (Dos33FileWriter::WriteHello (buffer));

        if (withDos)
        {
            buffer[Dos33Skeleton::GetSectorOffset (0, 0)] = kDosPlaceholder;
        }

        return buffer;
    }

    TEST_METHOD (HasDosImage_BlankDosTracks_IsFalse)
    {
        vector<Byte>         buffer = MakeVolumeWithHello (false);
        Dos33SectorAccess    access (buffer);
        Dos33StartupProgram  startup (access);

        Assert::IsFalse (startup.HasDosImage());
    }

    TEST_METHOD (SetStartupProgram_WithoutDos_IsNotSupported)
    {
        vector<Byte>         buffer = MakeVolumeWithHello (false);
        vector<Byte>         result;
        Dos33SectorAccess    access (buffer);
        Dos33StartupProgram  startup (access);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED),
                          startup.SetStartupProgram (FilePath::Parse ("HELLO"), result));
    }

    TEST_METHOD (SetStartupProgram_WritesTheCatalogNameIntoTheDosImage)
    {
        vector<Byte>         buffer = MakeVolumeWithHello (true);
        vector<Byte>         result;
        vector<Byte>         encoded;
        Dos33SectorAccess    access (buffer);
        Dos33StartupProgram  startup (access);
        size_t               at     = Dos33Skeleton::GetSectorOffset (kGreetingTrack, kGreetingSector) + kGreetingOffset;

        Assert::IsTrue (startup.HasDosImage());
        AssertSucceeded (startup.SetStartupProgram (FilePath::Parse ("hello"), result));
        Assert::IsTrue (Dos33Catalog::TryEncodeName ("HELLO", encoded));

        Assert::IsTrue (vector<Byte> (result.begin() + (ptrdiff_t) at, result.begin() + (ptrdiff_t) (at + kNameBytes)) == encoded,
                        L"the catalog's stored form, not the form the caller passed");
    }

    TEST_METHOD (SetStartupProgram_AMissingFile_IsNotFound)
    {
        vector<Byte>         buffer = MakeVolumeWithHello (true);
        vector<Byte>         result;
        Dos33SectorAccess    access (buffer);
        Dos33StartupProgram  startup (access);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND),
                          startup.SetStartupProgram (FilePath::Parse ("NOPE"), result));
    }

    TEST_METHOD (IsRunnableAsGreeting_OnlyBasicProgramsRun)
    {
        VolumeListing  listing;
        FileEntry      binary;
        FileEntry      program;

        binary.name  = "PROG";
        binary.type  = Dos33Volume::kTypeBinary;
        program.name = "HELLO";
        program.type = Dos33Volume::kTypeApplesoft;

        listing.entries = { binary, program };

        Assert::IsFalse (Dos33StartupProgram::IsRunnableAsGreeting (listing, "PROG"));
        Assert::IsTrue  (Dos33StartupProgram::IsRunnableAsGreeting (listing, "hello"));
        Assert::IsTrue  (Dos33StartupProgram::IsRunnableAsGreeting (listing, "ABSENT"));
    }
};





