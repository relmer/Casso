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

    //  The machine's directory name, as Apple2c. Fails with outError when
    //  the configuration or a ROM isn't installed.
    HRESULT           Build      (const std::string & machineId, std::string & outError);

    MachineBuilder  & GetBuilder () noexcept { return m_builder; }

private:
    static constexpr uint64_t  s_kSeed = 0xCA550002ULL;

    MachineBuildServices  m_nothingListening;
    MachineBuilder        m_builder;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BootCheck
//
//  A disk image booted on one machine after another until it boots: the
//  model it was captured from, when Casso has that machine, then the
//  enhanced //e, then the ][+. The report is the best of them, with the
//  machine it is from. A machine that can't be built, as with a ROM not
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

    //  S_OK with a report from at least one machine; a failure with none,
    //  as when no machine's ROMs are installed.
    static HRESULT      Run (const std::vector<Byte> & image, DiskFormat format, const std::string & capturedFrom, const BootProbe::Limits & limits,
                             const MachineFn & build, BootReport & outReport, std::string & outMachine, std::vector<std::string> & outMissing);

    //  The machines, in order, with none twice.
    static std::vector<std::string>  GetMachines (const std::string & capturedFrom);

    //  A model's name as CassoLink has it, such as Apple //c (ROM 3), to
    //  Casso's machine, such as Apple2c; empty for a model Casso lacks.
    static std::string  GetMachineId (const std::string & model);

    //  Whether a verdict is better than another: booted, then not bootable,
    //  then unclear, then did not boot.
    static bool         IsBetter (BootVerdict verdict, BootVerdict than);

    //  The machines of an installation, built by BootMachine.
    static HRESULT      BuildInstalled (const std::string & machineId, Built & outBuilt, std::string & outError);
};
