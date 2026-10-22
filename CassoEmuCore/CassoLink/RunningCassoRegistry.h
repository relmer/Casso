#pragma once

#include "Pch.h"

class IFileSystem;
class IProcessProbe;





////////////////////////////////////////////////////////////////////////////////
//
//  RunningCasso
//
//  One running Casso's serial port on TCP, as its file has it (FR-083b). The
//  port is a //c's port number, or with `onCard`, a Super Serial Card's slot.
//
////////////////////////////////////////////////////////////////////////////////

struct RunningCasso
{
    DWORD        pid        = 0;
    uint64_t     started    = 0;
    std::string  machine;
    int          serialPort = 0;
    bool         onCard     = false;
    std::string  address;
};





////////////////////////////////////////////////////////////////////////////////
//
//  RunningCassoRegistry
//
//  The files through which Casso Explorer finds each running Casso whose
//  serial port is on TCP: one per port, <pid>-<port>.json in a folder under
//  %LOCALAPPDATA%\Casso. Casso writes a port's file when it connects the
//  port to TCP and deletes it when the port is connected elsewhere or the
//  machine goes. Casso Explorer reads them, and deletes any whose process
//  has exited, or whose process id is now another process's, started at
//  another time, so a Casso that crashed is not listed.
//
//  The process's start time is a decimal string in the file, as a JSON
//  number cannot represent a FILETIME exactly.
//
////////////////////////////////////////////////////////////////////////////////

class RunningCassoRegistry
{
public:
    RunningCassoRegistry (IFileSystem & fileSystem, IProcessProbe & probe, std::wstring folder);

    //  %LOCALAPPDATA%\Casso\Links.
    static std::wstring  GetDefaultFolder ();

    const std::wstring &  GetFolder () const { return m_folder; }

    HRESULT  Publish   (DWORD pid, int serialPort, const std::string & machine, uint16_t tcpPort, bool onCard = false);
    void     Withdraw  (DWORD pid, int serialPort);

    //  The running Cassos, by process id and then port.
    HRESULT  Enumerate (std::vector<RunningCasso> & outRunning);

    static std::string  ToJson   (const RunningCasso & running);
    static bool         FromJson (const std::string & text, RunningCasso & outRunning);

private:
    std::wstring  MakePath (DWORD pid, int serialPort) const;

    IFileSystem    & m_fileSystem;
    IProcessProbe  & m_probe;
    std::wstring     m_folder;
};
