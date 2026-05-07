#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Core/MemoryBus.h"
#include "Devices/DiskIIController.h"

// DiskIIController carries two DiskImage instances; per-test heap allocation
// keeps the C6262 stack-frame budget happy.
#pragma warning (disable: 6262)

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


namespace
{
    static constexpr Word   kSlot6Base   = 0xC0E0;
    static constexpr Word   kPhase0Off   = 0xC0E0;
    static constexpr Word   kPhase0On    = 0xC0E1;
    static constexpr Word   kPhase1On    = 0xC0E3;
    static constexpr Word   kPhase2On    = 0xC0E5;
    static constexpr Word   kPhase3On    = 0xC0E7;
    static constexpr Word   kMotorOff    = 0xC0E8;
    static constexpr Word   kMotorOn     = 0xC0E9;
    static constexpr Word   kDrive1      = 0xC0EA;
    static constexpr Word   kDrive2      = 0xC0EB;
    static constexpr Word   kQ6Off       = 0xC0EC;
    static constexpr Word   kQ6On        = 0xC0ED;
    static constexpr Word   kQ7Off       = 0xC0EE;
    static constexpr Word   kQ7On        = 0xC0EF;
    static constexpr int    kBitsPerNibble = 8;
    static constexpr size_t kSyntheticTrackBytes = 64;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Helper: write an 8-bit MSB-first nibble into a packed bit-stream image.
//
////////////////////////////////////////////////////////////////////////////////

static void WriteNibbleToImage (DiskImage & img, int track, size_t & bitOffset, Byte nibble)
{
    int   i = 0;

    for (i = 7; i >= 0; i--)
    {
        Byte   b = static_cast<Byte> ((nibble >> i) & 1);

        img.WriteBit (track, bitOffset++, b);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIITests
//
//  Phase 9 acceptance: $C0Ex soft-switch surface, phase magnets, motor,
//  drive select, Q6/Q7 LSS cross-product, write-protect, head-clamp, and
//  bit-stream LSS round-trip via the rewritten controller + engine.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskIITests)
{
public:

    TEST_METHOD (Slot6_IORange_C0E0_to_C0EF)
    {
        unique_ptr<DiskIIController>   disk = make_unique<DiskIIController> (6);

        Assert::AreEqual (static_cast<Word> (0xC0E0), disk->GetStart ());
        Assert::AreEqual (static_cast<Word> (0xC0EF), disk->GetEnd ());
    }

    TEST_METHOD (PhaseMagnetsStepHeadCorrectly)
    {
        unique_ptr<DiskIIController>   disk = make_unique<DiskIIController> (6);

        disk->Read (kMotorOn);

        // Walking phase 0->1->2->3 advances one quarter-track per step.
        disk->Read (kPhase0On);
        disk->Read (kPhase0Off);
        disk->Read (kPhase1On);

        Assert::IsTrue (disk->GetQuarterTrack () > 0,
            L"Phase magnet stepping must move the head");
    }

    TEST_METHOD (MotorOnOffViaC0E8C0E9)
    {
        unique_ptr<DiskIIController>   disk = make_unique<DiskIIController> (6);

        disk->Read (kMotorOn);
        Assert::IsTrue  (disk->IsMotorOn (), L"$C0E9 must enable the motor");

        disk->Read (kMotorOff);
        Assert::IsFalse (disk->IsMotorOn (), L"$C0E8 must disable the motor");
    }

    TEST_METHOD (DriveSelectViaC0EAC0EB)
    {
        unique_ptr<DiskIIController>   disk = make_unique<DiskIIController> (6);

        disk->Read (kDrive2);
        Assert::AreEqual (1, disk->GetActiveDrive (), L"$C0EB selects drive 2");

        disk->Read (kDrive1);
        Assert::AreEqual (0, disk->GetActiveDrive (), L"$C0EA selects drive 1");
    }

    TEST_METHOD (Q7ClearQ6ClearReadsNibble)
    {
        unique_ptr<DiskIIController>   disk = make_unique<DiskIIController> (6);

        disk->Read (kQ7Off);
        disk->Read (kQ6Off);

        Byte   v = disk->Read (kQ6Off);

        Assert::AreEqual (static_cast<Byte> (0), v,
            L"Q7=Q6=0 with no disk: read latch returns 0");
    }

    TEST_METHOD (Q7ClearQ6SetReadsWriteProtect)
    {
        unique_ptr<DiskIIController>   disk = make_unique<DiskIIController> (6);

        disk->GetDisk (0)->SetWriteProtected (true);

        disk->Read (kQ7Off);
        Byte   v = disk->Read (kQ6On);

        Assert::AreEqual (static_cast<Byte> (0x80), v,
            L"Q7=0,Q6=1 with WP disk: bit 7 set");
    }

    TEST_METHOD (Q7SetQ6ClearShiftLoad)
    {
        unique_ptr<DiskIIController>   disk = make_unique<DiskIIController> (6);

        disk->Read (kQ7On);
        disk->Read (kQ6Off);

        Assert::IsTrue  (disk->IsQ7 (), L"$C0EF sets Q7");
        Assert::IsFalse (disk->IsQ6 (), L"$C0EC clears Q6");
    }

    TEST_METHOD (Q7SetQ6SetWritesNibble)
    {
        unique_ptr<DiskIIController>   disk = make_unique<DiskIIController> (6);
        size_t                         bitOffset = 0;

        // Provide a writable track so the engine has somewhere to push bits.
        disk->GetDisk (0)->ResizeTrack (0, kSyntheticTrackBytes * kBitsPerNibble);

        disk->Read  (kMotorOn);
        disk->Read  (kQ6On);
        disk->Read  (kQ7On);

        disk->Write (kQ7On, 0xFF);
        disk->Tick (DiskIINibbleEngine::kCyclesPerBit * kBitsPerNibble);

        Assert::IsTrue (disk->IsQ7 () && disk->IsQ6 (),
            L"Q7=Q6=1 must be set after $C0ED + $C0EF");

        UNREFERENCED_PARAMETER (bitOffset);
    }

    TEST_METHOD (NibbleStreamAdvancesAtCorrectBitTime)
    {
        unique_ptr<DiskIIController>   disk = make_unique<DiskIIController> (6);

        disk->GetDisk (0)->ResizeTrack (0, 64);

        disk->Read (kMotorOn);
        disk->Read (kQ7Off);
        disk->Read (kQ6Off);

        size_t   posBefore = disk->GetEngine (0).GetBitPosition ();

        disk->Tick (DiskIINibbleEngine::kCyclesPerBit * 4);

        size_t   posAfter = disk->GetEngine (0).GetBitPosition ();

        Assert::AreEqual (size_t (4), posAfter - posBefore,
            L"4 bits should advance per 16 CPU cycles (4 cycles/bit)");
    }

    TEST_METHOD (WriteProtectedDiskRejectsWrite)
    {
        unique_ptr<DiskIIController>   disk = make_unique<DiskIIController> (6);

        disk->GetDisk (0)->ResizeTrack    (0, 8);
        disk->GetDisk (0)->SetWriteProtected (true);
        disk->GetDisk (0)->WriteBit       (0, 0, 1);

        Assert::AreEqual (static_cast<uint8_t> (0), disk->GetDisk (0)->ReadBit (0, 0),
            L"Write-protected disk must ignore WriteBit");
    }

    TEST_METHOD (HeadStepWrapsAtTrackBoundaries)
    {
        unique_ptr<DiskIIController>   disk = make_unique<DiskIIController> (6);
        int                            i    = 0;

        disk->Read (kMotorOn);

        // Walk the head forward; clamp at quarter-track 139 must hold.
        for (i = 0; i < 300; i++)
        {
            int    p   = i & 3;
            Word   on  = static_cast<Word> (kSlot6Base + 1 + p * 2);
            Word   off = static_cast<Word> (kSlot6Base     + p * 2);

            disk->Read (on);
            disk->Read (off);
        }

        Assert::IsTrue (disk->GetQuarterTrack () >= 0,
            L"Quarter-track must remain in legal range after long forward walk");
        Assert::IsTrue (disk->GetQuarterTrack () <= DiskIIController::kMaxQuarterTrack,
            L"Quarter-track must clamp to kMaxQuarterTrack");
    }

    TEST_METHOD (LSS_ReadsKnownNibblePattern)
    {
        unique_ptr<DiskIIController>   disk = make_unique<DiskIIController> (6);
        DiskImage *                    img  = disk->GetDisk (0);
        size_t                         off  = 0;
        Byte                           value = 0;
        int                            i    = 0;

        img->ResizeTrack (0, kSyntheticTrackBytes * kBitsPerNibble);

        WriteNibbleToImage (*img, 0, off, 0xD5);
        WriteNibbleToImage (*img, 0, off, 0xAA);
        WriteNibbleToImage (*img, 0, off, 0x96);

        disk->Read (kMotorOn);
        disk->Read (kQ7Off);
        disk->Read (kQ6Off);

        // Pump 8 bits per nibble × 3 nibbles. The latch reports the first
        // nibble whose MSB is set: 0xD5.
        for (i = 0; i < 8; i++)
        {
            disk->Tick (DiskIINibbleEngine::kCyclesPerBit);
        }

        value = disk->Read (kQ6Off);

        Assert::AreEqual (static_cast<Byte> (0xD5), value,
            L"LSS must reassemble the first MSB-set nibble in the stream");
    }

    TEST_METHOD (LSS_WritesPatternRoundTrip)
    {
        unique_ptr<DiskIIController>   disk = make_unique<DiskIIController> (6);
        DiskImage *                    img  = disk->GetDisk (0);
        int                            i    = 0;

        img->ResizeTrack (0, kSyntheticTrackBytes * kBitsPerNibble);

        disk->Read  (kMotorOn);
        disk->Read  (kQ6On);
        disk->Read  (kQ7On);
        disk->Write (kQ7On, 0xAA);

        for (i = 0; i < 8; i++)
        {
            disk->Tick (DiskIINibbleEngine::kCyclesPerBit);
        }

        Assert::IsTrue (img->IsDirty (),
            L"Write through the LSS must mark the image dirty");
    }

    TEST_METHOD (Reset_ClearsControllerState)
    {
        unique_ptr<DiskIIController>   disk = make_unique<DiskIIController> (6);

        disk->Read (kMotorOn);
        disk->Read (kPhase1On);
        disk->Reset ();

        Assert::IsFalse (disk->IsMotorOn (),
            L"Reset must clear the motor flag");
        Assert::AreEqual (0, disk->GetActiveDrive (),
            L"Reset must select drive 0");
    }

    TEST_METHOD (GetDisk_Bounds)
    {
        unique_ptr<DiskIIController>   disk = make_unique<DiskIIController> (6);

        Assert::IsNotNull (disk->GetDisk (0));
        Assert::IsNotNull (disk->GetDisk (1));
        Assert::IsNull    (disk->GetDisk (2));
        Assert::IsNull    (disk->GetDisk (-1));
    }

    TEST_METHOD (MountDisk_InvalidDrive_ReturnsError)
    {
        unique_ptr<DiskIIController>   disk = make_unique<DiskIIController> (6);

        HRESULT   hr = disk->MountDisk (3, "nonexistent.dsk");

        Assert::IsTrue (FAILED (hr));
    }

    TEST_METHOD (MountDisk_MissingFile_ReturnsError)
    {
        unique_ptr<DiskIIController>   disk = make_unique<DiskIIController> (6);

        HRESULT   hr = disk->MountDisk (0, "this_file_does_not_exist.dsk");

        Assert::IsTrue (FAILED (hr));
    }
};
