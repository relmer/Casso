#pragma once

#include "Pch.h"

#include "Ui/Debugger/IDebuggerWindowHost.h"

class CpuManager;
struct DebuggerSettings;
struct DebugSourceFile;
class IDebuggerHost;
struct SourceLookup;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs
//
//  The debugger window's preferences: its layout, docks, theme and keys, the
//  heat map's and the event logs' options, its placement on each monitor
//  arrangement, and the folders its source searches found files in. Each
//  setter saves the preferences through the host only when the value
//  changed; the strings the window reads are written under the project
//  mutex, which the project capture reads them under.
//
////////////////////////////////////////////////////////////////////////////////

class ShellDebuggerPrefs
{
public:
    ShellDebuggerPrefs (IDebuggerHost & host, CpuManager & cpuManager, DebuggerSettings & settings, std::mutex & projectMutex) :
        m_host (host), m_cpuManager (cpuManager), m_settings (settings), m_projectMutex (projectMutex) {}

    std::string     GetDebuggerKeyScheme          ();
    void            SetDebuggerKeyScheme          (const std::string & name);
    std::string     GetDebuggerTheme              ();
    void            SetDebuggerTheme              (const std::string & name);
    ReverseOptions  GetReverseOptions             ();
    void            SetReverseOptions             (const ReverseOptions & options);
    std::string     GetDebuggerLayout             ();
    void            SetDebuggerLayout             (const std::string & text);
    std::string     GetDebuggerClosedPanes        ();
    void            SetDebuggerClosedPanes        (const std::string & text);
    std::string     GetDebuggerCommandBarDock     ();
    void            SetDebuggerCommandBarDock     (const std::string & text);
    std::string     GetDebuggerTimelineDock       ();
    void            SetDebuggerTimelineDock       (const std::string & text);
    std::string     GetDebuggerFocusedPane        ();
    void            SetDebuggerFocusedPane        (const std::string & text);
    bool            IsDebuggerEchoingActions      ();
    void            SetDebuggerEchoingActions     (bool isEchoing);
    int             GetDebuggerTextZoomPercent    ();
    void            SetDebuggerTextZoomPercent    (int percent);
    std::string     GetDebuggerDisassemblyOptions ();
    void            SetDebuggerDisassemblyOptions (const std::string & text);
    std::string     GetDebuggerHeatMapOptions     ();
    void            SetDebuggerHeatMapOptions     (const std::string & text);
    std::string     GetDebuggerHeatMapRanges      ();
    void            SetDebuggerHeatMapRanges      (const std::string & text);
    std::string     GetDebuggerOpenViews          ();
    void            SetDebuggerOpenViews          (const std::string & text);
    std::string     GetDebuggerEventLogOptions    (DebugLogKind kind);
    void            SetDebuggerEventLogOptions    (DebugLogKind kind, const std::string & text);
    std::string     GetDebuggerWindowState        (const std::string & name);
    void            SetDebuggerWindowState        (const std::string & name, const std::string & text);
    bool            TryGetDebuggerPlacement       (RECT & rectPx);
    void            SetDebuggerPlacement          (const RECT & rectPx);

    SourceLookup  FindDebuggerSource         (const DebugSourceFile & record, const std::wstring & debugFilePath,
                                              const std::string & programKey);
    SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> & files, const std::wstring & path,
                                              const std::string & programKey, int & recordIndex);

private:
    std::string  GetDebuggerPlacementKey () const;

    IDebuggerHost     & m_host;
    CpuManager        & m_cpuManager;
    DebuggerSettings  & m_settings;
    std::mutex        & m_projectMutex;
};
