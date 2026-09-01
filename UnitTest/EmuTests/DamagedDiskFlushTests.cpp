#include "Pch.h"

#include "DamagedDisk.h"

#include "Devices/Disk/BlankDiskBuilder.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/Disk/WozLoader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DamagedDiskFlushTests
//
//  The defect in the clothes it shipped in.
//
//  THE FORMAT IS THE WHOLE POINT. DiskImage::Serialize denibblizes for Dsk, Do
//  and Po, and writes WOZ and NIB byte-for-byte. Only a SECTOR format can lose
//  anything on the way out: a WOZ flush hands back the same nibble stream it
//  was given, damage included, and is lossless whatever the tracks hold. An
//  earlier version of this file mounted a WOZ and proved nothing -- the write
//  it observed was correct behavior, not the bug.
//
//  This is the path a user reaches the decoder by: a .dsk in a bay, the guest
//  writing to it, and the store persisting on eject, power cycle or reset.
//  That is where a partly decoded track became a truncated FILE, because the
//  decode came back S_OK and the store wrote whatever it was handed.
//
//  NOTHING TOUCHES THE FILESYSTEM. The store persists through its flush sink,
//  so a test supplies one, mounts from bytes, and watches exactly what would
//  have been written without a file existing anywhere.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (DamagedDiskFlushTests)
{
public:

    //  A blank DOS 3.3 disk as SECTOR bytes -- the format whose flush has to
    //  decode, and therefore the only one that can truncate.
    static vector<Byte> MakeDsk()
    {
        BlankDiskSpec  spec;
        vector<Byte>   dsk;
        HRESULT        hr = S_OK;

        spec.format = DiskFormat::Dsk;

        hr = BlankDiskBuilder::Build (spec, BootPayload{}, dsk);
        Assert::IsTrue (SUCCEEDED (hr), L"the builder must produce a .dsk to mount");

        return dsk;
    }

    //  The control: a healthy .dsk with a dirty track persists whole, so the
    //  test below is about the damage rather than about flushing being broken.
    TEST_METHOD (AHealthyDiskWithGuestWrites_IsWrittenBackWhole)
    {
        DiskImageStore  store;
        vector<Byte>    dsk     = MakeDsk();
        size_t          written = 0;
        int             writes  = 0;
        DiskImage    *  mounted = nullptr;
        HRESULT         hr      = S_OK;

        store.SetFlushSink ([&writes, &written] (const string &, const vector<Byte> & bytes) -> HRESULT
        {
            writes++;
            written = bytes.size();
            return S_OK;
        });

        hr = store.MountFromBytes (6, 1, "mem:good.dsk", DiskFormat::Dsk, dsk);
        Assert::IsTrue (SUCCEEDED (hr), L"a healthy .dsk must mount");

        mounted = store.GetImage (6, 1);
        Assert::IsNotNull (mounted, L"the bay must hold the image it just mounted");
        mounted->MarkTrackDirty (20);

        hr = store.Flush (6, 1);

        Assert::IsTrue (SUCCEEDED (hr), L"flushing a healthy dirty image must succeed");
        Assert::AreEqual (1, writes, L"and must write it back");
        Assert::AreEqual (static_cast<size_t> (NibblizationLayer::kImageByteSize), written,
                          L"as a whole image, not a truncated one");
    }

    //  THE REGRESSION. The guest wrote to a track that has become undecodable.
    //  Serializing to a sector format has to decode, the decode is lossy, and
    //  writing the result back would replace the user's disk with a copy
    //  missing whatever could not be read.
    //
    //  THE ORIGINAL IS WHAT MUST NOT BE WRITTEN -- not "nothing is written".
    //  A failed serialize preserves the session losslessly beside the original
    //  first, so a write does happen and counting writes reads that recovery
    //  copy as the bug. Assert on the PATH.
    TEST_METHOD (ADamagedTrackWithGuestWrites_NeverWritesTheOriginal)
    {
        DiskImageStore       store;
        vector<Byte>         dsk     = MakeDsk();
        std::vector<string>  written;
        DiskImage         *  mounted = nullptr;
        HRESULT              hr      = S_OK;

        store.SetFlushSink ([&written] (const string & path, const vector<Byte> &) -> HRESULT
        {
            written.push_back (path);
            return S_OK;
        });

        hr = store.MountFromBytes (6, 1, "mem:good.dsk", DiskFormat::Dsk, dsk);
        Assert::IsTrue (SUCCEEDED (hr), L"the disk mounts healthy; the damage arrives after");

        //  Damage the MOUNTED image, which is what a failing drive or a
        //  half-finished write leaves behind mid-session.
        mounted = store.GetImage (6, 1);
        Assert::IsNotNull (mounted, L"the bay must hold the image it just mounted");

        DamagedDisk::BreakSector (*mounted, 20, 5);
        mounted->MarkTrackDirty (20);

        hr = store.Flush (6, 1);

        Assert::IsTrue (FAILED (hr), L"the flush must report that it could not persist");

        for (const string & path : written)
        {
            Assert::AreNotEqual (string ("mem:good.dsk"), path,
                                 L"a lossy decode must never be written over the user's disk");
        }
    }

    //  Eject is the path the emulator actually takes, and it drops FlushEntry's
    //  HRESULT -- which is how the loss stayed silent. The guarantee has to
    //  hold when nobody is reading the return value.
    TEST_METHOD (EjectingADamagedTrack_AlsoSparesTheOriginal)
    {
        DiskImageStore       store;
        vector<Byte>         dsk     = MakeDsk();
        std::vector<string>  written;
        DiskImage         *  mounted = nullptr;
        HRESULT              hr      = S_OK;

        store.SetFlushSink ([&written] (const string & path, const vector<Byte> &) -> HRESULT
        {
            written.push_back (path);
            return S_OK;
        });

        hr = store.MountFromBytes (6, 1, "mem:good.dsk", DiskFormat::Dsk, dsk);
        Assert::IsTrue (SUCCEEDED (hr), L"the disk mounts healthy; the damage arrives after");

        mounted = store.GetImage (6, 1);
        Assert::IsNotNull (mounted, L"the bay must hold the image it just mounted");

        DamagedDisk::BreakSector (*mounted, 20, 5);
        mounted->MarkTrackDirty (20);

        store.Eject (6, 1);

        for (const string & path : written)
        {
            Assert::AreNotEqual (string ("mem:good.dsk"), path,
                                 L"eject must not write a lossy decode over the original either");
        }
    }

    //  A WOZ is a nibble format, so its flush copies the stream out verbatim
    //  and cannot lose a sector however broken the track is. Recorded as a
    //  test because it is the trap the first version of this file fell into:
    //  a write here is CORRECT, and reading it as the bug under test hides the
    //  bug rather than catching it.
    TEST_METHOD (ADamagedWozIsWrittenBack_BecauseANibbleFormatCannotLoseAnything)
    {
        DiskImageStore  store;
        vector<Byte>    dsk     = MakeDsk();
        vector<Byte>    woz;
        int             writes  = 0;
        DiskImage    *  mounted = nullptr;
        DiskImage       staging;
        HRESULT         hr      = S_OK;

        //  The same disk in nibble form, so the two cases differ only in the
        //  format the flush has to produce.
        hr = NibblizationLayer::NibblizeDsk (dsk, staging);
        Assert::IsTrue (SUCCEEDED (hr), L"the sector image must nibblize");

        DamagedDisk::BreakSector (staging, 20, 5);

        hr = WozLoader::Serialize (staging, woz);
        Assert::IsTrue (SUCCEEDED (hr), L"the damaged image must serialize as a WOZ");

        store.SetFlushSink ([&writes] (const string &, const vector<Byte> &) -> HRESULT
        {
            writes++;
            return S_OK;
        });

        hr = store.MountFromBytes (6, 1, "mem:damaged.woz", DiskFormat::Woz, woz);
        Assert::IsTrue (SUCCEEDED (hr), L"a WOZ with a broken sector still mounts");

        mounted = store.GetImage (6, 1);
        Assert::IsNotNull (mounted, L"the bay must hold the image it just mounted");
        mounted->MarkTrackDirty (20);

        hr = store.Flush (6, 1);

        Assert::IsTrue (SUCCEEDED (hr), L"a nibble flush needs no decode, so it succeeds");
        Assert::AreEqual (1, writes, L"and writes the stream back, damage and all");
    }

    //  The other half of the refusal. Declining to write the original would
    //  strand whatever the guest did this session, so the store preserves it
    //  losslessly beside the original and names that copy in the report.
    TEST_METHOD (ADamagedTrackWithGuestWrites_PreservesTheSessionElsewhere)
    {
        DiskImageStore       store;
        vector<Byte>         dsk     = MakeDsk();
        std::vector<string>  written;
        DiskImage         *  mounted = nullptr;
        HRESULT              hr      = S_OK;

        store.SetFlushSink ([&written] (const string & path, const vector<Byte> &) -> HRESULT
        {
            written.push_back (path);
            return S_OK;
        });

        hr = store.MountFromBytes (6, 1, "mem:good.dsk", DiskFormat::Dsk, dsk);
        Assert::IsTrue (SUCCEEDED (hr), L"the disk mounts healthy; the damage arrives after");

        mounted = store.GetImage (6, 1);
        Assert::IsNotNull (mounted, L"the bay must hold the image it just mounted");

        DamagedDisk::BreakSector (*mounted, 20, 5);
        mounted->MarkTrackDirty (20);

        hr = store.Flush (6, 1);

        Assert::IsTrue (FAILED (hr), L"the flush refuses, which is what forces the recovery copy");

        Assert::AreEqual (static_cast<size_t> (1), written.size(),
                          L"the session is preserved in exactly one recovery copy");
        Assert::AreNotEqual (string ("mem:good.dsk"), written[0],
                             L"and it is beside the original, not on top of it");
    }

    //  WHERE THIS DAMAGE DOES *NOT* GO. Salvage is the other escape from a bad
    //  disk, and it is gated on HasSourceCrcMismatch -- the FILE arriving
    //  corrupt -- because that query runs every time the Disk menu is drawn and
    //  decoding both drives to answer it cost 154 ms on a copy-protected disk.
    //  A session that damages its own tracks leaves the file's checksum intact,
    //  so it is not offered salvage and does not need to be: the refusal above
    //  already caught it and the recovery copy already preserved it.
    //
    //  Recorded here so the next reader does not join the two paths up and
    //  "fix" the gate into decoding both drives on every menu draw.
    TEST_METHOD (ASectorBrokenAfterMount_IsNotOfferedSalvage_BecauseTheGateIsTheChecksum)
    {
        DiskImageStore     store;
        vector<Byte>       dsk     = MakeDsk();
        SalvageAssessment  assessment;
        DiskImage       *  mounted = nullptr;
        HRESULT            hr      = S_OK;

        hr = store.MountFromBytes (6, 1, "mem:good.dsk", DiskFormat::Dsk, dsk);
        Assert::IsTrue (SUCCEEDED (hr), L"the disk mounts healthy; the damage arrives after");

        mounted = store.GetImage (6, 1);
        Assert::IsNotNull (mounted, L"the bay must hold the image it just mounted");

        DamagedDisk::BreakSector (*mounted, 20, 5);

        hr = store.AssessSalvage (6, 1, assessment);

        Assert::IsTrue (SUCCEEDED (hr), L"assessing an intact FILE succeeds; it just finds nothing");
        Assert::IsFalse (assessment.isOffered,
                         L"the file still matches its checksum, so there is no salvage to offer");
    }
};
