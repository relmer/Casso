#pragma once

struct CassoTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerThemes
//
//  The themes the debugger window can use apart from the emulator's: Casso's
//  three, colors only, and the system's light and dark ones, as Casso Explorer
//  offers them. The empty name, and any name this build does not know, follows
//  the emulator's theme.
//
////////////////////////////////////////////////////////////////////////////////

class DebuggerThemes
{
public:
    static constexpr const char *  kSameAsCasso   = "";
    static constexpr const char *  kSystemLight   = "SystemLight";
    static constexpr const char *  kSystemDark    = "SystemDark";

    struct Choice
    {
        const char     * name  = nullptr;
        const wchar_t  * label = nullptr;
    };

    //  In the order the Theme drop-down lists them.
    static const std::vector<Choice> &  GetChoices ();

    //  Whether `name` is a theme other than the emulator's.
    static bool  IsKnown (const std::string & name);

    //  The tooltip colors and Visual Studio's light pane colors on the
    //  debugger's own system themes. Run after ApplySystemColors, which sets
    //  the content.
    static void  ApplyOwnColors (DxuiLightTheme & light, DxuiDarkTheme & dark);

    //  The theme `name` gives. A Casso theme is built into `own`, which the
    //  caller keeps alive for as long as it uses the result.
    static const DxuiTheme &  Choose (const std::string    & name,
                                      const DxuiTheme      & emulator,
                                      const DxuiLightTheme & light,
                                      const DxuiDarkTheme  & dark,
                                      CassoTheme           & own);
};
