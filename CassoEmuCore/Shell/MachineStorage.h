#pragma once

#include "Pch.h"

#include "Devices/BlockDevice/IReplayMedia.h"
#include "Devices/Disk/IDiskImage.h"

class DiskImageStore;
class IDiskFileIo;
struct MachineConfig;
struct MachineParts;
class Sony35Drive;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStorage
//
//  A machine's disks outside the 5.25-inch drive bays: its hard disks and
//  3.5-inch drives, how they access their image files, and their copies for
//  a replay on a second machine. And the link from the drive bays to the
//  slot 6 Disk II.
//
////////////////////////////////////////////////////////////////////////////////

class MachineStorage
{
public:
    //  How a file's write protection is found.
    using WriteProtectProbe = std::function<WriteProtectInfo (const std::string & path)>;

    MachineStorage (MachineParts & parts, MachineConfig & config, DiskImageStore & diskStore);
    ~MachineStorage();

    MachineStorage (const MachineStorage &)             = delete;
    MachineStorage & operator= (const MachineStorage &) = delete;

    //  How the hard disks and 3.5-inch drives access their image files, and
    //  how a file's write protection is found. Both default to the host's
    //  file system. A test can use an in-memory file system, whose probe
    //  reports every file writable unless the test sets a probe too.
    void                SetBlockMediaFileIo (IDiskFileIo * fileIo, WriteProtectProbe probe = nullptr);
    IDiskFileIo       & GetBlockMediaFileIo ();
    WriteProtectInfo    ProbeBlockMedia     (const std::string & path) const;

    //  Why an image the machine file lists did not attach, one line each, for
    //  a notice; cleared at each build.
    std::vector<std::wstring>  & GetBlockMediaProblems() noexcept { return m_blockMediaProblems; }

    //  Where the stock hard disk image is made and found; with none, an
    //  HD20SC that lists "stock" has no image.
    void                 SetStockImageFolder (const std::string & folder) { m_stockImageFolder = folder; }
    const std::string  & GetStockImageFolder () const noexcept            { return m_stockImageFolder; }

    //  The 3.5-inch drive at `drive` of the UniDisk 3.5 or 3.5 controller
    //  card in `slot`, or for slot 0 the //c disk port's: the //c Plus's
    //  internal drive as drive 0, then the drives on the chain. Null for a
    //  slot or drive without one. Inserting or ejecting runs on the CPU
    //  thread.
    Sony35Drive  * FindFloppy35   (int slot, int drive);
    HRESULT        InsertFloppy35 (int slot, int drive, const std::string & path, std::wstring & outError);
    HRESULT        EjectFloppy35  (int slot, int drive);

    //  A hard disk's image, by its card's slot and the drive's SCSI ID (0 for
    //  a ProFile), with the machine build's checks. When an image fails them,
    //  the drive has its image from before. The machine's configuration takes
    //  the new image. Runs on the CPU thread.
    HRESULT  AttachHardDisk (int slot, int drive, const std::string & path, std::wstring & outError);

    //  Points the slot 6 controller's drive at the image the disk store has
    //  in that bay, or at the drive's empty disk when the bay is empty. The
    //  store's default bay-change sink; a shell that installs another sink
    //  calls this from it. BindDiskDrives does it for both drives.
    void  ConnectDiskToController (int slot, int drive);
    void  BindDiskDrives          ();

    //  The hard disks, ProFiles and 3.5-inch drives, in an order two machines
    //  built from one configuration share. A replay on a second machine
    //  copies the first machine's disks (CopyReplayMedia, on the thread that
    //  runs the machine) and mounts them in the second's matching drives
    //  (MountReplayMedia), except where `mounted` already has that copy.
    //  Copies from a machine with other drives are bad data.
    void     GetReplayMedia   (std::vector<IReplayMedia *> & outDrives);
    HRESULT  CopyReplayMedia  (std::vector<ReplayBlockMedia> & outCopies);
    HRESULT  MountReplayMedia (const std::vector<ReplayBlockMedia> & copies, std::vector<ReplayBlockMedia> & mounted);

private:
    //  The slot whose Disk II the refs have, which the drive bays feed.
    static constexpr int  kDiskControllerSlot = 6;

    MachineParts                   & m_parts;
    MachineConfig                  & m_config;
    DiskImageStore                 & m_diskStore;
    std::unique_ptr<IDiskFileIo>     m_hostBlockFileIo;
    IDiskFileIo                    * m_blockFileIo = nullptr;
    WriteProtectProbe                m_blockProbe;
    std::vector<std::wstring>        m_blockMediaProblems;
    std::string                      m_stockImageFolder;
};
