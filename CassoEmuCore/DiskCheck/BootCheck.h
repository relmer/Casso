#pragma once

#include "Pch.h"

#include "DiskCheck/BootProbe.h"
#include "Shell/MachineBuilder.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BootMachine
//
//  One of Casso's machines, built from its installed configuration and ROMs
//  with no window, audio or printer, for BootProbe.
//
////////////////////////////////////////////////////////////////////////////////

class BootMachine : public MachineHost
{
public:
    BootMachine ();

    //  The machine's directory name, as Apple2c, and BootCheck::kThirteen
    //  after it for its Disk II with the 13-sector boot ROM. Fails with
    //  outError when the configuration or a ROM isn't installed.
    HRESULT           Build      (const std::string & machineId, std::string & outError);

    MachineBuilder  & GetBuilder () noexcept { return m_builder; }

    //  The machine's JSON again, with only its Disk II cards in its slots,
    //  each with the 13-sector boot ROM when `thirteen` is set.
    static HRESULT  KeepDiskSlots (std::string & ioJson, bool thirteen = false);

private:
    static constexpr uint64_t  s_kSeed = 0xCA550002ULL;

    MachineBuildServices  m_nothingListening;
    MachineBuilder        m_builder;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BootCheck
//
//  A disk image booted on each machine at once, a thread for each: the
//  model it was captured from, when Casso has that machine, then the
//  enhanced //e, then the ][+. A WOZ whose boot sector format is 13-sector
//  is booted on a ][+ with the 13-sector Disk II boot ROM first. The report is that of the first of them, in
//  that order, on which the image boots, or the best of them when it boots
//  on none, with the machine it is from. A machine after one the image
//  boots on is stopped. A machine that can't be built, as with a ROM not
//  installed, is passed over and listed in outMissing.
//
////////////////////////////////////////////////////////////////////////////////

class BootCheck
{
public:
    //  A built machine and its builder. The host is shared, not unique:
    //  MachineHost's destructor isn't virtual, and a shared pointer made
    //  from the machine's class destroys it as that class.
    struct Built
    {
        std::shared_ptr<MachineHost>  host;
        MachineBuilder              * builder = nullptr;
    };

    using MachineFn = std::function<HRESULT (const std::string & machineId, Built & outBuilt, std::string & outError)>;

    static constexpr const char *  kEnhanced = "Apple2eEnhanced";
    static constexpr const char *  kPlus     = "Apple2Plus";
    static constexpr const char *  kThirteen = "/13";                  // after a machine's id, its Disk II has the 13-sector boot ROM
    static constexpr const char *  kRom13    = "Disk2_13Sector.rom";

    //  S_OK with a report from at least one machine; a failure with none,
    //  as when no machine's ROMs are installed.
    static HRESULT      Run (const std::vector<Byte> & image, DiskFormat format, const std::string & capturedFrom, const BootProbe::Limits & limits,
                             const MachineFn & build, BootReport & outReport, std::string & outMachine, std::vector<std::string> & outMissing);

    //  The machines, in order, with none twice: a 13-sector disk's first.
    static std::vector<std::string>  GetMachines (const std::string & capturedFrom, bool isThirteen = false);

    //  Whether an image is a WOZ whose boot sector format is 13-sector.
    static bool         IsThirteenSector (const std::vector<Byte> & image, DiskFormat format);

    //  A model's name as CassoLink has it, such as Apple //c (ROM 3), to
    //  Casso's machine, such as Apple2c; empty for a model Casso lacks.
    static std::string  GetMachineId (const std::string & model);

    //  A machine as text for the user, such as enhanced Apple //e.
    static std::string  GetMachineName (const std::string & machineId);

    //  Whether a verdict is better than another: booted, then not bootable,
    //  then unclear, then did not boot.
    static bool         IsBetter (BootVerdict verdict, BootVerdict than);

    //  The machines of an installation, built by BootMachine.
    static HRESULT      BuildInstalled (const std::string & machineId, Built & outBuilt, std::string & outError);
};
