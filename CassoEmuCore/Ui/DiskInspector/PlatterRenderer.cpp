#include "Pch.h"

#include "Ui/DiskInspector/PlatterRenderer.h"
#include "platter.vs.h"
#include "platter.ps.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterRenderer::Initialize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PlatterRenderer::Initialize (ID3D11Device * pDevice)
{
    HRESULT                           hr        = S_OK;
    D3D11_BUFFER_DESC                 constants = {};
    D3D11_BUFFER_DESC                 rings     = {};
    D3D11_SHADER_RESOURCE_VIEW_DESC   ringsView = {};
    D3D11_RASTERIZER_DESC             raster    = {};



    CBRA (pDevice);

    m_device = pDevice;

    hr = m_device->CreateVertexShader (g_PlatterVs, sizeof (g_PlatterVs), nullptr, &m_vs);
    CHRA (hr);

    hr = m_device->CreatePixelShader (g_PlatterPs, sizeof (g_PlatterPs), nullptr, &m_ps);
    CHRA (hr);

    constants.ByteWidth      = sizeof (Constants);
    constants.Usage          = D3D11_USAGE_DYNAMIC;
    constants.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    constants.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = m_device->CreateBuffer (&constants, nullptr, &m_constants);
    CHRA (hr);

    rings.ByteWidth = sizeof (uint32_t) * kRingCount * kRingStride;
    rings.Usage     = D3D11_USAGE_DEFAULT;
    rings.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    hr = m_device->CreateBuffer (&rings, nullptr, &m_rings);
    CHRA (hr);

    ringsView.Format              = DXGI_FORMAT_R32_UINT;
    ringsView.ViewDimension       = D3D11_SRV_DIMENSION_BUFFER;
    ringsView.Buffer.NumElements  = kRingCount * kRingStride;

    hr = m_device->CreateShaderResourceView (m_rings.Get(), &ringsView, &m_ringsView);
    CHRA (hr);

    raster.FillMode        = D3D11_FILL_SOLID;
    raster.CullMode        = D3D11_CULL_NONE;
    raster.ScissorEnable   = TRUE;
    raster.DepthClipEnable = TRUE;

    hr = m_device->CreateRasterizerState (&raster, &m_raster);
    CHRA (hr);

    m_isDirty = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterRenderer::SetRing
//
////////////////////////////////////////////////////////////////////////////////

void PlatterRenderer::SetRing (int ring, PlatterRingState state, std::shared_ptr<const Levels> levels)
{
    if (ring >= 0 && ring < kRingCount)
    {
        m_ringData[ring].state  = state;
        m_ringData[ring].levels = std::move (levels);
        m_isDirty               = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterRenderer::SetPalette
//
////////////////////////////////////////////////////////////////////////////////

void PlatterRenderer::SetPalette (const DiskInspectorPalette & palette)
{
    m_palette = palette;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterRenderer::Render
//
//  Sets every pipeline state it uses: a viewport and scissor at the visible
//  part of the rectangle, no blending (the disk is opaque and the corners
//  are discarded), and its shaders, buffers and views, which it unbinds
//  after.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PlatterRenderer::Render (const DxuiCustomDrawArgs & args, const PlatterView & view)
{
    static constexpr float  kGrooveMinPx = 4.0f;



    HRESULT                     hr          = S_OK;
    ID3D11DeviceContext       * context     = args.context;
    D3D11_MAPPED_SUBRESOURCE    mapped      = {};
    Constants                   c           = {};
    D3D11_VIEWPORT              viewport    = {};
    ID3D11ShaderResourceView  * views[2]    = {};
    ID3D11ShaderResourceView  * nulls[2]    = {};
    const DiskInspectorColors & colors      = m_palette.colors;
    size_t                      i           = 0;



    CBRA (context);
    CBRA (m_vs);

    if (m_isDirty)
    {
        hr = Upload (context);
        CHRA (hr);
    }

    c.center[0]     = view.centerXPx;
    c.center[1]     = view.centerYPx;
    c.outerRadiusPx = view.outerRadiusPx;
    c.innerFraction = kInnerFraction;
    c.ringCount     = static_cast<float> (kRingCount);
    c.rotation      = view.rotation;
    c.headLimitRing = static_cast<float> (view.headLimitRing);
    c.grooveMinPx   = kGrooveMinPx;

    for (i = 0; i < m_palette.kinds.size(); i++)
    {
        ToFloat4 (m_palette.kinds[i], c.kindColors[i]);
    }

    ToFloat4 (m_palette.background,     c.grooveColor);
    ToFloat4 (colors.nothingRecorded,   c.nothingColor);
    ToFloat4 (colors.pendingPattern,    c.pendingColor);
    ToFloat4 (colors.damaged,           c.damagedColor);
    ToFloat4 (colors.damageHatch,       c.hatchColor);
    ToFloat4 (colors.beyondReach,       c.beyondColor);

    hr = context->Map (m_constants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    CHRA (hr);

    memcpy (mapped.pData, &c, sizeof (c));
    context->Unmap (m_constants.Get(), 0);

    viewport.TopLeftX = static_cast<float> (args.clipPx.left);
    viewport.TopLeftY = static_cast<float> (args.clipPx.top);
    viewport.Width    = static_cast<float> (args.clipPx.right - args.clipPx.left);
    viewport.Height   = static_cast<float> (args.clipPx.bottom - args.clipPx.top);
    viewport.MaxDepth = 1.0f;

    views[0] = m_kindsView.Get();
    views[1] = m_ringsView.Get();

    context->OMSetRenderTargets      (1, &args.target, nullptr);
    context->OMSetBlendState         (nullptr, nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState  (nullptr, 0);
    context->RSSetState              (m_raster.Get());
    context->RSSetViewports          (1, &viewport);
    context->RSSetScissorRects       (1, &args.clipPx);
    context->IASetInputLayout        (nullptr);
    context->IASetPrimitiveTopology  (D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader             (m_vs.Get(), nullptr, 0);
    context->PSSetShader             (m_ps.Get(), nullptr, 0);
    context->PSSetConstantBuffers    (0, 1, m_constants.GetAddressOf());
    context->PSSetShaderResources    (0, 2, views);

    context->Draw (3, 0);

    context->PSSetShaderResources (0, 2, nulls);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterRenderer::Upload
//
//  Lays every distinct record's levels end to end, records where each ring's
//  levels start, and uploads both. Rings that play the same record point at
//  the same texels.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PlatterRenderer::Upload (ID3D11DeviceContext * pContext)
{
    HRESULT                                hr      = S_OK;
    vector<uint32_t>                       table (kRingCount * kRingStride, 0);
    vector<Byte>                           texels;
    std::map<const Levels *, size_t>       placed;
    int                                    ring    = 0;
    size_t                                 level   = 0;
    size_t                                 start   = 0;
    UINT                                   rows    = 0;



    for (ring = 0; ring < kRingCount; ring++)
    {
        const Ring &  r    = m_ringData[ring];
        uint32_t *    row  = table.data() + ring * kRingStride;

        row[1] = static_cast<uint32_t> (r.state);

        if (r.state != PlatterRingState::Data || r.levels == nullptr || r.levels->empty())
        {
            continue;
        }

        row[0] = static_cast<uint32_t> ((*r.levels)[0].size());

        if (!placed.contains (r.levels.get()))
        {
            placed[r.levels.get()] = texels.size();

            for (const vector<Byte> & l : *r.levels)
            {
                texels.insert (texels.end(), l.begin(), l.end());
            }
        }

        start = placed[r.levels.get()];

        for (level = 0; level < r.levels->size(); level++)
        {
            row[2 + level] = static_cast<uint32_t> (start);
            start         += (*r.levels)[level].size();
        }

        for (; level < PlatterCells::kMaxLevels; level++)
        {
            row[2 + level] = row[2 + level - 1];
        }
    }

    rows = std::max<UINT> (1, static_cast<UINT> ((texels.size() + kTextureWidth - 1) / kTextureWidth));
    texels.resize (static_cast<size_t> (rows) * kTextureWidth, 0);

    hr = EnsureKinds (rows);
    CHRA (hr);

    pContext->UpdateSubresource (m_kinds.Get(), 0, nullptr, texels.data(), kTextureWidth, 0);
    pContext->UpdateSubresource (m_rings.Get(), 0, nullptr, table.data(), 0, 0);

    m_isDirty = false;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterRenderer::EnsureKinds
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PlatterRenderer::EnsureKinds (UINT rows)
{
    HRESULT                           hr   = S_OK;
    D3D11_TEXTURE2D_DESC              desc = {};



    BAIL_OUT_IF (m_kinds != nullptr && m_kindRows == rows, S_OK);

    m_kinds.Reset();
    m_kindsView.Reset();

    desc.Width            = kTextureWidth;
    desc.Height           = rows;
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    desc.Format           = DXGI_FORMAT_R8_UINT;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_DEFAULT;
    desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    hr = m_device->CreateTexture2D (&desc, nullptr, &m_kinds);
    CHRA (hr);

    hr = m_device->CreateShaderResourceView (m_kinds.Get(), nullptr, &m_kindsView);
    CHRA (hr);

    m_kindRows = rows;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterRenderer::ToFloat4
//
////////////////////////////////////////////////////////////////////////////////

void PlatterRenderer::ToFloat4 (uint32_t argb, float (&out)[4])
{
    static constexpr float  kScale = 1.0f / 255.0f;



    out[0] = ((argb >> 16) & 0xFF) * kScale;
    out[1] = ((argb >>  8) & 0xFF) * kScale;
    out[2] = ( argb        & 0xFF) * kScale;
    out[3] = ((argb >> 24) & 0xFF) * kScale;
}
