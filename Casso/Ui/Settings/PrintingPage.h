#pragma once

#include "Pch.h"

#include "../../Config/GlobalUserPrefs.h"

#include "Window/DxuiPropertyPage.h"
#include "Widgets/DxuiDropdown.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiButton.h"


class DxuiHwndSource;




////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage
//
//  Settings > Printing (FR-011): the host print-service preferences, shared by
//  every emulated machine (host print services are host resources, like the
//  keyboard). Edits write straight into GlobalUserPrefs; the sheet's apply
//  controller persists them on OK and reverts them on Cancel. None have a live
//  effect -- they bind at the next eject.
//
//      * Destination   (DxuiDropdown: PNG file / Windows printer, FR-013)
//      * Resolution     (DxuiDropdown: 288 / 576 dpi, FR-028)
//      * Dot style      (DxuiDropdown: ink / plain, FR-027)
//      * PNG folder     (path label + Browse..., dimmed when the destination
//                        is the Windows printer)
//
////////////////////////////////////////////////////////////////////////////////

class PrintingPage : public DxuiPropertyPage
{
public:
    explicit PrintingPage (std::wstring title = L"Printing");

    // Backing store; seeds the controls and wires their change callbacks.
    void  SetPrefs             (GlobalUserPrefs * prefs);

    // The resolved default PNG folder (<Pictures>\Casso Prints), shown when
    // printPngFolder is empty so the row never looks blank.
    void  SetDefaultPngFolder  (std::wstring folder) { m_defaultPngFolder = std::move (folder); RefreshFolderValue (); }

    // Folder picker seam: returns true + the chosen folder, false on cancel.
    // The sheet supplies the IFileDialog implementation (it owns the HWND).
    using BrowseFolderFn = std::function<bool (std::wstring & outFolder)>;
    void  SetOnBrowseFolder    (BrowseFolderFn fn) { m_browseFolder = std::move (fn); }

    void  SetPopupHost         (DxuiHwndSource * host);

    void  Layout               (const RECT & rect, const DxuiDpiScaler & scaler) override;
    void  Rebuild              ();

    // Test / wiring accessors.
    DxuiDropdown       & DestinationDropdown ()       { return m_destination; }
    DxuiDropdown       & ResolutionDropdown  ()       { return m_dpi;         }
    DxuiDropdown       & DotStyleDropdown    ()       { return m_dotStyle;     }
    const DxuiDropdown & DestinationDropdown () const { return m_destination; }
    const DxuiDropdown & ResolutionDropdown  () const { return m_dpi;         }
    const DxuiDropdown & DotStyleDropdown    () const { return m_dotStyle;     }

private:
    void  RefreshFolderValue      ();
    void  ApplyDestinationEnabled (bool pngSelected);

    GlobalUserPrefs *  m_prefs = nullptr;
    std::wstring       m_defaultPngFolder;
    BrowseFolderFn     m_browseFolder;

    DxuiLabel     m_destLabel;
    DxuiDropdown  m_destination;
    DxuiLabel     m_dpiLabel;
    DxuiDropdown  m_dpi;
    DxuiLabel     m_styleLabel;
    DxuiDropdown  m_dotStyle;
    DxuiLabel     m_folderLabel;
    DxuiLabel     m_folderValue;
    DxuiButton    m_browse;
};
