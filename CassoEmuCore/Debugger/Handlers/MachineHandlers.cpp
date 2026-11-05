#include "Pch.h"

#include "Debugger/Handlers/MachineHandlers.h"

#include "Core/TextEncoding.h"
#include "Debugger/DebugSession.h"
#include "Debugger/IMachineControl.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHandlers::TryExecute
//
////////////////////////////////////////////////////////////////////////////////

bool MachineHandlers::TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    switch (command.verb)
    {
    case DebugVerb::ResetMachine:   Reset      (session, command, reply); return true;
    case DebugVerb::PasteText:      Paste      (session, command, reply); return true;
    case DebugVerb::ShowScreen:     Screen     (session, command, reply); return true;
    case DebugVerb::SaveScreenshot: Screenshot (session, command, reply); return true;
    case DebugVerb::ExitCasso:      Exit       (session, command, reply); return true;
    default:                        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHandlers::GetControl
//
////////////////////////////////////////////////////////////////////////////////

IMachineControl * MachineHandlers::GetControl (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    IMachineControl  * control = session.GetMachineControl();



    if (control == nullptr)
    {
        reply.SetError (CommandStatus::NotAvailable, "command not available",
                        std::format ("{} requires the emulator's machine.", command.sourceName));
    }

    return control;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHandlers::Reset
//
////////////////////////////////////////////////////////////////////////////////

void MachineHandlers::Reset (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    std::string        kind    = command.text;
    IMachineControl  * control = nullptr;



    std::ranges::transform (kind, kind.begin(), [] (char c) { return (char) std::toupper ((unsigned char) c); });

    if (!kind.empty() && kind != "SOFT" && kind != "POWER")
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "RESET takes SOFT or POWER.");
        return;
    }

    control = GetControl (session, command, reply);

    if (control == nullptr)
    {
        return;
    }

    control->Reset (kind == "POWER");
    reply.data = MessageData { { (kind == "POWER") ? "Power cycling the machine." : "Resetting the machine." } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHandlers::Paste
//
////////////////////////////////////////////////////////////////////////////////

void MachineHandlers::Paste (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    std::wstring       text;
    IMachineControl  * control = nullptr;



    if (!TryUnescape (command.text, text))
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "PASTE takes \\n, \\r, \\t, \\\" and \\\\ as escapes.");
        return;
    }

    if (text.empty())
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "PASTE requires the text to type.");
        return;
    }

    control = GetControl (session, command, reply);

    if (control == nullptr)
    {
        return;
    }

    control->PasteText (text);
    reply.data = MessageData { { std::format ("Typing {} characters.", text.size()) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHandlers::TryUnescape
//
////////////////////////////////////////////////////////////////////////////////

bool MachineHandlers::TryUnescape (const std::string & text, std::wstring & outText)
{
    std::string  body     = text;
    bool         isQuoted = body.size() >= 2 && body.front() == '"' && body.back() == '"';



    outText.clear();

    if (isQuoted)
    {
        body = body.substr (1, body.size() - 2);
    }

    for (size_t i = 0; i < body.size(); i++)
    {
        if (body[i] != '\\')
        {
            outText.push_back ((wchar_t) (unsigned char) body[i]);
            continue;
        }

        if (++i >= body.size())
        {
            return false;
        }

        switch (body[i])
        {
        case 'n':
        case 'r':  outText.push_back (L'\r'); break;
        case 't':  outText.push_back (L'\t'); break;
        case '"':  outText.push_back (L'"');  break;
        case '\\': outText.push_back (L'\\'); break;
        default:   return false;
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHandlers::Screen
//
////////////////////////////////////////////////////////////////////////////////

void MachineHandlers::Screen (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    IMachineControl  * control = GetControl (session, command, reply);
    TextScreenData     screen;



    if (control == nullptr)
    {
        return;
    }

    if (!control->GetScreenText (screen.columns, screen.rows))
    {
        reply.SetError (CommandStatus::NotAvailable, "command not available", "The machine has no text screen to read.");
        return;
    }

    reply.data = std::move (screen);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHandlers::Screenshot
//
////////////////////////////////////////////////////////////////////////////////

void MachineHandlers::Screenshot (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    IMachineControl  * control = GetControl (session, command, reply);
    std::string        name    = command.text;
    SYSTEMTIME         now     = {};
    ScreenshotData     shot;
    std::wstring       path;
    HRESULT            hr      = S_OK;



    if (control == nullptr)
    {
        return;
    }

    if (name.size() >= 2 && name.front() == '"' && name.back() == '"')
    {
        name = name.substr (1, name.size() - 2);
    }

    if (name.empty())
    {
        GetLocalTime (&now);
        name = GetDefaultScreenshotName (now);
    }

    path = session.ResolvePath (name);
    hr   = control->SaveScreenshot (path, shot.width, shot.height);

    if (FAILED (hr))
    {
        reply.SetError (CommandStatus::Error, "screenshot failed", std::format ("The screenshot could not be saved to {} (0x{:08X}).", TextEncoding::WideToNarrow (path), (uint32_t) hr));
        return;
    }

    shot.path  = TextEncoding::WideToNarrow (path);
    reply.data = std::move (shot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHandlers::GetDefaultScreenshotName
//
////////////////////////////////////////////////////////////////////////////////

std::string MachineHandlers::GetDefaultScreenshotName (const SYSTEMTIME & when)
{
    return std::format ("casso-screenshot-{:04}{:02}{:02}-{:02}{:02}{:02}-{:03}.png",
                        when.wYear, when.wMonth, when.wDay, when.wHour, when.wMinute, when.wSecond, when.wMilliseconds);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHandlers::Exit
//
////////////////////////////////////////////////////////////////////////////////

void MachineHandlers::Exit (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    IMachineControl  * control = GetControl (session, command, reply);



    if (control == nullptr)
    {
        return;
    }

    control->Exit();
    reply.data = MessageData { { "Closing Casso." } };
}
