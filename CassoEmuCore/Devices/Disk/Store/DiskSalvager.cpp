#include "Pch.h"

#include "Devices/Disk/Store/DiskSalvager.h"
#include "Devices/Disk/Store/DiskBayTable.h"
#include "Devices/Disk/Store/DiskFileCommit.h"
#include "Devices/Disk/Store/DiskStoreHooks.h"
#include "Devices/Disk/Store/DiskStoreMessages.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/Nibblizer.h"
#include "Machines/Apple2/Common/WozLoader.h"





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

HRESULT DiskSalvager::DecodeForSalvage (
    DiskBay             &  entry,
    vector<Byte>      &  outSectors,
    DenibblizeReport  &  report)
{
    HRESULT  hr       = S_OK;
    bool     hasImage = false;



    hasImage = (entry.mounted && entry.image != nullptr);
    CBRAEx (hasImage, E_INVALIDARG);

    hr = Denibblizer::SalvageSectors (*entry.image, DiskFormat::Dsk, outSectors, report);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildSalvagedImage
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskSalvager::BuildSalvagedImage (
    DiskBay             &  entry,
    vector<Byte>      &  outBytes,
    DenibblizeReport  &  report)
{
    HRESULT       hr       = S_OK;
    DiskImage     rebuilt;
    WozMetadata   carried;
    vector<Byte>  sectors;



    hr = DecodeForSalvage (entry, sectors, report);
    CHR (hr);

    hr = Nibblizer::NibblizeDsk (sectors, rebuilt);
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

HRESULT DiskSalvager::AssessSalvage (int slot, int drive, SalvageAssessment & out)
{
    HRESULT       hr         = S_OK;
    bool          bayOk      = false;
    bool          hasImage   = false;
    bool          isDamaged  = false;
    bool          isStandard = false;
    vector<Byte>  sectors;



    out = SalvageAssessment();

    bayOk = DiskBayTable::IsValidBay (slot, drive);
    CBRAEx (bayOk, E_INVALIDARG);

    {
        DiskBay &  entry = m_bays.GetBay (slot, drive);

        hasImage = (entry.mounted && entry.image != nullptr);
        CBREx (hasImage, HRESULT_FROM_WIN32 (ERROR_NOT_READY));

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

        out.totalSectors = out.report.tracksPresent * NibbleFormat::kSectorsPerTrack;

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

HRESULT DiskSalvager::SalvageToFile (
    int                 slot,
    int                 drive,
    const string     &  path,
    DenibblizeReport &  report)
{
    HRESULT       hr       = S_OK;
    bool          bayOk    = false;
    bool          hasImage = false;
    bool          hasPath  = !path.empty();
    bool          isSource = false;
    vector<Byte>  bytes;



    bayOk = DiskBayTable::IsValidBay (slot, drive);
    CBRAEx (bayOk, E_INVALIDARG);
    CBRAEx (hasPath, E_INVALIDARG);

    {
        DiskBay &  entry = m_bays.GetBay (slot, drive);

        hasImage = (entry.mounted && entry.image != nullptr);
        CBREx (hasImage, HRESULT_FROM_WIN32 (ERROR_NOT_READY));

        // Refusing to write over the source is not defensive coding: the
        // entire point of salvage is that the damaged original survives to
        // stay detectably damaged.
        isSource = (path == entry.path);
        CBRAEx (!isSource, E_INVALIDARG);

        hr = BuildSalvagedImage (entry, bytes, report);
        CHRN (hr, DiskStoreMessages::FormatSalvageFailedMessage (path).c_str());

        if (m_hooks.flushSink)
        {
            hr = m_hooks.flushSink (path, bytes);
        }
        else
        {
            hr = DiskFileCommit::WriteFileAtomically (path, bytes);
        }

        CHRN (hr, DiskStoreMessages::FormatSalvageFailedMessage (path).c_str());
    }

Error:
    return hr;
}





