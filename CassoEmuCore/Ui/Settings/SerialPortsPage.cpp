#include "Pch.h"

#include "Ui/Settings/SerialPortsPage.h"
#include "Ui/Settings/SettingsPanelState.h"





// Layout metrics (DIP), matching the Serial card page.
static constexpr int  s_kRowHeightDp     = 28;
static constexpr int  s_kLabelWidthDp    = 210;
static constexpr int  s_kControlsWidthDp = 400;
static constexpr int  s_kSectionGapDp    = 14;
static constexpr int  s_kRowGapDp        = 8;
static constexpr int  s_kPagePadDp       = 16;
static constexpr int  s_kNoteHeightDp    = 36;





////////////////////////////////////////////////////////////////////////////////
//
//  SerialPortsPage::SerialPortsPage
//
////////////////////////////////////////////////////////////////////////////////

SerialPortsPage::SerialPortsPage (std::wstring title)
    : DxuiPropertyPage (std::move (title))
{
    m_port1.AdoptInto (*this);
    m_port2.AdoptInto (*this);

    m_port1.SetOnChange ([this] (const SerialAttachmentChoice & choice) { OnChanged (SerialPortId::Port1, choice); });
    m_port2.SetOnChange ([this] (const SerialAttachmentChoice & choice) { OnChanged (SerialPortId::Port2, choice); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialPortsPage::SetState
//
////////////////////////////////////////////////////////////////////////////////

void SerialPortsPage::SetState (SettingsPanelState * state)
{
    m_state = state;
    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialPortsPage::SetPopupHost
//
////////////////////////////////////////////////////////////////////////////////

void SerialPortsPage::SetPopupHost (DxuiHwndSource * host)
{
    m_port1.SetPopupHost (host);
    m_port2.SetPopupHost (host);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialPortsPage::SetSerialPorts
//
////////////////////////////////////////////////////////////////////////////////

void SerialPortsPage::SetSerialPorts (IHostSerialPorts * ports)
{
    m_port1.SetSerialPorts (ports);
    m_port2.SetSerialPorts (ports);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialPortsPage::SetOnBrowse
//
////////////////////////////////////////////////////////////////////////////////

void SerialPortsPage::SetOnBrowse (SerialAttachmentView::BrowseFn fn)
{
    m_port1.SetOnBrowse (fn);
    m_port2.SetOnBrowse (std::move (fn));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialPortsPage::Rebuild
//
//  Each port's view from the staged machine.
//
////////////////////////////////////////////////////////////////////////////////

void SerialPortsPage::Rebuild()
{
    SerialAttachmentChoice  none;



    m_port1.SetChoice ((m_state != nullptr) ? m_state->GetSerialAttachment (SerialPortId::Port1) : none);
    m_port2.SetChoice ((m_state != nullptr) ? m_state->GetSerialAttachment (SerialPortId::Port2) : none);
    Relayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialPortsPage::Layout
//
//  Port 1, then port 2 below it.
//
////////////////////////////////////////////////////////////////////////////////

void SerialPortsPage::Layout (const RECT & rect, const DxuiDpiScaler & scaler)
{
    int                            pad           = scaler.ToPx (s_kPagePadDp);
    int                            labelWidth    = scaler.ToPx (s_kLabelWidthDp);
    int                            sectionGap    = scaler.ToPx (s_kSectionGapDp);
    int                            controlsWidth = std::min (scaler.ToPx (s_kControlsWidthDp), (int) (rect.right - rect.left) - pad * 2 - labelWidth);
    int                            x             = rect.left + pad;
    int                            y             = rect.top  + pad;
    SerialAttachmentView::Metrics  metrics       = { scaler.ToPx (s_kRowHeightDp), scaler.ToPx (s_kRowGapDp), sectionGap, scaler.ToPx (s_kNoteHeightDp) };



    m_hasLayout  = true;
    m_lastRect   = rect;
    m_lastScaler = scaler;

    y = m_port1.Layout (x, y, labelWidth, controlsWidth, metrics, scaler);
    y = m_port2.Layout (x, y + sectionGap * 2, labelWidth, controlsWidth, metrics, scaler);

    DxuiPanel::SetBounds (rect);
    SetContentHeightPx (GetLowestChildBottomPx() + pad - rect.top);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialPortsPage::OnChanged
//
////////////////////////////////////////////////////////////////////////////////

void SerialPortsPage::OnChanged (
    SerialPortId                    port,
    const SerialAttachmentChoice  & choice)
{
    HRESULT  hr = S_OK;



    if (m_state != nullptr)
    {
        hr = m_state->SetSerialAttachment (port, choice);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    Relayout();
    MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialPortsPage::Relayout
//
////////////////////////////////////////////////////////////////////////////////

void SerialPortsPage::Relayout()
{
    if (m_hasLayout)
    {
        Layout (m_lastRect, m_lastScaler);
    }
}
