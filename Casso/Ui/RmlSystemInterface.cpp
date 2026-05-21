#include "Pch.h"

#include "RmlSystemInterface.h"






////////////////////////////////////////////////////////////////////////////////
//
//  RmlSystemInterface
//
////////////////////////////////////////////////////////////////////////////////

RmlSystemInterface::RmlSystemInterface()
{
    QueryPerformanceFrequency (&m_qpcFrequency);
    QueryPerformanceCounter   (&m_qpcStart);
}



RmlSystemInterface::~RmlSystemInterface()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetElapsedTime
//
////////////////////////////////////////////////////////////////////////////////

double RmlSystemInterface::GetElapsedTime()
{
    LARGE_INTEGER now = {};

    QueryPerformanceCounter (&now);

    LONGLONG ticks = now.QuadPart - m_qpcStart.QuadPart;

    if (m_qpcFrequency.QuadPart == 0)
    {
        return 0.0;
    }

    return static_cast<double> (ticks) / static_cast<double> (m_qpcFrequency.QuadPart);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TranslateString
//
////////////////////////////////////////////////////////////////////////////////

int RmlSystemInterface::TranslateString (Rml::String & translated, const Rml::String & input)
{
    translated = input;
    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LogMessage
//
////////////////////////////////////////////////////////////////////////////////

bool RmlSystemInterface::LogMessage (Rml::Log::Type type, const Rml::String & message)
{
    const char * label = "INFO";

    switch (type)
    {
        case Rml::Log::LT_ERROR:   label = "ERROR";   break;
        case Rml::Log::LT_ASSERT:  label = "ASSERT";  break;
        case Rml::Log::LT_WARNING: label = "WARN";    break;
        case Rml::Log::LT_INFO:    label = "INFO";    break;
        case Rml::Log::LT_DEBUG:   label = "DEBUG";   break;
        case Rml::Log::LT_ALWAYS:  label = "ALWAYS";  break;
        default:                   label = "MSG";     break;
    }

    string line = string ("[RmlUi:") + label + "] " + message + "\n";

    OutputDebugStringA (line.c_str());

    // Returning true keeps execution flowing past asserts; we don't
    // want a RmlUi LT_ASSERT to crash the emulator in release builds.
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetMouseCursor
//
//  RmlUi documents typically request one of: "", "arrow", "pointer",
//  "text", "move", "wait", "cross", "help", "not-allowed", "ns-resize",
//  "ew-resize", "nesw-resize", "nwse-resize". The strings come straight
//  from RCSS `cursor:` properties so we silently fall back to IDC_ARROW
//  on anything we don't recognise rather than logging.
//
////////////////////////////////////////////////////////////////////////////////

void RmlSystemInterface::SetMouseCursor (const Rml::String & cursor_name)
{
    LPCWSTR id = IDC_ARROW;

    if (cursor_name == "pointer")            { id = IDC_HAND;     }
    else if (cursor_name == "text")          { id = IDC_IBEAM;    }
    else if (cursor_name == "move")          { id = IDC_SIZEALL;  }
    else if (cursor_name == "wait")          { id = IDC_WAIT;     }
    else if (cursor_name == "cross")         { id = IDC_CROSS;    }
    else if (cursor_name == "help")          { id = IDC_HELP;     }
    else if (cursor_name == "not-allowed")   { id = IDC_NO;       }
    else if (cursor_name == "ns-resize")     { id = IDC_SIZENS;   }
    else if (cursor_name == "ew-resize")     { id = IDC_SIZEWE;   }
    else if (cursor_name == "nesw-resize")   { id = IDC_SIZENESW; }
    else if (cursor_name == "nwse-resize")   { id = IDC_SIZENWSE; }
    else                                     { id = IDC_ARROW;    }

    HCURSOR cur = LoadCursor (nullptr, id);

    if (cur != nullptr)
    {
        SetCursor (cur);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetClipboardText
//
////////////////////////////////////////////////////////////////////////////////

void RmlSystemInterface::SetClipboardText (const Rml::String & text)
{
    if (!OpenClipboard (nullptr))
    {
        return;
    }

    EmptyClipboard();

    // UTF-8 -> UTF-16
    int wlen = MultiByteToWideChar (CP_UTF8, 0, text.c_str(), -1, nullptr, 0);

    if (wlen > 0)
    {
        HGLOBAL hMem = GlobalAlloc (GMEM_MOVEABLE, static_cast<SIZE_T> (wlen) * sizeof (wchar_t));

        if (hMem != nullptr)
        {
            wchar_t * dst = static_cast<wchar_t *> (GlobalLock (hMem));

            if (dst != nullptr)
            {
                MultiByteToWideChar (CP_UTF8, 0, text.c_str(), -1, dst, wlen);
                GlobalUnlock (hMem);
                SetClipboardData (CF_UNICODETEXT, hMem);
                // ownership transferred to clipboard; do NOT GlobalFree.
            }
            else
            {
                GlobalFree (hMem);
            }
        }
    }

    CloseClipboard();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetClipboardText
//
////////////////////////////////////////////////////////////////////////////////

void RmlSystemInterface::GetClipboardText (Rml::String & text)
{
    text.clear();

    if (!IsClipboardFormatAvailable (CF_UNICODETEXT))
    {
        return;
    }

    if (!OpenClipboard (nullptr))
    {
        return;
    }

    HANDLE handle = GetClipboardData (CF_UNICODETEXT);

    if (handle != nullptr)
    {
        const wchar_t * src = static_cast<const wchar_t *> (GlobalLock (handle));

        if (src != nullptr)
        {
            int n = WideCharToMultiByte (CP_UTF8, 0, src, -1, nullptr, 0, nullptr, nullptr);

            if (n > 1)
            {
                text.resize (static_cast<size_t> (n - 1));
                WideCharToMultiByte (CP_UTF8, 0, src, -1, text.data(), n, nullptr, nullptr);
            }

            GlobalUnlock (handle);
        }
    }

    CloseClipboard();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActivateKeyboard / DeactivateKeyboard
//
//  Desktop Win32 has a real keyboard. No-op.
//
////////////////////////////////////////////////////////////////////////////////

void RmlSystemInterface::ActivateKeyboard (Rml::Vector2f /*caret_position*/, float /*line_height*/)
{
}



void RmlSystemInterface::DeactivateKeyboard()
{
}
