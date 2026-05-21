#pragma once

#include "Pch.h"

#include "DriveWidgetState.h"
#include "IDriveCommandSink.h"







////////////////////////////////////////////////////////////////////////////////
//
//  DriveWidgetElement
//
//  Custom `Rml::Element` subclass that backs the `<drive-widget>`
//  RML tag. One element instance per physical drive (slot 6 + drive 0/1
//  in the integrated Disk II).
//
//  Required attributes on the RML tag
//  ----------------------------------
//      slot   -- integer slot number  (e.g. "6")
//      drive  -- integer drive index  (0 or 1)
//
//  Optional child markup (the element does not enforce a strict
//  template; missing children are simply not animated):
//      <div class="disk-graphic"/>      // gets is-spinning class
//      <div class="door"/>              // gets door-open / door-closed
//      <div class="led"/>               // gets led--idle/--present/--active
//      <div class="eject-btn"/>         // separate click region
//
//  Behavior
//  --------
//      * Click on the widget body (not on the eject child) opens an
//        IFileDialog filtered to .dsk/.nib/.woz/.po (FR-022b) and routes
//        the result to `IDriveCommandSink::Mount(slot,drive,path)`.
//      * Click on the eject child dispatches
//        `IDriveCommandSink::Eject(slot,drive)`.
//      * `SyncFromState` is called by `EmulatorShell` once per UI frame
//        with the current `DriveWidgetState`; it updates is-spinning,
//        is-mounted / is-empty, door-open / door-closed, and LED state.
//
//  Sink + parent HWND injection
//  ----------------------------
//  RmlUi instantiates the element via `ElementInstancerGeneric<>`, so we
//  cannot pass the sink through the constructor. `SetCommandSink` /
//  `SetOwnerHwnd` are called after the document is loaded by the host
//  (typically by walking the document tree once and configuring every
//  `<drive-widget>` it finds).
//
////////////////////////////////////////////////////////////////////////////////

class DriveWidgetElement : public Rml::Element
{
public:
    explicit DriveWidgetElement (const Rml::String & tag);
    ~DriveWidgetElement() override;

    void     SetCommandSink       (IDriveCommandSink * pSink) { m_pSink = pSink; }
    void     SetOwnerHwnd         (HWND hwnd) { m_ownerHwnd = hwnd; }
    void     SyncFromState        (const DriveWidgetState & state);
    int      GetSlotAttr          () const;
    int      GetDriveAttr         () const;
    HRESULT  HandleDroppedFile    (const std::wstring & path);

protected:
    void     ProcessDefaultAction (Rml::Event & event) override;

private:
    void     OpenAndMount         ();
    bool     ShowFileOpenDialog   (std::wstring & outPath) const;
    void     ApplyDoorClasses     (DriveWidgetState::Door door);

    IDriveCommandSink *  m_pSink     = nullptr;
    HWND                 m_ownerHwnd = nullptr;

    // Last applied snapshot. Avoids dirtying Rml state when nothing changed.
    bool                   m_lastMounted  = false;
    bool                   m_lastSpinning = false;
    DriveWidgetState::Door m_lastDoor     = DriveWidgetState::Door::Closed;
    int                    m_lastLed      = -1;
};
