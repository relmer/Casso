#pragma once

#include "Pch.h"
#include "Core/IDxuiLayout.h"



////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStackLayout
//
//  Arranges children in a single horizontal or vertical row. Spacing
//  between children is supplied in DIPs. Per-child weight (>0)
//  distributes the leftover space along the main axis after fixed-size
//  children (weight == 0) consume their natural size. Cross-axis
//  alignment controls how children narrower / shorter than the band
//  are positioned.
//
//  v1 treats children with weight == 0 as "fill cross-axis to band
//  width; main-axis size derived from leftover quota or zero-sized."
//  Tests pin down the exact arithmetic.
//
////////////////////////////////////////////////////////////////////////////////



class DxuiStackLayout : public IDxuiLayout
{
public:
    enum class Orientation
    {
        Horizontal,
        Vertical,
    };

    enum class Align
    {
        Start,
        Center,
        End,
        Stretch,
    };

    DxuiStackLayout (Orientation orientation,
                     float       spacingDip,
                     Align       crossAxisAlign);

    void  SetWeight (IDxuiControl * child, int weight);

    void  Arrange   (const RECT                          & boundsDip,
                     const DxuiDpiScaler                 & scaler,
                     std::span<IDxuiControl * const>       children) override;

private:
    Orientation                              m_orientation;
    float                                    m_spacingDip;
    Align                                    m_crossAxisAlign;
    std::unordered_map<IDxuiControl *, int>  m_weights;
};
