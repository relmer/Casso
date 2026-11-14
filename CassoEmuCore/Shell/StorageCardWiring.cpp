#include "Pch.h"

#include "StorageCardWiring.h"

#include "Audio/DriveAudioMixer.h"
#include "Audio/Floppy35AudioSource.h"
#include "Audio/HardDiskAudioSource.h"
#include "Core/MachineConfig.h"
#include "Devices/BlockDevice/AppleScsiCard.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Devices/BlockDevice/ProFileCard.h"
#include "Devices/BlockDevice/ScsiHardDisk.h"
#include "Devices/BlockDevice/StockHardDiskImage.h"
#include "Devices/Floppy35/Apple2cPlusDiskPort.h"
#include "Devices/Floppy35/Apple2cSmartPort.h"
#include "Devices/Floppy35/Apple35ControllerCard.h"
#include "Devices/Floppy35/LironCard.h"
#include "Machines/Apple2/Apple2cPlus/MigChip.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/CxxxRomRouter.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Apple2c/Apple2cRomBank.h"
#include "WasapiAudio.h"
#include "../AssetBootstrap.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StorageCardWiring::StorageCardWiring
//
////////////////////////////////////////////////////////////////////////////////

StorageCardWiring::StorageCardWiring (MachineBuilder & builder, MachineHost & host, MachineBuildServices & services)
    : m_builder  (builder),
      m_host     (host),
      m_services (services)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  WireAppleScsiCard
//
//  Connects an Apple II SCSI Card the registry just created: its firmware,
//  its page and the expansion space, and an image on each SCSI ID its ports
//  list. A missing firmware file leaves the card without firmware, so the
//  slot scan passes over it. An image that fails to attach leaves its drive
//  empty, with the reason added to the machine's media problems.
//
////////////////////////////////////////////////////////////////////////////////

void StorageCardWiring::WireAppleScsiCard (const SlotConfig & slot, AppleScsiCard & card)
{
    HRESULT                hr      = S_OK;
    const BlockDriveSpec * drive   = BlockDeviceCatalog::FindDrive (kpszHd20scDevice);
    vector<Byte>           firmware;



    if (slot.firmwareStatus == SlotFirmwareStatus::Present)
    {
        hr = MachineBuilder::ReadRomFileBytes (slot.firmwarePath, firmware);

        if (SUCCEEDED (hr))
        {
            hr = card.SetFirmware (move (firmware));
        }

        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    m_builder.AddExpansionRomCard (slot.slot, card, card.GetSlotPage());

    for (size_t id = 0; id < slot.ports.size() && id < (size_t) AppleScsiCard::kTargets; id++)
    {
        const PortConfig  & port   = slot.ports[id];
        ScsiHardDisk      & target = card.GetTarget ((int) id);
        std::string         image  = port.image;
        std::wstring        error;



        target.SetRealTiming (port.realTiming, m_host.GetConfig().clockSpeed, 0);

        if (!port.device.empty())
        {
            card.SetHasDrive (true);
        }

        if (port.device.empty() || image.empty())
        {
            continue;
        }

        if (image == kpszStockImage)
        {
            if (m_host.GetStockImageFolder().empty())
            {
                continue;
            }

            hr = StockHardDiskImage::Ensure (m_host.GetBlockMediaFileIo(), m_host.GetStockImageFolder(), image);

            if (FAILED (hr))
            {
                m_host.GetBlockMediaProblems().push_back (std::format (L"Slot {}, SCSI ID {}: the stock hard disk image can't be made.", slot.slot, id));
                continue;
            }
        }

        hr = target.GetMedia().Attach (m_host.GetBlockMediaFileIo(), image, drive->maxBlocks, m_host.ProbeBlockMedia (image), error);

        if (FAILED (hr))
        {
            m_host.GetBlockMediaProblems().push_back (std::format (L"Slot {}, SCSI ID {}: {}", slot.slot, id, error));
        }
    }

    m_host.GetRefs().scsiCards.push_back (&card);
    m_host.GetRefs().blockDeviceSources.push_back (&card);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WireProFileCard
//
//  Connects a ProFile interface card: the drive its port lists, 5 or 10 MB,
//  the ROM for that model, its page and the expansion space, and the drive's
//  image. The 10 MB drive takes 341-0299-B from the same folder when the
//  entry lists the 5 MB ROM or none. A missing ROM leaves the card without
//  firmware; an image that fails to attach leaves the drive empty, with the
//  reason added to the machine's media problems.
//
////////////////////////////////////////////////////////////////////////////////

void StorageCardWiring::WireProFileCard (const SlotConfig & slot, ProFileCard & card)
{
    constexpr const char *  kpsz5MbRom  = "341-0271-A.rom";
    constexpr const char *  kpsz10MbRom = "341-0299-B.rom";
    HRESULT                 hr          = S_OK;
    std::string             device      = slot.ports.empty() ? std::string (kpszProFile5Device) : slot.ports[0].device;
    const BlockDriveSpec  * drive       = BlockDeviceCatalog::FindDrive (device);
    std::string             romPath     = slot.firmwarePath;
    std::wstring            error;
    vector<Byte>            firmware;



    if (drive == nullptr)
    {
        drive  = BlockDeviceCatalog::FindDrive (kpszProFile5Device);
        device = kpszProFile5Device;
    }

    card.SetDrive (std::make_unique<ProFileDrive> (slot.slot, drive->maxBlocks));

    if (device == kpszProFile10Device && std::filesystem::path (romPath).filename() == kpsz5MbRom)
    {
        romPath = (std::filesystem::path (romPath).parent_path() / kpsz10MbRom).string();
    }

    if (slot.firmwareStatus == SlotFirmwareStatus::Present || std::filesystem::exists (romPath))
    {
        hr = MachineBuilder::ReadRomFileBytes (romPath, firmware);

        if (SUCCEEDED (hr))
        {
            hr = card.SetFirmware (move (firmware));
        }

        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    m_builder.AddExpansionRomCard (slot.slot, card, card.GetSlotPage());

    if (!slot.ports.empty() && !slot.ports[0].image.empty())
    {
        const std::string  & image = slot.ports[0].image;



        hr = card.GetDrive().GetMedia().Attach (m_host.GetBlockMediaFileIo(), image, drive->maxBlocks, m_host.ProbeBlockMedia (image), error);

        if (FAILED (hr))
        {
            m_host.GetBlockMediaProblems().push_back (std::format (L"Slot {}, ProFile: {}", slot.slot, error));
        }
    }

    m_host.GetRefs().proFileCards.push_back (&card);
    m_host.GetRefs().blockDeviceSources.push_back (&card);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WireApple35Card
//
//  Connects an Apple II 3.5 Disk Controller Card: its firmware, its slot page
//  and the expansion space. The disk for each Apple 3.5 Drive in the slot's
//  ports is inserted. A disk that fails to insert is added to the machine's
//  media problems, and its drive is left empty.
//
////////////////////////////////////////////////////////////////////////////////

void StorageCardWiring::WireApple35Card (const SlotConfig & slot, Apple35ControllerCard & card)
{
    HRESULT       hr = S_OK;
    vector<Byte>  firmware;



    if (slot.firmwareStatus == SlotFirmwareStatus::Present)
    {
        hr = MachineBuilder::ReadRomFileBytes (slot.firmwarePath, firmware);

        if (SUCCEEDED (hr))
        {
            hr = card.SetFirmware (move (firmware));
        }

        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    m_builder.AddExpansionRomCard (slot.slot, card, card.GetSlotPage());

    for (size_t port = 0; port < slot.ports.size() && port < (size_t) Apple35CardMemory::kDrives; port++)
    {
        const PortConfig  & entry = slot.ports[port];
        std::wstring        error;



        if (entry.device.empty() || entry.image.empty())
        {
            continue;
        }

        hr = card.GetCardMemory().GetDrive ((int) port).Insert (m_host.GetBlockMediaFileIo(), entry.image, m_host.ProbeBlockMedia (entry.image), error);

        if (FAILED (hr))
        {
            m_host.GetBlockMediaProblems().push_back (std::format (L"Slot {}, drive {}: {}", slot.slot, port + 1, error));
        }
    }

    m_host.GetRefs().apple35Cards.push_back (&card);
    m_host.GetRefs().floppy35Sources.push_back (&card);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WireLironCard
//
//  Connects a UniDisk 3.5 interface card: its firmware, its slot page and the
//  expansion space, and a UniDisk 3.5 for each port in chain order, with its
//  ROM and its disk. A drive without its ROM is left off the chain; a disk
//  that fails to insert is added to the machine's media problems, and its
//  drive is left empty.
//
////////////////////////////////////////////////////////////////////////////////

void StorageCardWiring::WireLironCard (const SlotConfig & slot, LironCard & card)
{
    HRESULT       hr = S_OK;
    vector<Byte>  firmware;



    if (slot.firmwareStatus == SlotFirmwareStatus::Present)
    {
        hr = MachineBuilder::ReadRomFileBytes (slot.firmwarePath, firmware);

        if (SUCCEEDED (hr))
        {
            hr = card.SetFirmware (move (firmware));
        }

        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    m_builder.AddExpansionRomCard (slot.slot, card, card.GetSlotPage());

    for (size_t port = 0; port < slot.ports.size(); port++)
    {
        const PortConfig                & entry = slot.ports[port];
        std::unique_ptr<UniDisk35Drive>   drive;
        vector<Byte>                      driveRom;
        std::wstring                      error;



        if (entry.device != kpszUniDisk35Device || entry.firmwarePath.empty())
        {
            continue;
        }

        drive = std::make_unique<UniDisk35Drive>();

        hr = MachineBuilder::ReadRomFileBytes (entry.firmwarePath, driveRom);

        if (SUCCEEDED (hr))
        {
            hr = drive->SetFirmware (move (driveRom));
        }

        if (FAILED (hr))
        {
            continue;
        }

        if (!entry.image.empty())
        {
            hr = drive->GetMechanism().Insert (m_host.GetBlockMediaFileIo(), entry.image, m_host.ProbeBlockMedia (entry.image), error);

            if (FAILED (hr))
            {
                m_host.GetBlockMediaProblems().push_back (std::format (L"Slot {}, drive {}: {}", slot.slot, port + 1, error));
            }
        }

        card.AddDrive (std::move (drive));
    }

    m_host.GetRefs().lironCards.push_back (&card);
    m_host.GetRefs().floppy35Sources.push_back (&card);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WireDiskPortChain
//
//  The //c's disk port with a SmartPort chain: a UniDisk 3.5 for each entry,
//  with its ROM and its disk, on the port's IWM. A drive without its ROM is
//  left off the chain, and a drive whose disk fails to insert is empty;
//  each reason is added to the machine's media problems.
//
////////////////////////////////////////////////////////////////////////////////

void StorageCardWiring::WireDiskPortChain (Disk2Controller & controller)
{
    for (const PortConfig & port : m_host.GetConfig().ports)
    {
        std::unique_ptr<Apple2cSmartPort>  chain;
        bool                               hasFloppy = false;



        if (port.chain.empty())
        {
            continue;
        }

        hasFloppy = port.chain.back().device == kpszDiskIicDevice;
        chain     = std::make_unique<Apple2cSmartPort> (hasFloppy);

        AddChainDrives (port, chain->GetPort());

        controller.SetPortHook (chain.get());
        m_host.GetRefs().apple2cSmartPort = chain.get();
        m_host.GetRefs().floppy35Sources.push_back (chain.get());
        m_host.GetOwnedDevices().push_back (std::move (chain));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddChainDrives
//
//  A UniDisk 3.5 for each of a port's chain entries, with its ROM and its
//  disk. A drive without its ROM is left off the chain, and a drive whose
//  disk fails to insert is empty; each reason is added to the machine's
//  media problems.
//
////////////////////////////////////////////////////////////////////////////////

void StorageCardWiring::AddChainDrives (const PortConfig & port, SmartPortHost & host)
{
    for (size_t index = 0; index < port.chain.size(); index++)
    {
        const PortConfig                & entry = port.chain[index];
        std::unique_ptr<UniDisk35Drive>   drive;
        vector<Byte>                      driveRom;
        std::wstring                      error;
        HRESULT                           hr    = S_OK;



        if (entry.device != kpszUniDisk35Device)
        {
            continue;
        }

        if (entry.firmwarePath.empty())
        {
            m_host.GetBlockMediaProblems().push_back (std::format (L"Disk port, drive {}: the UniDisk 3.5's ROM, Devices/UniDisk35/UniDisk35.rom, is missing.", index + 1));
            continue;
        }

        drive = std::make_unique<UniDisk35Drive>();

        hr = MachineBuilder::ReadRomFileBytes (entry.firmwarePath, driveRom);

        if (SUCCEEDED (hr))
        {
            hr = drive->SetFirmware (move (driveRom));
        }

        if (FAILED (hr))
        {
            continue;
        }

        if (!entry.image.empty())
        {
            hr = drive->GetMechanism().Insert (m_host.GetBlockMediaFileIo(), entry.image, m_host.ProbeBlockMedia (entry.image), error);

            if (FAILED (hr))
            {
                m_host.GetBlockMediaProblems().push_back (std::format (L"Disk port, drive {}: {}", index + 1, error));
            }
        }

        host.AddDrive (std::move (drive));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WireApple2cPlusDrives
//
//  The //c Plus's internal Apple 3.5 Drive, with the disk the machine's
//  `internal` port has, and the disk port's chain.
//
////////////////////////////////////////////////////////////////////////////////

void StorageCardWiring::WireApple2cPlusDrives (Apple2cPlusDiskPort & port)
{
    for (const PortConfig & entry : m_host.GetConfig().ports)
    {
        std::wstring  error;
        HRESULT       hr = S_OK;



        if (entry.name == "internal" && !entry.image.empty())
        {
            hr = port.GetInternalDrive().Insert (m_host.GetBlockMediaFileIo(), entry.image, m_host.ProbeBlockMedia (entry.image), error);

            if (FAILED (hr))
            {
                m_host.GetBlockMediaProblems().push_back (std::format (L"Internal drive: {}", error));
            }
        }

        if (!entry.chain.empty())
        {
            AddChainDrives (entry, port.GetPort());
        }
    }

    m_host.GetRefs().apple2cPlusDiskPort = &port;
    m_host.GetRefs().floppy35Sources.push_back (&port);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WireMig
//
//  The //c Plus's MIG: in the internal ROM window ahead of the ROM, the
//  switch for the internal drive, and the IWM's reset.
//
////////////////////////////////////////////////////////////////////////////////

void StorageCardWiring::WireMig()
{
    Apple2cPlusDiskPort       * port = m_host.GetRefs().apple2cPlusDiskPort;
    std::unique_ptr<MigChip>    mig;



    if (port == nullptr || m_host.GetMmu() == nullptr)
    {
        return;
    }

    mig = std::make_unique<MigChip> (m_host.GetApple2cRomBank());

    m_host.GetMmu()->GetCxxxRouter()->SetWindowHandler (mig.get());
    m_host.GetRefs().migChip = mig.get();
    port->AdoptMig (std::move (mig));

    m_host.GetMmu()->RebindCxxxInternalRom();
}





////////////////////////////////////////////////////////////////////////////////
//
//  WireDeviceAudio
//
//  A synthesized sound source for each 3.5-inch drive controller and each
//  hard disk card, fed by its event feed and mixed with the Disk II's. A
//  card's hard disks spin up from the machine's start.
//
////////////////////////////////////////////////////////////////////////////////

void StorageCardWiring::WireDeviceAudio()
{
    constexpr uint32_t  kHd20scSpinUpMs  = 10'000;
    constexpr uint32_t  kProFileSpinUpMs = 4'000;
    constexpr float     kHd20scTick      = 0.15f;
    constexpr float     kProFileTick     = 0.25f;
    MachineRefs       & refs             = m_host.GetRefs();
    uint32_t            rate             = 0;
    float               motor            = 1.0f;
    float               head             = 1.0f;
    float               door             = 1.0f;



    if (m_services.deviceAudioSources == nullptr || m_services.driveAudioMixer == nullptr)
    {
        return;
    }

    m_services.deviceAudioSources->clear();

    if (m_services.wasapiAudio != nullptr && m_services.wasapiAudio->IsInitialized())
    {
        rate = m_services.wasapiAudio->GetSampleRate();
    }

    if (m_services.driveMotorVolume != nullptr)
    {
        motor = *m_services.driveMotorVolume;
        head  = *m_services.driveHeadVolume;
        door  = *m_services.driveDoorVolume;
    }

    for (IFloppy35EventSource * source : refs.floppy35Sources)
    {
        auto  audio = std::make_unique<Floppy35AudioSource>();



        audio->SetSampleRate (rate);
        audio->SetVolumes (motor, head, door);
        source->SetFloppy35EventSink (audio.get());
        m_services.driveAudioMixer->RegisterSource (audio.get());
        m_services.deviceAudioSources->push_back (std::move (audio));
    }

    for (AppleScsiCard * card : refs.scsiCards)
    {
        auto  audio = std::make_unique<HardDiskAudioSource> (kHd20scSpinUpMs, kHd20scTick);



        audio->SetSampleRate (rate);
        audio->SetVolumes (motor, head, door);

        if (card->HasDrive())
        {
            audio->PowerOn();
        }

        card->SetBlockDeviceEventSink (audio.get());
        m_services.driveAudioMixer->RegisterSource (audio.get());
        m_services.deviceAudioSources->push_back (std::move (audio));
    }

    for (ProFileCard * card : refs.proFileCards)
    {
        auto  audio = std::make_unique<HardDiskAudioSource> (kProFileSpinUpMs, kProFileTick);



        audio->SetSampleRate (rate);
        audio->SetVolumes (motor, head, door);
        audio->PowerOn();
        card->SetBlockDeviceEventSink (audio.get());
        m_services.driveAudioMixer->RegisterSource (audio.get());
        m_services.deviceAudioSources->push_back (std::move (audio));
    }
}
