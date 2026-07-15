#pragma once

#include "Pch.h"

#include "../../Config/GlobalUserPrefs.h"

#include "Window/DxuiPropertyPage.h"
#include "Widgets/DxuiDropdown.h"
#include "Widgets/DxuiLabel.h"


class DxuiHwndSource;




////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage
//
//  Settings > Printing (FR-011): the host print-service preferences, shared by
//  every emulated machine (host print services are host resources, like the
//  keyboard). Edits write straight into GlobalUserPrefs; the sheet's apply
//  controller persists them on OK and reverts them on Cancel. None have a live
//  effect -- they bind at the next delivery.
//
//      * Resolution     (DxuiDropdown: 288 / 576 dpi, FR-028)
//      * Dot style      (DxuiDropdown: ink / plain, FR-027)
//
//  The delivery destination is no longer a preference: the preview's Print /
//  Save buttons (and the File menu's Copy) choose it per action, and Save
//  always prompts for the PNG path, so a stored destination + folder are gone.
//
////////////////////////////////////////////////////////////////////////////////

class PrintingPage : public DxuiPropertyPage
{
public:
    explicit PrintingPage (std::wstring title = L"Printing");

    // Backing store; seeds the controls and wires their change callbacks.
    void  SetPrefs             (GlobalUserPrefs * prefs);

    void  SetPopupHost         (DxuiHwndSource * host);

    void  Layout               (const RECT & rect, const DxuiDpiScaler & scaler) override;
    void  Rebuild              ();

    // Test / wiring accessors.
    DxuiDropdown       & ResolutionDropdown  ()       { return m_dpi;         }
    DxuiDropdown       & DotStyleDropdown    ()       { return m_dotStyle;     }
    const DxuiDropdown & ResolutionDropdown  () const { return m_dpi;         }
    const DxuiDropdown & DotStyleDropdown    () const { return m_dotStyle;     }

private:
    GlobalUserPrefs *  m_prefs = nullptr;

    DxuiLabel     m_dpiLabel;
    DxuiDropdown  m_dpi;
    DxuiLabel     m_styleLabel;
    DxuiDropdown  m_dotStyle;
};
