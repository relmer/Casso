#include "Pch.h"

#include "KeyboardMapText.h"
#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  KeyboardMapText::BuildBody
//
//  Assemble the dialog body for one machine, naming only keys that machine
//  actually has.
//
//  A row about a key the hardware lacks is worse than no row: on a ][+ the up
//  arrow does nothing, and a help text promising cursor movement sends the
//  reader looking for a fault in the emulator. Marking such a row "//e and //c
//  only" was the same information wearing an apology, so the row is simply not
//  built, and the dialog sizes itself to what is left.
//
//  Everything here is something a menu cannot carry. Shortcuts are printed
//  beside their own menu items, and repeating a subset of them here once made
//  this dialog read as the complete list while holding a third of it.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DialogTextRun> KeyboardMapText::BuildBody (const Machine & machine)
{
    std::vector<DialogTextRun>  body;
    DialogTextRun               row;



    if (machine.hasAppleKeys)
    {
        // Glyph and words together. The keycaps carry only the apples, so the
        // glyph is what the reader will be hunting for on the keyboard, but
        // the two differ by fill alone and a reader who has not met them
        // cannot tell which is which from the picture. The words say which.
        row.text      = L"Left Alt";
        row.rightText = std::wstring (s_kpszOpenApple) + L" open Apple";
        body.push_back (row);

        row.text      = L"Right Alt";
        row.rightText = std::wstring (s_kpszClosedApple) + L" closed Apple";
        body.push_back (row);
    }

    // True of every model: the Apple has no separate backspace key, so the
    // host's reaches the left arrow, which is the one that erases.
    row.text = L"Backspace";  row.rightText = L"Left arrow, the Apple's backspace";
    body.push_back (row);

    if (machine.hasGamePort)
    {
        body.push_back ({ L"", false, L"" });
        body.push_back ({ L"In joystick mode:", false, L"" });

        row.text = L"Arrow keys";  row.rightText = L"Joystick, not the keyboard";
        body.push_back (row);
        row.text = L"X / Z";       row.rightText = L"Buttons 0 and 1 (either Alt fires too)";
        body.push_back (row);
    }

    return body;
}
