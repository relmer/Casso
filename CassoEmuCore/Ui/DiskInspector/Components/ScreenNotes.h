#pragma once

#include "Pch.h"

class InspectorSession;





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenNotes
//
//  The notes the window draws in place of its views: the empty drive's
//  outline and why there is no disk (FR-060), and on a 3.5" disk the note
//  over every view but the Image view (FR-054), where the views under the
//  note get no input.
//
////////////////////////////////////////////////////////////////////////////////

class ScreenNotes
{
public:
    explicit ScreenNotes (const InspectorSession & session) : m_session (session) {}

    void  PaintNoDisk  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    void  PaintNot35   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);
    bool  IsCovered    (POINT pointPx) const;
    bool  IsImageShown () const;

private:
    static bool  IsInside (const RECT & rect, POINT pointPx);

    const InspectorSession  & m_session;
};