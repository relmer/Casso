#pragma once

#include "Pch.h"

#include "DriveWidgetState.h"
#include "IDriveCommandSink.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Types.h>

#include <string>





////////////////////////////////////////////////////////////////////////////////
//
//  DriveWidgetElement
//
//  Custom `Rml::Element` subclass (P6-T3) that backs the `<drive-widget>`
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

    // Wires the click-to-browse + eject command target. May be called
    // any time; clicks before SetCommandSink is called are silently
    // ignored.
    void  SetCommandSink (IDriveCommandSink * pSink) { m_pSink = pSink; }

    // Parent HWND used as the owner window for the IFileDialog. Without
    // this the file dialog will be modeless w.r.t. the emulator window.
    void  SetOwnerHwnd (HWND hwnd) { m_ownerHwnd = hwnd; }

    // Push the current state into the element. Idempotent; called once
    // per UI frame from `EmulatorShell::Update`. Reads the atomic
    // motorOn/diskActive flags and the UI-only door / mounted fields.
    void  SyncFromState (const DriveWidgetState & state);

    // Read the slot/drive attributes from the RML tag. Result is cached
    // on first call. Returns -1 if the attribute is missing or invalid.
    int   GetSlotAttr() const;
    int   GetDriveAttr() const;

    // Drag-drop entry point. Called by `DragDropTarget::Drop` after it
    // has confirmed the dropped file's extension is supported. Returns
    // S_OK on successful dispatch to the sink.
    HRESULT HandleDroppedFile (const std::wstring & path);

protected:
    // Click routing -- forwards body clicks to the open-file path and
    // eject-btn child clicks to `IDriveCommandSink::Eject`.
    void  ProcessDefaultAction (Rml::Event & event) override;

private:
    void  OpenAndMount();
    bool  ShowFileOpenDialog (std::wstring & outPath) const;
    void  ApplyDoorClasses (DriveWidgetState::Door door);

    IDriveCommandSink *  m_pSink      = nullptr;
    HWND                 m_ownerHwnd  = nullptr;

    // Last applied snapshot. Used to skip Rml class mutations when the
    // state hasn't changed (avoids dirtying the layout / style cache
    // every UI frame even when the drive sits idle).
    bool                            m_lastMounted     = false;
    bool                            m_lastSpinning    = false;
    DriveWidgetState::Door          m_lastDoor        = DriveWidgetState::Door::Closed;
    int                             m_lastLed         = -1;
};
