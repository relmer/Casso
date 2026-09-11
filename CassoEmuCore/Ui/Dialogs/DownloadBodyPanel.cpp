#include "Pch.h"

#include "DownloadBodyPanel.h"

#include "DialogDefinition.h"
#include "../Chrome/CassoTheme.h"
#include "Core/DxuiEvents.h"
#include "Render/DxuiPainter.h"
#include "Render/DxuiTextRenderer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
////////////////////////////////////////////////////////////////////////////////

void DownloadBodyPanel::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    SetBounds  (boundsPx);
    m_dpiScale = (float) scaler.GetDpi() / (float) DxuiDpiScaler::kBaseDpi;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
////////////////////////////////////////////////////////////////////////////////

void DownloadBodyPanel::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    DialogPaintContext  ctx;



    ctx.painter        = static_cast<DxuiPainter *> (&painter);
    ctx.text           = static_cast<DxuiTextRenderer *> (&text);
    ctx.theme          = static_cast<const CassoTheme *> (&theme);
    ctx.customBodyRect = GetBounds();
    ctx.dpiScale       = m_dpiScale;

    if (m_paint)
    {
        m_paint (ctx);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool DownloadBodyPanel::OnMouse (const DxuiMouseEvent & ev)
{
    DialogInputEvent  die;
    RECT              b        = GetBounds();
    bool              consumed = true;



    die.xPx = ev.positionDip.x - b.left;
    die.yPx = ev.positionDip.y - b.top;

    switch (ev.kind)
    {
    case DxuiMouseEventKind::Down: die.kind = DialogInputEvent::Kind::LeftButtonDown; break;
    case DxuiMouseEventKind::Up:   die.kind = DialogInputEvent::Kind::LeftButtonUp;   break;
    case DxuiMouseEventKind::Move: die.kind = DialogInputEvent::Kind::MouseMove;      break;
    default:                       consumed = false;                                  break;
    }

    if (consumed && m_input)
    {
        m_input (die);
    }

    return consumed;
}
