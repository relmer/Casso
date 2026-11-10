#pragma once

#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/TestMachine.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/LinkProgram.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Seams/InProcessByteStream.h"
#include "InMemoryCassoLinkFolder.h"
#include "SteppedByteStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProgramRig
//
//  An emulated //c running the shim from $0300 at the link's speed, on port
//  2, with the host on the test's thread through a SteppedByteStream. The
//  host's first request loads the program into the shim. Nothing waits on
//  the PC's clock.
//
////////////////////////////////////////////////////////////////////////////////

struct ProgramRig
{
    static constexpr DWORD  kLinkBaud = 115200;
    static constexpr int    kPort     = 2;
    static constexpr int    kSlot     = 6;

    TestMachine                           machine { "Apple2c" };
    std::shared_ptr<InProcessByteStream>  machineEnd;
    std::shared_ptr<InProcessByteStream>  pcEnd;
    std::shared_ptr<SteppedByteStream>    stream;
    InMemoryCassoLinkFolder               folder;
    std::unique_ptr<CassoLinkHost>        host;

    //  `drive1` and `drive2` as DOS-ordered images, and the shim built for
    //  `shimBaud`. For an empty image, no disk is mounted in that drive.
    explicit ProgramRig (const std::vector<Byte> & drive1, const std::vector<Byte> & drive2 = {}, DWORD shimBaud = kLinkBaud)
    {
        std::vector<Byte>  shim;
        Word               at = LinkProgram::kShimAddress;



        machine.PowerCycle();

        if (!drive1.empty())
        {
            AssertSucceeded (machine.GetDiskStore().MountFromBytes (kSlot, 0, "", DiskFormat::Dsk, drive1));
        }

        if (!drive2.empty())
        {
            AssertSucceeded (machine.GetDiskStore().MountFromBytes (kSlot, 1, "", DiskFormat::Dsk, drive2));
        }

        InProcessByteStream::CreatePair (machineEnd, pcEnd);
        AssertSucceeded (machine.AttachSerialStream (kPort, machineEnd));

        stream = std::make_shared<SteppedByteStream> (machine, pcEnd);
        host   = std::make_unique<CassoLinkHost> (stream, folder, [] () { return std::array<Byte, 4> {}; });

        AssertSucceeded (LinkProgram::BuildShim (shimBaud, shim));

        for (Byte b : shim)
        {
            machine.GetCpu()->WriteByte (at++, b);
        }

        machine.GetCpu()->SetPC (LinkProgram::kShimAddress);
    }

    //  A drive's disk, as DOS-logical sectors; `drive` is 1 or 2.
    std::vector<Byte>  Sectors (int drive = 1)
    {
        std::vector<Byte>   file;
        std::vector<Byte>   sectors;
        SectorDecodeReport  report;



        AssertSucceeded (machine.GetDiskStore().GetImage (kSlot, drive - 1)->Serialize (file));
        AssertSucceeded (VolumeImage::Load (file, "d.dsk", sectors, report));
        return sectors;
    }

    void  Press (Byte key)
    {
        machine.GetRefs().iieKeyboard->PressKey (key);
    }
};
