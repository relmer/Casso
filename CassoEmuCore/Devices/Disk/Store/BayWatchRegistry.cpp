#include "Pch.h"

#include "Devices/Disk/Store/BayWatchRegistry.h"
#include "Devices/Disk/IImageWatcher.h"
#include "Devices/Disk/Store/DiskBayTable.h"
#include "Devices/Disk/Store/DiskStoreHooks.h"
#include "Devices/Disk/Store/DiskStoreInterfaces.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BayWatchRegistry::BeginWatching
//
//  Starts a watch on the directory holding a bay's image.
//
//  A DIRECTORY THAT CANNOT BE WATCHED IS RECORDED, NOT REPORTED. A network
//  share or a synchronizing folder is exactly the case that produces one, and
//  the guarantee does not live here: the check made before every write is what
//  carries it, and notification only makes it prompt.
//
////////////////////////////////////////////////////////////////////////////////

void BayWatchRegistry::BeginWatching (int slot, int drive)
{
    DiskBay  & entry     = m_bays.GetBay (slot, drive);
    string     directory = MountedImageState::GetDirectory (entry.path);
    bool       watching  = false;



    if (m_hooks.watcher != nullptr && !directory.empty())
    {
        watching = m_hooks.watcher->Watch (directory,
                                           [this] (const string & path)
                                           {
                                               m_notes.NoteExternalChange (path, ExternalChangeIntent::Unstated);
                                           });
    }

    entry.sharedState.SetWatching (watching);

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BayWatchRegistry::EndWatching
//
//  Drops a bay's watch, unless another bay still needs the same directory.
//
//  TWO DISKS OUT OF ONE FOLDER IS THE ORDINARY CASE -- a boot disk and a work
//  disk built by the same script -- and ejecting the first must not blind the
//  second.
//
////////////////////////////////////////////////////////////////////////////////

void BayWatchRegistry::EndWatching (int slot, int drive)
{
    DiskBay  & entry     = m_bays.GetBay (slot, drive);
    string     directory = MountedImageState::GetDirectory (entry.path);
    bool       stillUsed = m_bays.IsDirectoryInAnotherBay (directory, slot, drive);



    if (m_hooks.watcher != nullptr && !directory.empty() && !stillUsed)
    {
        m_hooks.watcher->Unwatch (directory);
    }

    entry.sharedState.SetWatching (false);

    return;
}





