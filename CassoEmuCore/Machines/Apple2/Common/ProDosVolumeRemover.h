#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/IVolume.h"
#include "Devices/Disk/ChainWalkGuard.h"

class ProDosVolume;





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeRemover
//
//  Removal from a ProDOS volume: a file deleted, an empty directory removed,
//  and a whole subtree planned and removed, deepest first.
//
////////////////////////////////////////////////////////////////////////////////

class ProDosVolumeRemover
{
public:
    explicit ProDosVolumeRemover (const ProDosVolume & volume);

    HRESULT  BuildRemovalPlan (const FilePath & path, DirectoryRemovalPlan & outPlan) const;

    HRESULT  RemoveDirectory  (const FilePath  & path,
                               bool              force,
                               vector<Byte>    & outBuffer,
                               DeleteOutcome   & outOutcome) const;

    HRESULT  Delete    (const FilePath & path, vector<Byte> & outBuffer) const;

    //  Delete, plus an account of what it freed, what it refused to free, and
    //  the conditions that bound the answer. The interface form forwards here
    //  and throws the account away; anything reporting to a user wants it.
    HRESULT  Delete    (const FilePath  & path,
                        vector<Byte>    & outBuffer,
                        DeleteOutcome   & outOutcome) const;

private:
    friend class ProDosVolume;
    friend class ProDosVolumeWriter;

    //  Turns what a delete decided into text a user can act on.
    static void  AppendDeleteWarnings (DeleteOutcome & inOutOutcome);

    //  One directory's contents into the plan, deepest first, each path written
    //  out from the volume directory.
    void  CollectRemovalEntries (int                     keyBlock,
                                 const std::string     & prefix,
                                 ChainWalkGuard        & guard,
                                 DirectoryRemovalPlan  & inOutPlan) const;

    //  What Delete does, plus the override a subtree removal needs: with
    //  `force` a locked entry goes too, which is the only difference between
    //  removing one file and removing one inside a subtree the caller has
    //  already been answered on.
    HRESULT  DeleteEntry (const FilePath  & path,
                          bool              force,
                          vector<Byte>    & outBuffer,
                          DeleteOutcome   & outOutcome) const;

    //  Removes a directory that holds nothing: its chain of blocks goes back to
    //  the free map and its record in the parent becomes a tombstone.
    HRESULT  RemoveEmptyDirectory (const FilePath  & path,
                                   vector<Byte>    & outBuffer,
                                   DeleteOutcome   & outOutcome) const;

    const ProDosVolume  & m_volume;
};