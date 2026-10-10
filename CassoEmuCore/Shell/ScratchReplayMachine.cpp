#include "Pch.h"

#include "Shell/ScratchReplayMachine.h"

#include "Core/Prng.h"
#include "Devices/Disk/DiskImage.h"
#include "Shell/HeadlessMachineFactory.h"
#include "Shell/MachineHost.h"
#include "Debugger/Reverse/ReplayDiskCopier.h"
#include "Debugger/Reverse/Replayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchReplayMachine::~ScratchReplayMachine
//
////////////////////////////////////////////////////////////////////////////////

ScratchReplayMachine::~ScratchReplayMachine()
{
    Release();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchReplayMachine::SetMachine
//
////////////////////////////////////////////////////////////////////////////////

void ScratchReplayMachine::SetMachine (
    const MachineConfig  & config,
    const std::wstring   & name)
{
    std::lock_guard<std::mutex>  held (m_configLock);



    m_config = config;
    m_name   = name;
    m_configGeneration++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchReplayMachine::Build
//
//  As the headless factory builds a machine, again whenever the running
//  machine's configuration changed: a Prng and a video timing model before
//  the devices. Its disk store never writes a file, and replays only. A
//  machine that does not build is dropped, so the next replay tries again.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScratchReplayMachine::Build()
{
    HRESULT        hr         = S_OK;
    MachineConfig  config;
    std::wstring   name;
    uint64_t       generation = 0;



    {
        std::lock_guard<std::mutex>  held (m_configLock);

        generation = m_configGeneration;

        if (generation != m_builtGeneration)
        {
            config = m_config;
            name   = m_name;
        }
    }

    CBR (generation != 0);

    BAIL_OUT_IF (generation == m_builtGeneration && m_machine != nullptr, S_OK);

    Release();

    m_machine = std::make_unique<MachineHost>();
    m_builder = std::make_unique<MachineBuilder> (*m_machine, m_services);

    m_machine->SetPrng               (std::make_unique<Prng> (HeadlessMachineFactory::kDefaultSeed));
    m_machine->SetVideoTiming        (std::make_unique<VideoTiming>());
    m_machine->SetCurrentMachineName (name);
    m_machine->GetConfig() = config;

    hr = m_builder->Build (config);
    CHR (hr);

    m_machine->GetDiskStore().SetFlushSink ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });

    m_replayer = std::make_unique<Replayer> (*m_machine, m_noKeyframes);
    m_replayer->SetOverMountedMedia (true);

    m_builtGeneration = generation;

Error:
    if (FAILED (hr))
    {
        Release();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchReplayMachine::MountDisks
//
//  Each bay holds the medium given for it, mounted from its image only when
//  a different one is there, with as many track slots as the medium has,
//  and bays the list leaves out are emptied.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScratchReplayMachine::MountDisks (const std::vector<ReplayDisk> & disks)
{
    HRESULT       hr       = S_OK;
    DiskImage   * image    = nullptr;
    BayIds        wanted   = {};
    size_t        bay      = 0;
    bool          hasImage = false;



    CBRA (m_machine);

    for (const ReplayDisk & disk : disks)
    {
        wanted[(size_t) disk.slot * DiskImageStore::kDriveCount + (size_t) disk.drive] = disk.mediaId;
    }

    for (bay = 0; bay < wanted.size(); bay++)
    {
        if (wanted[bay] == 0 && m_mounted[bay] != 0)
        {
            m_machine->GetDiskStore().Eject ((int) (bay / DiskImageStore::kDriveCount), (int) (bay % DiskImageStore::kDriveCount));
            m_mounted[bay] = 0;
        }
    }

    for (const ReplayDisk & disk : disks)
    {
        bay = (size_t) disk.slot * DiskImageStore::kDriveCount + (size_t) disk.drive;

        if (m_mounted[bay] == disk.mediaId)
        {
            continue;
        }

        hasImage = disk.image != nullptr;
        CBRA (hasImage);

        m_mounted[bay] = 0;

        hr = m_machine->GetDiskStore().MountFromBytes (disk.slot, disk.drive, std::format ("history-replay-s{}d{}", disk.slot, disk.drive + 1), disk.format, *disk.image);
        CHR (hr);

        image = m_machine->GetDiskStore().GetImage (disk.slot, disk.drive);
        CBRA (image);

        image->EnsureTrackSlots (disk.trackCount);

        m_mounted[bay] = disk.mediaId;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchReplayMachine::Release
//
//  The replayer and the builder go before the machine they hold.
//
////////////////////////////////////////////////////////////////////////////////

void ScratchReplayMachine::Release()
{
    m_replayer.reset();
    m_builder.reset();
    m_machine.reset();
    m_mounted.fill (0);
}





