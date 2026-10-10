#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/DiskInspectorPalette.h"
#include "Ui/DiskInspector/PlatterCells.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterRingState / PlatterPlacement
//
//  What a ring shows apart from its cells, and where the disk sits in the
//  target: its center and outer radius in target pixels, how far it has
//  turned as a fraction of a turn, and the first ring past the head's reach.
//
////////////////////////////////////////////////////////////////////////////////

enum class PlatterRingState : uint32_t
{
    Data    = 0,
    Nothing = 1,
    Pending = 2,
    Damaged = 3,
};


struct PlatterPlacement
{
    float  centerXPx     = 0.0f;
    float  centerYPx     = 0.0f;
    float  outerRadiusPx = 0.0f;
    float  rotation      = 0.0f;
    int    headLimitRing = DiskImage::kQuarterTrackCount;
};





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterRenderer
//
//  Draws the platter on the GPU through a Dxui custom draw (R19). Each ring
//  holds its cells and their coarser levels (PlatterCells); rings that play
//  the same record share one copy. Changes are uploaded at the next draw.
//
////////////////////////////////////////////////////////////////////////////////

class PlatterRenderer
{
public:
    using Levels = vector<vector<Byte>>;

    static constexpr int    kRingCount     = DiskImage::kQuarterTrackCount;
    static constexpr float  kInnerFraction = 0.42f;
    static constexpr UINT   kTextureWidth  = 4096;
    static constexpr int    kRingStride    = PlatterCells::kMaxLevels + 2;
    static constexpr UINT   kTimingFlag    = 0x100;
    static constexpr double kDefaultRange  = 0.05;

    HRESULT  Initialize (ID3D11Device * pDevice);
    void     SetRing    (int ring, PlatterRingState state, std::shared_ptr<const Levels> levels, std::shared_ptr<const Levels> timing = nullptr);
    void     SetTiming  (bool isTimingMode, double range);
    void     SetPalette (const DiskInspectorPalette & palette);
    HRESULT  Render     (const DxuiCustomDrawArgs & args, const PlatterPlacement & view);

private:
    struct Constants
    {
        float  center[2];
        float  outerRadiusPx;
        float  innerFraction;
        float  ringCount;
        float  rotation;
        float  headLimitRing;
        float  grooveMinPx;
        float  kindColors[16][4];
        float  grooveColor[4];
        float  nothingColor[4];
        float  pendingColor[4];
        float  damagedColor[4];
        float  hatchColor[4];
        float  beyondColor[4];
        float  fastColor[4];
        float  nominalColor[4];
        float  slowColor[4];
        float  timingMode;
        float  timingRange;
        float  padding[2];
    };

    struct Ring
    {
        PlatterRingState               state  = PlatterRingState::Nothing;
        std::shared_ptr<const Levels>  levels;
        std::shared_ptr<const Levels>  timing;
    };

    HRESULT  Upload      (ID3D11DeviceContext * pContext);
    HRESULT  EnsureKinds (UINT rows);
    static void  ToFloat4 (uint32_t argb, float (&out)[4]);

    ID3D11Device                      * m_device       = nullptr;   // non-owning
    ComPtr<ID3D11VertexShader>          m_vs;
    ComPtr<ID3D11PixelShader>           m_ps;
    ComPtr<ID3D11Buffer>                m_constants;
    ComPtr<ID3D11Buffer>                m_rings;
    ComPtr<ID3D11ShaderResourceView>    m_ringsView;
    ComPtr<ID3D11Texture2D>             m_kinds;
    ComPtr<ID3D11ShaderResourceView>    m_kindsView;
    ComPtr<ID3D11Texture2D>             m_timing;
    ComPtr<ID3D11ShaderResourceView>    m_timingView;
    ComPtr<ID3D11RasterizerState>       m_raster;
    UINT                                m_kindRows     = 0;
    std::array<Ring, kRingCount>        m_ringData;
    DiskInspectorPalette                m_palette      = DiskInspectorPalette::MakeFallback (true);
    bool                                m_isDirty      = true;
    bool                                m_isTimingMode = false;
    double                              m_timingRange  = kDefaultRange;
};
