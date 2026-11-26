#pragma once

#include "Pch.h"
#include "Core/DxuiEvents.h"
#include "Core/DxuiKeyMap.h"
#include "Window/IDxuiHostClient.h"


class DxuiDialogController;
class DxuiFocusManager;
class DxuiPopupMenu;





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiKeyRouterHost
//
//  What DxuiKeyRouter calls in its window: the input filters, the modal
//  overlay, the open context menu, the control tree and the mapped command
//  handler, and the paint batch around each key.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiKeyRouterHost
{
public:
    virtual ~IDxuiKeyRouterHost() = default;

    virtual HWND             GetKeyHwnd          () const                     = 0;
    virtual bool             RouteFilteredKey    (WPARAM vk)                  = 0;
    virtual bool             RouteOverlayKey     (WPARAM vk)                  = 0;   // false when no overlay is up
    virtual bool             RouteOverlayChar    (wchar_t ch)                 = 0;   // false when no overlay is up
    virtual DxuiPopupMenu *  GetOpenContextMenu  ()                           = 0;
    virtual bool             RouteKeyToControls  (const DxuiKeyEvent & ev)    = 0;
    virtual bool             RunMappedCommand    (int commandId)              = 0;
    virtual void             BeginKeyBatch       ()                           = 0;
    virtual void             EndKeyBatch         ()                           = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiKeyRouter
//
//  Routes the window's keys and characters: the input filters first, then
//  a modal overlay, an open context menu, a dialog's navigation, and the
//  controls, with a key no control claimed looked up in the key map.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiKeyRouter
{
public:
    DxuiKeyRouter (IDxuiKeyRouterHost & host, DxuiDialogController & dialog, DxuiFocusManager & focus) : m_host (host), m_dialog (dialog), m_focus (focus) {}

    DxuiMessageResult  OnKeyDown (WPARAM vk);
    DxuiMessageResult  OnChar    (wchar_t ch);

    //  Dxui's IsDialogMessage for a modeless dialog: the host's message loop
    //  calls it before TranslateMessage. Only Tab, Enter and Escape for this
    //  window are taken.
    bool  ProcessDialogMessage (const MSG & msg);

    //  The map a key-down nothing claimed is looked up in, borrowed; null
    //  looks nothing up. Swapping it is how an application changes keyboard
    //  scheme.
    void                SetKeyMap (const DxuiKeyMap * map) { m_keyMap = map; }
    const DxuiKeyMap *  GetKeyMap () const                 { return m_keyMap; }

    //  Looks an unclaimed key up in the map and runs a match's command.
    bool  RouteMappedKey (const DxuiKeyEvent & ev);

    //  Whether the first key of a two-key chord was pressed and the second
    //  is awaited.
    bool  IsKeyPrefixPending () const { return m_keyPrefix.has_value(); }

private:
    DxuiMessageResult  DispatchKey       (DxuiKeyEventKind kind, WPARAM code);
    DxuiMessageResult  DispatchDialogKey (WPARAM vk);

    IDxuiKeyRouterHost            & m_host;
    DxuiDialogController          & m_dialog;
    DxuiFocusManager              & m_focus;
    const DxuiKeyMap              * m_keyMap = nullptr;
    std::optional<DxuiKeyStroke>    m_keyPrefix;          // the first key of a two-key chord, while it waits
};
