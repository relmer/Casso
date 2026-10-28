#pragma once

#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/ChangePrompt.h"
#include "Devices/Disk/DiskImageStore.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/MountDiagnosis.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreTestSupport
//
//  What the DiskImageStoreTests test classes share: the images they mount, the
//  capture of a reported save failure, and the bays they use. Each class
//  derives from it beside TestClass.
//
////////////////////////////////////////////////////////////////////////////////

class DiskStoreTestSupport
{
public:
    //  Breaks one address field's checksum so the track can no longer be
    //  denibblized without loss, which is what makes Serialize refuse.
    static void CorruptOneAddressField (DiskImage & img, int track)
    {
        vector<Byte> &  bits   = img.GetTrackBitsForWrite (track);
        size_t          i      = 0;
        bool            done   = false;



        for (i = 0; i + 10 < bits.size() && !done; i++)
        {
            if (bits[i] == 0xD5 && bits[i + 1] == 0xAA && bits[i + 2] == 0x96)
            {
                // Write a checksum that cannot be right for any header. The
                // encoded odd/even pair carries only the value's own bits, so
                // replacing both is unconditional where flipping one is not.
                bits[i + 9]  = 0xAA;
                bits[i + 10] = 0xAA;
                done         = true;
            }
        }

        Assert::IsTrue (done, L"the track must carry an address field to corrupt");
    }


    static constexpr int   kSlot  = 6;


    static constexpr int   kDrive = 0;


    vector<Byte> MakeDsk (Byte fill)
    {
        return vector<Byte> (NibblizationLayer::kImageByteSize, fill);
    }


    // A full-size sector image with a distinct per-byte pattern so a
    // round-trip actually exercises every sector (not a uniform fill).
    vector<Byte> MakeSectorImage (uint32_t seed)
    {
        vector<Byte>  raw (NibblizationLayer::kImageByteSize);

        for (size_t i = 0; i < raw.size(); i++)
        {
            raw[i] = static_cast<Byte> ((i * 31u + seed) & 0xFF);
        }

        return raw;
    }


    // A minimal single-track WOZ v2 image with a recognizable bit pattern
    // (mostly 0xFF sync + a D5 AA 96 address prolog marker).
    vector<Byte> MakeWoz (bool writeProtected = false)
    {
        vector<Byte>   bits (6400, 0xFF);
        vector<Byte>   woz;

        bits[20] = 0xD5;
        bits[21] = 0xAA;
        bits[22] = 0x96;

        WozLoader::BuildSyntheticV2 (1, writeProtected, bits, 51200, woz);
        return woz;
    }


    // Captures EhmNotifyUser output for the flush-error tests. FlushEntry
    // surfaces losses through the shared EHM notifier (CHRN/CBRN), so these
    // route the global notifier to file-local counters for the span of one
    // test. The notifier is a raw function pointer, hence a free function +
    // static state rather than a capturing lambda.
    static inline int      s_flushNotifyCount = 0;


    static inline wstring  s_flushNotifyLast;


    static void CaptureFlushNotify (const wchar_t * message)
    {
        s_flushNotifyCount++;
        s_flushNotifyLast = (message != nullptr) ? message : L"";
    }


    // RAII: routes EhmNotifyUser to the capture (resetting the counters)
    // for the test's lifetime, then clears the global hook so the next test
    // starts clean even if an assertion throws out of the body.
    struct ScopedFlushNotifyCapture
    {
        ScopedFlushNotifyCapture()
        {
            s_flushNotifyCount = 0;
            s_flushNotifyLast.clear();
            SetNotifyFunction (CaptureFlushNotify);
        }

        ~ScopedFlushNotifyCapture()
        {
            SetNotifyFunction (nullptr);
        }
    };


    // mount raw -> dirty (no bit corruption) -> flush -> captured == raw.
    void VerifyFaithfulSectorFlush (DiskFormat fmt, const char * path)
    {
        DiskImageStore   store;
        vector<Byte>     raw = MakeSectorImage (0xABCD1234u);
        vector<Byte>     captured;

        store.SetFlushSink ([&] (const string &, const vector<Byte> & b) { captured = b; return S_OK; });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, path, fmt, raw));

        // Mark dirty without touching the bit stream so the flush must
        // reproduce the original image exactly.
        store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);

        AssertSucceeded (store.Flush (kSlot, kDrive));
        Assert::IsTrue   (raw == captured,
            L"flushed bytes must faithfully round-trip the sector image");
    }


    //  A whole .nib of legal nibbles, so a track derived from the live bits
    //  is the same length and is not rotated.
    static vector<Byte> MakeLegalNib()
    {
        vector<Byte>  file (NibbleImageCodec::kNibImageSize);
        size_t        i    = 0;



        for (i = 0; i < file.size(); i++)
        {
            file[i] = static_cast<Byte> (0x80 | (i % 127));
        }

        return file;
    }


    static bool IsNibTrackEqual (const vector<Byte> & left, const vector<Byte> & right, int track)
    {
        size_t  first = static_cast<size_t> (track) * NibbleImageCodec::kNibTrackSize;



        return std::equal (left.begin()  + first, left.begin() + first + NibbleImageCodec::kNibTrackSize,
                           right.begin() + first);
    }


    //  Writes track 3 and saves, then writes track 20 and saves, and checks
    //  that the second save has both writes.
    static void SaveNibTwice (DiskImageStore & store, const vector<Byte> & file, std::function<vector<Byte>()> readSaved)
    {
        DiskImage *   image = store.GetImage (kSlot, kDrive);
        vector<Byte>  first;
        vector<Byte>  second;
        DiskImage     reloaded;



        image->WriteBit (3, 7, image->ReadBit (3, 7) ^ 1);
        AssertSucceeded (store.Flush (kSlot, kDrive));
        first = readSaved();

        Assert::IsFalse (IsNibTrackEqual (first, file, 3), L"the first save has the write to track 3");

        image->WriteBit (20, 7, image->ReadBit (20, 7) ^ 1);
        AssertSucceeded (store.Flush (kSlot, kDrive));
        second = readSaved();

        Assert::IsTrue  (IsNibTrackEqual (second, first, 3), L"track 3 of the second save is as the first saved it");
        Assert::IsFalse (IsNibTrackEqual (second, file, 20), L"and has the write to track 20");

        AssertSucceeded (NibbleImageCodec::Load (second, reloaded));
        Assert::AreEqual (image->ReadBit (3, 7), reloaded.ReadBit (3, 7), L"a reload has the first write");
    }


    static constexpr const char *  kSeamImagePath = "Q:/casso-no-such-root/image.dsk";


    static constexpr const char *  kSeamWozPath   = "Q:/casso-no-such-root/image.woz";


    //  Points the store's reads, identity checks and commits at `io`.
    static void UseFileIo (DiskImageStore & store, FakeDiskFileIo & io)
    {
        store.SetFileIo         (&io);
        store.SetImageReader    ([&io] (const string & path, vector<Byte> & bytes) { return io.ReadAllBytes (path, bytes); });
        store.SetIdentityReader ([&io] (const string & path) { return ImageIdentity::Read (io, path); });
    }


    //  Flips the first bit of track 0, so the image has a write to save.
    static void WriteOneBit (DiskImageStore & store)
    {
        DiskImage *  image = store.GetImage (kSlot, kDrive);



        image->WriteBit (0, 0, static_cast<uint8_t> (image->ReadBit (0, 0) ^ 1));
    }


    //  The recovery copies of a disk whose format cannot store its writes,
    //  through the seam.
    static size_t CountRecoveryFiles (const FakeDiskFileIo & io)
    {
        size_t  count = 0;



        for (const auto & [path, bytes] : io.files)
        {
            if (path.find (".recovered") != string::npos)
            {
                count++;
            }
        }

        return count;
    }


    //  The eject of a disk whose writes could not be saved to its file.
    static constexpr const char *  kEjectPath  = "C:\\work\\e.dsk";


    static constexpr const char *  kChosenPath = "C:\\work\\saved.dsk";


    //  The questions the store raised. `isDelivered` is what the sink returns.
    struct AskRecorder
    {
        vector<ChangePrompt>  prompts;
        bool                  isDelivered = true;
    };


    static void RecordAsks (DiskImageStore & store, AskRecorder & recorder)
    {
        store.SetAskSink ([&recorder] (int, int, const ChangePrompt & prompt)
        {
            recorder.prompts.push_back (prompt);
            return recorder.isDelivered;
        });
    }


    //  Every write to kEjectPath fails, and every other one is added to `written`.
    static void FailWritesToTheEjectedFile (DiskImageStore & store, vector<string> & written)
    {
        store.SetFlushSink ([&written] (const string & path, const vector<Byte> &)
        {
            if (path == kEjectPath)
            {
                return HRESULT_FROM_WIN32 (ERROR_DISK_FULL);
            }

            written.push_back (path);
            return S_OK;
        });
    }


    // A WOZ carrying a valid CRC, then damaged -- the stored checksum now
    // describes what the file used to be.
    vector<Byte> MakeCrcDamagedWoz()
    {
        DiskImage     src;
        vector<Byte>  bits (6400, 0xFF);
        vector<Byte>  woz;
        vector<Byte>  out;

        bits[20] = 0xD5;
        bits[21] = 0xAA;
        bits[22] = 0x96;

        AssertSucceeded (WozLoader::BuildSyntheticV2 (1, false, bits, 51200, woz));
        AssertSucceeded (WozLoader::Load (woz, src));
        AssertSucceeded (WozLoader::Serialize (src, out));

        out[out.size() - 1] = static_cast<Byte> (out[out.size() - 1] ^ 0xFF);

        return out;
    }


    // A WOZ built from a real sector image, so its tracks are ordinary
    // 16-sector data, with the header CRC then broken so the image mounts as
    // damaged. That combination -- damaged AND standard -- is what salvage
    // is for.
    vector<Byte> MakeDamagedStandardWoz()
    {
        DiskImage     src;
        vector<Byte>  sectors (NibblizationLayer::kImageByteSize, 0);
        vector<Byte>  woz;
        uint32_t      seed = 0xC0FFEEu;
        size_t        i    = 0;

        for (i = 0; i < sectors.size(); i++)
        {
            seed       = seed * 1664525u + 1013904223u;
            sectors[i] = static_cast<Byte> ((seed >> 16) & 0xFF);
        }

        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, src));
        src.SetSourceFormat (DiskFormat::Woz);
        AssertSucceeded (WozLoader::Serialize (src, woz));

        // Break the stored checksum without touching the data, so the image
        // is damaged in exactly the way that write-protects it.
        woz[8] = static_cast<Byte> (woz[8] ^ 0xFF);

        return woz;
    }


    //  Runs the drive's motor and lets it stop, `cycles` times, each past the
    //  one-second spindown.
    static void SpinDown (Disk2Controller & ctrl, int cycles)
    {
        int  cycle = 0;



        for (cycle = 0; cycle < cycles; cycle++)
        {
            ctrl.Write (0xC0E9, 0x00);
            ctrl.Write (0xC0E8, 0x00);
            ctrl.Tick  (1100000);
        }
    }


    static MountDiagnosis UnsavedWritesOver (const char * keptPath, HRESULT saveError, bool isSameFile)
    {
        MountDiagnosis  diagnosis;



        diagnosis.failure       = MountFailure::UnsavedWrites;
        diagnosis.keptPath      = keptPath;
        diagnosis.saveError     = saveError;
        diagnosis.isSameFile    = isSameFile;
        diagnosis.occupiedDrive = 0;

        return diagnosis;
    }


    //  A diagnosis of one reason, for the message tests, so each of them
    //  says which reason it is wording rather than assembling a struct.
    static MountDiagnosis  DiagnosisOf (MountFailure failure,
                                        DiskFormat   format = DiskFormat::Dsk,
                                        size_t       fileByteSize = 0)
    {
        MountDiagnosis  diagnosis;



        diagnosis.failure      = failure;
        diagnosis.format       = format;
        diagnosis.fileByteSize = fileByteSize;

        return diagnosis;
    }
};
