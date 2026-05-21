#pragma once

#include "Pch.h"

#include "SettingsPanelState.h"

#include "Core/MachineScanner.h"




class UiShell;
class UserConfigStore;
struct GlobalUserPrefs;
class ThemeManager;
class EmulatorShell;
class IFileSystem;





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsPanel
//
//  P7-T2..T5 -- RmlUi front end for `SettingsPanelState`. Loads the
//  active theme's `settings.rml`, populates the machine selector
//  from `MachineScanner`, binds every control to a transient
//  `SettingsPanelState` snapshot, and dispatches Apply / Cancel to
//  the underlying stores + the live `EmulatorShell`.
//
//  The DOM structure is fixed across all three built-in themes
//  (themes restyle via RCSS only). Element IDs the C++ side relies
//  on:
//
//      #machine-selector       <select> of installed machines
//      #emulation-speed        radio group (radio name="speed")
//      #video-color            radio group (radio name="color")
//      #floppy-sound           checkbox
//      #floppy-mechanism       <select>
//      #wp-drive-0/1           checkboxes
//      #hardware-tree          container -- populated dynamically
//      #theme-selector         <select> of installed themes
//      #crt-brightness         slider
//      #crt-scanlines          checkbox
//      #crt-bloom              checkbox
//      #crt-color-bleed        checkbox
//      #btn-apply / #btn-cancel  footer buttons
//      #modal-confirm-reset    overlay with #confirm-reset / #confirm-discard
//
//  Lifecycle
//  ---------
//      Initialize(shell, ucs, prefs, themes, emuShell, fs)
//          -- records dependencies, does no DOM work.
//      Show()
//          -- loads the theme's settings.rml, populates selectors,
//             binds state to the active machine, displays the doc.
//             Idempotent: a second Show() reloads cleanly.
//      Hide()
//          -- unloads the doc; the underlying snapshot is discarded.
//      IsVisible()
//          -- returns whether the doc is loaded + visible.
//
//  Per FR-041 the emulator thread is NEVER paused. The panel is
//  purely a view over `SettingsPanelState`; mutations to the live
//  emulator only happen at Apply time and go through the existing
//  command-queue path (lock-free atomics + posted commands).
//
////////////////////////////////////////////////////////////////////////////////

class SettingsPanel : public Rml::EventListener
{
public:

    SettingsPanel();
    ~SettingsPanel() override;

    HRESULT Initialize (
        UiShell          & uiShell,
        UserConfigStore  & ucs,
        GlobalUserPrefs  & prefs,
        ThemeManager     & themes,
        EmulatorShell    & emuShell,
        IFileSystem      & fs);

    HRESULT Show ();
    void    Hide ();

    bool IsVisible () const { return m_doc != nullptr; }

    // Pure-data side. Exposed so the EmulatorShell command path
    // (Settings menu item) can directly inspect what is currently
    // staged.
    const SettingsPanelState & State () const { return m_state; }

    // ---- Rml::EventListener --------------------------------------------
    void ProcessEvent (Rml::Event & event) override;

    // ---- Helpers exposed for tests (no Rml needed) ---------------------

    // Returns the JSON-serialisable string identifier for a speed mode.
    static const char * SpeedRadioValue (SettingsSpeedMode m);
    static const char * ColorRadioValue (SettingsColorMode m);

private:

    HRESULT  ReloadDocument           ();
    void     PopulateMachineSelector  (Rml::ElementDocument * doc);
    void     PopulateThemeSelector    (Rml::ElementDocument * doc);
    HRESULT  RebindToMachine          (const std::string & machineName);
    void     RebuildHardwareTree      (Rml::ElementDocument * doc);
    void     ReflectStateToDom        (Rml::ElementDocument * doc);
    void     AttachListeners          (Rml::ElementDocument * doc);
    void     DetachListeners          ();
    void     ShowResetConfirm         (bool show);
    void     CommitApply              ();
    HRESULT  LoadMergedForMachine     (const std::string & machineName,
                                       JsonValue         & outDefault,
                                       JsonValue         & outMerged);
    void     ApplyGlobalPrefsToDom    (Rml::ElementDocument * doc);

    UiShell          * m_uiShell   = nullptr;
    UserConfigStore  * m_ucs       = nullptr;
    GlobalUserPrefs  * m_prefs     = nullptr;
    ThemeManager     * m_themes    = nullptr;
    EmulatorShell    * m_emuShell  = nullptr;
    IFileSystem      * m_fs        = nullptr;

    Rml::ElementDocument * m_doc   = nullptr;

    SettingsPanelState     m_state;
    std::vector<MachineInfo>  m_machines;

    // Elements we attached as event listeners to -- so we can detach
    // before unloading the doc.
    std::vector<Rml::Element *>  m_listenerElements;

    // True between user clicking Apply on a hardware-changing edit
    // and the modal "Reset?" confirm resolving. Blocks further
    // Apply while modal is up.
    bool                   m_resetConfirmPending = false;
};
