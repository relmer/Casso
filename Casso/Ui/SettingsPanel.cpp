#include "Pch.h"

#include "SettingsPanel.h"


#include "Core/JsonParser.h"
#include "Core/MachineConfig.h"
#include "Core/PathResolver.h"

#include "../Config/IFileSystem.h"
#include "../Config/UserConfigStore.h"
#include "../Config/GlobalUserPrefs.h"
#include "../EmulatorShell.h"
#include "../MenuSystem.h"
#include "../resource.h"

#include "ThemeManager.h"
#include "UiShell.h"







////////////////////////////////////////////////////////////////////////////////
//
//  Apply sink that bridges SettingsPanelState -> EmulatorShell. Lives
//  inside this TU because it depends on the full EmulatorShell.h.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    class ShellApplySink : public ISettingsApplySink
    {
    public:
        explicit ShellApplySink (EmulatorShell & shell)
            : m_shell (shell)
        {
        }

        void ApplySpeedMode (SettingsSpeedMode mode) override
        {
            switch (mode)
            {
                case SettingsSpeedMode::Authentic: m_shell.HandleCommand (IDM_MACHINE_SPEED_1X);  break;
                case SettingsSpeedMode::Double:    m_shell.HandleCommand (IDM_MACHINE_SPEED_2X);  break;
                case SettingsSpeedMode::Maximum:   m_shell.HandleCommand (IDM_MACHINE_SPEED_MAX); break;
            }
        }

        void ApplyColorMode (SettingsColorMode mode) override
        {
            switch (mode)
            {
                case SettingsColorMode::Color: m_shell.HandleCommand (IDM_VIEW_COLOR); break;
                case SettingsColorMode::Green: m_shell.HandleCommand (IDM_VIEW_GREEN); break;
                case SettingsColorMode::Amber: m_shell.HandleCommand (IDM_VIEW_AMBER); break;
                case SettingsColorMode::White: m_shell.HandleCommand (IDM_VIEW_WHITE); break;
            }
        }

        void ApplyFloppySound (bool enabled) override
        {
            // FR-011 partial: floppy-sound master mute is currently
            // mediated by `DriveAudioMixer` which the shell wires up
            // privately. Persisted in the per-machine `_user.json`;
            // live-effect propagation is deferred to a future
            // EmulatorShell::SetFloppySoundEnabled hook so the
            // settings panel doesn't bypass the mixer's lock.
            (void) enabled;
        }

        void ApplyMechanism (const std::string & mechanism) override
        {
            // Same story as ApplyFloppySound -- value is persisted via
            // SaveDelta; runtime swap will require a controller rewire
            // (deferred to follow-up work).
            (void) mechanism;
        }

        void ApplyWriteProtect (int drive, bool wp) override
        {
            // Drive 0 / 1 -> existing IDM_DISK_WRITEPROTECT toggles.
            // The handlers are toggles, so we don't fire them
            // here -- the persisted value is sufficient. Future work:
            // expose a level-set entry point on the shell.
            (void) drive;
            (void) wp;
        }

        void QueueMachineReset () override
        {
            m_shell.HandleCommand (IDM_MACHINE_RESET);
        }

    private:
        EmulatorShell & m_shell;
    };



    std::string  Narrow (const std::wstring & w)
    {
        std::string  out;
        out.reserve (w.size());
        for (wchar_t c : w)
        {
            out.push_back ((char) (unsigned char) c);
        }
        return out;
    }



    std::string  CapabilityCss (CapabilityFlag c)
    {
        switch (c)
        {
            case CapabilityFlag::Optional:       return "cap-optional";
            case CapabilityFlag::Required:       return "cap-required";
            case CapabilityFlag::PlatformLocked: return "cap-platform-locked";
        }
        return "cap-optional";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsPanel
//
////////////////////////////////////////////////////////////////////////////////

SettingsPanel::SettingsPanel()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~SettingsPanel
//
////////////////////////////////////////////////////////////////////////////////

SettingsPanel::~SettingsPanel()
{
    Hide();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsPanel::Initialize (
    UiShell          & uiShell,
    UserConfigStore  & ucs,
    GlobalUserPrefs  & prefs,
    ThemeManager     & themes,
    EmulatorShell    & emuShell,
    IFileSystem      & fs)
{
    m_uiShell  = &uiShell;
    m_ucs      = &ucs;
    m_prefs    = &prefs;
    m_themes   = &themes;
    m_emuShell = &emuShell;
    m_fs       = &fs;

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Show
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsPanel::Show()
{
    HRESULT  hr = S_OK;



    CBRA (m_uiShell);
    CBRA (m_themes);

    if (m_doc != nullptr)
    {
        // Already up. Bring to top by re-pulling.
        m_doc->PullToFront();
        m_doc->Show();
        return S_OK;
    }

    hr = ReloadDocument();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Hide
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::Hide()
{
    if (m_doc == nullptr)
    {
        return;
    }

    DetachListeners();

    Rml::Context * ctx = (m_uiShell != nullptr) ? m_uiShell->GetContext() : nullptr;
    if (ctx != nullptr)
    {
        ctx->UnloadDocument (m_doc);
    }

    m_doc                 = nullptr;
    m_resetConfirmPending = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReloadDocument
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsPanel::ReloadDocument()
{
    HRESULT             hr      = S_OK;
    Rml::Context      * ctx     = nullptr;
    const LoadedTheme * theme   = nullptr;
    std::string         rmlPath;



    CBRA (m_uiShell);
    CBRA (m_themes);

    ctx = m_uiShell->GetContext();
    CBRA (ctx);

    theme = m_themes->GetActiveTheme();
    CBRA (theme);

    CBR (! theme->entryDocs.settings.empty());

    // RmlUi expects narrow paths.
    rmlPath = Narrow (theme->entryDocs.settings);

    m_doc = ctx->LoadDocument (rmlPath);
    CBRA (m_doc);

    // Populate selectors from the live state.
    PopulateMachineSelector (m_doc);
    PopulateThemeSelector   (m_doc);
    AttachListeners         (m_doc);

    // Bind to the current emulator machine if known, else first scan entry.
    {
        std::string  bindTarget;
        if (m_prefs != nullptr && ! m_prefs->lastSelectedMachine.empty())
        {
            bindTarget = m_prefs->lastSelectedMachine;
        }
        else if (! m_machines.empty())
        {
            bindTarget = Narrow (m_machines[0].fileName);
        }
        if (! bindTarget.empty())
        {
            (void) RebindToMachine (bindTarget);
        }
    }

    ApplyGlobalPrefsToDom (m_doc);
    ShowResetConfirm      (false);
    m_doc->Show();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PopulateMachineSelector
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::PopulateMachineSelector (Rml::ElementDocument * doc)
{
    Rml::Element *  sel        = nullptr;
    fs::path        exeDir;
    fs::path        cwd;



    if (doc == nullptr)
    {
        return;
    }

    sel = doc->GetElementById ("machine-selector");
    if (sel == nullptr)
    {
        return;
    }

    exeDir       = PathResolver::GetExecutableDirectory();
    cwd          = PathResolver::GetWorkingDirectory();
    m_machines   = MachineScanner::Scan (
        PathResolver::BuildSearchPaths (exeDir, cwd),
        &MachineScanner::ListDirectory,
        &MachineScanner::ReadFile);

    sel->SetInnerRML ("");

    for (const MachineInfo & info : m_machines)
    {
        std::string  fileName    = Narrow (info.fileName);
        std::string  displayName = Narrow (info.displayName);
        std::string  optHtml;

        optHtml  = "<option value=\"" + fileName + "\">";
        optHtml += displayName;
        optHtml += "</option>";

        sel->SetInnerRML (sel->GetInnerRML() + Rml::String (optHtml.c_str()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PopulateThemeSelector
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::PopulateThemeSelector (Rml::ElementDocument * doc)
{
    Rml::Element *  sel = nullptr;



    if (doc == nullptr || m_themes == nullptr)
    {
        return;
    }

    sel = doc->GetElementById ("theme-selector");
    if (sel == nullptr)
    {
        return;
    }

    sel->SetInnerRML ("");

    for (const LoadedTheme & t : m_themes->GetAvailableThemes())
    {
        std::string  optHtml;
        optHtml  = "<option value=\"" + t.name + "\">";
        optHtml += t.name;
        optHtml += "</option>";
        sel->SetInnerRML (sel->GetInnerRML() + Rml::String (optHtml.c_str()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadMergedForMachine
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsPanel::LoadMergedForMachine (
    const std::string & machineName,
    JsonValue         & outDefault,
    JsonValue         & outMerged)
{
    HRESULT          hr = S_OK;
    fs::path         exeDir;
    fs::path         cwd;
    fs::path         configPath;
    std::ifstream    in;
    std::stringstream ss;
    std::string      text;
    JsonParseError   perr;



    CBRA (m_fs);
    CBRA (m_ucs);

    exeDir     = PathResolver::GetExecutableDirectory();
    cwd        = PathResolver::GetWorkingDirectory();
    configPath = PathResolver::FindFile (
        PathResolver::BuildSearchPaths (exeDir, cwd),
        fs::path ("Machines") / machineName / (machineName + ".json"));
    CBR (! configPath.empty());

    in.open (configPath);
    CBR (in.good());

    ss << in.rdbuf();
    text = ss.str();

    hr = JsonParser::Parse (text, outDefault, perr);
    CHR (hr);

    hr = m_ucs->Load (machineName, outDefault, *m_fs, outMerged);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RebindToMachine
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsPanel::RebindToMachine (const std::string & machineName)
{
    HRESULT    hr = S_OK;
    JsonValue  defJson;
    JsonValue  merged;



    hr = LoadMergedForMachine (machineName, defJson, merged);
    CHR (hr);

    hr = m_state.LoadFromMachine (machineName, defJson, merged);
    CHR (hr);

    if (m_prefs != nullptr)
    {
        m_prefs->lastSelectedMachine = machineName;
    }

    if (m_doc != nullptr)
    {
        RebuildHardwareTree (m_doc);
        ReflectStateToDom   (m_doc);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RebuildHardwareTree
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::RebuildHardwareTree (Rml::ElementDocument * doc)
{
    Rml::Element *  tree = nullptr;
    std::string     html;
    size_t          i    = 0;



    if (doc == nullptr)
    {
        return;
    }

    tree = doc->GetElementById ("hardware-tree");
    if (tree == nullptr)
    {
        return;
    }

    const std::vector<HardwareEntry> & entries = m_state.Hardware();

    for (i = 0; i < entries.size(); ++i)
    {
        const HardwareEntry & e = entries[i];
        bool  interactive = (e.capability == CapabilityFlag::Optional);
        bool  checked     = e.enabled || (e.capability != CapabilityFlag::Optional);

        std::string  rowClass = "hw-row " + CapabilityCss (e.capability);
        std::string  rowId    = "hw-row-" + std::to_string (i);
        std::string  cbId     = "hw-cb-"  + std::to_string (i);
        std::string  lockTip;

        if (e.capability == CapabilityFlag::PlatformLocked && ! e.lockReason.empty())
        {
            lockTip = e.lockReason;
        }

        html += "<div class=\"" + rowClass + "\" id=\"" + rowId + "\">";

        html += "<input type=\"checkbox\" id=\"" + cbId + "\"";
        html += " data-hw-index=\"" + std::to_string (i) + "\"";
        if (checked)     html += " checked=\"checked\"";
        if (!interactive) html += " disabled=\"disabled\"";
        if (!lockTip.empty()) html += " title=\"" + lockTip + "\"";
        html += " tabindex=\"0\"/>";

        html += "<label for=\"" + cbId + "\" class=\"hw-label\">";
        if (!lockTip.empty()) html += "<span title=\"" + lockTip + "\">";
        html += e.displayName;
        if (!lockTip.empty()) html += "</span>";
        html += "</label>";

        if (e.capability == CapabilityFlag::PlatformLocked && ! e.lockReason.empty())
        {
            html += "<span class=\"hw-lock-reason\" title=\"";
            html += e.lockReason;
            html += "\">[locked]</span>";
        }

        html += "</div>";
    }

    tree->SetInnerRML (Rml::String (html.c_str()));

    // Re-attach listeners to new checkboxes.
    for (i = 0; i < entries.size(); ++i)
    {
        std::string  cbId = "hw-cb-" + std::to_string (i);
        Rml::Element * cb = doc->GetElementById (cbId);
        if (cb != nullptr)
        {
            cb->AddEventListener ("change", this);
            m_listenerElements.push_back (cb);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReflectStateToDom
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::ReflectStateToDom (Rml::ElementDocument * doc)
{
    Rml::Element *  speed   = nullptr;
    Rml::Element *  color   = nullptr;
    Rml::Element *  sound   = nullptr;
    Rml::Element *  mech    = nullptr;
    Rml::Element *  wp0     = nullptr;
    Rml::Element *  wp1     = nullptr;



    if (doc == nullptr)
    {
        return;
    }

    speed = doc->GetElementById (std::string ("speed-") + SpeedRadioValue (m_state.Prefs().speedMode));
    if (speed != nullptr)
    {
        speed->SetAttribute ("checked", "checked");
    }

    color = doc->GetElementById (std::string ("color-") + ColorRadioValue (m_state.Prefs().colorMode));
    if (color != nullptr)
    {
        color->SetAttribute ("checked", "checked");
    }

    sound = doc->GetElementById ("floppy-sound");
    if (sound != nullptr)
    {
        if (m_state.Prefs().floppySoundEnabled)
            sound->SetAttribute ("checked", "checked");
        else
            sound->RemoveAttribute ("checked");
    }

    mech = doc->GetElementById ("floppy-mechanism");
    if (mech != nullptr)
    {
        mech->SetAttribute ("value", m_state.Prefs().floppyMechanism);
    }

    wp0 = doc->GetElementById ("wp-drive-0");
    if (wp0 != nullptr)
    {
        if (m_state.Prefs().writeProtect[0])
            wp0->SetAttribute ("checked", "checked");
        else
            wp0->RemoveAttribute ("checked");
    }

    wp1 = doc->GetElementById ("wp-drive-1");
    if (wp1 != nullptr)
    {
        if (m_state.Prefs().writeProtect[1])
            wp1->SetAttribute ("checked", "checked");
        else
            wp1->RemoveAttribute ("checked");
    }

    // Select the active machine in the dropdown.
    Rml::Element * selM = doc->GetElementById ("machine-selector");
    if (selM != nullptr)
    {
        selM->SetAttribute ("value", m_state.MachineName());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyGlobalPrefsToDom
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::ApplyGlobalPrefsToDom (Rml::ElementDocument * doc)
{
    Rml::Element *  sel = nullptr;



    if (doc == nullptr || m_prefs == nullptr)
    {
        return;
    }

    sel = doc->GetElementById ("theme-selector");
    if (sel != nullptr)
    {
        sel->SetAttribute ("value", m_prefs->activeTheme);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AttachListeners
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::AttachListeners (Rml::ElementDocument * doc)
{
    static const char *  s_ids[] = {
        "machine-selector",
        "speed-authentic", "speed-double", "speed-maximum",
        "color-color", "color-green", "color-amber", "color-white",
        "floppy-sound", "floppy-mechanism",
        "wp-drive-0", "wp-drive-1",
        "theme-selector",
        "crt-brightness", "crt-scanlines", "crt-bloom", "crt-color-bleed",
        "btn-apply", "btn-cancel",
        "confirm-reset", "confirm-discard",
    };


    if (doc == nullptr)
    {
        return;
    }

    for (const char * id : s_ids)
    {
        Rml::Element * el = doc->GetElementById (id);
        if (el == nullptr)
        {
            continue;
        }
        el->AddEventListener ("change", this);
        el->AddEventListener ("click",  this);
        m_listenerElements.push_back (el);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DetachListeners
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::DetachListeners()
{
    for (Rml::Element * el : m_listenerElements)
    {
        if (el != nullptr)
        {
            el->RemoveEventListener ("change", this);
            el->RemoveEventListener ("click",  this);
        }
    }
    m_listenerElements.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProcessEvent
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::ProcessEvent (Rml::Event & event)
{
    Rml::Element *  target = event.GetTargetElement();
    if (target == nullptr || m_doc == nullptr)
    {
        return;
    }

    const Rml::String & id = target->GetId();

    if (id == "btn-cancel")
    {
        Hide();
        return;
    }
    if (id == "btn-apply")
    {
        if (m_state.RequiresReset() && ! m_resetConfirmPending)
        {
            m_resetConfirmPending = true;
            ShowResetConfirm (true);
        }
        else
        {
            CommitApply();
        }
        return;
    }
    if (id == "confirm-reset")
    {
        m_resetConfirmPending = false;
        ShowResetConfirm (false);
        CommitApply();
        return;
    }
    if (id == "confirm-discard")
    {
        m_resetConfirmPending = false;
        ShowResetConfirm (false);
        m_state.Cancel();
        ReflectStateToDom (m_doc);
        return;
    }

    if (id == "machine-selector")
    {
        Rml::String value = target->GetAttribute<Rml::String> ("value", "");
        if (! value.empty())
        {
            if (FAILED (RebindToMachine (std::string (value.c_str()))))
            {
                target->SetAttribute ("value", m_state.MachineName());
            }
        }
        return;
    }

    if (id == "theme-selector")
    {
        Rml::String value = target->GetAttribute<Rml::String> ("value", "");
        if (! value.empty() && m_themes != nullptr)
        {
            (void) m_themes->Activate (std::string (value.c_str()));
            if (m_prefs != nullptr)
            {
                m_prefs->activeTheme = std::string (value.c_str());
            }
        }
        return;
    }

    if (id == "speed-authentic") { m_state.SetSpeedMode (SettingsSpeedMode::Authentic); return; }
    if (id == "speed-double")    { m_state.SetSpeedMode (SettingsSpeedMode::Double);    return; }
    if (id == "speed-maximum")   { m_state.SetSpeedMode (SettingsSpeedMode::Maximum);   return; }

    if (id == "color-color")     { m_state.SetColorMode (SettingsColorMode::Color);     return; }
    if (id == "color-green")     { m_state.SetColorMode (SettingsColorMode::Green);     return; }
    if (id == "color-amber")     { m_state.SetColorMode (SettingsColorMode::Amber);     return; }
    if (id == "color-white")     { m_state.SetColorMode (SettingsColorMode::White);     return; }

    if (id == "floppy-sound")
    {
        bool checked = target->HasAttribute ("checked");
        m_state.SetFloppySound (checked);
        return;
    }

    if (id == "floppy-mechanism")
    {
        Rml::String value = target->GetAttribute<Rml::String> ("value", "shugart");
        m_state.SetMechanism (std::string (value.c_str()));
        return;
    }

    if (id == "wp-drive-0")
    {
        m_state.SetWriteProtect (0, target->HasAttribute ("checked"));
        return;
    }
    if (id == "wp-drive-1")
    {
        m_state.SetWriteProtect (1, target->HasAttribute ("checked"));
        return;
    }

    // Hardware-tree checkbox?
    if (target->HasAttribute ("data-hw-index"))
    {
        Rml::String idxStr = target->GetAttribute<Rml::String> ("data-hw-index", "");
        size_t      idx    = (size_t) std::stoul (idxStr.c_str());
        bool        checked = target->HasAttribute ("checked");
        HRESULT     hr      = m_state.SetHardwareEnabled (idx, checked);
        if (FAILED (hr))
        {
            // Reflect the disallowed change back out.
            if (! checked) target->SetAttribute ("checked", "checked");
        }
        return;
    }

    if (m_prefs != nullptr)
    {
        if (id == "crt-brightness")
        {
            Rml::String value = target->GetAttribute<Rml::String> ("value", "1.0");
            m_prefs->crt.brightness = (float) std::atof (value.c_str());
            return;
        }
        if (id == "crt-scanlines")
        {
            m_prefs->crt.scanlinesEnabled = target->HasAttribute ("checked");
            return;
        }
        if (id == "crt-bloom")
        {
            m_prefs->crt.bloomEnabled = target->HasAttribute ("checked");
            return;
        }
        if (id == "crt-color-bleed")
        {
            m_prefs->crt.colorBleedEnabled = target->HasAttribute ("checked");
            return;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShowResetConfirm
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::ShowResetConfirm (bool show)
{
    Rml::Element * modal = nullptr;



    if (m_doc == nullptr)
    {
        return;
    }

    modal = m_doc->GetElementById ("modal-confirm-reset");
    if (modal == nullptr)
    {
        return;
    }

    if (show)
    {
        modal->SetClass ("visible", true);
    }
    else
    {
        modal->SetClass ("visible", false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommitApply
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::CommitApply()
{
    ShellApplySink   sink (*m_emuShell);
    JsonValue        currentJson;
    HRESULT          hr = S_OK;


    if (m_emuShell == nullptr || m_ucs == nullptr || m_fs == nullptr)
    {
        return;
    }

    hr = m_state.Apply (sink, currentJson);
    if (FAILED (hr))
    {
        return;
    }

    // Persist per-machine delta.
    {
        JsonValue  defJson;
        JsonValue  merged;
        if (SUCCEEDED (LoadMergedForMachine (m_state.MachineName(), defJson, merged)))
        {
            (void) m_ucs->SaveDelta (m_state.MachineName(), currentJson, defJson, *m_fs);
        }
    }

    // Persist global prefs (theme + CRT mods).
    if (m_prefs != nullptr)
    {
        std::wstring baseDir = PathResolver::GetExecutableDirectory().wstring();
        (void) m_prefs->Save (baseDir, *m_fs);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Static helpers
//
////////////////////////////////////////////////////////////////////////////////

const char * SettingsPanel::SpeedRadioValue (SettingsSpeedMode m)
{
    switch (m)
    {
        case SettingsSpeedMode::Authentic: return "authentic";
        case SettingsSpeedMode::Double:    return "double";
        case SettingsSpeedMode::Maximum:   return "maximum";
    }
    return "authentic";
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorRadioValue
//
////////////////////////////////////////////////////////////////////////////////

const char * SettingsPanel::ColorRadioValue (SettingsColorMode m)
{
    switch (m)
    {
        case SettingsColorMode::Color: return "color";
        case SettingsColorMode::Green: return "green";
        case SettingsColorMode::Amber: return "amber";
        case SettingsColorMode::White: return "white";
    }
    return "color";
}
