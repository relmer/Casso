#pragma once

#include "Pch.h"

#include "Core/DxuiIconImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiVisualStyleCheckBox
//
//  The visual style's check box, the one File Explorer draws for an item check
//  box, rendered once for each DPI and state. The pixels are the system's, so
//  they match Explorer's in every theme.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiVisualStyleCheckBox
{
public:
    enum class State { Unchecked, Checked, CheckedDisabled };

    static const DxuiIconImage &  GetImage     (UINT dpi, State state);
    static int                    GetSizePx    (UINT dpi);

private:
    struct Entry
    {
        UINT           dpi   = 0;
        State          state = State::Unchecked;
        DxuiIconImage  image;
    };

    static HRESULT                Render       (UINT dpi, State state, DxuiIconImage & outImage);
    static int                    GetPartState (State state);
    static std::vector<Entry>  &  GetCache     ();
};
