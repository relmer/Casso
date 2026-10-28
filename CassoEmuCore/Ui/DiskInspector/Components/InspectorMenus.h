#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Components/IInspectorComponent.h"
#include "Ui/DiskInspector/Components/InspectorCommands.h"

class IInspectorComponentHost;
class InspectorSession;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorMenus
//
//  The menu bar under the title bar: File, Edit, View, Go, Disk, Window and
//  Help, each command with its shortcut, opened by a click, Alt, F10 or a
//  menu's underlined letter (FR-158).
//
////////////////////////////////////////////////////////////////////////////////

class InspectorMenus : public IInspectorComponent
{
public:
    InspectorMenus (InspectorSession & session, IInspectorComponentHost & host, const InspectorCommands & commands);

    void  Create () override;
    void  Tick   (int64_t nowMs) override;
    bool  OnKey  (const DxuiKeyEvent & ev) override;

    void  Layout      (const RECT & area, const DxuiDpiScaler & scaler);
    int   GetHeightPx (const DxuiDpiScaler & scaler) const;
    void  Rebuild     ();
    bool  IsActive    () const;

    const DxuiMenuBar &  GetMenuBar () const { return *m_menuBar; }

private:
    InspectorSession           & m_session;
    IInspectorComponentHost    & m_host;
    const InspectorCommands    & m_commands;
    DxuiMenuBar                * m_menuBar     = nullptr;
    bool                         m_isAltAlone  = false;
};
