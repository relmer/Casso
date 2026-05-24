#pragma once

#include "Pch.h"

#include "Config/IFileSystem.h"

#include "ThemeLoader.h"






////////////////////////////////////////////////////////////////////////////////
//
//  ThemeBootstrapPlanner
//
//  Pure-logic planner shared by `AssetBootstrap::EnsureThemes` (which
//  extracts embedded built-in themes on first launch / upgrade) and
//  the unit-test harness. Decides per-theme whether the on-disk copy
//  should be replaced by the embedded default, left alone, or backed
//  up. Mirrors the role of `MachineConfigUpgrade::Plan` for the
//  themes domain.
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
    static ThemeBootstrapAction Plan (const std::string * themeJsonOnDisk,
                                      int                 currentVersion);
};





////////////////////////////////////////////////////////////////////////////////
//
//  ThemeManager
//
//  Single owner of the currently-active theme. Wraps `ThemeLoader` for
//  discovery + parsing and tracks active theme metadata so the rest of
//  the shell can observe changes. The painter pipeline that actually
//  applies the theme to a live UI surface is reintroduced in a later
//  phase; in this baseline `Activate` validates the requested theme,
//  records it, and notifies listeners.
//
//  Thread-safety: UI thread only. No locks.
//
////////////////////////////////////////////////////////////////////////////////

class ThemeManager
{
public:
    using ChangeListener = std::function<void (const LoadedTheme &)>;


    ThemeManager (IFileSystem        & fs,
                  const std::wstring & themesBaseDir);

    HRESULT                          Discover                  ();
    const std::vector<LoadedTheme> & GetAvailableThemes        () const { return m_available; }
    HRESULT                          Activate                  (const std::string & themeName);
    HRESULT                          ActivateByFamilyVariant   (const std::string & familyId,
                                                                const std::string & variantId);
    HRESULT                          ReloadCurrent             ();
    const std::string              & GetActiveThemeName        () const { return m_activeName; }
    const std::string              & GetActiveFamilyId         () const { return m_activeFamilyId; }
    const std::string              & GetActiveVariantId        () const { return m_activeVariantId; }
    const LoadedTheme              * GetActiveTheme            () const;
    void                             AddChangeListener         (ChangeListener listener);

private:
    void    NotifyListeners (const LoadedTheme & theme);


    IFileSystem                & m_fs;
    std::wstring                 m_themesBaseDir;

    std::vector<LoadedTheme>     m_available;
    std::string                  m_activeName;
    std::string                  m_activeFamilyId;
    std::string                  m_activeVariantId;

    std::vector<ChangeListener>  m_listeners;
};
