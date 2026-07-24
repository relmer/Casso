#pragma once

#include "Pch.h"
#include "Core/DxuiDpiScaler.h"




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiButtonRow
//
//  Shared bottom button-row style for DxuiDialogWindow and DxuiPropertySheet,
//  so every dialog and property sheet gets an identical Win32-style command
//  row: the primary actions (OK / Cancel / Apply / Yes / No / ...) are a
//  right-aligned group in the canonical Win32 left-to-right order, while
//  secondary / navigation actions (e.g. "Browse...") anchor bottom-left.
//
//  Metrics are the app's standard command-button sizes in DIPs (kept a touch
//  larger than the native Win32 50x14 DLU so they line up with the in-content
//  buttons like "Restore defaults"). They live in one place so the whole app
//  can be re-tuned at a stroke.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiButtonRow
{
    inline constexpr int  kButtonWidthDip  = 96;
    inline constexpr int  kButtonHeightDip = 28;
    inline constexpr int  kGapDip          =  8;      // between buttons
    inline constexpr int  kEdgePadDip      = 16;      // edge margins
    inline constexpr int  kRowHeightDip    = 44;      // reserved bottom strip

    //  Apply's command id (property sheets). Not an OS IDOK / IDCANCEL, but
    //  shared here so StandardRank can order it after Cancel.
    inline constexpr int  kApplyCommandId  = 0x2000;

    enum class Anchor { Right, Left };


    //
    //  Canonical left-to-right rank for the right-aligned group. Lower sorts
    //  further left. Unrecognized ids (e.g. a dialog's synthetic result-code
    //  command ids) get a neutral middle rank, so a *stable* sort keeps them
    //  in registration order, sitting between the affirmative buttons and
    //  Cancel -- which is exactly where an author-ordered dialog wants them.
    //
    inline int  StandardRank (int commandId)
    {
        switch (commandId)
        {
        case IDOK:            return 10;
        case IDYES:           return 20;
        case IDRETRY:         return 30;
        case IDTRYAGAIN:      return 32;
        case IDCONTINUE:      return 34;
        case IDABORT:         return 36;
        case IDNO:            return 40;
        case IDIGNORE:        return 45;
        case IDCANCEL:        return 60;
        case IDCLOSE:         return 65;
        case kApplyCommandId: return 70;
        case IDHELP:          return 80;
        default:              return 50;   // synthetic / custom -> middle
        }
    }


    //
    //  Right-align a row of buttons whose per-button DIP widths are given in
    //  final left-to-right order along the bottom edge of boundsPx, writing
    //  each button's pixel rect into outRects[0 .. widthsDip.size()).
    //
    inline void  LayoutRightGroup (const RECT           & boundsPx,
                                   const DxuiDpiScaler  & scaler,
                                   std::span<const int>   widthsDip,
                                   std::span<RECT>        outRects)
    {
        int     btnH  = scaler.Px (kButtonHeightDip);
        int     gapPx = scaler.Px (kGapDip);
        int     edge  = scaler.Px (kEdgePadDip);
        int     y     = boundsPx.bottom - edge - btnH;
        int     total = 0;
        int     x     = 0;
        size_t  i     = 0;


        for (i = 0; i < widthsDip.size(); ++i)
        {
            total += scaler.Px (widthsDip[i]);
        }
        if (!widthsDip.empty())
        {
            total += (int) (widthsDip.size() - 1) * gapPx;
        }

        x = boundsPx.right - edge - total;

        for (i = 0; i < widthsDip.size() && i < outRects.size(); ++i)
        {
            int  w = scaler.Px (widthsDip[i]);

            outRects[i] = { x, y, x + w, y + btnH };
            x += w + gapPx;
        }
    }


    //
    //  Left-align a row of buttons from the left edge along the bottom, in the
    //  given order. Used for secondary / navigation actions (e.g. "Browse...").
    //
    inline void  LayoutLeftGroup (const RECT           & boundsPx,
                                  const DxuiDpiScaler  & scaler,
                                  std::span<const int>   widthsDip,
                                  std::span<RECT>        outRects)
    {
        int     btnH  = scaler.Px (kButtonHeightDip);
        int     gapPx = scaler.Px (kGapDip);
        int     edge  = scaler.Px (kEdgePadDip);
        int     y     = boundsPx.bottom - edge - btnH;
        int     x     = boundsPx.left + edge;
        size_t  i     = 0;


        for (i = 0; i < widthsDip.size() && i < outRects.size(); ++i)
        {
            int  w = scaler.Px (widthsDip[i]);

            outRects[i] = { x, y, x + w, y + btnH };
            x += w + gapPx;
        }
    }
}
