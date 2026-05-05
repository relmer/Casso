#include "Pch.h"

#include "MachinePickerDialog.h"
#include "Core/JsonParser.h"
#include "Core/JsonValue.h"
#include "Core/PathResolver.h"
#include "Ehm.h"


static constexpr int kDialogWidth  = 360;
static constexpr int kDialogHeight = 220;
static constexpr int kIdListView   = 100;
static constexpr int kIdOk         = IDOK;
static constexpr int kIdCancel     = IDCANCEL;





////////////////////////////////////////////////////////////////////////////////
//
//  Show
//
////////////////////////////////////////////////////////////////////////////////

wstring MachinePickerDialog::Show (HWND hwndParent, const wstring & currentMachine)
{
    MachinePickerDialog picker (hwndParent, currentMachine);

    picker.ScanMachines();

    // Build in-memory dialog template
    // DLGTEMPLATE must be DWORD-aligned
    alignas(4) BYTE buffer[512] = {};
    DLGTEMPLATE * dlg            = reinterpret_cast<DLGTEMPLATE *> (buffer);



    dlg->style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    dlg->cdit  = 0;
    dlg->cx    = static_cast<short> (kDialogWidth);
    dlg->cy    = static_cast<short> (kDialogHeight);

    // Menu, class, title follow DLGTEMPLATE as wide strings
    WORD * p   = reinterpret_cast<WORD *> (dlg + 1);
    *p++ = 0;    // no menu
    *p++ = 0;    // default class

    // Title: "Select Machine"
    const wchar_t * title = L"Select Machine";

    wcscpy_s (reinterpret_cast<wchar_t *> (p), 32, title);
    p += wcslen (title) + 1;

    DialogBoxIndirectParam (GetModuleHandle (nullptr),
                            dlg,
                            hwndParent,
                            DialogProc,
                            reinterpret_cast<LPARAM> (&picker));

    return picker.m_selectedMachine;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePickerDialog
//
////////////////////////////////////////////////////////////////////////////////

MachinePickerDialog::MachinePickerDialog (HWND hwndParent, const wstring & currentMachine)
    : m_hwndParent     (hwndParent),
      m_currentMachine (currentMachine)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogProc
//
////////////////////////////////////////////////////////////////////////////////

INT_PTR CALLBACK MachinePickerDialog::DialogProc (HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    MachinePickerDialog * self = nullptr;



    if (msg == WM_INITDIALOG)
    {
        self = reinterpret_cast<MachinePickerDialog *> (lParam);
        SetWindowLongPtr (hdlg, DWLP_USER, reinterpret_cast<LONG_PTR> (self));
        self->OnInitDialog (hdlg);
        return TRUE;
    }

    self = reinterpret_cast<MachinePickerDialog *> (GetWindowLongPtr (hdlg, DWLP_USER));

    if (self == nullptr)
    {
        return FALSE;
    }

    switch (msg)
    {
        case WM_COMMAND:
        {
            if (LOWORD (wParam) == kIdOk)
            {
                self->OnOK (hdlg);
                return TRUE;
            }

            if (LOWORD (wParam) == kIdCancel)
            {
                EndDialog (hdlg, IDCANCEL);
                return TRUE;
            }

            break;
        }

        case WM_NOTIFY:
        {
            NMHDR * pnmh = reinterpret_cast<NMHDR *> (lParam);

            if (pnmh->idFrom == kIdListView && pnmh->code == NM_DBLCLK)
            {
                self->OnListDoubleClick (hdlg);
                return TRUE;
            }

            break;
        }

        case WM_CLOSE:
        {
            EndDialog (hdlg, IDCANCEL);
            return TRUE;
        }
    }

    return FALSE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnInitDialog
//
////////////////////////////////////////////////////////////////////////////////

void MachinePickerDialog::OnInitDialog (HWND hdlg)
{
    RECT   clientRect = {};
    HWND   hList      = nullptr;
    HWND   hOk        = nullptr;
    HWND   hCancel    = nullptr;
    int    listHeight = 0;
    int    btnWidth   = 75;
    int    btnHeight  = 23;
    int    margin     = 8;
    int    btnY       = 0;
    int    selIndex   = -1;



    GetClientRect (hdlg, &clientRect);

    listHeight = clientRect.bottom - btnHeight - margin * 3;
    btnY       = clientRect.bottom - btnHeight - margin;

    // Create ListView
    hList = CreateWindowExW (WS_EX_CLIENTEDGE,
                             WC_LISTVIEWW,
                             L"",
                             WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                 LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                             margin,
                             margin,
                             clientRect.right - margin * 2,
                             listHeight,
                             hdlg,
                             reinterpret_cast<HMENU> (static_cast<INT_PTR> (kIdListView)),
                             GetModuleHandle (nullptr),
                             nullptr);

    ListView_SetExtendedListViewStyle (hList, LVS_EX_FULLROWSELECT);

    // Add columns
    LVCOLUMNW col = {};
    col.mask      = LVCF_TEXT | LVCF_WIDTH;

    col.pszText = const_cast<LPWSTR> (L"Machine");
    col.cx      = 180;
    ListView_InsertColumn (hList, 0, &col);

    col.pszText = const_cast<LPWSTR> (L"Config File");
    col.cx      = clientRect.right - margin * 2 - 180 - 20;
    ListView_InsertColumn (hList, 1, &col);

    // Populate list
    for (int i = 0; i < static_cast<int> (m_machines.size()); i++)
    {
        LVITEMW item = {};
        item.mask    = LVIF_TEXT;
        item.iItem   = i;

        item.pszText = const_cast<LPWSTR> (m_machines[i].displayName.c_str());
        ListView_InsertItem (hList, &item);

        ListView_SetItemText (hList,
                              i,
                              1,
                              const_cast<LPWSTR> (m_machines[i].fileName.c_str()));

        if (m_machines[i].fileName == m_currentMachine)
        {
            selIndex = i;
        }
    }

    // Select current machine (or first entry)
    if (selIndex < 0 && !m_machines.empty())
    {
        selIndex = 0;
    }

    if (selIndex >= 0)
    {
        ListView_SetItemState (hList, selIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible (hList, selIndex, FALSE);
    }

    // Create OK and Cancel buttons
    hOk = CreateWindowExW (0,
                           L"BUTTON",
                           L"OK",
                           WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                           clientRect.right - margin - btnWidth * 2 - margin,
                           btnY,
                           btnWidth,
                           btnHeight,
                           hdlg,
                           reinterpret_cast<HMENU> (static_cast<INT_PTR> (kIdOk)),
                           GetModuleHandle (nullptr),
                           nullptr);

    hCancel = CreateWindowExW (0,
                               L"BUTTON",
                               L"Cancel",
                               WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                               clientRect.right - margin - btnWidth,
                               btnY,
                               btnWidth,
                               btnHeight,
                               hdlg,
                               reinterpret_cast<HMENU> (static_cast<INT_PTR> (kIdCancel)),
                               GetModuleHandle (nullptr),
                               nullptr);

    // Set font on buttons to match system default
    HFONT hFont = reinterpret_cast<HFONT> (GetStockObject (DEFAULT_GUI_FONT));
    SendMessage (hOk,     WM_SETFONT, reinterpret_cast<WPARAM> (hFont), TRUE);
    SendMessage (hCancel, WM_SETFONT, reinterpret_cast<WPARAM> (hFont), TRUE);

    SetFocus (hList);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnOK
//
////////////////////////////////////////////////////////////////////////////////

void MachinePickerDialog::OnOK (HWND hdlg)
{
    HWND hList   = GetDlgItem (hdlg, kIdListView);
    int  selItem = ListView_GetNextItem (hList, -1, LVNI_SELECTED);



    if (selItem >= 0 && selItem < static_cast<int> (m_machines.size()))
    {
        m_selectedMachine = m_machines[selItem].fileName;
    }

    EndDialog (hdlg, IDOK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnListDoubleClick
//
////////////////////////////////////////////////////////////////////////////////

void MachinePickerDialog::OnListDoubleClick (HWND hdlg)
{
    OnOK (hdlg);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScanMachines
//
//  Searches for Machines/*.json using the same path resolution as Main.cpp.
//  Parses each file to extract the "name" field for display.
//
////////////////////////////////////////////////////////////////////////////////

void MachinePickerDialog::ScanMachines()
{
    vector<fs::path>   searchPaths;
    fs::path           machinesDir;



    searchPaths = PathResolver::BuildSearchPaths (PathResolver::GetExecutableDirectory(),
                                                   PathResolver::GetWorkingDirectory());

    for (const auto & basePath : searchPaths)
    {
        machinesDir = basePath / "Machines";

        if (!fs::is_directory (machinesDir))
        {
            continue;
        }

        for (const auto & entry : fs::directory_iterator (machinesDir))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".json")
            {
                continue;
            }

            MachineInfo info;
            info.fileName = entry.path().stem().wstring();

            // Try to extract the "name" field from the JSON
            ifstream     file (entry.path());
            stringstream ss;

            if (file.good())
            {
                ss << file.rdbuf();

                JsonValue      root;
                JsonParseError parseError;
                HRESULT        hr = JsonParser::Parse (ss.str(), root, parseError);

                if (SUCCEEDED (hr))
                {
                    string name;
                    hr = root.GetString ("name", name);

                    if (SUCCEEDED (hr))
                    {
                        info.displayName = wstring (name.begin(), name.end());
                    }
                }
            }

            if (info.displayName.empty())
            {
                info.displayName = info.fileName;
            }

            // Avoid duplicates (first search path wins)
            bool duplicate = false;

            for (const auto & existing : m_machines)
            {
                if (existing.fileName == info.fileName)
                {
                    duplicate = true;
                    break;
                }
            }

            if (!duplicate)
            {
                m_machines.push_back (move (info));
            }
        }

        // Stop after first Machines/ directory found
        break;
    }
}
