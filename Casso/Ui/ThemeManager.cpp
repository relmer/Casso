#include "Pch.h"

#include "ThemeManager.h"

#include "Core/JsonValue.h"
#include "Core/JsonParser.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ThemeBootstrapPlanner::Plan
//
//  Decides whether a theme file on disk should be replaced by the built-in
//  copy.
//
//  The one rule everything serves: a theme the USER owns is never touched. A
//  file without a truthy $cassoBuiltIn marker is theirs -- possibly a built-in
//  they copied and edited -- and overwriting it would destroy work.
//
//  For our own themes there are two independent reasons to re-extract, and
//  both are needed:
//
//    content drift  the file differs from the embedded canonical copy, which
//                   catches a developer editing a shipped theme without
//                   bumping the version
//    older stamp    the normal upgrade path
//
//  Missing and UNPARSEABLE are treated identically. A file that cannot be read
//  is indistinguishable from no file for bootstrap purposes, and installing
//  the built-in is what gets the user back to a working theme.
//
//  Written as a pure function over the file content -- no filesystem, no
//  manager state -- so the whole policy is unit-testable by passing strings.
//
////////////////////////////////////////////////////////////////////////////////

ThemeBootstrapAction ThemeBootstrapPlanner::Plan (
    const std::string * themeJsonOnDisk,
    const std::string & embeddedThemeJson,
    int                 currentVersion)
{
    HRESULT               hr        = S_OK;
    JsonValue             parsed;
    JsonParseError        err;
    bool                  isBuiltIn = false;
    int                   version   = 0;
    ThemeBootstrapAction  action    = ThemeBootstrapAction::Skip;
    bool                  parseable = (themeJsonOnDisk != nullptr);



    // Missing or unparseable: install the built-in copy. A file we cannot
    // read is indistinguishable from no file for bootstrap purposes.
    if (parseable)
    {
        hr        = JsonParser::Parse (*themeJsonOnDisk, parsed, err);
        parseable = SUCCEEDED (hr) && parsed.GetType() == JsonType::Object;
    }

    if (parseable)
    {
        hr = parsed.GetBool ("$cassoBuiltIn", isBuiltIn);
        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = parsed.GetInt ("$cassoThemeVersion", version);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    // A theme the USER owns ($cassoBuiltIn absent or false) is never touched.
    // For one of ours, either drift from the embedded canonical copy (a
    // developer edited it without bumping currentVersion) or an older stamp
    // re-extracts on the next launch.
    if (!parseable)
    {
        action = ThemeBootstrapAction::InstallBuiltIn;
    }
    else if (!isBuiltIn)
    {
        action = ThemeBootstrapAction::Skip;
    }
    else if ((!embeddedThemeJson.empty() && *themeJsonOnDisk != embeddedThemeJson)
             || version < currentVersion)
    {
        action = ThemeBootstrapAction::InstallBuiltIn;
    }

    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThemeManager
//
////////////////////////////////////////////////////////////////////////////////

ThemeManager::ThemeManager (
    IFileSystem        & fs,
    const std::wstring & themesBaseDir)
    : m_fs (fs)
    , m_themesBaseDir (themesBaseDir)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Discover
//
//  Enumerates every theme directory beneath `m_themesBaseDir` and
//  parses each `theme.json` via `ThemeLoader::Load`. Themes whose
//  metadata fails to validate are silently excluded; the remainder
//  populate `m_available`.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThemeManager::Discover()
{
    HRESULT                    hr      = S_OK;
    std::vector<std::wstring>  candidates;



    m_available.clear();

    hr = ThemeLoader::EnumerateCandidateDirs (m_fs, m_themesBaseDir, candidates);

    CHRA (hr);

    for (const std::wstring & name : candidates)
    {
        LoadedTheme     theme;
        ThemeLoadError  err;
        std::wstring    dir    = ThemeLoader::JoinPath (m_themesBaseDir, name);
        HRESULT         hrLoad = ThemeLoader::Load (m_fs, dir, theme, err);

        if (SUCCEEDED (hrLoad))
        {
            m_available.push_back (std::move (theme));
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Activate
//
//  Makes a discovered theme current and notifies listeners with the version
//  resolved for the active machine.
//
//  An unknown name is an ERROR, not a silent fallback. The caller knows better
//  than this class what to do about it -- the shell falls back to the
//  canonical built-in, while a test wants the failure -- so the decision
//  belongs there.
//
//  Listeners receive the MACHINE-RESOLVED theme rather than the raw one,
//  because a theme may carry per-machine overrides and every consumer wants
//  the version that actually applies. With no machine set the resolve is a
//  plain copy, so the same path serves both cases.
//
//  Family and variant ids are recorded alongside the name, since a theme is
//  addressed by name but reasoned about by family -- switching machines
//  re-resolves within the same family rather than picking a new theme.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThemeManager::Activate (const std::string & themeName)
{
    HRESULT              hr    = S_OK;
    const LoadedTheme *  match = nullptr;



    for (const LoadedTheme & t : m_available)
    {
        if (t.name == themeName)
        {
            match = &t;
            break;
        }
    }

    CBREx (match != nullptr, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    m_activeName      = match->name;
    m_activeFamilyId  = match->familyId;
    m_activeVariantId = match->variantId;

    // Apply any per-machine overrides for the currently-tracked
    // machine. With no machine set this is a no-op copy.
    {
        LoadedTheme  resolved = match->ResolveForMachine (m_activeMachine);
        NotifyListeners (resolved);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActivateByFamilyVariant
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThemeManager::ActivateByFamilyVariant (
    const std::string & familyId,
    const std::string & variantId)
{
    HRESULT              hr    = S_OK;
    const LoadedTheme *  match = nullptr;


    for (const LoadedTheme & t : m_available)
    {
        if (t.familyId == familyId && t.variantId == variantId)
        {
            match = &t;
            break;
        }
    }

    CBREx (match != nullptr, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    hr = Activate (match->name);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReloadCurrent
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThemeManager::ReloadCurrent()
{
    HRESULT      hr             = S_OK;
    std::string  previousActive = m_activeName;



    hr = Discover();
    CHR (hr);

    if (!previousActive.empty())
    {
        // A theme that vanished across a reload is not a reload failure, so
        // ERROR_NOT_FOUND is swallowed deliberately -- GetActiveThemeName
        // reports what is active. Any OTHER failure propagates.
        hr = Activate (previousActive);
        if (hr == HRESULT_FROM_WIN32 (ERROR_NOT_FOUND))
        {
            hr = S_OK;
        }
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetActiveTheme
//
////////////////////////////////////////////////////////////////////////////////

const LoadedTheme * ThemeManager::GetActiveTheme() const
{
    const LoadedTheme *  found = nullptr;



    // An empty active name matches nothing, so the loop covers that case too
    // and null means "no active theme" either way.
    for (const LoadedTheme & t : m_available)
    {
        if (found == nullptr && !m_activeName.empty() && t.name == m_activeName)
        {
            found = &t;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetActiveMachineName
//
////////////////////////////////////////////////////////////////////////////////

void ThemeManager::SetActiveMachineName (const std::string & machineDisplayName)
{
    const LoadedTheme *  active = nullptr;



    // Re-notifying listeners for the same machine would re-resolve and
    // re-publish an identical theme every frame.
    if (m_activeMachine != machineDisplayName)
    {
        m_activeMachine = machineDisplayName;

        active = GetActiveTheme();
        if (active != nullptr)
        {
            // Listeners see the resolved theme, not the base, so chrome /
            // CRT picks up the new variant on the next frame without any
            // additional plumbing on their end.
            LoadedTheme  resolved = active->ResolveForMachine (m_activeMachine);
            NotifyListeners (resolved);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetActiveResolvedTheme
//
////////////////////////////////////////////////////////////////////////////////

LoadedTheme ThemeManager::GetActiveResolvedTheme() const
{
    const LoadedTheme *  active = GetActiveTheme();



    // A default-constructed theme with no active one: callers read its
    // has-flags, which are all false, so nothing gets overridden.
    return (active != nullptr) ? active->ResolveForMachine (m_activeMachine)
                               : LoadedTheme {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddChangeListener
//
////////////////////////////////////////////////////////////////////////////////

void ThemeManager::AddChangeListener (ChangeListener listener)
{
    m_listeners.push_back (std::move (listener));
}





////////////////////////////////////////////////////////////////////////////////
//
//  NotifyListeners
//
////////////////////////////////////////////////////////////////////////////////

void ThemeManager::NotifyListeners (const LoadedTheme & theme)
{
    for (const ChangeListener & listener : m_listeners)
    {
        if (listener)
        {
            listener (theme);
        }
    }
}
