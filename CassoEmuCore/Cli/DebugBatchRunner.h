#pragma once

#include "CommandLineOptions.h"
#include "Cli/DebugBatchSink.h"
#include "Debugger/DebugHandlerSet.h"
#include "Debugger/MachineDebugTarget.h"
#include "Debugger/SynchronousRunDriver.h"
#include "Shell/MachineBuilder.h"
#include "Shell/MachineHost.h"

class IFileSystem;
class IRomSource;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugBatchResult
//
////////////////////////////////////////////////////////////////////////////////

struct DebugBatchResult
{
    int          exitStatus = 0;
    std::string  output;          // what goes to standard output
    std::string  diagnostics;     // what goes to the error stream
};





////////////////////////////////////////////////////////////////////////////////
//
//  DebugBatchRunner
//
//  `CassoCli debug` without its console: builds the machine from an
//  IRomSource, mounts the disks, runs every line of the script and the
//  command lines through one session, and collects the text or JSON Lines
//  output and the exit status. Host files go through the injected
//  IFileSystem, no wall clock is read, and the DRAM seed is the option's,
//  so two runs of one script produce the same bytes.
//
//  Disk images are read through the file system seam, and what the guest
//  writes to them is flushed through the same seam only under --write-disks;
//  otherwise the flush is discarded and the file keeps its bytes.
//
//  Prepare and Execute are the two halves of Run, separately callable so a
//  caller can reach the machine between the mount and the script.
//
////////////////////////////////////////////////////////////////////////////////

class DebugBatchRunner
{
public:
    static constexpr int  kOk             = 0;
    static constexpr int  kCommandFailed  = 1;
    static constexpr int  kNothingStarted = 2;
    static constexpr int  kBudgetStop     = 3;

    DebugBatchRunner (const IRomSource & roms, IFileSystem & files);

    // scriptText holds the --script file's lines; the command lines run
    // after them.
    HRESULT  Run     (const CommandLineOptions::DebugOptions & options, const std::string & scriptText, DebugBatchResult & result);

    // The machine and its disks. A failure leaves its reason in the
    // result's diagnostics and the status at kNothingStarted.
    HRESULT  Prepare (const CommandLineOptions::DebugOptions & options, DebugBatchResult & result);

    // The lines, then the disk flush; sets the exit status.
    void     Execute (const CommandLineOptions::DebugOptions & options, const std::string & scriptText, DebugBatchResult & result);

    MachineHost *  GetHost () { return m_host.get(); }

private:
    static constexpr int  kDiskSlot = 6;

    HRESULT  BuildMachine (const CommandLineOptions::DebugOptions & options, DebugBatchResult & result);
    void     RouteDiskIo  (bool writeThrough);
    HRESULT  MountDisk    (const std::string & path, int drive, DebugBatchResult & result);
    void     RunLines     (const CommandLineOptions::DebugOptions & options, const std::vector<std::string> & lines, DebugSession & session, DebugBatchResult & result);

    static void         SplitLines (const std::string & text, std::vector<std::string> & lines);
    static std::string  Trim       (const std::string & text);

    const IRomSource                     & m_roms;
    IFileSystem                          & m_files;
    MachineBuildServices                   m_services;
    std::unique_ptr<MachineHost>           m_host;
    std::unique_ptr<MachineBuilder>        m_builder;
    std::unique_ptr<MachineDebugTarget>    m_target;
    std::unique_ptr<SynchronousRunDriver>  m_driver;
    DebugBatchSink                         m_sink;
    DebugHandlerSet                        m_handlers;
};
