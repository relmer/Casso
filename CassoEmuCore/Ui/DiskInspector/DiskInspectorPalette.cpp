#include "Pch.h"

#include "Ui/DiskInspector/DiskInspectorPalette.h"
#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorPalette::MakeFallback
//
////////////////////////////////////////////////////////////////////////////////

DiskInspectorPalette DiskInspectorPalette::MakeFallback (bool isDarkSurface)
{
    DiskInspectorPalette  palette;



    palette.isDarkSurface = isDarkSurface;
    palette.colors        = isDarkSurface ? DiskInspectorColors::MakeDark() : DiskInspectorColors::MakeLight();
    palette.background    = isDarkSurface ? DxuiTheme::Dark().panelBg  : DxuiTheme::Light().panelBg;
    palette.text          = isDarkSurface ? DxuiTheme::Dark().bodyText : DxuiTheme::Light().bodyText;

    Fill (palette);

    return palette;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorPalette::Resolve
//
//  Every color the theme leaves zero comes from the fallback for the theme's
//  surface.
//
////////////////////////////////////////////////////////////////////////////////

DiskInspectorPalette DiskInspectorPalette::Resolve (const DxuiTheme & theme)
{
    using ColorArray = std::array<uint32_t, sizeof (DiskInspectorColors) / sizeof (uint32_t)>;

    static_assert (sizeof (DiskInspectorColors) == sizeof (ColorArray));



    DiskInspectorPalette  palette  = MakeFallback (IsDarkSurface (theme.panelBg));
    ColorArray            resolved = std::bit_cast<ColorArray> (palette.colors);
    ColorArray            own      = std::bit_cast<ColorArray> (theme.diskInspector);
    size_t                i        = 0;



    for (i = 0; i < resolved.size(); i++)
    {
        if (own[i] != 0)
        {
            resolved[i] = own[i];
        }
    }

    palette.colors     = std::bit_cast<DiskInspectorColors> (resolved);
    palette.background = theme.panelBg;
    palette.text       = theme.bodyText;

    Fill (palette);

    return palette;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorPalette::IsDarkSurface
//
//  Dark when white text reads better on it than black text does.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorPalette::IsDarkSurface (uint32_t background)
{
    return GetTextColorOn (background) == 0xFFFFFFFF;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorPalette::GetTextColorOn
//
//  Black or white, whichever contrasts more with the color: one of the two
//  always reaches 4.5:1, so text drawn over a kind's color is legible in
//  every theme.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DiskInspectorPalette::GetTextColorOn (uint32_t background)
{
    static constexpr uint32_t  kBlack = 0xFF000000;
    static constexpr uint32_t  kWhite = 0xFFFFFFFF;



    return DxuiColor::ComputeContrastRatio (background, kWhite) >= DxuiColor::ComputeContrastRatio (background, kBlack) ? kWhite : kBlack;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorPalette::GetStateSymbol
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DiskInspectorPalette::GetStateSymbol (SectorState state)
{
    LPCWSTR  symbol = s_kpszCheckMark;



    switch (state)
    {
        case SectorState::Good:        symbol = s_kpszCheckMark; break;
        case SectorState::BadAddress:  symbol = s_kpszBallotX;   break;
        case SectorState::BadData:     symbol = s_kpszBallotX;   break;
        case SectorState::NotChecked:  symbol = L"?";            break;
        case SectorState::NoDataField: symbol = s_kpszEmptySet;  break;
    }

    return symbol;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorPalette::GetMarkSymbol
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DiskInspectorPalette::GetMarkSymbol (bool isAddressMark)
{
    return isAddressMark ? s_kpszBlackDiamond : s_kpszBlackCircle;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorPalette::GetTimingColor
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DiskInspectorPalette::GetTimingColor (double deviation, double range) const
{
    double  t = std::clamp (deviation / std::max (range, 1e-6), -1.0, 1.0);



    return DxuiColor::Lerp (colors.timingNominal, (t < 0) ? colors.timingFast : colors.timingSlow, static_cast<float> (std::abs (t)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorPalette::Fill
//
//  The indexed tables from the named colors.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorPalette::Fill (DiskInspectorPalette & inOut)
{
    const DiskInspectorColors &  c = inOut.colors;



    inOut.kinds = { c.sync, c.addressMark, c.addressField, c.dataMark, c.dataField,
                    c.failedChecksum, c.other, c.noise, c.randomBits, c.nothingRecorded };

    inOut.roles = { c.mapFree, c.mapBoot, c.mapVolumeHeader, c.mapDirectory, c.mapUnusedCatalog, c.mapBitmap, c.mapSubdirectory,
                    c.mapIndex, c.mapFileData, c.mapBadBlocks, c.mapUnowned, c.mapOwnedFree, c.mapCrossLinked };

    inOut.sectorStates = { c.sectorGood, c.sectorBad, c.sectorBad, c.sectorNotChecked, c.sectorNoData };
}
