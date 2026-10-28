#include "Pch.h"
#include "DiskStoreTestSupport.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSalvageTests
//
//  A damaged disk: the write protection a checksum mismatch sets, salvage's
//  assessment and its copy, and the bay status.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskStoreSalvageTests), public DiskStoreTestSupport
{
public:

    TEST_METHOD (Mount_CrcMismatchedImage_IsWriteProtected)
    {
        // A damaged image used to be rewritten, with a warning first. That
        // traded a detectable problem for an undetectable one: the rewrite
        // stamps a freshly computed checksum over the same damage, so nothing
        // afterwards can tell the file is wrong. It is now held read-only for
        // the session instead, and never rewritten at all.
        DiskImageStore  store;
        vector<Byte>    damaged = MakeCrcDamagedWoz();
        DiskImage *     img     = nullptr;

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "suspect.woz",
                                               DiskFormat::Woz, damaged));

        img = store.GetImage (kSlot, kDrive);
        Assert::IsNotNull (img);
        Assert::IsTrue (img->HasSourceCrcMismatch(),
            L"precondition: the mount must have seen the bad checksum");

        Assert::IsTrue (img->IsWriteProtected(),
            L"a damaged image must be write-protected");
        Assert::IsTrue (img->GetWriteProtectInfo().checksumMismatch,
            L"and it must give the reason, so the UI can explain a state the user did not choose");

        // Not the image flag: that lives in the WOZ's INFO chunk, so setting
        // it would mean writing the very file being protected from writes.
        Assert::IsFalse (img->GetWriteProtectInfo().imageFlag,
            L"the damaged state must not be recorded as the in-file flag");
        Assert::IsFalse (img->GetWriteProtectInfo().userSetting,
            L"nor confused with the user's own toggle");
    }


    TEST_METHOD (Flush_CrcMismatchedImage_NeverRewritesTheFile)
    {
        // The gate that makes the protection real. A guest cannot dirty a
        // write-protected image, so this reaches past the emulation to set the
        // bit directly -- the point is that the flush path itself refuses.
        DiskImageStore  store;
        vector<Byte>    damaged   = MakeCrcDamagedWoz();
        DiskImage *     img       = nullptr;
        int             sinkCalls = 0;

        store.SetFlushSink ([&sinkCalls] (const string &, const vector<Byte> &)
        {
            sinkCalls++;
            return S_OK;
        });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "suspect.woz",
                                               DiskFormat::Woz, damaged));

        img = store.GetImage (kSlot, kDrive);
        Assert::IsNotNull (img);

        img->WriteBit (0, 0, static_cast<uint8_t> (img->ReadBit (0, 0) ^ 1));
        AssertSucceeded (store.Flush (kSlot, kDrive));

        Assert::AreEqual (0, sinkCalls,
            L"a damaged image must never be written back -- the rewrite is what "
            L"would make its damage undetectable");
        Assert::IsTrue (img->HasSourceCrcMismatch(),
            L"and the file is still damaged, so the flag must stay set");
    }


    TEST_METHOD (SetImageWriteProtect_OnADamagedImage_IsRefused)
    {
        // The one remaining route that would have rewritten a damaged file.
        // Patching the write-protect flag recomputes the header checksum, and
        // that checksum failing to match IS the damage report -- so the write
        // that is otherwise harmless is precisely the one that would destroy
        // the evidence.
        DiskImageStore  store;
        vector<Byte>    file      = MakeCrcDamagedWoz();
        vector<Byte>    original;
        int             sinkCalls = 0;
        HRESULT         hr        = S_OK;

        original = file;

        store.SetImageReader ([&file] (const string &, vector<Byte> & out)
        {
            out = file;
            return S_OK;
        });

        store.SetFlushSink ([&sinkCalls, &file] (const string &, const vector<Byte> & bytes)
        {
            sinkCalls++;
            file = bytes;
            return S_OK;
        });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "suspect.woz",
                                               DiskFormat::Woz, file));

        hr = store.SetImageWriteProtect (kSlot, kDrive, true);

        Assert::IsTrue (FAILED (hr), L"the toggle must refuse a damaged image");
        Assert::AreEqual (0, sinkCalls, L"and must not write anything on the way to refusing");
        Assert::IsTrue (file == original, L"so the file is byte-for-byte untouched");
    }


    TEST_METHOD (AssessSalvage_DamagedStandardDisk_IsOffered)
    {
        DiskImageStore     store;
        SalvageAssessment  assessment;

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "broken.woz",
                                               DiskFormat::Woz, MakeDamagedStandardWoz()));

        AssertSucceeded (store.AssessSalvage (kSlot, kDrive, assessment));

        Assert::IsTrue (assessment.isOffered,
            L"a damaged disk with ordinary sectors is exactly what salvage is for");
        Assert::AreEqual (560, assessment.totalSectors,
            L"the total counts the tracks this disk has, not a flat 35 x 16 assumption");
        Assert::AreEqual (560, assessment.report.sectorsVerified,
            L"the damage here is the file checksum, not the sectors -- all still verify");
        Assert::IsTrue (assessment.suggestedPath.find ("broken.salvaged.woz") != string::npos,
            L"the suggested name describes what the file is");
    }


    TEST_METHOD (AssessSalvage_UndamagedDisk_IsNotOffered)
    {
        // Nothing to escape from: an undamaged disk is not write-protected, so
        // a lossy copy could only lose data.
        DiskImageStore     store;
        SalvageAssessment  assessment;
        vector<Byte>       healthy = MakeDamagedStandardWoz();

        healthy[8] = static_cast<Byte> (healthy[8] ^ 0xFF);   // put the checksum back

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "fine.woz",
                                               DiskFormat::Woz, healthy));
        Assert::IsFalse (store.GetImage (kSlot, kDrive)->HasSourceCrcMismatch(),
            L"precondition: this image is not damaged");

        AssertSucceeded (store.AssessSalvage (kSlot, kDrive, assessment));

        Assert::IsFalse (assessment.isOffered,
            L"salvage must not be offered for a disk the user can already write to");
    }


    TEST_METHOD (AssessSalvage_UndamagedDisk_DoesNotDecodeTheDisk)
    {
        // Pins the cheap path, because the expensive one is not merely slow --
        // this runs from the Disk menu's enable query, so it runs every time
        // that menu is drawn. Decoding unconditionally cost 11 ms for an
        // ordinary disk and 154 ms for a copy-protected one, per drive.
        //
        // Damage is free to test, salvage is only ever offered for a damaged
        // disk, so an undamaged one must return before decoding anything. An
        // untouched report is how that is observable from here.
        DiskImageStore     store;
        SalvageAssessment  assessment;
        vector<Byte>       healthy = MakeDamagedStandardWoz();

        healthy[8] = static_cast<Byte> (healthy[8] ^ 0xFF);   // put the checksum back

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "fine.woz",
                                               DiskFormat::Woz, healthy));
        AssertSucceeded (store.AssessSalvage (kSlot, kDrive, assessment));

        Assert::AreEqual (0, assessment.report.tracksPresent,
            L"an undamaged disk must not be decoded at all");
        Assert::AreEqual (0, assessment.totalSectors,
            L"and no counts are produced, because none were needed");
    }


    TEST_METHOD (AssessSalvage_NonStandardDisk_IsNotOffered)
    {
        // A copy-protected disk has no standard sectors to recover, and
        // rebuilding it from sectors would destroy the tracks it depends on.
        // It never reaches the dialog, which is why the dialog never has to
        // explain copy protection.
        DiskImageStore     store;
        SalvageAssessment  assessment;
        vector<Byte>       woz;
        vector<Byte>       bits ((51200 + 7) / 8, 0xFF);

        // A track of pure sync bytes: a legal bit stream with no address
        // fields at all, which is what protection looks like from here.
        AssertSucceeded (WozLoader::BuildSyntheticV2 (1, false, bits, 51200, woz));
        woz[8] = static_cast<Byte> (woz[8] ^ 0xFF);   // damaged too, so only structure decides

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "protected.woz",
                                               DiskFormat::Woz, woz));

        AssertSucceeded (store.AssessSalvage (kSlot, kDrive, assessment));

        Assert::IsTrue (assessment.report.tracksUnformatted > 0,
            L"precondition: this disk has no standard structure");
        Assert::IsFalse (assessment.isOffered,
            L"salvage must refuse a disk whose tracks it cannot rebuild");
    }


    TEST_METHOD (AssessSalvage_WritesNothing)
    {
        // It exists to inform a decision, so it must not make one.
        DiskImageStore     store;
        SalvageAssessment  assessment;
        int                sinkCalls = 0;

        store.SetFlushSink ([&sinkCalls] (const string &, const vector<Byte> &)
        {
            sinkCalls++;
            return S_OK;
        });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "broken.woz",
                                               DiskFormat::Woz, MakeDamagedStandardWoz()));
        AssertSucceeded (store.AssessSalvage (kSlot, kDrive, assessment));

        Assert::AreEqual (0, sinkCalls, L"assessing must write nothing at all");
    }


    TEST_METHOD (SalvageToFile_WritesTheCopyAndLeavesTheOriginalAlone)
    {
        // The guarantee the whole feature rests on. The damaged original has
        // to survive, still damaged and still detectably so, or salvage has
        // destroyed the evidence it was built to preserve.
        DiskImageStore    store;
        DenibblizeReport  report;
        vector<Byte>      original = MakeDamagedStandardWoz();
        string            writtenTo;
        vector<Byte>      written;
        DiskImage         reloaded;

        store.SetFlushSink ([&writtenTo, &written] (const string & path,
                                                    const vector<Byte> & bytes)
        {
            writtenTo = path;
            written   = bytes;
            return S_OK;
        });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "broken.woz",
                                               DiskFormat::Woz, original));

        AssertSucceeded (store.SalvageToFile (kSlot, kDrive, store.GetMediaId (kSlot, kDrive), "broken.salvaged.woz", report));

        Assert::AreEqual (string ("broken.salvaged.woz"), writtenTo,
            L"the copy goes to its own file");
        Assert::IsTrue (written.size() > 0, L"and it actually holds something");

        // The salvaged copy is a valid, undamaged WOZ.
        AssertSucceeded (WozLoader::Load (written, reloaded));
        Assert::IsFalse (reloaded.HasSourceCrcMismatch(),
            L"the salvaged copy carries a correct checksum of its own");

        Assert::AreEqual (string ("broken.woz"), store.GetSourcePath (kSlot, kDrive),
            L"and the mount still points at the untouched original");
    }


    TEST_METHOD (SalvageToFile_RefusesToOverwriteTheSource)
    {
        // Salvage exists so the damaged original survives. Writing over it
        // would defeat the entire point, so the path is checked rather than
        // trusted.
        DiskImageStore    store;
        DenibblizeReport  report;
        int               sinkCalls = 0;
        HRESULT           hr        = S_OK;

        store.SetFlushSink ([&sinkCalls] (const string &, const vector<Byte> &)
        {
            sinkCalls++;
            return S_OK;
        });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "broken.woz",
                                               DiskFormat::Woz, MakeDamagedStandardWoz()));

        UnitTestHelpers::ExpectedEhmAssert  expected;

        hr = store.SalvageToFile (kSlot, kDrive, store.GetMediaId (kSlot, kDrive), "broken.woz", report);

        Assert::IsTrue (FAILED (hr), L"salvaging onto the source must be refused");
        Assert::AreEqual (0, sinkCalls, L"and nothing may be written on the way to refusing");
    }


    TEST_METHOD (SalvageToFile_KeepsTheMetadataButClaimsTheFile)
    {
        // The salvaged copy is still the same disk -- title, publisher and
        // provenance travel -- but Casso wrote this particular file, and
        // leaving someone else's name in creator would put a preservation
        // tool's signature on a lossy reconstruction.
        DiskImageStore    store;
        DenibblizeReport  report;
        vector<Byte>      original = MakeDamagedStandardWoz();
        vector<Byte>      written;
        string            meta     = "title\tSalvage Test\npublisher\tCasso\n";
        Byte              header[8] = { 'M', 'E', 'T', 'A', 0, 0, 0, 0 };

        header[4] = static_cast<Byte> (meta.size() & 0xFF);
        original.insert (original.end(), header, header + sizeof (header));
        original.insert (original.end(), meta.begin(), meta.end());

        store.SetFlushSink ([&written] (const string &, const vector<Byte> & bytes)
        {
            written = bytes;
            return S_OK;
        });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "broken.woz",
                                               DiskFormat::Woz, original));
        AssertSucceeded (store.SalvageToFile (kSlot, kDrive, store.GetMediaId (kSlot, kDrive), "broken.salvaged.woz", report));

        {
            string  blob (reinterpret_cast<const char *> (written.data()), written.size());

            Assert::IsTrue (blob.find ("title\tSalvage Test") != string::npos,
                L"the salvaged copy is still the same disk, so META travels");
            Assert::IsTrue (blob.find ("Casso ") != string::npos,
                L"but Casso wrote this file and records that in creator");
        }
    }


    //  A bay's published status copies the bay as it is: nothing for an empty
    //  or invalid bay, and the path, format, write-protect causes and salvage
    //  verdict for a mounted one.
    TEST_METHOD (BayStatusReportsEmptyInvalidAndMountedBays)
    {
        DiskImageStore     store;
        std::vector<Byte>  raw (NibblizationLayer::kImageByteSize, 0);
        BayStatus          empty;
        BayStatus          invalid;
        BayStatus          mounted;
        HRESULT            hr = S_OK;



        hr = store.MountFromBytes (kSlot, kDrive, "status.dsk", DiskFormat::Dsk, raw);
        AssertSucceeded (hr, L"MountFromBytes");

        empty   = store.GetBayStatus (kSlot, 1);
        invalid = store.GetBayStatus (DiskImageStore::kSlotCount, 0);
        mounted = store.GetBayStatus (kSlot, kDrive);

        Assert::IsTrue  (empty == BayStatus(),   L"an empty bay reports nothing");
        Assert::IsTrue  (invalid == BayStatus(), L"an invalid bay reports nothing");
        Assert::IsTrue  (mounted.isMounted,      L"the mounted bay reports its disk");
        Assert::AreEqual (std::string ("status.dsk"), mounted.path);
        Assert::IsTrue  (mounted.format == DiskFormat::Dsk, L"and its format");
        Assert::IsTrue  (mounted.writeProtect == store.GetImage (kSlot, kDrive)->GetWriteProtectInfo(), L"and its write-protect causes");
        Assert::AreEqual (store.IsSalvageOffered (kSlot, kDrive), mounted.isSalvageOffered, L"and its salvage verdict");
    }


    //  Reads, an idle reload walk and a flush with nothing to save change no
    //  bay, so the status generation does not move and an idle publish costs
    //  only the compare.
    TEST_METHOD (TheStatusGenerationStaysPutOnReadsIdleWalksAndCleanFlushes)
    {
        DiskImageStore     store;
        std::vector<Byte>  raw (NibblizationLayer::kImageByteSize, 0);
        uint64_t           before = 0;
        HRESULT            hr     = S_OK;



        hr = store.MountFromBytes (kSlot, kDrive, "idle.dsk", DiskFormat::Dsk, raw);
        AssertSucceeded (hr, L"MountFromBytes");

        before = store.GetStatusGeneration();

        (void) store.GetImage (kSlot, kDrive);
        (void) store.IsMounted (kSlot, kDrive);
        (void) store.GetSourcePath (kSlot, kDrive);
        (void) store.GetMountedSourcePaths();
        (void) store.IsSalvageOffered (kSlot, kDrive);
        (void) store.GetBayStatus (kSlot, kDrive);
        store.ApplyPendingReload();

        hr = store.Flush (kSlot, kDrive);
        AssertSucceeded (hr, L"Flush of a clean disk");

        Assert::AreEqual (before, store.GetStatusGeneration(), L"nothing a bay shows changed");
    }


    TEST_METHOD (SalvageToFile_AfterTheBayChanged_WritesNothing)
    {
        // The copy is of the disk the user saw assessed. If another disk went
        // in since, writing it would salvage a disk nobody looked at.
        DiskImageStore    store;
        DenibblizeReport  report;
        uint64_t          assessed  = 0;
        int               sinkCalls = 0;
        HRESULT           hr        = S_OK;

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "broken.woz",
                                               DiskFormat::Woz, MakeDamagedStandardWoz()));
        assessed = store.GetMediaId (kSlot, kDrive);

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "other.woz",
                                               DiskFormat::Woz, MakeDamagedStandardWoz()));

        store.SetFlushSink ([&sinkCalls] (const string &, const vector<Byte> &)
        {
            sinkCalls++;
            return S_OK;
        });

        hr = store.SalvageToFile (kSlot, kDrive, assessed, "broken.salvaged.woz", report);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_MEDIA_CHANGED), hr, L"the bay holds another disk now");
        Assert::AreEqual (0, sinkCalls, L"so nothing is written");
    }


    TEST_METHOD (SalvageToFile_AfterEjectAndReinsertOfTheSameFile_WritesNothing)
    {
        // The same file put back is a new medium: the guest may have written
        // the first one before it came out.
        DiskImageStore    store;
        DenibblizeReport  report;
        uint64_t          assessed  = 0;
        int               sinkCalls = 0;
        HRESULT           hr        = S_OK;

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "broken.woz",
                                               DiskFormat::Woz, MakeDamagedStandardWoz()));
        assessed = store.GetMediaId (kSlot, kDrive);

        store.Eject (kSlot, kDrive);
        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "broken.woz",
                                               DiskFormat::Woz, MakeDamagedStandardWoz()));

        store.SetFlushSink ([&sinkCalls] (const string &, const vector<Byte> &)
        {
            sinkCalls++;
            return S_OK;
        });

        hr = store.SalvageToFile (kSlot, kDrive, assessed, "broken.salvaged.woz", report);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_MEDIA_CHANGED), hr, L"the reinserted disk is another medium");
        Assert::AreEqual (0, sinkCalls, L"so nothing is written");
    }


    TEST_METHOD (AssessSalvage_GivesTheMediaIdAndSourcePath)
    {
        // The window shows the assessment and later asks for the copy; both
        // facts travel with it so nothing has to read the bay again.
        DiskImageStore     store;
        SalvageAssessment  assessment;

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, "broken.woz",
                                               DiskFormat::Woz, MakeDamagedStandardWoz()));
        AssertSucceeded (store.AssessSalvage (kSlot, kDrive, assessment));

        Assert::AreEqual (store.GetMediaId (kSlot, kDrive), assessment.mediaId, L"the medium that was assessed");
        Assert::AreEqual (string ("broken.woz"), assessment.sourcePath, L"and the file it came from");
    }
};
