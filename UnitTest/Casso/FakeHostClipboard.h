#pragma once

#include "Pch.h"

#include "Seams/IHostClipboard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostClipboard
//
//  A clipboard made of vectors: keeps what it was given, hands back what it
//  was told to, and can be made to refuse.
//
////////////////////////////////////////////////////////////////////////////////

class FakeHostClipboard : public IHostClipboard
{
public:

    std::wstring       placedText;
    std::vector<Byte>  placedDib;
    std::wstring       textToPaste;
    bool               hasText  = false;   // whether GetText has anything to give
    bool               refusing = false;   // another process holds the clipboard
    int                setCalls = 0;


    bool  SetText (HWND, const std::wstring & text) override
    {
        setCalls++;

        if (refusing)
        {
            return false;
        }

        placedText = text;

        return true;
    }


    bool  SetDib (HWND, const std::vector<Byte> & dib) override
    {
        setCalls++;

        if (refusing)
        {
            return false;
        }

        placedDib = dib;

        return true;
    }


    bool  GetText (HWND, std::wstring & outText) override
    {
        outText.clear();

        if (refusing || !hasText)
        {
            return false;
        }

        outText = textToPaste;

        return true;
    }
};
