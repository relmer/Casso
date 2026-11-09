#include "Pch.h"

#include "Machines/Apple2/Common/ProDosVolumeRemover.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Utils.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeRemover::ProDosVolumeRemover
//
////////////////////////////////////////////////////////////////////////////////

ProDosVolumeRemover::ProDosVolumeRemover (const ProDosVolume & volume)
    : m_volume (volume)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeRemover::AppendDeleteWarnings
//
////////////////////////////////////////////////////////////////////////////////

void ProDosVolumeRemover::AppendDeleteWarnings (DeleteOutcome & inOutOutcome)
{
    size_t  leaked = inOutOutcome.leakedUnits.size();



    if (leaked > 0)
    {
        inOutOutcome.warnings.push_back (
            std::to_string (leaked) + " "
          + Utils::GetSingularOrPluralForm ((long long) leaked, "block", "blocks")
          + " this file referenced are claimed by another directory entry"
            " as well, and were left allocated rather than freed");
    }

    if (inOutOutcome.chainWasDamaged)
    {
        inOutOutcome.warnings.push_back (
            "this file's index structure could not be followed to its end, so only"
            " the blocks the walk reached were considered");
    }

    if (!inOutOutcome.catalogFullyParsed)
    {
        inOutOutcome.warnings.push_back (
            "the directory did not parse completely, so an entry that could not be"
            " read claims nothing this pass can see, and a block it shares with the"
            " deleted file may have been freed while that entry still uses it");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeRemover::Delete
//
//  The interface form. Everything it decided is available from the overload
//  below; this one discards it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosVolumeRemover::Delete (const FilePath & path, vector<Byte> & outBuffer) const
{
    DeleteOutcome  outcome;



    return Delete (path, outBuffer, outcome);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeRemover::Delete
//
//  Removes a directory entry and returns ONLY the blocks the integrity report
//  shows this entry alone claims.
//
//  That restriction is the whole point. Freeing a block a second file also
//  references does not fail: the delete succeeds, the volume looks healthy, and
//  the other file is destroyed the next time anything allocates -- possibly
//  weeks later, with nothing connecting the two events. Space left allocated
//  with nothing owning it is recoverable at any time; a block handed out from
//  under a live file is not. So everything not uniquely owned is reported as
//  leaked and left exactly where it is.
//
//  A FILE WHOSE INDEX STRUCTURE IS DAMAGED IS STILL DELETABLE, for the same
//  reason as on DOS 3.3: refusing would strand the volume, and the developer
//  this serves is working with precisely those disks.
//
//  A SUBDIRECTORY IS REFUSED. This layer does not walk into one, so the report
//  credits it with its key block and nothing else -- deleting it would free
//  that one block and silently orphan everything beneath. Refusing is honest
//  until traversal exists.
//
//  The tombstone is ProDOS's own: the storage-type nibble goes to zero and the
//  name length and name stay behind, which is how an undelete tool finds the
//  file again. Nothing may key off the name to decide whether a record is live.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosVolumeRemover::Delete (
    const FilePath  & path,
    vector<Byte>    & outBuffer,
    DeleteOutcome   & outOutcome) const
{
    return DeleteEntry (path, false, outBuffer, outOutcome);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeRemover::DeleteEntry
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosVolumeRemover::DeleteEntry (
    const FilePath  & path,
    bool              force,
    vector<Byte>    & outBuffer,
    DeleteOutcome   & outOutcome) const
{
    HRESULT                         hr          = S_OK;
    size_t                          bufferBytes = m_volume.m_sectors.size();
    bool                            found       = false;
    bool                            isLocked    = false;
    bool                            permitted   = false;
    bool                            isDirectory = false;
    bool                            fullyParsed = true;
    uint16_t                        owner       = 0;
    uint16_t                        local       = 0;
    int                             dirKeyBlock = ProDosSkeleton::kDirKeyBlock;
    Byte                            typeLen     = 0;
    vector<ProDosVolume::RawEntry>  entries;
    vector<ProDosVolume::RawEntry>  all;
    vector<std::string>             damage;
    vector<Byte>                    result;
    VolumeIntegrityReport           report;



    CBREx (bufferBytes == (size_t) NibblizationLayer::kImageByteSize, E_INVALIDARG);

    hr = m_volume.ResolveDirectory (ProDosVolume::GetParentPath (path), dirKeyBlock);
    CHR (hr);

    //  The record is found in the directory the path walks to, and its owner
    //  index comes from the whole-volume order the report speaks in.
    m_volume.CollectEntries (dirKeyBlock, entries, damage, fullyParsed);

    //  TWO INDICES, AND THEY COUNT DIFFERENT THINGS. `local` is the record's
    //  place among the directory's own records; `owner` is its place in the
    //  whole-volume order the report speaks in. They agree in the volume
    //  directory and nowhere else, so one variable for both reads a record
    //  from beyond the end of a subdirectory's list.
    found = ProDosVolume::TryFindEntry (entries, path.GetLeaf(), local);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    m_volume.CollectAllEntries (all, damage, fullyParsed);

    found = ProDosVolume::TryFindOwnerIndex (all, entries[local], owner);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    // ProDOS gates removal on destroy-enable, not on write-enable, and the
    // access byte can carry one without the other.
    isLocked    = (entries[local].access & ProDosVolume::kAccessDestroyEnable) == 0;
    isDirectory = entries[local].storage == ProDosSkeleton::kStorageSubdir;

    // Being a directory is reported FIRST, and the order is not arbitrary. The
    // subdirectories on Merlin's own ProDOS disk have destroy-enable clear, so
    // testing the lock first tells the user their file is protected -- sending
    // them off to unlock something that would still be refused afterwards. The
    // capability refusal is the true one and belongs in front of the permission
    // refusal.
    permitted = force || !isLocked;

    CBREx (!isDirectory, HRESULT_FROM_WIN32 (ERROR_DIRECTORY_NOT_SUPPORTED));
    CBREx (permitted,    HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

    hr = m_volume.BuildIntegrityReport (report);
    CHRA (hr);

    outOutcome                    = DeleteOutcome();
    outOutcome.catalogFullyParsed = report.IsCatalogFullyParsed();

    result = m_volume.m_sectors;

    // The report walks the same directory in the same order over the same
    // buffer, so an owner index names the same entry on both sides.
    for (uint32_t block : report.GetClaimsOf (owner))
    {
        bool  mineAlone = report.IsUniquelyOwnedBy (block, owner);

        if (mineAlone)
        {
            ProDosVolumeWriter::SetFreeInBitmap (result, block, true);
            outOutcome.freedUnits.push_back (block);
        }
        else
        {
            outOutcome.leakedUnits.push_back (block);
        }
    }

    for (uint16_t broken : report.GetUnfollowableChains())
    {
        if (broken == owner)
        {
            outOutcome.chainWasDamaged = true;
        }
    }

    typeLen = m_volume.ReadByte (entries[local].dirBlock,
                        entries[local].entryOffset + ProDosSkeleton::kEntOffTypeName);

    ProDosVolumeWriter::WriteByteAt (result,
                 entries[local].dirBlock,
                 entries[local].entryOffset + ProDosSkeleton::kEntOffTypeName,
                 (Byte) (typeLen & 0x0F));

    ProDosVolumeWriter::AdjustFileCount (result, dirKeyBlock, -1);
    AppendDeleteWarnings (outOutcome);

    hr = ProDosVolume::HandBackVerifiedResult (report, result, outBuffer);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeRemover::CollectRemovalEntries
//
//  Depth first, and each directory's contents land in the plan before the
//  directory itself, which is the order a caller can apply without ever
//  removing a directory that still holds something.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosVolumeRemover::CollectRemovalEntries (
    int                     keyBlock,
    const std::string     & prefix,
    ChainWalkGuard        & guard,
    DirectoryRemovalPlan  & inOutPlan) const
{
    vector<ProDosVolume::RawEntry>  entries;
    vector<std::string>             damage;
    bool                            parsed  = true;
    bool                            visitOk = false;
    size_t                          i       = 0;



    visitOk = keyBlock > 0 && keyBlock < ProDosSkeleton::kTotalBlocks && guard.TryVisit ((uint32_t) keyBlock);

    if (!visitOk)
    {
        inOutPlan.catalogFullyParsed = false;

        return;
    }

    m_volume.CollectEntries (keyBlock, entries, damage, parsed);

    inOutPlan.catalogFullyParsed = inOutPlan.catalogFullyParsed && parsed;

    for (i = 0; i < entries.size(); i++)
    {
        DirectoryRemovalEntry  listed;
        ChainWalkGuard         blockGuard ((uint32_t) ProDosSkeleton::kTotalBlocks);
        vector<uint32_t>       blocks;
        bool                   walked = true;

        listed.path        = prefix + entries[i].name;
        listed.isDirectory = entries[i].storage == ProDosSkeleton::kStorageSubdir;

        if (listed.isDirectory)
        {
            CollectRemovalEntries ((int) entries[i].keyPointer, listed.path + "/", guard, inOutPlan);
            m_volume.CollectDirectoryBlocks ((int) entries[i].keyPointer, blocks, blockGuard);

            //  A directory is removable when ProDOS says its record may go, the
            //  same bit a file is gated on.
            listed.isLocked = (entries[i].access & ProDosVolume::kAccessDestroyEnable) == 0;
        }
        else
        {
            //  A file whose chain cannot be walked is still listed, with the
            //  blocks the walk did reach: the plan reports what removal would
            //  free, and a damaged file is exactly what one of these disks has.
            walked = m_volume.CollectFileBlocks (entries[i], blocks, blockGuard);

            IGNORE_RETURN_VALUE (walked, true);

            listed.isLocked = (entries[i].access & ProDosVolume::kAccessDestroyEnable) == 0;
        }

        listed.blocks = (uint32_t) blocks.size();

        inOutPlan.blocksFreed     += listed.blocks;
        inOutPlan.hasLockedEntries = inOutPlan.hasLockedEntries || listed.isLocked;

        inOutPlan.entries.push_back (listed);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeRemover::BuildRemovalPlan
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosVolumeRemover::BuildRemovalPlan (const FilePath & path, DirectoryRemovalPlan & outPlan) const
{
    HRESULT                hr          = S_OK;
    size_t                 bufferBytes = m_volume.m_sectors.size();
    int                    keyBlock    = ProDosSkeleton::kDirKeyBlock;
    bool                   named       = !path.IsEmpty();
    ChainWalkGuard         guard ((uint32_t) ProDosSkeleton::kTotalBlocks);
    ChainWalkGuard         blockGuard ((uint32_t) ProDosSkeleton::kTotalBlocks);
    vector<uint32_t>                blocks;
    DirectoryRemovalEntry           itself;
    int                             parentKey = ProDosSkeleton::kDirKeyBlock;
    vector<ProDosVolume::RawEntry>  siblings;
    vector<std::string>             damage;
    bool                            parsed    = true;
    bool                            listed    = false;
    uint16_t                        local     = 0;



    CBREx (bufferBytes == (size_t) NibblizationLayer::kImageByteSize, E_INVALIDARG);
    CBREx (named, HRESULT_FROM_WIN32 (ERROR_INVALID_NAME));

    outPlan = DirectoryRemovalPlan();

    hr = m_volume.ResolveDirectory (path, keyBlock);
    CHR (hr);

    CollectRemovalEntries (keyBlock, path.ToString() + "/", guard, outPlan);

    //  The directory itself goes last, after everything it holds.
    m_volume.CollectDirectoryBlocks (keyBlock, blocks, blockGuard);

    //  The directory's record is gated on destroy-enable as each entry
    //  below it is.
    hr = m_volume.ResolveDirectory (ProDosVolume::GetParentPath (path), parentKey);
    CHR (hr);

    m_volume.CollectEntries (parentKey, siblings, damage, parsed);
    listed = ProDosVolume::TryFindEntry (siblings, path.GetLeaf(), local);

    itself.path        = path.ToString();
    itself.isDirectory = true;
    itself.blocks      = (uint32_t) blocks.size();
    itself.isLocked    = listed && (siblings[local].access & ProDosVolume::kAccessDestroyEnable) == 0;

    outPlan.blocksFreed      += itself.blocks;
    outPlan.hasLockedEntries  = outPlan.hasLockedEntries || itself.isLocked;

    outPlan.entries.push_back (itself);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeRemover::RemoveEmptyDirectory
//
//  The record becomes a tombstone the way a file's does, and the directory's
//  own chain of blocks goes back to the free map -- only the blocks this
//  record alone claims, by the rule every delete here follows.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosVolumeRemover::RemoveEmptyDirectory (
    const FilePath  & path,
    vector<Byte>    & outBuffer,
    DeleteOutcome   & outOutcome) const
{
    HRESULT                         hr          = S_OK;
    bool                            found       = false;
    bool                            fullyParsed = true;
    bool                            isEmpty     = false;
    int                             dirKeyBlock = ProDosSkeleton::kDirKeyBlock;
    uint16_t                        owner       = 0;
    uint16_t                        local       = 0;
    Byte                            typeLen     = 0;
    VolumeListing                   inside;
    vector<ProDosVolume::RawEntry>  entries;
    vector<ProDosVolume::RawEntry>  all;
    vector<std::string>             damage;
    vector<Byte>                    result;
    VolumeIntegrityReport           report;



    hr = m_volume.EnumerateDirectory (path, inside);
    CHR (hr);

    isEmpty = inside.entries.empty();
    CBREx (isEmpty, HRESULT_FROM_WIN32 (ERROR_DIR_NOT_EMPTY));

    hr = m_volume.ResolveDirectory (ProDosVolume::GetParentPath (path), dirKeyBlock);
    CHR (hr);

    m_volume.CollectEntries (dirKeyBlock, entries, damage, fullyParsed);

    found = ProDosVolume::TryFindEntry (entries, path.GetLeaf(), local);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    m_volume.CollectAllEntries (all, damage, fullyParsed);

    found = ProDosVolume::TryFindOwnerIndex (all, entries[local], owner);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    hr = m_volume.BuildIntegrityReport (report);
    CHRA (hr);

    result = m_volume.m_sectors;

    for (uint32_t block : report.GetClaimsOf (owner))
    {
        bool  mineAlone = report.IsUniquelyOwnedBy (block, owner);

        if (mineAlone)
        {
            ProDosVolumeWriter::SetFreeInBitmap (result, block, true);
            outOutcome.freedUnits.push_back (block);
        }
        else
        {
            outOutcome.leakedUnits.push_back (block);
        }
    }

    typeLen = m_volume.ReadByte (all[owner].dirBlock, all[owner].entryOffset + ProDosSkeleton::kEntOffTypeName);

    ProDosVolumeWriter::WriteByteAt (result,
                 all[owner].dirBlock,
                 all[owner].entryOffset + ProDosSkeleton::kEntOffTypeName,
                 (Byte) (typeLen & 0x0F));

    ProDosVolumeWriter::AdjustFileCount (result, dirKeyBlock, -1);

    hr = ProDosVolume::HandBackVerifiedResult (report, result, outBuffer);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeRemover::RemoveDirectory
//
//  THE WHOLE SUBTREE OR NONE OF IT. Every step is applied to a staged buffer
//  that no caller can see, and only a finished one is handed back, so a
//  removal that cannot finish leaves the image exactly as it was rather than
//  half-emptied.
//
//  A LOCKED ENTRY ANYWHERE STOPS IT unless `force` was given, and that is
//  decided before the first record changes -- discovering a locked file
//  half-way through would leave the caller with a directory partly removed and
//  nothing to say about the rest.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosVolumeRemover::RemoveDirectory (
    const FilePath  & path,
    bool              force,
    vector<Byte>    & outBuffer,
    DeleteOutcome   & outOutcome) const
{
    HRESULT               hr          = S_OK;
    size_t                bufferBytes = m_volume.m_sectors.size();
    bool                  permitted   = false;
    size_t                i           = 0;
    vector<Byte>          staged;
    DirectoryRemovalPlan  plan;



    CBREx (bufferBytes == (size_t) NibblizationLayer::kImageByteSize, E_INVALIDARG);

    hr = BuildRemovalPlan (path, plan);
    CHR (hr);

    permitted = force || !plan.hasLockedEntries;
    CBREx (permitted, HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

    outOutcome                    = DeleteOutcome();
    outOutcome.catalogFullyParsed = plan.catalogFullyParsed;

    staged = m_volume.m_sectors;

    for (i = 0; i < plan.entries.size(); i++)
    {
        ProDosVolume   step (staged);
        FilePath       entryPath = FilePath::Parse (plan.entries[i].path);
        DeleteOutcome  removal;
        vector<Byte>   next;

        if (plan.entries[i].isDirectory)
        {
            hr = step.m_remover.RemoveEmptyDirectory (entryPath, next, removal);
        }
        else
        {
            hr = step.m_remover.DeleteEntry (entryPath, force, next, removal);
        }

        CHR (hr);

        outOutcome.freedUnits.insert (outOutcome.freedUnits.end(), removal.freedUnits.begin(), removal.freedUnits.end());
        outOutcome.leakedUnits.insert (outOutcome.leakedUnits.end(), removal.leakedUnits.begin(), removal.leakedUnits.end());
        outOutcome.chainWasDamaged = outOutcome.chainWasDamaged || removal.chainWasDamaged;

        staged = next;
    }

    AppendDeleteWarnings (outOutcome);

    outBuffer = staged;

Error:
    return hr;
}
