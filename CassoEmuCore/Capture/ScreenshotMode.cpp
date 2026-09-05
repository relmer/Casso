#include "Pch.h"

#include "Capture/ScreenshotMode.h"




static constexpr char    s_kTokenScene[] = "scene";
static constexpr char    s_kTokenCrt[]   = "crt";
static constexpr char    s_kTokenRaw[]   = "raw";





////////////////////////////////////////////////////////////////////////////////
//
//  Parse
//
//  Token to mode, with the default as the catch-all.
//
//  There is deliberately no failure return. The only caller is the prefs
//  loader, and every value it can hand over -- a token from an older build, a
//  token from a newer one, an empty string where the key was absent, or a
//  hand-edited typo -- resolves to a usable mode. A mode the user cannot name
//  is not worth an error path; the setting simply reads as its default and the
//  Settings page shows what it resolved to.
//
////////////////////////////////////////////////////////////////////////////////

ScreenshotMode ScreenshotModeToken::Parse (const string & token)
{
    ScreenshotMode   mode = kDefault;



    if (token == s_kTokenCrt)
    {
        mode = ScreenshotMode::Crt;
    }
    else if (token == s_kTokenRaw)
    {
        mode = ScreenshotMode::Raw;
    }

    return mode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Format
//
//  Mode to token. The switch has no default arm on purpose: adding a mode
//  without giving it a token should be a compiler warning here, not a silent
//  fall-through to "scene" that writes the wrong value into every prefs file.
//
////////////////////////////////////////////////////////////////////////////////

const char * ScreenshotModeToken::Format (ScreenshotMode mode)
{
    const char *   token = s_kTokenScene;



    switch (mode)
    {
        case ScreenshotMode::Scene:  token = s_kTokenScene;  break;
        case ScreenshotMode::Crt:    token = s_kTokenCrt;    break;
        case ScreenshotMode::Raw:    token = s_kTokenRaw;    break;
    }

    return token;
}
