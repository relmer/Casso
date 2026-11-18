#include "Pch.h"

#include "Apple3MachineBuilder.h"
#include "MachineBuilder.h"
#include "Core/EmuCpu.h"
#include "Core/MachineConfig.h"
#include "Core/Prng.h"
#include "Machines/Apple3/Apple3System.h"
#include "Audio/DriveAudioMixer.h"
#include "Machines/Apple3/Apple3VideoOutput.h"
#include "Machines/Apple3/Cpu6502Apple3.h"
#include "Shell/MachineHost.h"





static constexpr size_t  s_kBytesPerK = 1024;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3MachineBuilder
//
////////////////////////////////////////////////////////////////////////////////

Apple3MachineBuilder::Apple3MachineBuilder (MachineHost & host, const MachineBuildServices & services, MachineBuilder & builder)
    : m_host     (host),
      m_services (services),
      m_builder  (builder)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Build
//
//  The system first, since the CPU has a reference to its memory map; the
//  CPU before the map is initialized, since the CPU's constructor maps its
//  RAM into the bus and the map replaces that mapping.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple3MachineBuilder::Build (const MachineConfig & config)
{
    HRESULT  hr = S_OK;



    m_host.SetApple3 (std::make_unique<Apple3System> (m_host.GetMemoryBus()));

    CreateCpu();

    hr = CreateSystem (config);
    CHR (hr);

    WireDiskDrive();
    m_builder.WireDriveAudio (Apple3DiskController::kDriveCount);
    WireSound (config);
    CreateVideo();

    hr = m_host.GetMemoryBus().Validate();
    CHR (hr);

    m_host.GetApple3()->PowerOn (*m_host.GetPrng());
    m_host.GetStorage().BindDiskDrives();
    m_host.GetCpu()->InitForEmulation (*m_host.GetPrng());

    m_host.AttachDevices();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateCpu
//
////////////////////////////////////////////////////////////////////////////////

void Apple3MachineBuilder::CreateCpu()
{
    std::unique_ptr<ICpu>   cpu = std::make_unique<Cpu6502Apple3> (m_host.GetMemoryBus(), m_host.GetApple3()->GetMmu());



    m_host.SetCpu (std::make_unique<EmuCpu> (m_host.GetMemoryBus(), std::move (cpu)));

    if (m_services.GetTraceCapacity() > 0)
    {
        m_host.GetCpu()->EnableTrace (m_services.GetTraceCapacity());
    }

    m_host.GetInterruptController().SetCpu (m_host.GetCpu()->GetCpu());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateSystem
//
//  The system ROM must be the Apple ///'s 4K; a file of another size fails
//  the build with ERROR_INVALID_DATA. A missing file fails the config load
//  before this, through the missing-ROM path every machine uses.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple3MachineBuilder::CreateSystem (const MachineConfig & config)
{
    HRESULT            hr        = S_OK;
    std::vector<Byte>  rom;
    Apple3System     & system    = *m_host.GetApple3();
    bool               isRomSize = false;



    hr = MachineBuilder::ReadRomFileBytes (config.systemRom.resolvedPath, rom);
    CHR (hr);

    isRomSize = rom.size() == kRomBytes;
    CBREx (isRomSize, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = system.Initialize (m_host.GetInterruptController(), (size_t) config.memoryK * s_kBytesPerK, rom);
    CHR (hr);

    system.GetDiskController().SetExternalDriveCount (config.externalDrives);

    m_host.GetRefs().keyboard = &system.GetKeyboard();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WireDiskDrive
//
//  The built-in drive is the Disk II logic's drive 1, at the slot 6
//  addresses, so the disk store, the drive widgets and the debugger access
//  it as the Disk II controller. Its clock is the floppy controller's, in
//  master clocks, not the CPU's ticks. The flush and reload callbacks are
//  the Apple II machines', on the host, which outlives this builder.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3MachineBuilder::WireDiskDrive()
{
    Disk2Controller  & diskII = m_host.GetApple3()->GetDiskController().GetDiskII();
    MachineHost      * host   = &m_host;



    m_host.GetRefs().diskController = &diskII;
    m_host.GetRefs().apple3Disk     = &m_host.GetApple3()->GetDiskController();
    m_host.GetApple3()->GetDiskController().SetCycleSources (m_host.GetCpu()->GetBusCyclePtr(), m_host.GetCpu()->GetCycleCounterPtr());

    diskII.GetRecorder().SetInstructionAddressSource (m_host.GetCpu()->GetInstructionStartPtr());

    diskII.GetSignals().SetMotorOffFlushCallback ([host] ()
    {
        host->GetDiskStore().FlushAllUnlessHeld();
        host->GetDiskStore().ApplyPendingReload();
    });

    if (!m_services.IsImageWatchDisabled())
    {
        diskII.GetSignals().SetIdleCallback ([host] ()
        {
            host->GetDiskStore().ApplyPendingReload();
        });
    }

    m_host.GetDiskStore().SetMachineRestartCallback (m_services.requestPowerCycle);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WireSound
//
//  The speaker is the audio path's toggle log, as on an Apple II. The
//  beeper and the DAC are a source in the sound card mixer. Both count the
//  CPU's cycles, which are the time base's ticks.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3MachineBuilder::WireSound (const MachineConfig & config)
{
    Apple3Sound  & sound = m_host.GetApple3()->GetSound();



    sound.GetSpeaker().SetCycleCounter (m_host.GetCpu()->GetCycleCounterPtr());
    sound.SetTickSource (m_host.GetCpu()->GetCycleCounterPtr(), (double) config.clockSpeed);

    m_host.GetRefs().speaker = &sound.GetSpeaker();

    if (m_services.mockingboardAudioMixer != nullptr)
    {
        m_services.mockingboardAudioMixer->UnregisterAllSources();
        m_services.mockingboardAudioMixer->RegisterSource (&sound);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateVideo
//
////////////////////////////////////////////////////////////////////////////////

void Apple3MachineBuilder::CreateVideo()
{
    auto  output = std::make_unique<Apple3VideoOutput> (m_host.GetApple3()->GetVideo());



    m_builder.CreateVideoModes();

    m_host.GetRefs().apple3Video     = output.get();
    m_host.GetRefs().activeVideoMode = output.get();
    m_host.GetVideoModes().push_back (std::move (output));
}
