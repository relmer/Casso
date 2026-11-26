#include "Pch.h"

#include "Widgets/DxuiTimedInfoBar.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTimedInfoBar::DxuiTimedInfoBar
//
//  Starts hidden: nothing has been reported yet.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTimedInfoBar::DxuiTimedInfoBar()
{
    SetPlacement  (Placement::Overlay);
    SetDurationMs (ResolveDefaultDurationMs());
    SetSeverity   (Severity::Info);
    SetCentered   (true);
    SetVisible    (false);
}
