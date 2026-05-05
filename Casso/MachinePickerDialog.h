#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineInfo
//
////////////////////////////////////////////////////////////////////////////////

struct MachineInfo
{
    wstring   displayName;
    wstring   fileName;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePickerDialog
//
//  Modal dialog that lists available machine configs from the Machines/ folder.
//  Returns the selected machine base name (e.g. "apple2plus") or empty on cancel.
//
////////////////////////////////////////////////////////////////////////////////

class MachinePickerDialog
{
public:
    static wstring Show (HWND hwndParent, const wstring & currentMachine);

private:
    MachinePickerDialog (HWND hwndParent, const wstring & currentMachine);

    static INT_PTR CALLBACK DialogProc (HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);

    void OnInitDialog (HWND hdlg);
    void OnOK         (HWND hdlg);
    void OnListDoubleClick (HWND hdlg);

    void ScanMachines();

    HWND                    m_hwndParent;
    wstring                 m_currentMachine;
    wstring                 m_selectedMachine;
    vector<MachineInfo>     m_machines;
};
