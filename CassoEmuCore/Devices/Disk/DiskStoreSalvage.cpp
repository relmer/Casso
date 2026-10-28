#include "Pch.h"

#include "DiskStoreSalvage.h"
#include "DiskImageStore.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Core/TextEncoding.h"
#include "ChangePrompt.h"
#include "CommitPlan.h"
#include "Seams/Win32DiskFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BuildSalvagedImage
//
//  Recover every sector that decoded, verified or not, and rebuild the result
//  as a WOZ. Re-nibblizing is what makes a recovered sector readable: it gets
//  a correct checksum by construction, so a sector that would have made DOS
//  report an I/O error becomes an ordinary one holding possibly-wrong bytes.
//
//  The rebuilt image carries the source's META across but not its INFO, so
//  the copy still says which disk it is while the creator field says Casso
//  wrote this file. Putting Applesauce's name on a lossy reconstruction would
//  be the same class of lie the creator policy exists to prevent.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskStoreSalvage::DecodeForSalvage (
    Entry             &  entry,
    vector<Byte>      &  outSectors,
    DenibblizeReport  &  report)
{
    HRESULT  hr       = S_OK;
    bool     hasImage = false;



    hasImage = (entry.mounted && entry.image != nullptr);
    CBRAEx (hasImage, E_INVALIDARG);

    hr = NibblizationLayer::SalvageSectors (*entry.image, DiskFormat::Dsk, outSectors, report);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildSalvagedImage
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskStoreSalvage::BuildSalvagedImage (
    Entry             &  entry,
    vector<Byte>      &  outBytes,
    DenibblizeReport  &  report)
{
    HRESULT       hr       = S_OK;
    DiskImage     rebuilt;
    WozMetadata   carried;
    vector<Byte>  sectors;



    hr = DecodeForSalvage (entry, sectors, report);
    CHR (hr);

    hr = NibblizationLayer::NibblizeDsk (sectors, rebuilt);
    CHR (hr);

    // META travels, INFO does not: the disk is still the same title, but the
    // file is Casso's work now. An empty infoPayload is what tells the writer
    // to stamp its own creator.
    carried.passThrough = entry.image->GetWozMetadata().passThrough;
    rebuilt.SetWozMetadata (carried);
    rebuilt.SetSourceFormat (DiskFormat::Woz);

    hr = WozLoader::Serialize (rebuilt, outBytes);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsSalvageOffered
//
////////////////////////////////////////////////////////////////////////////////

bool DiskStoreSalvage::IsSalvageOffered (int slot, int drive) const
{
    ASSERT_THREAD_OWNERSHIP (m_store.m_ownership);

    if (!DiskImageStore::IsValidBay (slot, drive))
    {
        return false;
    }

    return m_store.GetEntry (slot, drive).salvageOffered;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssessSalvage
//
//  What salvage would cost, without doing it.
//
//  Offered only for a disk that is BOTH damaged and ordinarily formatted.
//  Undamaged disks are writable already, so a lossy copy would be pure loss;
//  copy-protected ones have no standard sectors to recover, and rebuilding
//  them from sectors would destroy the non-standard tracks they depend on.
//  Neither ever reaches the dialog, which is why the dialog never has to
//  explain copy protection.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskStoreSalvage::AssessSalvage (int slot, int drive, SalvageAssessment & out)
{
    HRESULT       hr         = S_OK;
    bool          bayOk      = false;
    bool          hasImage   = false;
    bool          isDamaged  = false;
    bool          isStandard = false;
    vector<Byte>  sectors;



    ASSERT_THREAD_OWNERSHIP (m_store.m_ownership);

    out = SalvageAssessment();

    bayOk = DiskImageStore::IsValidBay (slot, drive);
    CBRAEx (bayOk, E_INVALIDARG);

    {
        Entry &  entry = m_store.GetEntry (slot, drive);

        hasImage = (entry.mounted && entry.image != nullptr);
        CBREx (hasImage, HRESULT_FROM_WIN32 (ERROR_NOT_READY));

        out.mediaId    = m_store.GetMediaId (slot, drive);
        out.sourcePath = entry.path;

        // Damage is free to test and decoding is not, so test damage first.
        // This runs from the Disk menu's enable query, which means it runs
        // every time that menu is drawn: decoding both drives unconditionally
        // cost 11 ms an ordinary disk and 154 ms a copy-protected one, where a
        // protected track burns its whole attempt budget before giving up.
        // Salvage is only ever offered for a damaged disk, so an undamaged one
        // never needs the decode at all.
        isDamaged = entry.image->IsDamaged();
        BAIL_OUT_IF (!isDamaged, S_OK);

        hr = DecodeForSalvage (entry, sectors, out.report);
        CHR (hr);

        out.totalSectors = out.report.tracksPresent * NibblizationLayer::kSectorsPerTrack;

        // A track with no standard structure at all is the signature of copy
        // protection, and rebuilding from sectors would destroy it.
        isStandard    = (out.report.tracksPresent > 0) && (out.report.tracksUnformatted == 0);
        out.isOffered = isStandard;

        if (!entry.path.empty())
        {
            fs::path  source = fs::path (entry.path);

            out.suggestedPath = (source.parent_path()
                                 / (source.stem().string() + ".salvaged.woz")).string();
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SalvageToFile
//
//  Writes the salvaged copy. The original is never opened for writing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskStoreSalvage::SalvageToFile (
    int                 slot,
    int                 drive,
    uint64_t            mediaId,
    const string     &  path,
    DenibblizeReport &  report)
{
    HRESULT       hr           = S_OK;
    bool          bayOk        = false;
    bool          hasImage     = false;
    bool          hasPath      = !path.empty();
    bool          isSource     = false;
    bool          isSameMedium = false;
    vector<Byte>  bytes;



    ASSERT_THREAD_OWNERSHIP (m_store.m_ownership);

    bayOk = DiskImageStore::IsValidBay (slot, drive);
    CBRAEx (bayOk, E_INVALIDARG);
    CBRAEx (hasPath, E_INVALIDARG);

    // The copy is of the disk the user saw assessed. Another disk in the bay
    // since, or the same file put back, is another medium.
    isSameMedium = m_store.GetMediaId (slot, drive) == mediaId;
    CBREx (isSameMedium, HRESULT_FROM_WIN32 (ERROR_MEDIA_CHANGED));

    {
        Entry &  entry = m_store.GetEntry (slot, drive);

        hasImage = (entry.mounted && entry.image != nullptr);
        CBREx (hasImage, HRESULT_FROM_WIN32 (ERROR_NOT_READY));

        // Refusing to write over the source is not defensive coding: the
        // entire point of salvage is that the damaged original survives to
        // stay detectably damaged.
        isSource = (path == entry.path);
        CBRAEx (!isSource, E_INVALIDARG);

        hr = BuildSalvagedImage (entry, bytes, report);
        CHRN (hr, DiskStoreText::FormatSalvageFailedMessage (path).c_str());

        hr = m_store.m_save.CommitFile (path, bytes, CommitMode::Replace);
        CHRN (hr, DiskStoreText::FormatSalvageFailedMessage (path).c_str());
    }

Error:
    return hr;
}




