#include "Pch.h"

#include "Shell/ShellDebuggerPrefs.h"

#include "Config/WindowPlacementProfile.h"
#include "Debugger/Source/SourcePathList.h"
#include "Debugger/Source/SourceService.h"
#include "resource.h"
#include "Shell/CpuCommandDispatcher.h"
#include "Shell/CpuManager.h"
#include "Shell/IDebuggerHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::GetDebuggerKeyScheme
//
////////////////////////////////////////////////////////////////////////////////

std::string ShellDebuggerPrefs::GetDebuggerKeyScheme()
{
    return m_settings.keyScheme;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::SetDebuggerKeyScheme
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebuggerPrefs::SetDebuggerKeyScheme (const std::string & name)
{
    m_settings.keyScheme = name;
    m_host.SaveSettings();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::GetDebuggerTheme
//
////////////////////////////////////////////////////////////////////////////////

std::string ShellDebuggerPrefs::GetDebuggerTheme()
{
    return m_settings.theme;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::SetDebuggerTheme
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebuggerPrefs::SetDebuggerTheme (const std::string & name)
{
    m_settings.theme = name;
    m_host.SaveSettings();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::GetReverseOptions
//
////////////////////////////////////////////////////////////////////////////////

ReverseOptions ShellDebuggerPrefs::GetReverseOptions()
{
    ReverseOptions  options;



    options.isRecording = m_settings.reverseRecording;
    options.budgetMb    = m_settings.reverseBudgetMb;
    return options;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::SetReverseOptions
//
//  Saved, and posted to the CPU thread, which owns the reverse host and
//  applies them at once.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebuggerPrefs::SetReverseOptions (const ReverseOptions & options)
{
    m_settings.reverseRecording = options.isRecording;
    m_settings.reverseBudgetMb  = options.budgetMb;
    m_host.SaveSettings();

    m_host.PostCommand (IDM_DEBUG_REVERSE_OPTIONS, CpuCommandDispatcher::FormatReverseOptionsPayload (options.isRecording, options.budgetMb));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::GetDebuggerLayout
//
////////////////////////////////////////////////////////////////////////////////

std::string ShellDebuggerPrefs::GetDebuggerLayout()
{
    return m_settings.layout;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::SetDebuggerLayout
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebuggerPrefs::SetDebuggerLayout (const std::string & text)
{
    if (m_settings.layout == text)
    {
        return;
    }

    {
        std::lock_guard<std::mutex>  lock (m_projectMutex);



        m_settings.layout = text;
    }

    m_host.SaveSettings();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::GetDebuggerClosedPanes
//
////////////////////////////////////////////////////////////////////////////////

std::string ShellDebuggerPrefs::GetDebuggerClosedPanes()
{
    return m_settings.closedPanes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::SetDebuggerClosedPanes
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebuggerPrefs::SetDebuggerClosedPanes (const std::string & text)
{
    if (m_settings.closedPanes == text)
    {
        return;
    }

    {
        std::lock_guard<std::mutex>  lock (m_projectMutex);



        m_settings.closedPanes = text;
    }

    m_host.SaveSettings();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::GetDebuggerCommandBarDock
//
////////////////////////////////////////////////////////////////////////////////

std::string ShellDebuggerPrefs::GetDebuggerCommandBarDock()
{
    return m_settings.commandBarDock;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::SetDebuggerCommandBarDock
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebuggerPrefs::SetDebuggerCommandBarDock (const std::string & text)
{
    if (m_settings.commandBarDock == text)
    {
        return;
    }

    {
        std::lock_guard<std::mutex>  lock (m_projectMutex);



        m_settings.commandBarDock = text;
    }

    m_host.SaveSettings();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::GetDebuggerTimelineDock
//
////////////////////////////////////////////////////////////////////////////////

std::string ShellDebuggerPrefs::GetDebuggerTimelineDock()
{
    return m_settings.timelineDock;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::SetDebuggerTimelineDock
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebuggerPrefs::SetDebuggerTimelineDock (const std::string & text)
{
    if (m_settings.timelineDock == text)
    {
        return;
    }

    {
        std::lock_guard<std::mutex>  lock (m_projectMutex);



        m_settings.timelineDock = text;
    }

    m_host.SaveSettings();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::GetDebuggerFocusedPane
//
////////////////////////////////////////////////////////////////////////////////

std::string ShellDebuggerPrefs::GetDebuggerFocusedPane()
{
    return m_settings.focusedPane;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::SetDebuggerFocusedPane
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebuggerPrefs::SetDebuggerFocusedPane (const std::string & text)
{
    if (m_settings.focusedPane == text)
    {
        return;
    }

    {
        std::lock_guard<std::mutex>  lock (m_projectMutex);



        m_settings.focusedPane = text;
    }

    m_host.SaveSettings();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::IsDebuggerEchoingActions
//
////////////////////////////////////////////////////////////////////////////////

bool ShellDebuggerPrefs::IsDebuggerEchoingActions()
{
    return m_settings.echoActions;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::SetDebuggerEchoingActions
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebuggerPrefs::SetDebuggerEchoingActions (bool isEchoing)
{
    if (m_settings.echoActions == isEchoing)
    {
        return;
    }

    {
        std::lock_guard<std::mutex>  lock (m_projectMutex);

        m_settings.echoActions = isEchoing;
    }

    m_host.SaveSettings();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::GetDebuggerTextZoomPercent
//
////////////////////////////////////////////////////////////////////////////////

int ShellDebuggerPrefs::GetDebuggerTextZoomPercent()
{
    return m_settings.textZoomPercent;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::SetDebuggerTextZoomPercent
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebuggerPrefs::SetDebuggerTextZoomPercent (int percent)
{
    if (m_settings.textZoomPercent == percent)
    {
        return;
    }

    m_settings.textZoomPercent = percent;
    m_host.SaveSettings();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::GetDebuggerDisassemblyOptions
//
////////////////////////////////////////////////////////////////////////////////

std::string ShellDebuggerPrefs::GetDebuggerDisassemblyOptions()
{
    return m_settings.disassemblyOptions;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::SetDebuggerDisassemblyOptions
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebuggerPrefs::SetDebuggerDisassemblyOptions (const std::string & text)
{
    if (m_settings.disassemblyOptions == text)
    {
        return;
    }

    {
        std::lock_guard<std::mutex>  lock (m_projectMutex);



        m_settings.disassemblyOptions = text;
    }

    m_host.SaveSettings();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::GetDebuggerHeatMapOptions
//
////////////////////////////////////////////////////////////////////////////////

std::string ShellDebuggerPrefs::GetDebuggerHeatMapOptions()
{
    return m_settings.heatMapOptions;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::SetDebuggerHeatMapOptions
//
//  UI thread. The CPU thread is told every time, since the window sends its
//  options as it opens, when the machine has only the defaults; the setting
//  is saved only when it changed.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebuggerPrefs::SetDebuggerHeatMapOptions (const std::string & text)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, "heatmap options " + text);

    if (m_settings.heatMapOptions == text)
    {
        return;
    }

    {
        std::lock_guard<std::mutex>  lock (m_projectMutex);



        m_settings.heatMapOptions = text;
    }

    m_host.SaveSettings();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::GetDebuggerHeatMapRanges
//
////////////////////////////////////////////////////////////////////////////////

std::string ShellDebuggerPrefs::GetDebuggerHeatMapRanges()
{
    return m_settings.heatMapRanges;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::SetDebuggerHeatMapRanges
//
//  UI thread. The ranges are the window's alone, read against the symbols
//  each snapshot carries, so the CPU thread is told nothing.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebuggerPrefs::SetDebuggerHeatMapRanges (const std::string & text)
{
    if (m_settings.heatMapRanges == text)
    {
        return;
    }

    {
        std::lock_guard<std::mutex>  lock (m_projectMutex);



        m_settings.heatMapRanges = text;
    }

    m_host.SaveSettings();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::GetDebuggerOpenViews
//
////////////////////////////////////////////////////////////////////////////////

std::string ShellDebuggerPrefs::GetDebuggerOpenViews()
{
    return m_settings.openViews;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::SetDebuggerOpenViews
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebuggerPrefs::SetDebuggerOpenViews (const std::string & text)
{
    if (m_settings.openViews == text)
    {
        return;
    }

    {
        std::lock_guard<std::mutex>  lock (m_projectMutex);



        m_settings.openViews = text;
    }

    m_host.SaveSettings();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::GetDebuggerPlacementKey
//
//  Saving and restoring must agree on the key. The topology key names the
//  monitor arrangement and nothing else, so the debugger window carries its
//  placement with it across screens.
//
////////////////////////////////////////////////////////////////////////////////

std::string ShellDebuggerPrefs::GetDebuggerPlacementKey() const
{
    return WindowPlacementProfile::BuildTopologyKey();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::TryGetDebuggerPlacement
//
//  Where the debugger window was left on this monitor arrangement. A
//  placement that no longer lands on any monitor is declined, so a window
//  saved on a screen since removed opens at its default place.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellDebuggerPrefs::TryGetDebuggerPlacement (RECT & rectPx)
{
    WindowPlacementProfile::Bounds  bounds;
    WindowPlacementProfile          profile  = m_host.GetWindowPlacements();
    std::string                     key      = GetDebuggerPlacementKey();
    RECT                            saved    = {};



    WindowTrace::Log ("restore.lookup", "debugger", "key=" + key);

    //  THE KEY STANDS FOR THE MONITOR ARRANGEMENT, and an arrangement
    //  differing by a screen not yet attached at startup is a different key
    //  with nothing under it. Rather than open at a default the user never chose,
    //  any placement they made under another arrangement is taken, so long as
    //  it still lands on a screen.
    if (!profile.TryLoad (key, bounds, WindowPlacementProfile::Target::Debugger))
    {
        for (const auto & [otherKey, other] : profile.GetAll (WindowPlacementProfile::Target::Debugger))
        {
            RECT  candidate = RECT { other.x, other.y, other.x + other.w, other.y + other.h };

            if (other.w > 0 && other.h > 0 && MonitorFromRect (&candidate, MONITOR_DEFAULTTONULL) != nullptr)
            {
                WindowTrace::LogRect ("restore.other", "debugger", candidate, "saved under key=" + otherKey);
                rectPx = candidate;
                return true;
            }
        }

        return false;
    }

    saved = RECT { bounds.x, bounds.y, bounds.x + bounds.w, bounds.y + bounds.h };

    if (MonitorFromRect (&saved, MONITOR_DEFAULTTONULL) == nullptr)
    {
        return false;
    }

    rectPx = saved;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::SetDebuggerPlacement
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebuggerPrefs::SetDebuggerPlacement (const RECT & rectPx)
{
    WindowPlacementProfile          profile = m_host.GetWindowPlacements();
    WindowPlacementProfile::Bounds  bounds;
    std::string                     key     = GetDebuggerPlacementKey();



    bounds.x = rectPx.left;
    bounds.y = rectPx.top;
    bounds.w = (int) (rectPx.right - rectPx.left);
    bounds.h = (int) (rectPx.bottom - rectPx.top);

    WindowTrace::LogRect ("save.prefs", "debugger", rectPx, "key=" + key);
    profile.Save (key, bounds, WindowPlacementProfile::Target::Debugger);
    m_host.SaveSettings();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::FindDebuggerSource
//
//  A folder where a file was found goes into the preferences, so they are
//  saved; the next search, in this session or another, looks there first.
//
////////////////////////////////////////////////////////////////////////////////

SourceLookup ShellDebuggerPrefs::FindDebuggerSource (const DebugSourceFile & record, const std::wstring & debugFilePath,
                                                const std::string & programKey)
{
    SourcePathList  paths   (m_settings);
    SourceService   service (m_host.GetUiFileSystem(), paths);
    SourceLookup    lookup  = service.Find (record, debugFilePath, programKey);



    if (lookup.match == SourceMatch::Exact || lookup.match == SourceMatch::Unverified)
    {
        m_host.SaveSettings();
    }

    return lookup;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::MatchDroppedDebuggerSource
//
////////////////////////////////////////////////////////////////////////////////

SourceLookup ShellDebuggerPrefs::MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> & files, const std::wstring & path,
                                                        const std::string & programKey, int & recordIndex)
{
    SourcePathList  paths   (m_settings);
    SourceService   service (m_host.GetUiFileSystem(), paths);
    SourceLookup    lookup  = service.MatchDropped (files, path, programKey, recordIndex);



    if (recordIndex >= 0)
    {
        m_host.SaveSettings();
    }

    return lookup;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::GetDebuggerEventLogOptions
//
////////////////////////////////////////////////////////////////////////////////

std::string ShellDebuggerPrefs::GetDebuggerEventLogOptions (DebugLogKind kind)
{
    const std::string  * options = m_settings.GetLogOptions ((int) kind);



    return (options != nullptr) ? *options : std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::SetDebuggerEventLogOptions
//
//  Saved only when it changed.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebuggerPrefs::SetDebuggerEventLogOptions (
    DebugLogKind          kind,
    const std::string   & text)
{
    std::string  * options = m_settings.GetLogOptions ((int) kind);



    if (options == nullptr || *options == text)
    {
        return;
    }

    {
        std::lock_guard<std::mutex>  lock (m_projectMutex);



        *options = text;
    }

    m_host.SaveSettings();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::GetDebuggerWindowState
//
////////////////////////////////////////////////////////////////////////////////

std::string ShellDebuggerPrefs::GetDebuggerWindowState (const std::string & name)
{
    auto  found = m_settings.windowState.find (name);



    return (found != m_settings.windowState.end()) ? found->second : std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebuggerPrefs::SetDebuggerWindowState
//
//  Saved only when it changed.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebuggerPrefs::SetDebuggerWindowState (const std::string & name, const std::string & text)
{
    if (GetDebuggerWindowState (name) == text)
    {
        return;
    }

    {
        std::lock_guard<std::mutex>  lock (m_projectMutex);



        m_settings.windowState[name] = text;
    }

    m_host.SaveSettings();
}
