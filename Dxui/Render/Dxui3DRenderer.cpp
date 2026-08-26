#include "Pch.h"

#include "Render/Dxui3DRenderer.h"




// Row-vector convention (clip = v * M) with a row_major cbuffer matrix, so
// the CPU-side float[16] goes into the constant buffer untransposed.
static const char s_kVertexShaderSrc[] =
    "cbuffer Mvp : register(b0) { row_major float4x4 mvp; };\n"
    "struct VSIn  { float3 pos : POSITION; float2 uv : TEXCOORD0; float4 col : COLOR;\n"
    "               float3 nrm : NORMAL;   float3 emi : COLOR1;   float peb : TEXCOORD1; };\n"
    "struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; float4 col : COLOR;\n"
    "               float3 nrm : NORMAL;     float3 emi : COLOR1; float3 wp : TEXCOORD1;\n"
    "               float  peb : TEXCOORD2; };\n"
    "VSOut main (VSIn input)\n"
    "{\n"
    "    VSOut output;\n"
    "    output.pos = mul (float4 (input.pos, 1.0f), mvp);\n"
    "    output.uv  = input.uv;\n"
    "    output.col = input.col;\n"
    "    output.nrm = input.nrm;\n"
    "    output.emi = input.emi;\n"
    "    output.wp  = input.pos;\n"     // pre-transform: lights live in this space
    "    output.peb = input.peb;\n"
    "    return output;\n"
    "}\n";


// Lambert per PIXEL, from the same two point lights the CPU used to bake per
// face. Three things this fixes that baking could not:
//
//   - The dot product keeps its SIGN. The old code took |dot| because
//     culling is off and a signed dot might shade a visible back face black
//     -- which made an up-facing wall and a down-facing wall identical and
//     destroyed every directional cue in the scene. Molded relief reads
//     precisely because one flank catches light and the other does not, so
//     that guard cost the thing the relief was for.
//   - The ramp rolls off instead of clamping. A hard min(1, sum) collapsed
//     every face past the threshold onto one value, so relief flattened
//     wherever two nearby point lights happened to sum over 1.
//   - Falloff is evaluated per pixel rather than per vertex.
//
// A signed dot needs no |abs| guard even with culling off: the CAD kernel
// tessellates solids with consistent OUTWARD winding, so a face the viewer
// can see already has its normal pointing at them.
//
// The view vector is a DIRECTION, not eye-minus-position. The eye sits 762 mm
// back from a 368 mm-wide monitor, so the true direction swings about 14
// degrees corner to corner -- enough to slide a highlight a little, not
// enough to justify inverting each device's world matrix to recover an eye
// position the composition never carried. Specular is what makes molded
// plastic read at all when the relief is viewed near head-on: the flanks
// foreshorten to almost nothing at this camera's 10-degree downward tilt,
// and a highlight on the rounded edge does not care about foreshortening.
static const char s_kPixelShaderSrc[] =
    "Texture2D    tex  : register(t0);\n"
    "SamplerState samp : register(s0);\n"
    "cbuffer Light : register(b1)\n"
    "{\n"
    "    float4 l0;        // xyz light 0\n"
    "    float4 l1;        // xyz light 1\n"
    "    float4 eye;       // xyz DIRECTION toward the viewer\n"
    "    float4 parm;      // x refDist, y span, z gain, w specStrength\n"
    "    float4 parm2;     // x specPower\n"
    "    float4 ambUp;     // xyz ceiling bounce\n"
    "    float4 ambDown;   // xyz desk bounce\n"
    "    float4 lampPos;   // xyz, w refDist\n"
    "    float4 lampDir;   // xyz lens facing, w range\n"
    "    float4 lampCol;   // xyz, zero disables; w = emission wrap\n"
    "    row_major float4x4 shadow0;\n"
    "    row_major float4x4 shadow1;\n"
    "    float4 shadowParm;   // x texel (0 disables), y bias, z strength\n"
    "    row_major float4x4 lampShadow;\n"
    "    float4 lampShadowParm;   // x texel (0 disables), y bias\n"
    "    float4 parm3;            // x pebble pitch (mm), y pebble amount\n"
    "};\n"
    "Texture2D              shadowTex0 : register(t1);\n"
    "Texture2D              shadowTex1 : register(t2);\n"
    "Texture2D              lampShadowTex : register(t3);\n"
    "SamplerComparisonState shadowSamp : register(s1);\n"
    "struct PSIn { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; float4 col : COLOR;\n"
    "              float3 nrm : NORMAL;      float3 emi : COLOR1;   float3 wp : TEXCOORD1;\n"
    "              float  peb : TEXCOORD2; };\n"
    // A molded-in pebble finish, computed rather than sampled.
    //
    // An INTEGER hash of the quantized position, not the usual
    // frac(sin(dot(p,k)) * 43758.5453): that one rides on transcendental
    // float precision and gives visibly different grain on different GPUs
    // and driver versions. Bit ops on integers are exact, so every machine
    // renders the same drive -- which also keeps screenshot comparisons
    // meaningful.
    //
    // Nothing here reads a clock or a frame counter. Same point, same value,
    // every frame and every run; the only thing it shares with randomness is
    // that it looks irregular.
    "uint HashCell (int3 c)\n"
    "{\n"
    "    uint h = (uint) (c.x * 374761393 + c.y * 668265263 + c.z * 1274126177);\n"
    "    h ^= h >> 13;\n"
    "    h *= 1274126177u;\n"
    "    return h ^ (h >> 16);\n"
    "}\n"
    "float CellRand (int3 c, int salt)\n"
    "{\n"
    "    return (float) (HashCell (c + int3 (salt * 7, salt * 13, salt * 29)) & 0xFFFFu)\n"
    "         * (1.0f / 65535.0f);\n"
    "}\n"
    // Smooth value noise, INTERPOLATED across the cell rather than constant
    // within it. The cell value alone is a cube of one number, so the finish
    // rendered as flat blocks a few pixels across -- digital noise, not a
    // molded grain. Reading all eight corners and easing between them turns
    // the same hash into a continuous undulation, which is what a pebbled
    // surface actually is: the mold's texture is smooth at every scale, only
    // irregular.
    //
    // The ease is the standard smoothstep weighting, so the field's slope is
    // continuous at the cell walls too -- linear weights leave a crease on
    // every boundary, which reads as a faint grid.
    "float SmoothRand (float3 p, int salt)\n"
    "{\n"
    "    int3   c = (int3) floor (p);\n"
    "    float3 f = p - (float3) c;\n"
    "    float3 w = f * f * (3.0f - 2.0f * f);\n"
    "    float  x00 = lerp (CellRand (c + int3 (0,0,0), salt), CellRand (c + int3 (1,0,0), salt), w.x);\n"
    "    float  x10 = lerp (CellRand (c + int3 (0,1,0), salt), CellRand (c + int3 (1,1,0), salt), w.x);\n"
    "    float  x01 = lerp (CellRand (c + int3 (0,0,1), salt), CellRand (c + int3 (1,0,1), salt), w.x);\n"
    "    float  x11 = lerp (CellRand (c + int3 (0,1,1), salt), CellRand (c + int3 (1,1,1), salt), w.x);\n"
    "    return lerp (lerp (x00, x10, w.y), lerp (x01, x11, w.y), w.z);\n"
    "}\n"
    "float4 main (PSIn input) : SV_TARGET\n"
    "{\n"
    "    float4 texel = tex.Sample (samp, input.uv);\n"
    "    float4 base  = texel * input.col;\n"
    "    float3 lit   = base.rgb;\n"
    "    if (dot (input.nrm, input.nrm) > 0.5f)\n"
    "    {\n"
    "        float3 n    = normalize (input.nrm);\n"
    "        if (input.peb > 0.0f)\n"
    "        {\n"
    // THREE octaves, and a CAVITY term. Tilting the normal alone cannot read
    // as depth on a near-black surface viewed head-on: a matte plastic
    // returns almost the same value however it is tilted, so the grain came
    // out flat no matter how hard the normal was pushed.
    //
    // What the eye actually reads as three-dimensional is that the pits are
    // DARKER -- less of the room reaches the bottom of a dimple than its rim.
    // So the same field that bends the normal also drives an occlusion term,
    // which is what turns a shimmer into a texture with a floor and a top.
    //
    // Three frequencies rather than two because a molded grain is not one
    // size of bump: the coarse octave gives the mottle, the middle one the
    // grain proper, and the finest keeps it from reading as cells at all.
    "            float3 p  = input.wp / parm3.x;\n"
    "            float3 j  = float3 (SmoothRand (p, 1), SmoothRand (p, 2),\n"
    "                                SmoothRand (p, 3)) * 2.0f - 1.0f;\n"
    "            float3 j2 = float3 (SmoothRand (p * 2.7f, 4), SmoothRand (p * 2.7f, 5),\n"
    "                                SmoothRand (p * 2.7f, 6)) * 2.0f - 1.0f;\n"
    "            float3 j3 = float3 (SmoothRand (p * 6.9f, 7), SmoothRand (p * 6.9f, 8),\n"
    "                                SmoothRand (p * 6.9f, 9)) * 2.0f - 1.0f;\n"
    "            n = normalize (n + (j + j2 * 0.45f + j3 * 0.22f)\n"
    "                             * (parm3.y * input.peb));\n"
    // Height at this point, from the same octaves, centered on zero. Pits go
    // negative and rims positive, so the base color is scaled by a factor
    // straddling 1 -- the surface keeps its average value rather than simply
    // going darker.
    "            float  h  = (SmoothRand (p, 1) - 0.5f)\n"
    "                      + (SmoothRand (p * 2.7f, 4) - 0.5f) * 0.45f\n"
    "                      + (SmoothRand (p * 6.9f, 7) - 0.5f) * 0.22f;\n"
    // A HORIZON TEST, which is what actually makes a bump look like a bump.
    // Cavity darkening alone says "this spot is low"; it never says "this
    // spot is low BECAUSE something beside it is in the way", so the finish
    // still read as a stain rather than as relief.
    //
    // The room's fixtures are overhead, so the occluder is whatever sits just
    // ABOVE a point -- model +Z. Sampling the height there and darkening when
    // it stands higher gives every bump a small shadow on its underside,
    // which is the cue the eye uses for depth and the one that was missing.
    //
    // Two octaves for the probe rather than three: the finest is below a
    // shadow's scale anyway, and this is the one term that costs an extra
    // pair of noise fetches.
    "            float3 up = p + float3 (0.0f, 0.0f, 0.55f);\n"
    "            float  hu = (SmoothRand (up, 1) - 0.5f)\n"
    "                      + (SmoothRand (up * 2.7f, 4) - 0.5f) * 0.45f;\n"
    "            float  occ = saturate ((hu - h) * 2.2f);\n"
    "            base.rgb *= saturate (1.0f + h * parm3.z * input.peb)\n"
    "                      * saturate (1.0f - occ * parm3.w * input.peb);\n"
    "        }\n"
    "        float3 v    = normalize (eye.xyz);\n"
    "        float  diff = 0.0f;\n"
    "        float  spec = 0.0f;\n"
    "        [unroll] for (int k = 0; k < 2; k++)\n"
    "        {\n"
    "            float3 lp = (k == 0) ? l0.xyz : l1.xyz;\n"
    "            float3 d  = lp - input.wp;\n"
    "            float  r  = max (length (d), 1e-4f);\n"
    "            float3 L  = d / r;\n"
    "            float  at = (parm.x * parm.x) / (r * r);\n"
    "            float  nl = saturate (dot (n, L));\n"
    "            float  vis = 1.0f;\n"
    // Shadow lookup. The texture fetch is written out per light rather than
    // indexed because ps_4_0 has no texture arrays and cannot take a
    // Texture2D as a parameter; the k loop is unrolled, so both ternaries
    // fold away at compile time and this costs nothing at runtime.
    "            if (shadowParm.x > 0.0f && nl > 0.0f)\n"
    "            {\n"
    "                float4 lc = (k == 0) ? mul (float4 (input.wp, 1.0f), shadow0)\n"
    "                                     : mul (float4 (input.wp, 1.0f), shadow1);\n"
    "                if (lc.w > 0.0f)\n"
    "                {\n"
    "                    float3 p  = lc.xyz / lc.w;\n"
    "                    float2 uv = float2 (p.x * 0.5f + 0.5f, 0.5f - p.y * 0.5f);\n"
    // Outside the map is LIT, never shadowed: a caster's frustum covers the
    // scene, so falling outside means nothing was in the way.
    "                    if (max (abs (p.x), abs (p.y)) <= 1.0f && p.z >= 0.0f && p.z <= 1.0f)\n"
    "                    {\n"
    // Hardware comparison filtering: each tap is already bilinear across
    // four texels, so nine of them span an effective six-by-six and the
    // edge comes out graded instead of staircased. Doing the compare by
    // hand cost the same nine fetches and gave nine hard yes-or-no answers.
    "                        float lit = 0.0f;\n"
    "                        float ref = p.z - shadowParm.y;\n"
    "                        [unroll] for (int sy = -1; sy <= 1; sy++)\n"
    "                        {\n"
    "                            [unroll] for (int sx = -1; sx <= 1; sx++)\n"
    "                            {\n"
    "                                float2 o = uv + float2 (sx, sy) * shadowParm.x * 1.5f;\n"
    "                                lit += (k == 0)\n"
    "                                     ? shadowTex0.SampleCmpLevelZero (shadowSamp, o, ref)\n"
    "                                     : shadowTex1.SampleCmpLevelZero (shadowSamp, o, ref);\n"
    "                            }\n"
    "                        }\n"
    "                        vis = lerp (1.0f, lit / 9.0f, shadowParm.z);\n"
    "                    }\n"
    "                }\n"
    "            }\n"
    "            diff += nl * at * vis;\n"
    "            if (nl > 0.0f)\n"
    "            {\n"
    "                spec += pow (saturate (dot (n, normalize (L + v))), parm2.x) * at * vis;\n"
    "            }\n"
    "        }\n"
    "        // Ambient by FACING: ceiling bounce above, desk bounce below.\n"
    "        float3 amb = lerp (ambDown.rgb, ambUp.rgb, saturate (n.z * 0.5f + 0.5f));\n"
    "        float  ramp = parm.y * (1.0f - exp (-diff * parm.z));\n"
    "        lit = base.rgb * (amb + ramp) + spec * parm.w;\n"
    // The device's own lamp, with its own occlusion. Facing the lens was once
    // taken as proof of seeing it -- "a face inside the notch points at the
    // lens and lights" -- and that is wrong wherever something stands between
    // the two. The notch floor ahead of the power button is exactly that case:
    // it points squarely at the lamp and the button blocks every ray.

    "        if (dot (lampCol.rgb, lampCol.rgb) > 0.0f)\n"
    "        {\n"
    "            float3 d  = lampPos.xyz - input.wp;\n"
    "            float  r  = length (d);\n"
    "            if (r < lampDir.w)\n"
    "            {\n"
    "                float3 L    = d / max (r, 1e-4f);\n"
    "                // -L is the direction the lamp SHINES to reach this\n"
    "                // pixel, so it is measured against the lens facing\n"
    "                // itself. Negating that facing instead aimed the cone\n"
    "                // backwards into the case: everything in front of the\n"
    "                // lens -- the notch walls, the button top, the whole\n"
    "                // point of having the lamp -- fell on the zero side of\n"
    "                // the saturate and took no light at all.\n"
    // WRAP IS THE CALLER'S, because how far past its own equator a lens throws
    // is a fact about the PART. Zero is a true hemisphere -- a flat window
    // flush in a panel, which cannot see its own plane and therefore does not
    // light it. A domed LED is not flat, does see it, and asks for more.
    //
    // One hard-coded 0.65 for every lamp is what made a flush window light the
    // frame it sits in, and a bezel thirty millimeters away facing somewhere
    // else: emission thrown past the equator in a direction the part does not
    // emit in.
    "                float  wrap = lampCol.w;\n"
    "                float  emit = saturate ((dot (lampDir.xyz, -L) + wrap)\n"
    "                                        / (1.0f + wrap));\n"
    "                float  recv = saturate (dot (n, L));\n"
    "                float  rr   = max (r, lampPos.w);\n"
    "                float  fade = saturate (1.0f - r / lampDir.w);\n"
    "                float  lvis = 1.0f;\n"
    "                if (lampShadowParm.x > 0.0f && emit * recv > 0.0f)\n"
    "                {\n"
    "                    float4 lc = mul (float4 (input.wp, 1.0f), lampShadow);\n"
    "                    if (lc.w > 0.0f)\n"
    "                    {\n"
    "                        float3 p  = lc.xyz / lc.w;\n"
    "                        float2 uv = float2 (p.x * 0.5f + 0.5f, 0.5f - p.y * 0.5f);\n"
    "                        if (max (abs (p.x), abs (p.y)) <= 1.0f && p.z >= 0.0f && p.z <= 1.0f)\n"
    "                        {\n"
    "                            float acc = 0.0f;\n"
    "                            float ref = p.z - lampShadowParm.y;\n"
    "                            [unroll] for (int ly = -1; ly <= 1; ly++)\n"
    "                            {\n"
    "                                [unroll] for (int lx = -1; lx <= 1; lx++)\n"
    "                                {\n"
    "                                    float2 o = uv + float2 (lx, ly) * lampShadowParm.x;\n"
    "                                    acc += lampShadowTex.SampleCmpLevelZero (shadowSamp, o, ref);\n"
    "                                }\n"
    "                            }\n"
    "                            lvis = acc / 9.0f;\n"
    "                        }\n"
    "                    }\n"
    "                }\n"
    "                lit += base.rgb * lampCol.rgb * emit * recv * fade * lvis *\n"
    "                       (lampPos.w * lampPos.w) / (rr * rr);\n"
    "            }\n"
    "        }\n"
    "    }\n"
    "    return float4 (lit + input.emi, base.a);\n"
    "}\n";





////////////////////////////////////////////////////////////////////////////////
//
//  ~Dxui3DRenderer
//
////////////////////////////////////////////////////////////////////////////////

Dxui3DRenderer::~Dxui3DRenderer()
{
    Shutdown();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
//  Brings up the 3D renderer against a borrowed device and context: shaders,
//  pipeline state, and the 1x1 white texture.
//
//  That white texture is what lets ONE shader pair serve both textured and
//  untextured geometry. Untextured draws sample it, so the sample is always
//  opaque white and the vertex tint alone becomes the surface color -- no
//  second shader, no branch in the pixel shader, and no state swap between
//  the two cases.
//
//  It is created IMMUTABLE, since a single white pixel never changes and the
//  driver is free to place it wherever it likes.
//
//  A failure anywhere calls Shutdown before returning, so a half-built
//  renderer is never left for a caller to discover -- the object is either
//  fully usable or fully empty.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dxui3DRenderer::Initialize (ID3D11Device * device, ID3D11DeviceContext * context)
{
    HRESULT   hr = S_OK;



    CBREx (device != nullptr && context != nullptr, E_INVALIDARG);

    m_device  = device;
    m_context = context;

    hr = CreateShaders();
    CHR (hr);

    hr = CreatePipelineState();
    CHR (hr);

    // 1x1 opaque white: untextured geometry samples it so the vertex tint IS
    // the surface color, and the one shader pair covers both cases.
    {
        uint32_t                 white   = 0xFFFFFFFFu;
        D3D11_TEXTURE2D_DESC     desc    = {};
        D3D11_SUBRESOURCE_DATA   initial = {};

        desc.Width            = 1;
        desc.Height           = 1;
        desc.MipLevels        = 1;
        desc.ArraySize        = 1;
        desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage            = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

        initial.pSysMem     = &white;
        initial.SysMemPitch = sizeof (white);

        hr = m_device->CreateTexture2D (&desc, &initial, m_whiteTex.GetAddressOf());
        CHR (hr);

        hr = m_device->CreateShaderResourceView (m_whiteTex.Get(), nullptr, m_whiteSrv.GetAddressOf());
        CHR (hr);
    }

Error:
    if (FAILED (hr))
    {
        Shutdown();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Shutdown
//
//  Releases every GPU resource and returns the renderer to its pre-Initialize
//  state.
//
//  The cached SIZES are cleared alongside the resources, and that is the part
//  that matters. This object is reused across a device reset, and the growth
//  checks in UpdateContentTexture and the depth-buffer path compare against
//  those cached dimensions -- leaving them set would let a later frame decide
//  a texture it no longer owns is already big enough.
//
//  Device and context are borrowed, not owned, so they are dropped rather than
//  released.
//
//  Safe to call on a partially built renderer, which is what lets Initialize
//  use it as its own failure path.
//
////////////////////////////////////////////////////////////////////////////////

void Dxui3DRenderer::Shutdown()
{
    m_vs.Reset();
    m_ps.Reset();
    m_layout.Reset();
    m_vertexBuffer.Reset();
    m_mvpBuffer.Reset();
    m_lightBuffer.Reset();
    m_shadowDepthState.Reset();
    m_shadowRasterState.Reset();
    m_shadowSampler.Reset();
    m_shadowSavedRtv.Reset();
    m_shadowSavedDsv.Reset();
    for (int i = 0; i < kShadowMaps; i++)
    {
        m_shadow[i].tex.Reset();
        m_shadow[i].dsv.Reset();
        m_shadow[i].srv.Reset();
        m_shadow[i].size = 0;
    }

    m_shadowSlot = -1;
    m_blendState.Reset();
    m_rasterState.Reset();
    m_depthState.Reset();
    m_depthStateTest.Reset();
    m_sampler.Reset();
    m_contentTex.Reset();
    m_contentSrv.Reset();
    m_whiteTex.Reset();
    m_whiteSrv.Reset();
    m_depthTex.Reset();
    m_depthDsv.Reset();
    m_msaaTex.Reset();
    m_msaaRtv.Reset();
    m_resolveTex.Reset();
    m_resolveSrv.Reset();
    m_savedRtv.Reset();

    m_msaaWidth            = 0;
    m_msaaHeight           = 0;
    m_inMsaaScene          = false;
    m_depthSamples         = 0;
    m_vertexBufferCapacity = 0;
    m_contentWidth         = 0;
    m_contentHeight        = 0;
    m_depthWidth           = 0;
    m_depthHeight          = 0;
    m_externalSrv          = nullptr;
    m_device               = nullptr;
    m_context              = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateShaders
//
//  Compiles the single vertex / pixel shader pair and builds the input layout
//  that matches it.
//
//  One pair covers everything this renderer draws. The vertex format carries
//  position, texture coordinate, and per-vertex color, and the pixel shader
//  simply multiplies the sample by the color -- which is why untextured
//  geometry can sample the 1x1 white texture and get its vertex tint back
//  unchanged, with no second shader to maintain.
//
//  Shader source is embedded as string literals rather than as resources,
//  because these two are small and belong with the vertex struct they have to
//  agree with.
//
//  The input layout is validated against the compiled VERTEX SHADER BLOB, so a
//  mismatch between the struct and the shader's expected signature fails here
//  at startup rather than as garbage geometry at draw time.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dxui3DRenderer::CreateShaders()
{
    HRESULT            hr      = S_OK;
    ComPtr<ID3DBlob>   vsBlob;
    ComPtr<ID3DBlob>   psBlob;
    ComPtr<ID3DBlob>   errors;



    D3D11_INPUT_ELEMENT_DESC   layout[] =
    {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",     0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",     1, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  1, DXGI_FORMAT_R32_FLOAT,          0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = D3DCompile (s_kVertexShaderSrc, sizeof (s_kVertexShaderSrc) - 1,
                     nullptr, nullptr, nullptr, "main", "vs_4_0",
                     0, 0, vsBlob.GetAddressOf(), errors.GetAddressOf());
    CHR (hr);

    hr = D3DCompile (s_kPixelShaderSrc, sizeof (s_kPixelShaderSrc) - 1,
                     nullptr, nullptr, nullptr, "main", "ps_4_0",
                     0, 0, psBlob.GetAddressOf(), errors.ReleaseAndGetAddressOf());
    CHR (hr);

    hr = m_device->CreateVertexShader (vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                       nullptr, m_vs.GetAddressOf());
    CHR (hr);

    hr = m_device->CreatePixelShader (psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                                      nullptr, m_ps.GetAddressOf());
    CHR (hr);

    hr = m_device->CreateInputLayout (layout, ARRAYSIZE (layout),
                                      vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                      m_layout.GetAddressOf());
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreatePipelineState
//
//  Builds the fixed-function state objects: blend, rasterizer, the two depth
//  modes, the sampler, and the MVP constant buffer.
//
//  Blending is PREMULTIPLIED source-over, chosen to match DxuiPainter exactly.
//  3D scene pixels layer into the same surface as the panel-tree paint, and a
//  different blend equation would make a translucent scene composite
//  differently from every other translucent thing in the UI.
//
//  Culling is off. The paper curl's far side is legitimately visible from
//  behind, so back-face culling would punch holes in it, and these meshes are
//  far too small for culling to be worth anything.
//
//  Two depth modes exist because there are two kinds of caller. Hand-built
//  batches arrive already ordered back to front and want depth OFF, painter's-
//  algorithm style; loaded meshes arrive in arbitrary triangle order and need
//  a real LESS test with depth writes. Neither mode works for the other case,
//  which is why both are created up front and chosen per draw.
//
//  The sampler clamps so a UV that reaches past an edge repeats the edge
//  texel instead of wrapping content in from the opposite side.
//
//  The MVP buffer is DYNAMIC: it is rewritten per draw, since each batch
//  carries its own transform.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dxui3DRenderer::CreatePipelineState()
{
    HRESULT   hr = S_OK;



    // Premultiplied source-over -- identical compositing to DxuiPainter, so
    // scene pixels layer with the panel-tree paint without surprises.
    {
        D3D11_BLEND_DESC   blend = {};

        blend.RenderTarget[0].BlendEnable           = TRUE;
        blend.RenderTarget[0].SrcBlend              = D3D11_BLEND_ONE;
        blend.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
        blend.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
        blend.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
        blend.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA;
        blend.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        hr = m_device->CreateBlendState (&blend, m_blendState.GetAddressOf());
        CHR (hr);
    }

    // No cull: the paper curl's far side is legitimately visible from behind,
    // and the meshes are far too small for culling to matter.
    {
        D3D11_RASTERIZER_DESC   raster = {};

        raster.FillMode        = D3D11_FILL_SOLID;
        raster.CullMode        = D3D11_CULL_NONE;
        raster.DepthClipEnable = TRUE;

        hr = m_device->CreateRasterizerState (&raster, m_rasterState.GetAddressOf());
        CHR (hr);

        // The same state with the scissor honored, for callers that must keep
        // a scene inside a sub-rect of the target -- the settings preview
        // draws under a dropdown that is allowed to cover it. A viewport
        // cannot do this job: shrinking it would re-map the projection and
        // squash the scene rather than crop it.
        raster.ScissorEnable = TRUE;

        hr = m_device->CreateRasterizerState (&raster, m_rasterStateScissor.GetAddressOf());
        CHR (hr);
    }

    // Two depth modes: off for hand-ordered painter's-algorithm batches, and
    // LESS test+write for loaded meshes whose triangles arrive unordered.
    {
        D3D11_DEPTH_STENCIL_DESC   depth = {};

        depth.DepthEnable = FALSE;

        hr = m_device->CreateDepthStencilState (&depth, m_depthState.GetAddressOf());
        CHR (hr);

        // LESS_EQUAL, not LESS, and deliberately: the desk scene re-draws the
        // opaque bodies a second time to occlude its live picture layer (see
        // DeskScene::RenderPlate), and a re-draw of the same triangles at the
        // same transform produces the same depths -- which under LESS all
        // fail, and the second pass silently draws nothing.
        depth.DepthEnable    = TRUE;
        depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        depth.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;

        hr = m_device->CreateDepthStencilState (&depth, m_depthStateTest.GetAddressOf());
        CHR (hr);

        // Read-only depth, for translucent light: it must be HIDDEN by solids
        // standing in front of it -- a glow inside a recess cannot spill over
        // the housing around it -- while leaving its own transparent rim out
        // of the depth buffer, where it would punch a hole in whatever draws
        // next.
        depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;

        hr = m_device->CreateDepthStencilState (&depth, m_depthStateReadOnly.GetAddressOf());
        CHR (hr);
    }

    {
        D3D11_SAMPLER_DESC   samp = {};

        samp.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samp.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
        samp.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
        samp.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
        samp.MaxLOD         = D3D11_FLOAT32_MAX;

        hr = m_device->CreateSamplerState (&samp, m_sampler.GetAddressOf());
        CHR (hr);
    }

    // Shadow map state. BORDER addressing with a white border so a lookup
    // that lands off the map reads "nothing in the way" -- clamping instead
    // smears the edge texel across everything outside and paints phantom
    // shadows over the whole scene.
    {
        D3D11_SAMPLER_DESC        samp   = {};
        D3D11_RASTERIZER_DESC     raster = {};
        D3D11_DEPTH_STENCIL_DESC  depth  = {};

        samp.Filter         = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        samp.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
        samp.AddressU       = D3D11_TEXTURE_ADDRESS_BORDER;
        samp.AddressV       = D3D11_TEXTURE_ADDRESS_BORDER;
        samp.AddressW       = D3D11_TEXTURE_ADDRESS_BORDER;
        samp.BorderColor[0] = 1.0f;
        samp.BorderColor[1] = 1.0f;
        samp.BorderColor[2] = 1.0f;
        samp.BorderColor[3] = 1.0f;
        samp.MaxLOD         = D3D11_FLOAT32_MAX;

        hr = m_device->CreateSamplerState (&samp, m_shadowSampler.GetAddressOf());
        CHR (hr);

        // Front faces culled while filling the map. The scene draws with no
        // culling and its relief is millimetres proud on a case a third of a
        // metre across, so a depth written by the very surface being shaded
        // is the whole acne problem; recording the BACK faces moves the
        // recorded depth behind the lit surface and takes most of it away
        // before any bias is asked to.
        raster.FillMode              = D3D11_FILL_SOLID;
        raster.CullMode              = D3D11_CULL_FRONT;
        raster.DepthClipEnable       = TRUE;
        raster.DepthBias             = 24;
        raster.SlopeScaledDepthBias  = 2.0f;

        hr = m_device->CreateRasterizerState (&raster, m_shadowRasterState.GetAddressOf());
        CHR (hr);

        depth.DepthEnable    = TRUE;
        depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        depth.DepthFunc      = D3D11_COMPARISON_LESS;

        hr = m_device->CreateDepthStencilState (&depth, m_shadowDepthState.GetAddressOf());
        CHR (hr);
    }

    {
        D3D11_BUFFER_DESC   cb = {};

        cb.ByteWidth      = 16 * sizeof (float);
        cb.Usage          = D3D11_USAGE_DYNAMIC;
        cb.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        cb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        hr = m_device->CreateBuffer (&cb, nullptr, m_mvpBuffer.GetAddressOf());
        CHR (hr);

        // Lighting, updated per draw alongside the mvp: the desk scene keeps
        // every device in its own model space, so the light positions change
        // with each device rather than once per frame -- and so do the two
        // shadow matrices, which carry that device's placement.
        cb.ByteWidth = 25 * 4 * sizeof (float);

        hr = m_device->CreateBuffer (&cb, nullptr, m_lightBuffer.GetAddressOf());
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureVertexBuffer
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dxui3DRenderer::EnsureVertexBuffer (size_t requiredVerts)
{
    HRESULT             hr   = S_OK;
    D3D11_BUFFER_DESC   desc = {};



    BAIL_OUT_IF (requiredVerts <= m_vertexBufferCapacity && m_vertexBuffer != nullptr, S_OK);

    m_vertexBuffer.Reset();

    desc.ByteWidth      = (UINT) (requiredVerts * sizeof (Vertex));
    desc.Usage          = D3D11_USAGE_DYNAMIC;
    desc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = m_device->CreateBuffer (&desc, nullptr, m_vertexBuffer.GetAddressOf());
    CHR (hr);

    m_vertexBufferCapacity = requiredVerts;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateContentTexture
//
//  Uploads a BGRA image for textured geometry to sample, recreating the
//  texture only when the dimensions change.
//
//  The size test is what makes this cheap to call every frame: a page preview
//  that keeps its dimensions re-uses the same texture and pays only the
//  upload, while a resize gets a fresh one.
//
//  DYNAMIC usage with WRITE_DISCARD is the right pairing for per-frame
//  replacement -- discard tells the driver the previous contents are dead, so
//  it can hand back a fresh buffer instead of stalling until the GPU finishes
//  reading the old one.
//
//  Rows are copied ONE AT A TIME against the mapped RowPitch rather than as a
//  single memcpy. The driver's pitch is its own choice and is routinely larger
//  than width * 4 for alignment, so a flat copy would skew the image
//  progressively down the texture.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dxui3DRenderer::UpdateContentTexture (const uint32_t * bgra, int width, int height)
{
    HRESULT                    hr     = S_OK;
    D3D11_MAPPED_SUBRESOURCE   mapped = {};



    CBREx (m_device != nullptr, E_UNEXPECTED);
    CBREx (bgra != nullptr && width > 0 && height > 0, E_INVALIDARG);

    if (width != m_contentWidth || height != m_contentHeight || m_contentTex == nullptr)
    {
        D3D11_TEXTURE2D_DESC   desc = {};

        m_contentTex.Reset();
        m_contentSrv.Reset();

        desc.Width            = (UINT) width;
        desc.Height           = (UINT) height;
        desc.MipLevels        = 1;
        desc.ArraySize        = 1;
        desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage            = D3D11_USAGE_DYNAMIC;
        desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags   = D3D11_CPU_ACCESS_WRITE;

        hr = m_device->CreateTexture2D (&desc, nullptr, m_contentTex.GetAddressOf());
        CHR (hr);

        hr = m_device->CreateShaderResourceView (m_contentTex.Get(), nullptr, m_contentSrv.GetAddressOf());
        CHR (hr);

        m_contentWidth  = width;
        m_contentHeight = height;
    }

    hr = m_context->Map (m_contentTex.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    CHR (hr);

    for (int y = 0; y < height; y++)
    {
        memcpy ((uint8_t *) mapped.pData + (size_t) y * mapped.RowPitch,
                bgra + (size_t) y * width,
                (size_t) width * sizeof (uint32_t));
    }

    m_context->Unmap (m_contentTex.Get(), 0);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeginDepthPass
//
//  Sizes (or re-sizes) the depth buffer to the render target that is bound
//  RIGHT NOW, and clears it for this frame's depth-tested draws. Queried
//  rather than passed in so the caller (a before-present hook) needs no
//  knowledge of the host's back-buffer plumbing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dxui3DRenderer::BeginDepthPass()
{
    HRESULT                         hr   = S_OK;
    ComPtr<ID3D11RenderTargetView>  rtv;
    ComPtr<ID3D11Resource>          res;
    ComPtr<ID3D11Texture2D>         tex;
    D3D11_TEXTURE2D_DESC            desc = {};



    CBREx (m_device != nullptr, E_UNEXPECTED);

    m_context->OMGetRenderTargets (1, rtv.GetAddressOf(), nullptr);
    CBREx (rtv != nullptr, E_UNEXPECTED);

    rtv->GetResource (res.GetAddressOf());
    hr = res.As (&tex);
    CHR (hr);

    tex->GetDesc (&desc);

    // Sample count too: depth must match the bound target's, and during the
    // multisampled scene pass that target is the MSAA one. A single-sample
    // depth buffer against a multisampled color target is invalid, and the
    // whole scene silently stops drawing.
    if ((int) desc.Width != m_depthWidth || (int) desc.Height != m_depthHeight ||
        desc.SampleDesc.Count != m_depthSamples || m_depthDsv == nullptr)
    {
        D3D11_TEXTURE2D_DESC   depthDesc = {};

        m_depthTex.Reset();
        m_depthDsv.Reset();

        depthDesc.Width              = desc.Width;
        depthDesc.Height             = desc.Height;
        depthDesc.MipLevels          = 1;
        depthDesc.ArraySize          = 1;
        depthDesc.Format             = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.SampleDesc.Count   = desc.SampleDesc.Count;
        depthDesc.SampleDesc.Quality = desc.SampleDesc.Quality;
        depthDesc.Usage              = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags          = D3D11_BIND_DEPTH_STENCIL;

        hr = m_device->CreateTexture2D (&depthDesc, nullptr, m_depthTex.GetAddressOf());
        CHR (hr);

        hr = m_device->CreateDepthStencilView (m_depthTex.Get(), nullptr, m_depthDsv.GetAddressOf());
        CHR (hr);

        m_depthWidth   = (int) desc.Width;
        m_depthHeight  = (int) desc.Height;
        m_depthSamples = desc.SampleDesc.Count;
    }

    m_context->ClearDepthStencilView (m_depthDsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureShadowMap
//
//  A typeless depth texture, because the same surface is written as a DSV in
//  the shadow pass and read as an SRV in the color pass -- one format cannot
//  do both, so it is declared without one and each view names its own.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dxui3DRenderer::EnsureShadowMap (int slot, UINT texels)
{
    HRESULT                          hr   = S_OK;
    D3D11_TEXTURE2D_DESC             desc = {};
    D3D11_DEPTH_STENCIL_VIEW_DESC    dsv  = {};
    D3D11_SHADER_RESOURCE_VIEW_DESC  srv  = {};



    CBREx (slot >= 0 && slot < kShadowMaps, E_INVALIDARG);

    if (m_shadow[slot].dsv != nullptr && m_shadow[slot].size == texels)
    {
        return S_OK;
    }

    m_shadow[slot].tex.Reset();
    m_shadow[slot].dsv.Reset();
    m_shadow[slot].srv.Reset();

    desc.Width            = texels;
    desc.Height           = texels;
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    // 16 bits, not 32. The frustum spans about a metre of desk, so a texel of
    // depth is well under a hundredth of a millimetre either way -- and the
    // map is square, so halving the format is what pays for doubling the
    // edge, which is the axis that actually shows.
    desc.Format           = DXGI_FORMAT_R16_TYPELESS;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_DEFAULT;
    desc.BindFlags        = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    hr = m_device->CreateTexture2D (&desc, nullptr, m_shadow[slot].tex.GetAddressOf());
    CHR (hr);

    dsv.Format        = DXGI_FORMAT_D16_UNORM;
    dsv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

    hr = m_device->CreateDepthStencilView (m_shadow[slot].tex.Get(), &dsv,
                                           m_shadow[slot].dsv.GetAddressOf());
    CHR (hr);

    srv.Format                    = DXGI_FORMAT_R16_UNORM;
    srv.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv.Texture2D.MipLevels       = 1;

    hr = m_device->CreateShaderResourceView (m_shadow[slot].tex.Get(), &srv,
                                              m_shadow[slot].srv.GetAddressOf());
    CHR (hr);

    m_shadow[slot].size = texels;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeginShadowPass / EndShadowPass
//
//  The caller's render target is set aside and NO color target is bound while
//  the map fills: a depth-only pass is the cheapest thing this pipeline can
//  do, and binding a color target it would not write is what makes shadow
//  passes cost more than they should.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dxui3DRenderer::BeginShadowPass (int slot, UINT texels)
{
    HRESULT                   hr        = S_OK;
    ID3D11RenderTargetView *  noRtv     = nullptr;



    CBREx (m_device != nullptr && m_context != nullptr, E_UNEXPECTED);
    CBREx (slot >= 0 && slot < kShadowMaps, E_INVALIDARG);
    CBREx (m_shadowSlot < 0, E_UNEXPECTED);

    hr = EnsureShadowMap (slot, (texels < 256) ? 256 : texels);
    CHR (hr);

    m_shadowSavedRtv.Reset();
    m_shadowSavedDsv.Reset();
    m_context->OMGetRenderTargets (1, m_shadowSavedRtv.GetAddressOf(),
                                       m_shadowSavedDsv.GetAddressOf());

    m_context->OMSetRenderTargets    (1, &noRtv, m_shadow[slot].dsv.Get());
    m_context->ClearDepthStencilView (m_shadow[slot].dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    m_shadowSlot = slot;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EndShadowPass
//
////////////////////////////////////////////////////////////////////////////////

void Dxui3DRenderer::EndShadowPass()
{
    ID3D11RenderTargetView *  rtv = nullptr;



    if (m_shadowSlot < 0)
    {
        return;
    }

    rtv = m_shadowSavedRtv.Get();
    m_context->OMSetRenderTargets (1, &rtv, m_shadowSavedDsv.Get());

    m_shadowSavedRtv.Reset();
    m_shadowSavedDsv.Reset();
    m_shadowSlot = -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssueShadowDraw
//
//  The depth-only half of IssueDraw: no color target, no pixel shader, and the
//  light's own square viewport rather than the caller's.
//
////////////////////////////////////////////////////////////////////////////////

void Dxui3DRenderer::IssueShadowDraw (ID3D11Buffer * vertexBuffer, size_t vertexCount)
{
    D3D11_VIEWPORT  vp     = {};
    UINT            stride = sizeof (Vertex);
    UINT            offset = 0;



    vp.Width    = (float) m_shadow[m_shadowSlot].size;
    vp.Height   = (float) m_shadow[m_shadowSlot].size;
    vp.MaxDepth = 1.0f;

    m_context->RSSetViewports         (1, &vp);
    m_context->OMSetDepthStencilState (m_shadowDepthState.Get(), 0);
    m_context->RSSetState             (m_shadowRasterState.Get());

    m_context->IASetInputLayout       (m_layout.Get());
    m_context->IASetPrimitiveTopology (D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->IASetVertexBuffers     (0, 1, &vertexBuffer, &stride, &offset);

    m_context->VSSetShader            (m_vs.Get(), nullptr, 0);
    m_context->VSSetConstantBuffers   (0, 1, m_mvpBuffer.GetAddressOf());
    m_context->PSSetShader            (nullptr, nullptr, 0);

    m_context->Draw ((UINT) vertexCount, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetScissor
//
////////////////////////////////////////////////////////////////////////////////

void Dxui3DRenderer::SetScissor (const RECT * rect)
{
    m_hasScissor = (rect != nullptr) && (rect->right > rect->left) && (rect->bottom > rect->top);

    if (m_hasScissor)
    {
        m_scissor = *rect;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSceneSampleCount
//
//  Drops the cached multisampled targets so the next BeginMultisampledScene
//  rebuilds them at the new count. Not applied mid-scene: the detour in
//  flight still has to resolve out of the texture it started in, and the
//  next frame is soon enough for a settings change.
//
////////////////////////////////////////////////////////////////////////////////

void Dxui3DRenderer::SetSceneSampleCount (UINT samples)
{
    UINT   wanted = (samples >= 4) ? 4 : ((samples >= 2) ? 2 : 1);



    if (wanted == m_sampleCount || m_inMsaaScene)
    {
        return;
    }

    m_sampleCount = wanted;

    m_msaaTex.Reset();
    m_msaaRtv.Reset();
    m_resolveTex.Reset();
    m_resolveSrv.Reset();
    m_msaaWidth  = 0;
    m_msaaHeight = 0;

    // The depth buffer's sample count has to match the target it is bound
    // with, so it is stale now too. BeginDepthPass would notice by itself --
    // it compares against the bound target every pass -- but dropping it here
    // keeps "no target outlives a count change" true at the one place that
    // changes the count.
    m_depthTex.Reset();
    m_depthDsv.Reset();
    m_depthSamples = 0;
    m_depthWidth   = 0;
    m_depthHeight  = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeginMultisampledScene
//
//  Points the scene at an offscreen multisampled target. Flip-model swap
//  chains cannot themselves be multisampled -- DXGI pins SampleDesc.Count to
//  1 on them -- so antialiasing means rendering elsewhere and resolving.
//
//  The target is cleared TRANSPARENT rather than seeded with a copy of the
//  destination. Premultiplied source-over compositing is associative, so
//  layering the scene onto nothing and then compositing that layer over the
//  destination lands on exactly the pixels drawing directly would have.
//
//  Does nothing when the device will not multisample at this count, leaving
//  the scene to draw unantialiased rather than not at all.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dxui3DRenderer::BeginMultisampledScene()
{
    HRESULT                          hr       = S_OK;
    UINT                             quality  = 0;
    int                              width    = 0;
    int                              height   = 0;
    float                            clear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    D3D11_TEXTURE2D_DESC             dstDesc  = {};
    ComPtr<ID3D11RenderTargetView>   rtv;
    ComPtr<ID3D11Resource>           res;
    ComPtr<ID3D11Texture2D>          tex;



    CBREx (m_device != nullptr, E_UNEXPECTED);

    BAIL_OUT_IF (m_sampleCount <= 1 || m_inMsaaScene, S_OK);

    hr = m_device->CheckMultisampleQualityLevels (DXGI_FORMAT_B8G8R8A8_UNORM,
                                                  m_sampleCount, &quality);

    // Not an error, just a device that will not do it: draw unantialiased.
    BAIL_OUT_IF (FAILED (hr) || quality == 0, S_OK);

    // Size from the destination the caller just bound.
    m_context->OMGetRenderTargets (1, rtv.GetAddressOf(), nullptr);
    CBREx (rtv != nullptr, E_UNEXPECTED);

    rtv->GetResource (res.GetAddressOf());
    hr = res.As (&tex);
    CHR (hr);

    tex->GetDesc (&dstDesc);
    width  = (int) dstDesc.Width;
    height = (int) dstDesc.Height;

    CBREx (width > 0 && height > 0, E_UNEXPECTED);

    if (width != m_msaaWidth || height != m_msaaHeight || m_msaaRtv == nullptr)
    {
        D3D11_TEXTURE2D_DESC   desc = {};

        m_msaaTex.Reset();
        m_msaaRtv.Reset();
        m_resolveTex.Reset();
        m_resolveSrv.Reset();

        desc.Width              = (UINT) width;
        desc.Height             = (UINT) height;
        desc.MipLevels          = 1;
        desc.ArraySize          = 1;
        desc.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count   = m_sampleCount;
        desc.SampleDesc.Quality = quality - 1;
        desc.Usage              = D3D11_USAGE_DEFAULT;
        desc.BindFlags          = D3D11_BIND_RENDER_TARGET;

        hr = m_device->CreateTexture2D (&desc, nullptr, m_msaaTex.GetAddressOf());
        CHR (hr);

        hr = m_device->CreateRenderTargetView (m_msaaTex.Get(), nullptr, m_msaaRtv.GetAddressOf());
        CHR (hr);

        desc.SampleDesc.Count   = 1;
        desc.SampleDesc.Quality = 0;
        desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;

        hr = m_device->CreateTexture2D (&desc, nullptr, m_resolveTex.GetAddressOf());
        CHR (hr);

        hr = m_device->CreateShaderResourceView (m_resolveTex.Get(), nullptr,
                                                 m_resolveSrv.GetAddressOf());
        CHR (hr);

        m_msaaWidth  = width;
        m_msaaHeight = height;
    }

    // Remember where the finished layer goes back to.
    m_savedRtv = rtv;

    m_context->ClearRenderTargetView (m_msaaRtv.Get(), clear);
    m_context->OMSetRenderTargets    (1, m_msaaRtv.GetAddressOf(), nullptr);

    m_inMsaaScene = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EndMultisampledScene
//
//  Resolves the multisampled scene and composites it over the target the
//  caller had bound, as one textured quad in clip space with depth off.
//  Harmless when Begin declined.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dxui3DRenderer::EndMultisampledScene()
{
    HRESULT                  hr     = S_OK;
    ID3D11RenderTargetView * rawRtv = nullptr;



    BAIL_OUT_IF (!m_inMsaaScene, S_OK);

    m_inMsaaScene = false;

    m_context->ResolveSubresource (m_resolveTex.Get(), 0, m_msaaTex.Get(), 0,
                                   DXGI_FORMAT_B8G8R8A8_UNORM);

    rawRtv = m_savedRtv.Get();
    m_context->OMSetRenderTargets (1, &rawRtv, nullptr);

    hr = CompositeFullTarget (m_resolveSrv.Get(), m_msaaWidth, m_msaaHeight);

    m_savedRtv.Reset();
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CompositeFullTarget
//
//  Lay a premultiplied texture over the whole bound target as one quad.
//
//  The WHOLE target, never a scene viewport. The source is target-sized and
//  the quad's UVs span all of it, so mapping it through a sub-rect viewport
//  would squeeze the entire image into that rect -- scaling the scene down a
//  second time and shifting it off its own geometry. It cost a few pixels of
//  drift, and made a resize flicker as the mismatch changed with every size.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dxui3DRenderer::CompositeFullTarget (ID3D11ShaderResourceView * srv, int width, int height)
{
    HRESULT                    hr           = S_OK;
    ID3D11ShaderResourceView * saved        = m_externalSrv;
    Vertex                     quad[6]      = {};
    D3D11_VIEWPORT             full         = {};
    float                      identity[16] = { 1, 0, 0, 0,
                                                0, 1, 0, 0,
                                                0, 0, 1, 0,
                                                0, 0, 0, 1 };



    CBREx (srv != nullptr && width > 0 && height > 0, E_INVALIDARG);

    // Tinted opaque white so the shader's tex*col passes the premultiplied
    // pixels through untouched.
    quad[0] = { -1.0f,  1.0f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    quad[1] = {  1.0f,  1.0f, 0.5f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    quad[2] = {  1.0f, -1.0f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    quad[3] = { -1.0f,  1.0f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    quad[4] = {  1.0f, -1.0f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    quad[5] = { -1.0f, -1.0f, 0.5f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };

    full.Width    = (float) width;
    full.Height   = (float) height;
    full.MaxDepth = 1.0f;

    m_externalSrv = srv;
    hr            = DrawTriangles (quad, 6, identity, true, full, false);
    m_externalSrv = saved;

    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrawTriangles
//
//  Draws one triangle-list batch with the given transform, texture choice, and
//  depth mode.
//
//  The FULL pipeline state is set on every draw, mirroring what
//  DxuiPainter::End does. That is deliberate and is what lets 3D batches
//  interleave freely with the painter and the text renderer: nobody has to
//  save or restore anything, because every one of them assumes nothing about
//  the state it inherits. The cost is a handful of redundant sets per draw
//  against a handful of draws per frame.
//
//  Textured and untextured differ only in WHICH SRV is bound -- the content
//  texture or the 1x1 white one -- so there is no shader or state swap between
//  them.
//
//  Depth-tested draws must re-bind the render target together with our depth
//  view, because the host bound the RTV with no DSV at all. The current RTV is
//  QUERIED rather than passed in, so callers running from a before-present
//  hook need no knowledge of the host's back-buffer plumbing. Depth-off draws
//  leave the bindings untouched.
//
//  The SRV is unbound after the draw. Nothing today binds the content texture
//  as a render target, but a later frame that did would otherwise trip a
//  read/write hazard warning, and the unbind is nearly free.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dxui3DRenderer::DrawTriangles (const Vertex   * verts,
                                       size_t           vertexCount,
                                       const float      mvp[16],
                                       bool             textured,
                                       const D3D11_VIEWPORT & viewportPx,
                                       bool             depthTest,
                                       bool             depthWrite)
{
    HRESULT                     hr         = S_OK;
    D3D11_MAPPED_SUBRESOURCE    mapped     = {};
    ID3D11ShaderResourceView  * srv        = nullptr;
    bool                        useDepth   = depthTest && m_depthDsv != nullptr;
    ID3D11DepthStencilState   * depthState = m_depthState.Get();

    CBREx (m_device != nullptr, E_UNEXPECTED);
    CBREx (verts != nullptr && vertexCount > 0 && (vertexCount % 3) == 0, E_INVALIDARG);

    // Solids test and write; translucent light tests without writing, so it
    // hides behind what stands in front of it without masking what follows.
    if (useDepth)
    {
        depthState = depthWrite ? m_depthStateTest.Get() : m_depthStateReadOnly.Get();
    }

    // Externally-adopted SRV (the desk scene's CRT output) wins over the
    // CPU-uploaded content texture; untextured draws sample opaque white.
    if (textured && m_externalSrv != nullptr)
    {
        srv = m_externalSrv;
    }
    else
    {
        srv = (textured && m_contentSrv != nullptr) ? m_contentSrv.Get() : m_whiteSrv.Get();
    }

    hr = EnsureVertexBuffer (vertexCount);
    CHR (hr);

    hr = m_context->Map (m_vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    CHR (hr);
    memcpy (mapped.pData, verts, vertexCount * sizeof (Vertex));
    m_context->Unmap (m_vertexBuffer.Get(), 0);

    hr = IssueDraw (m_vertexBuffer.Get(), vertexCount, mvp, srv, viewportPx, useDepth, depthState);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrawStatic
//
//  The same draw, out of a buffer that lives on the GPU between frames.
//
//  Nearly all of the scene's geometry is furniture -- a case, a bezel, two
//  drive bodies, the shadows they cast -- and it does not move from one frame
//  to the next. Pushing it through the WRITE_DISCARD path anyway made this
//  file's DrawTriangles the most expensive thing in the process: a CPU profile
//  put 15% of the app's samples under it, and 90% of every sample that landed
//  in memcpy or memset. The monitor alone is 1.3 MB of vertices per frame.
//
//  The caller keeps a StaticMesh per array and hands back a revision it bumps
//  whenever it rebuilds. A revision that has not moved means the GPU's copy is
//  still good, so the draw is just state plus Draw().
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dxui3DRenderer::DrawStatic (StaticMesh           & mesh,
                                    const Vertex         * verts,
                                    size_t                 vertexCount,
                                    uint32_t               revision,
                                    const float            mvp[16],
                                    bool                   textured,
                                    const D3D11_VIEWPORT & viewportPx,
                                    bool                   depthTest,
                                    bool                   depthWrite)
{
    HRESULT                    hr         = S_OK;
    ID3D11ShaderResourceView * srv        = nullptr;
    bool                       useDepth   = depthTest && m_depthDsv != nullptr;
    ID3D11DepthStencilState  * depthState = m_depthState.Get();


    CBREx (m_device != nullptr, E_UNEXPECTED);
    CBREx (verts != nullptr && vertexCount > 0 && (vertexCount % 3) == 0, E_INVALIDARG);

    if (useDepth)
    {
        depthState = depthWrite ? m_depthStateTest.Get() : m_depthStateReadOnly.Get();
    }

    if (textured && m_externalSrv != nullptr)
    {
        srv = m_externalSrv;
    }
    else
    {
        srv = (textured && m_contentSrv != nullptr) ? m_contentSrv.Get() : m_whiteSrv.Get();
    }

    // Immutable rather than default-plus-update: the contents are fixed for
    // the life of the buffer by construction, and a revision change discards
    // the whole thing anyway.
    if (mesh.buffer == nullptr || mesh.revision != revision || mesh.vertexCount != vertexCount)
    {
        D3D11_BUFFER_DESC       desc = {};
        D3D11_SUBRESOURCE_DATA  init = {};

        mesh.buffer.Reset();

        desc.ByteWidth = (UINT) (vertexCount * sizeof (Vertex));
        desc.Usage     = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        init.pSysMem = verts;

        hr = m_device->CreateBuffer (&desc, &init, mesh.buffer.GetAddressOf());
        CHR (hr);

        mesh.vertexCount = vertexCount;
        mesh.revision    = revision;
    }

    hr = IssueDraw (mesh.buffer.Get(), vertexCount, mvp, srv, viewportPx, useDepth, depthState);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssueDraw
//
//  Everything both draw paths share once the vertices are on the GPU: the
//  transform, the full state set, and the draw itself.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dxui3DRenderer::IssueDraw (ID3D11Buffer             * vertexBuffer,
                                   size_t                     vertexCount,
                                   const float                mvp[16],
                                   ID3D11ShaderResourceView * srv,
                                   const D3D11_VIEWPORT     & viewportPx,
                                   bool                       useDepth,
                                   ID3D11DepthStencilState  * depthState)
{
    HRESULT                   hr             = S_OK;
    D3D11_MAPPED_SUBRESOURCE  mapped         = {};
    UINT                      stride         = sizeof (Vertex);
    UINT                      offset         = 0;
    float                     blendFactor[4] = {};


    CBREx (vertexBuffer != nullptr, E_UNEXPECTED);

    hr = m_context->Map (m_mvpBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    CHR (hr);
    memcpy (mapped.pData, mvp, 16 * sizeof (float));
    m_context->Unmap (m_mvpBuffer.Get(), 0);

    {
        float  lightCb[100] =
        {
            m_lighting.light0[0], m_lighting.light0[1], m_lighting.light0[2], 0.0f,
            m_lighting.light1[0], m_lighting.light1[1], m_lighting.light1[2], 0.0f,
            m_lighting.eye[0],    m_lighting.eye[1],    m_lighting.eye[2],    0.0f,
            m_lighting.refDist,   m_lighting.span,      m_lighting.gain,      m_lighting.specStrength,
            m_lighting.specPower, 0.0f, 0.0f, 0.0f,
            m_lighting.ambientUp[0],   m_lighting.ambientUp[1],   m_lighting.ambientUp[2],   0.0f,
            m_lighting.ambientDown[0], m_lighting.ambientDown[1], m_lighting.ambientDown[2], 0.0f,
            m_lighting.lampPos[0], m_lighting.lampPos[1], m_lighting.lampPos[2], m_lighting.lampRefDist,
            m_lighting.lampDir[0], m_lighting.lampDir[1], m_lighting.lampDir[2], m_lighting.lampRange,
            m_lighting.lampColor[0], m_lighting.lampColor[1], m_lighting.lampColor[2],
            m_lighting.lampWrap,
        };

        memcpy (&lightCb[40], m_lighting.shadowMatrix[0], 16 * sizeof (float));
        memcpy (&lightCb[56], m_lighting.shadowMatrix[1], 16 * sizeof (float));

        // Shadowing off inside a shadow pass: the depth-only draws never run
        // the pixel shader, and leaving a map bound while it is the render
        // target would be a read/write hazard.
        lightCb[72] = (m_shadowSlot >= 0) ? 0.0f : m_lighting.shadowTexel;
        lightCb[73] = m_lighting.shadowBias;
        lightCb[74] = m_lighting.shadowStrength;
        lightCb[75] = 0.0f;

        memcpy (&lightCb[76], m_lighting.lampShadow, 16 * sizeof (float));

        lightCb[92] = (m_shadowSlot >= 0 || m_lighting.lampShadowSlot < 0)
                    ? 0.0f : m_lighting.lampShadowTexel;
        lightCb[93] = m_lighting.lampShadowBias;
        lightCb[94] = 0.0f;
        lightCb[95] = 0.0f;

        // Never zero: the shader divides the position by this to find its
        // bump cell, and only the pebble branch reads it -- so a zero here
        // would sit harmless until the first textured surface appeared.
        lightCb[96] = (m_lighting.pebblePitchMm > 1e-4f) ? m_lighting.pebblePitchMm : 1.0f;
        lightCb[97] = m_lighting.pebbleAmount;
        lightCb[98] = m_lighting.pebbleCavity;
        lightCb[99] = m_lighting.pebbleShadow;

        hr = m_context->Map (m_lightBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        CHR (hr);
        memcpy (mapped.pData, lightCb, sizeof (lightCb));
        m_context->Unmap (m_lightBuffer.Get(), 0);
    }

    // Inside a shadow pass nothing else matters: depth only, no color target,
    // no pixel shader, the light's viewport rather than the caller's.
    if (m_shadowSlot >= 0)
    {
        IssueShadowDraw (vertexBuffer, vertexCount);

        return S_OK;
    }

    // Depth-tested draws re-bind the current RTV together with our DSV (the
    // host bound it without one); depth-off draws leave the bindings alone.
    if (useDepth)
    {
        ComPtr<ID3D11RenderTargetView>  rtv;
        ID3D11RenderTargetView       *  rawRtv = nullptr;

        m_context->OMGetRenderTargets (1, rtv.GetAddressOf(), nullptr);
        CBREx (rtv != nullptr, E_UNEXPECTED);

        rawRtv = rtv.Get();
        m_context->OMSetRenderTargets (1, &rawRtv, m_depthDsv.Get());
    }

    // Full state set every draw (mirrors DxuiPainter::End): interleaving with
    // the painter and text renderer needs no save/restore etiquette.
    if (m_hasScissor)
    {
        m_context->RSSetScissorRects (1, &m_scissor);
    }

    m_context->RSSetViewports         (1, &viewportPx);
    m_context->OMSetBlendState        (m_blendState.Get(), blendFactor, 0xFFFFFFFF);
    m_context->OMSetDepthStencilState (depthState, 0);
    m_context->RSSetState             (m_hasScissor ? m_rasterStateScissor.Get()
                                                    : m_rasterState.Get());

    m_context->IASetInputLayout       (m_layout.Get());
    m_context->IASetPrimitiveTopology (D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->IASetVertexBuffers     (0, 1, &vertexBuffer, &stride, &offset);

    m_context->VSSetShader            (m_vs.Get(), nullptr, 0);
    m_context->VSSetConstantBuffers   (0, 1, m_mvpBuffer.GetAddressOf());
    m_context->PSSetShader            (m_ps.Get(), nullptr, 0);
    m_context->PSSetConstantBuffers   (1, 1, m_lightBuffer.GetAddressOf());
    m_context->PSSetShaderResources   (0, 1, &srv);
    m_context->PSSetSamplers          (0, 1, m_sampler.GetAddressOf());

    {
        int  lamp = m_lighting.lampShadowSlot;

        ID3D11ShaderResourceView *  shadowSrvs[kShadowLights + 1] =
        {
            m_shadow[0].srv.Get(),
            m_shadow[1].srv.Get(),
            (lamp >= 0 && lamp < kShadowMaps) ? m_shadow[lamp].srv.Get() : nullptr,
        };

        m_context->PSSetShaderResources (1, kShadowLights + 1, shadowSrvs);
        m_context->PSSetSamplers        (1, 1, m_shadowSampler.GetAddressOf());
    }

    m_context->Draw ((UINT) vertexCount, 0);

    // Unbind the SRVs so a later frame binding one as a render target -- which
    // the shadow pass genuinely does -- never hits a read/write hazard.
    {
        ID3D11ShaderResourceView *  nullSrvs[2 + kShadowLights] = {};

        m_context->PSSetShaderResources (0, 2 + kShadowLights, nullSrvs);
    }

Error:
    return hr;
}
