#pragma once

#include "Pch.h"






////////////////////////////////////////////////////////////////////////////////
//
//  RmlSystemInterface
//
//  Glue between RmlUi's SystemInterface contract and the Win32 host:
//      * GetElapsedTime uses QueryPerformanceCounter pinned to the
//        instance's construction time so multiple shells inside a
//        single process don't fight over a global clock.
//      * LogMessage routes through OutputDebugStringA so messages
//        appear in the VS Output window alongside DEBUGMSG.
//      * Get/SetClipboardText use the OpenClipboard/GetClipboardData
//        UNICODE path and convert to/from RmlUi's UTF-8 String.
//      * SetMouseCursor maps RmlUi's standard cursor names
//        ("arrow", "pointer", "text", "move", "wait", ...) to the
//        Win32 IDC_* stock cursors. Anything unrecognised falls
//        back to IDC_ARROW.
//      * ActivateKeyboard/DeactivateKeyboard are no-ops on the
//        desktop Win32 platform (no soft-keyboard to summon).
//      * TranslateString is the identity pass-through; localization
//        is handled outside RmlUi.
//
////////////////////////////////////////////////////////////////////////////////

class RmlSystemInterface : public Rml::SystemInterface
{
public:
    RmlSystemInterface  ();
    ~RmlSystemInterface () override;

    double GetElapsedTime    () override;
    int    TranslateString   (Rml::String & translated, const Rml::String & input) override;
    bool   LogMessage        (Rml::Log::Type type, const Rml::String & message) override;
    void   SetMouseCursor    (const Rml::String & cursor_name) override;
    void   SetClipboardText  (const Rml::String & text) override;
    void   GetClipboardText  (Rml::String       & text) override;
    void   ActivateKeyboard   (Rml::Vector2f caret_position, float line_height) override;
    void   DeactivateKeyboard () override;

private:
    LARGE_INTEGER  m_qpcFrequency = {};
    LARGE_INTEGER  m_qpcStart     = {};
};
