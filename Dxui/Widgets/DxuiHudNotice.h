#pragma once

#include "Pch.h"

#include "Widgets/DxuiShadowedText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHudNotice
//
//  The persistent message laid across live content: a captured pointer, a mode
//  with a way out. One line, centered in whatever band the caller hands it,
//  which in practice is a strip along the bottom of the viewport.
//
//  DxuiShadowedText does the work and says why it looks the way it does. What
//  this adds is the POSITIONING RULE and nothing else: a notice is centered in
//  its band. Keeping that rule here is what lets an FPS readout share the
//  legibility treatment without inheriting a notification's layout.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiHudNotice : public DxuiShadowedText
{
public:
    DxuiHudNotice ()
    {
        DxuiShadowedText::SetAlign (DxuiTextHAlign::Center, DxuiTextVAlign::Center);
    }

    ~DxuiHudNotice () override = default;

private:
    // Centering is the whole of what this class is. A caller that wants the
    // text somewhere else wants DxuiShadowedText, not a notice that has been
    // talked out of being one.
    using DxuiShadowedText::SetAlign;
};
