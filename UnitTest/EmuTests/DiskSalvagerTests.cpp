#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Devices/Disk/Store/DiskBayTable.h"
#include "Devices/Disk/Store/DiskSalvager.h"
#include "Devices/Disk/Store/DiskStoreHooks.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/Nibblizer.h"
#include "Machines/Apple2/Common/WozLoader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSalvagerTests
//
//  Salvage on one bay: the gate, the counts, and the original never written.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskSalvagerTests)
{
public:

    static constexpr int  kSlot  = 6;
    static constexpr int  kDrive = 0;



    //  The salvager with a bay table filled directly and a flush sink, so no
    //  store and no file is involved.
    struct SalvageRig
    {
        DiskStoreHooks  hooks;
        DiskBayTable    bays;
        DiskSalvager    salvager { bays, hooks };

        void SetFlushSink (DiskStoreHooks::FlushSink sink)
        {
            hooks.flushSink = std::move (sink);
        }

        HRESULT LoadBay (const string & path, const vector<Byte> & bytes)
        {
            DiskBay  & bay = bays.GetBay (kSlot, kDrive);



            bay.image   = make_unique<DiskImage>();
            bay.path    = path;
            bay.format  = DiskFormat::Woz;
            bay.mounted = true;

            bay.image->LoadFromBytes (DiskFormat::Woz, bytes, path);

            return bay.image->IsLoaded() ? S_OK : E_FAIL;
        }

        DiskImage * GetImage (int slot, int drive)
        {
            return bays.GetBay (slot, drive).image.get();
        }

        const string & GetSourcePath (int slot, int drive)
        {
            return bays.GetBay (slot, drive).path;
        }
    };


    // A WOZ built from a real sector image, so its tracks are ordinary
    // 16-sector data, with the header CRC then broken so the image mounts as
    // damaged. That combination -- damaged AND standard -- is what salvage
    // is for.
    static vector<Byte> MakeDamagedStandardWoz()
    {
        DiskImage     src;
        vector<Byte>  sectors (NibbleFormat::kImageByteSize, 0);
        vector<Byte>  woz;
        uint32_t      seed = 0xC0FFEEu;
        size_t        i    = 0;

        for (i = 0; i < sectors.size(); i++)
        {
            seed       = seed * 1664525u + 1013904223u;
            sectors[i] = static_cast<Byte> ((seed >> 16) & 0xFF);
        }

        AssertSucceeded (Nibblizer::NibblizeDsk (sectors, src));
        src.SetSourceFormat (DiskFormat::Woz);
        AssertSucceeded (WozLoader::Serialize (src, woz));

        // Break the stored checksum without touching the data, so the image
        // is damaged in exactly the way that write-protects it.
        woz[8] = static_cast<Byte> (woz[8] ^ 0xFF);

        return woz;
    }

    TEST_METHOD (AssessSalvage_DamagedStandardDisk_IsOffered)
    {
        SalvageRig         store;
        SalvageAssessment  assessment;

        AssertSucceeded (store.LoadBay ("broken.woz", MakeDamagedStandardWoz()));

        AssertSucceeded (store.salvager.AssessSalvage (kSlot, kDrive, assessment));

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
        SalvageRig         store;
        SalvageAssessment  assessment;
        vector<Byte>       healthy = MakeDamagedStandardWoz();

        healthy[8] = static_cast<Byte> (healthy[8] ^ 0xFF);   // put the checksum back

        AssertSucceeded (store.LoadBay ("fine.woz", healthy));
        Assert::IsFalse (store.GetImage (kSlot, kDrive)->HasSourceCrcMismatch(),
            L"precondition: this image is not damaged");

        AssertSucceeded (store.salvager.AssessSalvage (kSlot, kDrive, assessment));

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
        SalvageRig         store;
        SalvageAssessment  assessment;
        vector<Byte>       healthy = MakeDamagedStandardWoz();

        healthy[8] = static_cast<Byte> (healthy[8] ^ 0xFF);   // put the checksum back

        AssertSucceeded (store.LoadBay ("fine.woz", healthy));
        AssertSucceeded (store.salvager.AssessSalvage (kSlot, kDrive, assessment));

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
        SalvageRig         store;
        SalvageAssessment  assessment;
        vector<Byte>       woz;
        vector<Byte>       bits ((51200 + 7) / 8, 0xFF);

        // A track of pure sync bytes: a legal bit stream with no address
        // fields at all, which is what protection looks like from here.
        AssertSucceeded (WozLoader::BuildSyntheticV2 (1, false, bits, 51200, woz));
        woz[8] = static_cast<Byte> (woz[8] ^ 0xFF);   // damaged too, so only structure decides

        AssertSucceeded (store.LoadBay ("protected.woz", woz));

        AssertSucceeded (store.salvager.AssessSalvage (kSlot, kDrive, assessment));

        Assert::IsTrue (assessment.report.tracksUnformatted > 0,
            L"precondition: this disk has no standard structure");
        Assert::IsFalse (assessment.isOffered,
            L"salvage must refuse a disk whose tracks it cannot rebuild");
    }

    TEST_METHOD (AssessSalvage_WritesNothing)
    {
        // It exists to inform a decision, so it must not make one.
        SalvageRig         store;
        SalvageAssessment  assessment;
        int                sinkCalls = 0;

        store.SetFlushSink ([&sinkCalls] (const string &, const vector<Byte> &)
        {
            sinkCalls++;
            return S_OK;
        });

        AssertSucceeded (store.LoadBay ("broken.woz", MakeDamagedStandardWoz()));
        AssertSucceeded (store.salvager.AssessSalvage (kSlot, kDrive, assessment));

        Assert::AreEqual (0, sinkCalls, L"assessing must write nothing at all");
    }

    TEST_METHOD (SalvageToFile_WritesTheCopyAndLeavesTheOriginalAlone)
    {
        // The guarantee the whole feature rests on. The damaged original has
        // to survive, still damaged and still detectably so, or salvage has
        // destroyed the evidence it was built to preserve.
        SalvageRig        store;
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

        AssertSucceeded (store.LoadBay ("broken.woz", original));

        AssertSucceeded (store.salvager.SalvageToFile (kSlot, kDrive, "broken.salvaged.woz", report));

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
        SalvageRig        store;
        DenibblizeReport  report;
        int               sinkCalls = 0;
        HRESULT           hr        = S_OK;

        store.SetFlushSink ([&sinkCalls] (const string &, const vector<Byte> &)
        {
            sinkCalls++;
            return S_OK;
        });

        AssertSucceeded (store.LoadBay ("broken.woz", MakeDamagedStandardWoz()));

        UnitTestHelpers::ExpectedEhmAssert  expected;

        hr = store.salvager.SalvageToFile (kSlot, kDrive, "broken.woz", report);

        Assert::IsTrue (FAILED (hr), L"salvaging onto the source must be refused");
        Assert::AreEqual (0, sinkCalls, L"and nothing may be written on the way to refusing");
    }

    TEST_METHOD (SalvageToFile_KeepsTheMetadataButClaimsTheFile)
    {
        // The salvaged copy is still the same disk -- title, publisher and
        // provenance travel -- but Casso wrote this particular file, and
        // leaving someone else's name in creator would put a preservation
        // tool's signature on a lossy reconstruction.
        SalvageRig        store;
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

        AssertSucceeded (store.LoadBay ("broken.woz", original));
        AssertSucceeded (store.salvager.SalvageToFile (kSlot, kDrive, "broken.salvaged.woz", report));

        {
            string  blob (reinterpret_cast<const char *> (written.data()), written.size());

            Assert::IsTrue (blob.find ("title\tSalvage Test") != string::npos,
                L"the salvaged copy is still the same disk, so META travels");
            Assert::IsTrue (blob.find ("Casso ") != string::npos,
                L"but Casso wrote this file and records that in creator");
        }
    }
};
