#pragma once

#include "Pch.h"

#include "Config/IFileSystem.h"

#include "ThemeLoader.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>

#include <functional>





////////////////////////////////////////////////////////////////////////////////
//
//  ThemeBootstrapPlanner
//
//  Pure-logic planner shared by `AssetBootstrap::EnsureThemes` (which
//  extracts embedded built-in themes on first launch / upgrade) and
//  the unit-test harness. Decides per-theme whether the on-disk copy
//  should be replaced by the embedded default, left alone, or backed
//  up. Mirrors the role of `MachineConfigUpgrade::Plan` for the
//  themes domain (FR-045 + the built-in-marker contract documented
//  in data-model.md § Theme).
//
//  Rules (in order):
//      1. theme directory does not exist                   -> InstallBuiltIn
//      2. theme.json does not exist or fails to parse      -> InstallBuiltIn
//         (a half-installed extract from a previous crash
//         is overwritten by the embedded current default)
//      3. theme.json parses; $cassoBuiltIn != true         -> Skip
//         (it's a user-authored theme — never overwritten)
//      4. theme.json parses; $cassoBuiltIn == true and
//         $cassoThemeVersion < embedded current version    -> InstallBuiltIn
//      5. otherwise (built-in, version current/newer)      -> Skip
//
////////////////////////////////////////////////////////////////////////////////

enum class ThemeBootstrapAction
{
    Skip,            // disk file is current or user-owned; leave untouched
    InstallBuiltIn,  // (re)extract the embedded built-in to disk
};


class ThemeBootstrapPlanner
{
public:
    // `themeJsonOnDisk` is the verbatim contents of the on-disk
    // theme.json, or nullptr if no such file exists. `currentVersion`
    // is the embedded built-in's `$cassoThemeVersion`. The planner
    // never reads from `fs`; it only consults the inputs.
    static ThemeBootstrapAction Plan (
        const std::string  * themeJsonOnDisk,
        int                  currentVersion);
};





////////////////////////////////////////////////////////////////////////////////
//
//  ThemeManager
//
//  Single owner of the currently-active theme. Wraps `ThemeLoader`
//  for discovery + parsing and `Rml::Context` for live document
//  swapping. Win11 Mica backdrop activation is performed via
//  `Win11DwmHelpers::ApplyMicaBackdrop` on the bound HWND when a
//  theme opts in.
//
//  Lifecycle
//  ---------
//      ctor (fs, themesBaseDir)
//          -> records dependencies; no I/O.
//      Discover()
//          -> walks `themesBaseDir`, parses + validates every
//             candidate theme.json, populates the available list.
//             Pure logic (no Rml or Win32 calls).
//      BindRml (ctx, hwnd)
//          -> attaches the manager to a live RmlUi context + window.
//             Called once from EmulatorShell after the UiShell is up.
//             Activate() before BindRml is legal — it just defers
//             document loading until BindRml fires.
//      Activate (name)
//          -> unloads previously loaded entry docs, loads the new
//             theme's docs, applies Mica, notifies listeners. If
//             the load fails the previous theme remains active
//             (FR-036). Returns S_FALSE for unknown names.
//      ReloadCurrent()
//          -> calls Discover() then Activate (current name). Used
//             by the dev hot-iteration path.
//
//  Thread-safety: UI thread only. No locks.
//
////////////////////////////////////////////////////////////////////////////////

class ThemeManager
{
public:
    using ChangeListener = std::function<void (const LoadedTheme &)>;


    ThemeManager (
        IFileSystem        & fs,
        const std::wstring & themesBaseDir);

    // Bind the manager to a live RmlUi context + main HWND. May be
    // called more than once (re-binding to a new context tears down
    // any currently-loaded docs on the old context).
    void  BindRml (Rml::Context * pContext, HWND hwnd);

    // Walk `themesBaseDir` and rebuild the available-themes list.
    // Cheap; safe to call from the Settings panel's Refresh button
    // (FR-035). Returns S_OK with an empty list when the base
    // directory is missing or empty.
    HRESULT  Discover ();

    const std::vector<LoadedTheme> & GetAvailableThemes () const { return m_available; }

    // Switch to `themeName`. On success notifies all registered
    // listeners. Returns S_FALSE when `themeName` doesn't match any
    // discovered theme; returns a failure HRESULT (with the previous
    // theme left active) on any RmlUi load failure.
    HRESULT  Activate (const std::string & themeName);

    // Re-runs Discover() then Activate (current name). No-op if no
    // theme is currently active.
    HRESULT  ReloadCurrent ();

    const std::string & GetActiveThemeName () const { return m_activeName; }
    const LoadedTheme * GetActiveTheme     () const;

    void  AddChangeListener (ChangeListener listener);

private:
    // Reload all entry documents into m_context using `theme`'s
    // resolved paths. Unloads anything currently in m_activeDocs.
    // No-op (returns S_OK) when m_context is null — Activate() may
    // legitimately be called before BindRml().
    HRESULT  ReattachDocuments (const LoadedTheme & theme);

    void     UnloadActiveDocuments ();

    void     ApplyDwm (const LoadedTheme & theme);

    void     NotifyListeners (const LoadedTheme & theme);


    IFileSystem                       & m_fs;
    std::wstring                        m_themesBaseDir;
    std::wstring                        m_sharedDir;

    std::vector<LoadedTheme>            m_available;
    std::string                         m_activeName;

    Rml::Context                      * m_context  = nullptr;
    HWND                                m_hwnd     = nullptr;

    // Documents currently loaded into m_context for the active theme.
    // Tracked so Activate() can unload them deterministically.
    std::vector<Rml::ElementDocument *> m_activeDocs;

    std::vector<ChangeListener>         m_listeners;
};
