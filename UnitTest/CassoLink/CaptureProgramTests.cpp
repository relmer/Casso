#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FixtureProvider.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/LinkProgram.h"
#include "CassoLink/Session/CaptureStream.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImage.h"
#include "ProgramRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureProgramTests
//
//  The capture module on an emulated //c: a track of a DOS 3.3 disk read
//  raw, cut to one revolution, and compared bit for bit with the track the
//  emulated drive reads.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CaptureProgramTests)
{
public:

    static std::vector<Byte>  LoadFixture()
    {
        FixtureProvider    fixtures;
        std::vector<Byte>  bytes;



        AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));

        return bytes;
    }

    //  The drive's track at a quarter-track position, one bit to a byte.
    static std::vector<uint8_t>  GetTrackBits (ProgramRig & rig, int quarterTrack)
    {
        DiskImage             * image = rig.machine.GetDiskStore().GetImage (ProgramRig::kSlot, 0);
        int                     slot  = image->ResolveQuarterTrack (quarterTrack);
        std::vector<uint8_t>    bits;



        for (size_t at = 0; at < image->GetTrackBitCount (slot); at++)
        {
            bits.push_back (image->ReadBit (slot, at));
        }

        return bits;
    }

    //  Whether `captured` is `track` started at some bit.
    static bool  IsRotationOf (const std::vector<uint8_t> & captured, const std::vector<uint8_t> & track)
    {
        size_t  n = track.size();



        if (captured.size() != n)
        {
            return false;
        }

        for (size_t start = 0; start < n; start++)
        {
            size_t  at = 0;

            while (at < n && track[(start + at) % n] == captured[at])
            {
                at++;
            }

            if (at == n)
            {
                return true;
            }
        }

        return false;
    }


    TEST_METHOD (WholeTrack_IsReadBitForBit)
    {
        ProgramRig            rig (LoadFixture());
        std::vector<Byte>     module;
        std::vector<Byte>     bytes;
        std::vector<uint8_t>  bits;
        std::vector<uint8_t>  track;
        CassoLinkStatus       status;
        size_t                length = 0;
        size_t                syncs  = 0;



        AssertSucceeded (LinkProgram::BuildCaptureModule (module));
        AssertSucceeded (rig.host->EnsureModule (module));
        AssertSucceeded (rig.host->CaptureQuarterTrack (1, 0, 0, 100, bytes, status));

        Assert::AreEqual ((Byte) 0, status.error);

        for (Byte b : bytes)
        {
            syncs += CaptureStream::IsSync (b) ? 1 : 0;
        }

        Assert::IsTrue (syncs > 100, L"Sync bytes are marked");
        Assert::IsTrue (CaptureStream::FindRevolution (bytes, length), L"The read repeats");

        CaptureStream::BuildRevolution (bytes, length, bits);
        track = GetTrackBits (rig, 0);

        Logger::WriteMessage (std::format ("revolution {} bytes, {} bits; the track has {} bits; {} syncs\n", length, bits.size(), track.size(), syncs).c_str());

        Assert::IsTrue (IsRotationOf (bits, track), L"One revolution is the drive's track");
    }
};
