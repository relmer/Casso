#include "Pch.h"

#include "Ui/Scene/DeskSceneModel.h"

#include "Devices/Printer/MeshBlob.h"
#include "Devices/Printer/ObjMeshParser.h"
#include "Ui/Chrome/CassoBranding.h"
#include "Ui/Chrome/DriveWidget.h"
#include "Ui/Scene/DeskSceneFaceLabels.h"





// The room's two ceiling lights, in DESK space: origin at the computer's
// center on the desk surface, X right, Y back (toward the wall), Z up. The
// ceiling hangs five feet above the desk; one fixture is two feet left of
// the computer, the other seven feet right, both slightly forward of the
// machines so the front faces catch some direct light. Load() translates
// these into each model's own coordinates.
static constexpr float   s_kRoomLights[2][3] =
{
    { -610.0f, -450.0f, 1524.0f },     // 2 ft left, 5 ft up
    { 2134.0f, -450.0f, 1524.0f },     // 7 ft right, 5 ft up
};

// Falloff reference and the shading ramp now live on the class, in the
// header: the pixel shader applies them and the scene passes them along.

// Where the monitor's model-space floor rests in desk space: on top of the
// drive stack. A nominal Disk II height serves both pairings -- at five
// feet of throw the shorter //c drives move the light angle by well under
// a degree.
static constexpr float   s_kMonitorRestMm = 96.0f;

// The lamp as a REAL emitter, baked at load into a second copy of the body.
// A glow disc drawn over the lens cannot know the lamp sits at the back of
// a pocket: only tracing the light and letting the notch's own walls block
// it puts the spill where the housing allows it. The range bounds both which
// faces receive light and which triangles are tested as occluders, which is
// what keeps the shadow rays cheap -- past it the inverse square has taken
// the contribution below a display step anyway.

// Brand stamp placement on the Monitor //c's chin (model mm): the cassowary
// spans this box, proud of the bezel plate's front face.
//
// TIED TO cad_monitor2c.py's FRAME -- the wide band that leans toward the
// viewer, the same width the whole way round, between the outer roll and the
// steeper bezel that drops to the tube. At the bottom it runs from z 4.8 to
// z 23.8, and the mark lives inside that: MARK_Z0..MARK_Z1 there is this box,
// and the two must be kept in step.
//
// The frame LEANS, so a flat stamp cannot lie on it everywhere. It is hung
// from the surface at its TOP edge -- the proud end -- and floats a
// millimeter or two at the bottom. Anchored at the middle it would sink into
// the slope above that line, and a case that eats half a stamp is worse than
// one the stamp stands a fraction off; seen head on, which is how this scene
// is seen, the plate hides its own gap.
static constexpr float   s_kBrandLeftMm   =  32.0f;
static constexpr float   s_kBrandTopZMm   =  19.5f;
static constexpr float   s_kBrandHeightMm =  13.0f;
static constexpr float   s_kBrandFrontY   = -7.86f;

// The Monitor II's mark: low on the right reveal, on the axis the MODEL
// names through its brand anchor -- the reveal's center line, which is the
// power notch's too, so mark, molded icon, and button share one column.
// The left edge is then solved at load from that axis: the cassowary's
// drawn mass sits off-center inside its 36-column grid, so centering the
// stamp's box mis-centers the visual weight, and the silhouette's mass
// centroid gives the exact correction. The //c chin and drive placements
// bake their bias into tuned constants instead.
//
// COUPLED TO THE GENERATOR. cad_monitor2.py cuts a rounded-corner pocket
// BRAND_D deep for this mark (see its BRAND_* block, which quotes the
// silhouette's drawn bounds). The stamp stands in that pocket at the same
// thickness, so its face finishes FLUSH with the frame -- front at y = 0,
// back on the pocket floor. Change either side and the other has to follow.
//
// BRAND_D tracks the icons' RIDGE_H by design -- the badge is as thick as the
// icons are proud. The hazard is not the coupling, it is that THIS side is a
// literal and cannot follow: thin the relief in the generator alone and the
// pocket shrinks while the badge does not, leaving it standing proud of the
// frame it is supposed to finish flush with.
static constexpr float   s_kMon2BrandTopZMm   = 46.0f;
static constexpr float   s_kMon2BrandHeightMm = 24.0f;

// Standing IN the pocket at its full depth, so the mark's face finishes flush
// with the frame. BuildBrandSolid gives it the smoothed outline and rolled top
// edge; laid flat on the pocket floor instead, it read as artwork in a tray
// rather than a badge set into the case.
static constexpr float   s_kMon2BrandThickMm  = 0.5f;    // == the CAD's BRAND_D
static constexpr float   s_kMon2BrandFrontY   = 0.0f;

// How the mark sits left-to-right inside its recess. 0 puts its drawn MASS on
// the pocket's center line, 1 centers its bounding BOX there instead. The two
// differ by about 1.5 mm, because the cassowary carries its weight left of its
// own box -- the tail reaches right while the body sits left.
//
// Mass wins. The eye centers a shape on where its weight is, not on the empty
// rectangle that circumscribes it, and box-centering visibly pushed the bird
// right in its panel. This is the same reasoning that put the mark on the
// strip axis by centroid before there was a panel around it at all.
static constexpr float   s_kMon2BrandBoxCenter = 0.0f;

// A QUARTER INCH from whichever edges a mark is nearest. Every piece of
// faceplate furniture is set to it -- the two legends and the logotype off
// the left, the padlock off the right, the logotype and the cassowary off the
// bottom -- which is what makes four separately-placed marks read as one
// layout instead of four decisions.
static constexpr float   s_kFaceMarginMm = 0.25f * 25.4f;

// Short local names for the plate the whole front is measured against.
static constexpr float   s_kFaceWmm = DeskSceneModel::kFaceWidthMm;
static constexpr float   s_kFaceHmm = DeskSceneModel::kFaceHeightMm;

// The legends are SILVER, not white. On the real drive they are a bright
// metallic ink that shifts as the light moves across it, so they are stamped
// lit: a flat value can be the right lightness but it cannot be shiny, and
// what says metal is the highlight moving, not the color sitting still.
static constexpr float   s_kFaceInkRgb[3] = { 0.780f, 0.790f, 0.815f };

// The drive's cassowary, lower-right of the faceplate like the 2D widget,
// proud of the black plate (front y = -1). Set to the face margin off the
// right edge and the bottom, which is where its height comes from: the mark
// is 36 x 54, so the width follows and the left edge is what it lands on.
static constexpr float   s_kDriveBrandHeightMm = 29.0f;
static constexpr float   s_kDriveBrandTopZMm   = s_kFaceMarginMm + s_kDriveBrandHeightMm;
static constexpr float   s_kDriveBrandLeftMm   = s_kFaceWmm - s_kFaceMarginMm
                                                 - s_kDriveBrandHeightMm * 36.0f / 54.0f;
static constexpr float   s_kDriveBrandFrontY   = -1.95f;

// NO THICKNESS. This is a mark printed on a flat plate, not molded into one
// like the monitor's badge, and giving it relief to buy the smoothing was
// paying in the wrong currency -- the smoothing is now what the flat path
// does, so the mark can be as flat as it really is.

// The DRIVE n legend, top-left of the faceplate. The cap height is the
// legends' shared size; the baked mask is exactly one cap tall, so the cell
// follows from it rather than being chosen.
static constexpr float   s_kDriveLabelLeftMm = s_kFaceMarginMm;
static constexpr float   s_kDriveLabelTopZMm = s_kFaceHmm - s_kFaceMarginMm;

// The mains blades are plated steel at the bottom of a dark socket, and
// diffuse shading cannot say so: almost no direct light reaches them, so
// they rendered flat gray. Real metal down a dark hole still returns the
// room -- this small constant emissive stands in for that environment
// reflection, enough to read as bright metal and far too dim to read as a
// lamp.
static constexpr float   s_kAcPinGlintRgb[3] = { 0.100f, 0.105f, 0.115f };
static constexpr float   s_kDriveLabelCapMm  = 3.1f;
static constexpr float   s_kDriveLabelFrontY = -1.8f;

// The NUMBER is what the legend is actually for -- which drive this is -- so
// it is set nearly twice the word's cap height and the two share one
// centerline rather than a baseline. Baseline-aligned, a number this much
// bigger reads as a different line of text that happens to start where the
// word ends; centered, the word reads as its label.
//
// The number is the taller of the two, so the number is what the top margin
// holds and the word centers on it.
static constexpr float   s_kDriveNumberScale = 1.8f;
static constexpr float   s_kDriveNumberGapMm = 1.9f;

// The IN-USE label: "IN USE" plus the pointer triangle, sitting to the
// LED's left at the LED's height (the 2D widget's arrangement).
static constexpr float   s_kInUseLeftMm  = s_kFaceMarginMm;
static constexpr float   s_kInUseCapMm   = 3.1f;    // the legend's cap height
static constexpr float   s_kInUseFrontY  = -1.8f;
static constexpr float   s_kInUseLampZ   = 26.966f;  // == LED_Z in cad_diskii.py

// The "disk ][" logotype, low on the FACEPLATE -- below the IN-USE legend and
// its lamp, opposite the cassowary in the other corner. It was briefly put on
// the lid, which was simply wrong about where the mark lives.
//
// EMBOSSED, not printed: the real drive's logotype is molded into the plate
// and stands proud of it, so it carries a lit edge along its top and a shadow
// under its bottom that no flat stamp can. Its front sits ahead of the plane
// the printed legends share, and it runs back THROUGH that plane into the
// plate, which is what the relief is measured against.
static constexpr float   s_kWordLeftMm    = s_kFaceMarginMm;
static constexpr float   s_kWordCellMm    = 0.52f;   // -> a 40 mm logotype
static constexpr float   s_kWordFrontY    = -2.10f;
static constexpr float   s_kWordThickMm   = 0.30f;

// How far the top edge rolls before it meets the face. It has to be a real
// fraction of the smoothed outline's own width or the chamfer lands inside a
// cell or two and reads as a crust of lit specks around the letters rather
// than as an edge rolling over.
static constexpr float   s_kWordRollMm    = 0.25f;

// The radius the door's path turns through, in mm, where its two legs meet.
// Clamped to the shorter leg by the caller, so a tiny travel still turns
// rather than rounding past its own end. Two millimeters is a curve at 350 ms
// and sixty frames while still leaving the motion plainly in-then-up rather
// than one diagonal.
static constexpr float   s_kDoorCornerMm = 2.0f;

// How near the lens's own plane a body triangle has to be to count as the
// PANEL the lamp is set into. A lamp is a millimeter-scale feature; anything
// this far from its face is a different part of the case.
static constexpr float   s_kLampPanelBandMm = 4.0f;

// DiskII interactive regions, model space (mm). The eject region wraps the
// slot + door bar + latch; the body box wraps the whole case including the
// proud front furniture.
static constexpr float   s_kDiskIiEjectMin[3] = {   8.0f, -5.0f, 45.81f };
static constexpr float   s_kDiskIiEjectMax[3] = { 145.08f,  3.0f, 65.60f };
static constexpr float   s_kDiskIiBodyMin[3]  = {   0.0f, -5.0f,  0.0f };
static constexpr float   s_kDiskIiBodyMax[3]  = { s_kFaceWmm, 217.325f, s_kFaceHmm };

// The //c drive's own, from cad_disk2c.py: a 152 x 46 x 216 case -- a flat
// slab, about a quarter of its width tall -- split so that the top shell is
// half the height of the bottom one WITH ITS FEET, with the slot sitting on
// that seam at z 28.2..32.7.
//
// IT TAKES TWO BOXES, and they are an L rather than one rectangle. The slot
// band runs the full width. The notch column is narrow, and it is the whole
// of the drive's height: the open stretch below the slot that a finger goes
// into, the latch above it, and the latch's top out over the lid. Boxing the
// two as one rectangle would claim the entire face -- including the plain
// corners, where the padlock lives and a click means browse.
static constexpr float   s_kDisk2cEjectMin[3] = {  16.0f, -8.0f, 26.5f };
static constexpr float   s_kDisk2cEjectMax[3] = { 136.0f,  3.0f, 34.0f };
static constexpr float   s_kDisk2cLatchMin[3] = {  52.0f, -8.0f, 16.0f };
static constexpr float   s_kDisk2cLatchMax[3] = { 100.0f,  3.0f, 46.0f };
static constexpr float   s_kDisk2cBodyMin[3]  = {   0.0f, -8.0f,  0.0f };
static constexpr float   s_kDisk2cBodyMax[3]  = { 152.0f, 216.0f, 46.0f };

// Write-protect padlock stamp on the drive faceplate (model mm), top-right
// like the 2D widget's badge. Flat proud quads in the brand-stamp style; each
// layer floats a hair nearer the viewer than the one it sits on, so depth
// never ties.
//
// DRAWN AS THE SHAPES A PADLOCK IS MADE OF rather than as bars. It had been
// three straight quads for a square-cornered arch, a square-cornered body,
// and a rectangular slot -- none of which a padlock has. The badge is ten
// millimeters across, so it has no room for detail; what it has instead is
// silhouette, and a padlock's silhouette is a round-topped U behind a
// soft-cornered body. Squared off it read as a briefcase.
//
// Everything below is a proportion, so the badge stays a badge if it moves or
// resizes. Its outer edges are set FROM the corner it holds, less the
// outline, so the face margin applies to what is actually visible.

// The outline that separates brass from the black face it sits on. Without
// one the badge's own edge is the only boundary, and a dark brass edge
// against a black plate has almost nothing to be a boundary WITH.
static constexpr float   s_kPadlockOutlineMm = 0.30f;

static constexpr float   s_kPadlockBodyW     = 10.2f;
static constexpr float   s_kPadlockBodyH     =  9.4f;
static constexpr float   s_kPadlockArchH     =  6.4f;
// WHERE it goes is the caller's, because it is the same badge on two drives
// of different heights. The Disk II holds its top-right corner at the face
// margin.
//
// The //c has no corner to spare on that side. Its slot runs nearly the full
// width, the notch column takes the middle from the desk to the lid, and the
// `/` indicator has the lower right -- and once the halves split two thirds
// up, the strip left above the slot is under 11 mm on a badge that needs 16.
// So it takes the LOWER LEFT, which is the one stretch of this face nothing
// else is on, and which no eject box reaches across.
static constexpr float   s_kDiskIiPadlockX1  = s_kFaceWmm - s_kFaceMarginMm - s_kPadlockOutlineMm;
static constexpr float   s_kDiskIiPadlockZ1  = s_kFaceHmm - s_kFaceMarginMm - s_kPadlockOutlineMm;
static constexpr float   s_kDisk2cPadlockX1  = 24.0f;
static constexpr float   s_kDisk2cPadlockZ1  = 19.0f;

// And how big, as a fraction of the Disk II's. The badge holds the same share
// of a face 46 mm tall that the full-size one holds of a face 90 mm tall --
// which is the thing to keep constant, since what makes it read as a padlock
// is its silhouette against the panel around it, not its size in millimeters.
static constexpr float   s_kDiskIiPadlockScale = 1.00f;
static constexpr float   s_kDisk2cPadlockScale = 0.60f;
static constexpr float   s_kPadlockCornerR   =  1.35f;   // the body's soft corners
static constexpr float   s_kPadlockShackleR  =  3.05f;   // the U's outer radius
static constexpr float   s_kPadlockShackleT  =  1.35f;   // the U's stock thickness
static constexpr float   s_kPadlockLegTopMm  =  1.50f;   // how far the legs run into the body
// A BIG bore over a SHORT slot. The proportions decide whether this reads as
// a keyhole or as an arrow, and a small bore over a long tapering slot is an
// arrow every time -- the barbs where the two meet are the whole of what the
// eye picks up at four pixels across. Keeping the slot shorter than the bore
// is wide leaves the bore as the shape, and the slot as a notch under it.
static constexpr float   s_kPadlockHoleR     =  1.35f;   // the keyhole's bore
static constexpr float   s_kPadlockHoleCzUp  =  5.5f;    // above the body's bottom
static constexpr float   s_kPadlockHoleZ0Up  =  2.9f;

// One rasterization cell, in mm. Small enough that the badge's outlines land
// under a screen pixel at any zoom the scene offers.
static constexpr float   s_kPadlockCellMm    = 0.05f;

// The badge's four layers, as offsets from the FACE IT SITS ON. Depth used
// to be absolute, which quietly assumed every drive's faceplate was where the
// Disk II's is -- and the //c's front is 0.6 mm further forward than that, so
// the whole badge was BURIED IN THE CASE. All that showed of it was the
// scrap crossing the slot's opening and a crescent up where the lid's edge
// rounds back past y = 0, which is exactly the "weird gold object" it looked
// like. A mark on a face has to be placed relative to that face.
static constexpr float   s_kPadlockOutlineDy = 0.00f;
static constexpr float   s_kPadlockShackleDy = 0.05f;
static constexpr float   s_kPadlockBodyDy    = 0.10f;
static constexpr float   s_kPadlockHoleDy    = 0.15f;

// Where each drive's badge floats: a hair in front of the face it marks.
static constexpr float   s_kDiskIiPadlockY   = -1.90f;
static constexpr float   s_kDisk2cPadlockY   = -2.55f;

// Slack around the padlock's hit box: the badge is ~10 mm across and the
// tooltip should answer a deliberate hover, not demand marksmanship.
static constexpr float   s_kPadlockHitPadMm  = 2.5f;

static constexpr float   s_kPadlockFill[3]    = { 0.847f, 0.718f, 0.416f };   // warm brass
static constexpr float   s_kPadlockShade[3]   = { 0.478f, 0.376f, 0.149f };   // darker brass
static constexpr float   s_kPadlockHole[3]    = { 0.165f, 0.129f, 0.035f };   // keyhole
static constexpr float   s_kPadlockOutline[3] = { 0.086f, 0.070f, 0.035f };   // near-black brass





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::ColorMatches
//
////////////////////////////////////////////////////////////////////////////////

bool DeskSceneModel::ColorMatches (float r, float g, float b, const float kd[3])
{
    return std::abs (r - kd[0]) <= kKdEpsilon &&
           std::abs (g - kd[1]) <= kKdEpsilon &&
           std::abs (b - kd[2]) <= kKdEpsilon;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::AppendLitTri
//
//  Emits geometry for the SHADER to light: material tint plus the face's
//  normal, both untouched by illumination.
//
//  This used to bake Lambert here on the CPU, and two things about that were
//  wrong in ways no amount of modeling could work around. It took |dot| of
//  the normal, because culling is off and a signed dot could shade a visible
//  back face black -- which made an up-facing wall and a down-facing wall
//  identical, so molded relief had no light-and-shadow flanks and an arrow
//  could not read as an arrow. And it clamped the summed contribution with
//  min(1, sum), so every face past the threshold collapsed onto one value;
//  with two point lights only a few feet away, whether a given face crossed
//  that line depended on where it sat, which is why the same glyph rendered
//  crisply on the bezel's lower band and as mush on its upper one.
//
//  The shader keeps the dot product's sign -- the CAD kernel winds solids
//  outward, so a face the viewer can see already points at them and the
//  guard was never earning its cost -- and rolls the ramp off instead of
//  clipping it. Light positions still arrive in each model's own space --
//  Load() puts them there, and the scene hands them to the renderer before
//  drawing this model.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::AppendLitTri (std::vector<Dxui3DRenderer::Vertex>  & out,
                                   const ObjTriangle                    & tri,
                                   const std::array<float, 3>           * smooth)
{
    float   e1[3]  = { tri.p1[0] - tri.p0[0], tri.p1[1] - tri.p0[1], tri.p1[2] - tri.p0[2] };
    float   e2[3]  = { tri.p2[0] - tri.p0[0], tri.p2[1] - tri.p0[1], tri.p2[2] - tri.p0[2] };
    float   n[3]   = { e1[1] * e2[2] - e1[2] * e2[1],
                       e1[2] * e2[0] - e1[0] * e2[2],
                       e1[0] * e2[1] - e1[1] * e2[0] };
    float   nl     = std::sqrt (n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);



    for (size_t corner = 0; corner < 3; corner++)
    {
        const float            * p = (corner == 0) ? tri.p0 : (corner == 1) ? tri.p1 : tri.p2;
        Dxui3DRenderer::Vertex   v = {};

        v.x = p[0];  v.y = p[1];  v.z = p[2];
        v.r = tri.r;
        v.g = tri.g;
        v.b = tri.b;
        v.a = 1.0f;

        // THE SMOOTHED NORMAL WHEN THE MESH CARRIES ONE. MeshNormals averaged
        // it at build time across the faces meeting this corner below the
        // smoothing angle, so a curved surface shades continuously across
        // however few triangles approximate it, while a moulded edge, whose
        // faces meet well above the angle, keeps each side its own normal
        // and stays crisp.
        //
        // Falling back to the face normal is the old flat behavior, which is
        // what the procedurally stamped geometry (brand marks, drive labels,
        // relief) wants: it is built flat and has no neighbors to average
        // with.
        //
        // A degenerate triangle leaves the normal ZERO, which the shader
        // reads as unlit and passes through at full tint. That matches what
        // the old baked path did with a zero-length normal, which skipped
        // the Lambert loop and kept shade at floor+span == 1.
        if (smooth != nullptr)
        {
            v.nx = smooth[corner][0];
            v.ny = smooth[corner][1];
            v.nz = smooth[corner][2];
        }
        else if (nl > 0.0f)
        {
            v.nx = n[0] / nl;
            v.ny = n[1] / nl;
            v.nz = n[2] / nl;
        }

        out.push_back (v);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::AppendFlatTri
//
//  Unlit append for the glass and lamp sub-meshes: the glass tint must stay
//  white so the display texture passes through unmodified, and lamp tints are
//  rewritten per frame by the scene's on/off state.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::AppendFlatTri (std::vector<Dxui3DRenderer::Vertex> & out, const ObjTriangle & tri)
{
    for (const float * p : { tri.p0, tri.p1, tri.p2 })
    {
        Dxui3DRenderer::Vertex   v = {};

        v.x = p[0];  v.y = p[1];  v.z = p[2];
        v.r = tri.r;
        v.g = tri.g;
        v.b = tri.b;
        v.a = 1.0f;

        out.push_back (v);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::Load
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeskSceneModel::Load (DeskDeviceKind kind, std::span<const uint8_t> meshBlob)
{
    HRESULT                              hr            = S_OK;
    std::vector<ObjTriangle>             triangles;
    std::vector<std::string>             materialNames;
    std::vector<std::array<float, 3>>    smoothNormals;
    std::vector<size_t>                  opaqueTris;
    const float                        * lampKd        = nullptr;
    float                                anchorLo      = FLT_MAX;
    float                                anchorHi      = -FLT_MAX;
    float                                frontLo       = FLT_MAX;
    float                                frontHi       = -FLT_MAX;
    bool                                 lampFound     = false;
    bool                                 doorOk        = false;



    m_kind = kind;
    m_opaque.clear();
    m_glass.clear();
    m_lamp.clear();
    m_door.clear();
    m_padlock.clear();
    m_lamps.clear();
    m_regions.clear();
    m_surface = {};

    // THE TILTING ASSEMBLY GOES WITH THE OLD MONITOR. It is built only from
    // parts this mesh names -- the Monitor II's bezel and its two tilt marks
    // -- and the //c's has none of them, so a load that left the previous
    // model's behind hung the last monitor's frame around the new one, kept
    // its grips live for the cursor and the drag, and tipped a tube that was
    // never part of it.
    m_tiltable.clear();
    m_tiltGrips.clear();
    m_tiltPivotY = 0.0f;
    m_tiltPivotZ = 0.0f;
    m_maxTiltRad = 0.0f;

    lampKd = IsMonitorKind (kind) ? kMonitorLampKd : kDriveLampKd;

    hr = MeshBlob::Read (meshBlob, triangles, materialNames, smoothNormals);
    CHRA (hr);

    // Move the room's ceiling lights into this model's own coordinates so
    // the per-face bake needs no transform: the model's x-center rests on
    // the computer's desk-space center line, and a monitor's floor sits on
    // top of the drive stack rather than on the desk.
    {
        float  minX = FLT_MAX;
        float  maxX = -FLT_MAX;
        float  rest = IsMonitorKind (kind) ? s_kMonitorRestMm : 0.0f;

        for (const ObjTriangle & tri : triangles)
        {
            for (const float * p : { tri.p0, tri.p1, tri.p2 })
            {
                minX = (std::min) (minX, p[0]);
                maxX = (std::max) (maxX, p[0]);
            }
        }

        for (size_t i = 0; i < std::size (s_kRoomLights); i++)
        {
            m_lightsModel[i][0] = s_kRoomLights[i][0] + (minX + maxX) * 0.5f;
            m_lightsModel[i][1] = s_kRoomLights[i][1];
            m_lightsModel[i][2] = s_kRoomLights[i][2] - rest;
        }
    }

    for (size_t t = 0; t < triangles.size(); t++)
    {
        const ObjTriangle &            tri     = triangles[t];
        const std::string &            part    = ObjMeshParser::MaterialName (tri, materialNames);

        // The three normals the baker averaged for this triangle, or null
        // when the mesh carries none and the face normal has to serve.
        const std::array<float, 3> *   corners = smoothNormals.empty()
                                                 ? nullptr
                                                 : &smoothNormals[t * 3];

        // Metadata first, and it never reaches a vertex buffer: the anchors
        // exist to be measured, not seen. Each names the midpoint of its own
        // extent, so any marker shape at all names the same line or plane.
        if (part == s_kpszBrandAnchor)
        {
            for (const float * p : { tri.p0, tri.p1, tri.p2 })
            {
                anchorLo = (std::min) (anchorLo, p[0]);
                anchorHi = (std::max) (anchorHi, p[0]);
            }

            continue;
        }

        if (part == s_kpszFrontAnchor)
        {
            for (const float * p : { tri.p0, tri.p1, tri.p2 })
            {
                frontLo = (std::min) (frontLo, p[1]);
                frontHi = (std::max) (frontHi, p[1]);
            }

            continue;
        }

        if (IsMonitorKind (kind) && part == s_kpszGlass)
        {
            AppendFlatTri (m_glass, tri);
        }
        else if (IsMonitorKind (kind) &&
                 (part == s_kpszBezel || part == s_kpszTubeSkirt ||
                  part == s_kpszTiltUp || part == s_kpszTiltDown))
        {
            // The assembly, and the two marks that move it. The marks stay
            // part of it -- they are molded into the bezel and travel with
            // it, so a grip measured here is where the glyph actually is at
            // any tilt, once the same transform is applied to both.
            size_t  first = m_tiltable.size();

            AppendLitTri (m_tiltable, tri, corners);

            // The two MARKS by name, rather than "anything but the bezel":
            // the skirt joined this group and would otherwise have been read
            // as a tilt grip and grown the hit box across the whole face.
            if (part == s_kpszTiltUp || part == s_kpszTiltDown)
            {
                GrowTiltGrip ((part == s_kpszTiltUp) ? 1 : -1, first);
            }
        }
        else if (part == s_kpszLamp || part == s_kpszLed)
        {
            AppendFlatTri (m_lamp, tri);
        }
        else if (part == s_kpszDoor || part == s_kpszLever || part == s_kpszTab)
        {
            // The door is molded in the same pebbled black as the face around
            // it, and can now SAY so: identity is the part name, so its Kd is
            // free to be the finish it actually has. The flag is still set
            // here because the door is its own batch and never reaches the
            // pebbled branch below.
            size_t  first = m_door.size();

            AppendLitTri (m_door, tri, corners);

            if (kind == DeskDeviceKind::DiskII)
            {
                for (size_t i = first; i < m_door.size(); i++)
                {
                    m_door[i].pebble = 1.0f;
                }
            }
        }
        else if (part.rfind (s_kpszAcPinPrefix, 0) == 0)
        {
            // Plated metal, not painted plastic: the blades keep their tint
            // and gain the standing glint -- see s_kAcPinGlintRgb.
            size_t  first = m_opaque.size();

            opaqueTris.push_back (t);
            AppendLitTri (m_opaque, tri, corners);

            for (size_t i = first; i < m_opaque.size(); i++)
            {
                m_opaque[i].er = s_kAcPinGlintRgb[0];
                m_opaque[i].eg = s_kAcPinGlintRgb[1];
                m_opaque[i].eb = s_kAcPinGlintRgb[2];
            }
        }
        else if (ColorMatches (tri.r, tri.g, tri.b, kPlatePebbledKd) ||
                 ColorMatches (tri.r, tri.g, tri.b, kPlateRecessKd))
        {
            // A finish, not a color. Take the marker off the tint and put it
            // on the pebble flag, so the grain shows up in the LIGHT and the
            // plastic stays the same black as the matte plate beside it.
            //
            // The recess marker is the same plastic down a pocket, and it
            // carries a darker tint because nothing here computes ambient
            // occlusion -- see kPlateRecessKd.
            bool    recess = ColorMatches (tri.r, tri.g, tri.b, kPlateRecessKd);
            size_t  first  = m_opaque.size();

            opaqueTris.push_back (t);
            AppendLitTri (m_opaque, tri, corners);

            for (size_t i = first; i < m_opaque.size(); i++)
            {
                m_opaque[i].r      = recess ? kPlateRecessRgb[0] : kPlateMatteRgb[0];
                m_opaque[i].g      = recess ? kPlateRecessRgb[1] : kPlateMatteRgb[1];
                m_opaque[i].b      = recess ? kPlateRecessRgb[2] : kPlateMatteRgb[2];
                m_opaque[i].pebble = 1.0f;
            }
        }
        else
        {
            opaqueTris.push_back (t);
            AppendLitTri (m_opaque, tri, corners);
        }
    }

    // Glass tint is forced white: the picture must pass through unmodified,
    // whatever Kd identified the sheet.
    for (Dxui3DRenderer::Vertex & v : m_glass)
    {
        v.r = v.g = v.b = v.a = 1.0f;
    }

    if (anchorHi >= anchorLo)
    {
        m_brandAxisX = (anchorLo + anchorHi) * 0.5f;
    }

    // No marker means no protruding front: the model's own frontmost point is
    // its frame, which is true of every device that is a plain box.
    m_frontPlaneY = (frontHi >= frontLo) ? (frontLo + frontHi) * 0.5f
                                         : m_boundsMin[1];

    if (IsMonitorKind (kind))
    {
        // The mark sits where each case has room for it: on the //c's chin
        // under the screen, and on the Monitor II's divided right strip, down
        // beside the power button.
        if (kind == DeskDeviceKind::Monitor2)
        {
            // Center the silhouette's MASS, not its grid box: the drawn
            // cassowary is heavier on one side, so box-centering leaves the
            // visual weight off-axis. Sum the set cells' column centers and
            // solve the left edge so the centroid lands on the strip's
            // axis. The proof is in the scene: the strip's calibration
            // ruler shares the stamp's height and perspective, so centroid-
            // on-ruler is judgeable straight off a capture.
            float   cell   = s_kMon2BrandHeightMm / (float) CassoBranding::kGridH;
            double  sumCol = 0.0;
            int     count  = 0;
            int     minCol = CassoBranding::kGridW;
            int     maxCol = -1;

            for (int row = 0; row < CassoBranding::kGridH; row++)
            {
                uint64_t  bits = CassoBranding::SilhouetteRow (row);

                for (int col = 0; col < CassoBranding::kGridW; col++)
                {
                    if ((bits & (1ULL << col)) != 0)
                    {
                        sumCol += (double) col + 0.5;
                        count++;
                        minCol = std::min (minCol, col);
                        maxCol = std::max (maxCol, col);
                    }
                }
            }

            {
                float  centroidCols = (count > 0) ? (float) (sumCol / count)
                                                  : (float) CassoBranding::kGridW * 0.5f;
                float  boxCols      = (maxCol >= minCol) ? (float) (minCol + maxCol + 1) * 0.5f
                                                         : centroidCols;
                float  centerCols   = centroidCols + (boxCols - centroidCols) * s_kMon2BrandBoxCenter;
                float  leftMm       = m_brandAxisX - centerCols * cell;

                BuildBrandStamp (leftMm, s_kMon2BrandTopZMm,
                                 s_kMon2BrandHeightMm, s_kMon2BrandFrontY,
                                 s_kMon2BrandThickMm);
            }
        }
        else
        {
            BuildBrandStamp (s_kBrandLeftMm, s_kBrandTopZMm, s_kBrandHeightMm, s_kBrandFrontY);
        }

        hr = BuildGlassSurface();
        CHRA (hr);

        // Lift the glass a hair toward the viewer: the generated sheet's
        // corners share the cavity front's plane, and equal depth loses the
        // LESS test -- the cavity would clip the corners off the picture.
        // Shifting verts AND the surface together keeps input mapping exact.
        for (Dxui3DRenderer::Vertex & v : m_glass)
        {
            v.y -= kGlassLiftMm;
        }

        m_surface.baseY -= kGlassLiftMm;

        AssignGlassUvs();
    }

    // A device that should carry a lamp but lost it (palette drift in a
    // refined model) is a broken asset, not a runtime condition. Likewise a
    // drive without its door assembly -- the mount/eject animation depends
    // on it.
    lampFound = !m_lamp.empty();
    CBRA (lampFound);

    doorOk = !IsDriveKind (kind) || !m_door.empty();
    CBRA (doorOk);

    // The shut door's extent, for the hit box that follows it open.
    {
        float  lo[3] = {  FLT_MAX,  FLT_MAX,  FLT_MAX };
        float  hi[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

        for (const Dxui3DRenderer::Vertex & v : m_door)
        {
            lo[0] = std::min (lo[0], v.x);  hi[0] = std::max (hi[0], v.x);
            lo[1] = std::min (lo[1], v.y);  hi[1] = std::max (hi[1], v.y);
            lo[2] = std::min (lo[2], v.z);  hi[2] = std::max (hi[2], v.z);
        }

        if (!m_door.empty())
        {
            memcpy (m_doorMin, lo, sizeof (m_doorMin));
            memcpy (m_doorMax, hi, sizeof (m_doorMax));
        }
    }

    if (kind == DeskDeviceKind::Disk2c)
    {
        // The //c's face needs nothing stamped on it at all: its brand, its
        // lamp, its slot and its lever are all MODELED.
        //
        // It emphatically does not want the Disk II's marks. Those are placed
        // against a face 20 mm taller, so DRIVE n and the badge landed off
        // the top of this one and printed on the monitor standing on it --
        // and "disk ][" belongs to a drive this is not.
        m_doorMotion = DeskDoorMotion::InThenUp;
    }

    if (kind == DeskDeviceKind::DiskII)
    {
        // NOT derived from the door's own top-back edge any more. That put the
        // pole ON the part, which is a plain hinge, and a plain hinge cannot
        // produce this motion -- see s_kDiskIiDoorPole*. Deriving it also made
        // the mechanism a silent function of the door's bounding box, so
        // remodeling the door moved the mechanism.
        m_doorMotion  = DeskDoorMotion::Cantilever;
        m_doorPivotY  = kDiskIiDoorPoleY;
        m_doorPivotZ  = kDiskIiDoorPoleZ;
        m_doorOpenRad = kDiskIiDoorOpenRad;

        // The cassowary (lower-right, the 2D widget's mark) and the IN-USE
        // label pointing at the LED. The DRIVE-number badge text is stamped
        // per-drive by the scene -- it cannot live in the shared model.
        BuildBrandStamp (s_kDriveBrandLeftMm, s_kDriveBrandTopZMm,
                         s_kDriveBrandHeightMm, s_kDriveBrandFrontY);
        BuildInUseStamp();

        BuildWordmarkStamp();
    }

    {
        DeskLampAnchor   anchor;
        float            lo[3] = { FLT_MAX, FLT_MAX, FLT_MAX };
        float            hi[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

        for (const Dxui3DRenderer::Vertex & v : m_lamp)
        {
            lo[0] = std::min (lo[0], v.x);  hi[0] = std::max (hi[0], v.x);
            lo[1] = std::min (lo[1], v.y);  hi[1] = std::max (hi[1], v.y);
            lo[2] = std::min (lo[2], v.z);  hi[2] = std::max (hi[2], v.z);
        }

        if (!m_lamp.empty())
        {
            anchor.center[0]   = (lo[0] + hi[0]) * 0.5f;
            anchor.center[1]   = (lo[1] + hi[1]) * 0.5f;
            anchor.center[2]   = (lo[2] + hi[2]) * 0.5f;
            anchor.frontY      = lo[1];                     // most proud (viewer at -Y)
            anchor.radiusX     = (hi[0] - lo[0]) * 0.5f;
            anchor.radiusZ     = (hi[2] - lo[2]) * 0.5f;
            anchor.firstVertex = 0;
            anchor.vertexCount = m_lamp.size();

            // The panel's facing, from the body triangle nearest the lens in
            // the plane of the face. Read off the mesh rather than declared,
            // because it is a fact about the model and the model is the only
            // thing that knows it -- and because a lamp that moves onto a
            // differently angled panel then brings its glow's plane with it.
            //
            // BOUNDED TO THE LENS'S OWN PLANE, which is the whole of the care
            // this needs. Accepting anything merely BEHIND the lens let the
            // search reach the far side of the case: on the //c monitor it
            // settled on an inward-facing wall of the rear panel recess, a
            // quarter of a meter back, which happens to be flat -- so the
            // facing came out (0,-1,0), the tilt was a no-op, and the glow
            // kept the hard edge this was written to remove.
            //
            // Centroids, not first vertices: a triangle is judged by where it
            // IS, and on a lofted panel one corner can be a long way off.
            {
                float  best = FLT_MAX;

                for (size_t i = 0; i + 2 < m_opaque.size(); i += 3)
                {
                    const Dxui3DRenderer::Vertex  & v  = m_opaque[i];
                    float                           gx = (m_opaque[i].x + m_opaque[i + 1].x
                                                          + m_opaque[i + 2].x) / 3.0f;
                    float                           gy = (m_opaque[i].y + m_opaque[i + 1].y
                                                          + m_opaque[i + 2].y) / 3.0f;
                    float                           gz = (m_opaque[i].z + m_opaque[i + 1].z
                                                          + m_opaque[i + 2].z) / 3.0f;
                    float                           dx = gx - anchor.center[0];
                    float                           dz = gz - anchor.center[2];
                    float                           d2 = dx * dx + dz * dz;

                    // Facing the viewer, and NEAR THE LENS'S OWN PLANE.
                    if (v.ny < -0.2f &&
                        std::fabs (gy - anchor.frontY) <= s_kLampPanelBandMm && d2 < best)
                    {
                        best = d2;
                        anchor.facing[0] = v.nx;
                        anchor.facing[1] = v.ny;
                        anchor.facing[2] = v.nz;
                    }
                }
            }
        }

        m_lamps.push_back (anchor);
    }

    AddRegionBoxes();
    ComputeTiltTravel();
    ComputeBounds();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::BuildGlassSurface
//
//  Derives the sag sphere from the measured glass mesh rather than assuming
//  the generator's parameters: the rect is the XZ bounding box, the front
//  plane is the highest Y (the corners), the sag is the depth of the lowest Y
//  (the center), and the radius follows from the circle through both --
//  R = (halfDiag^2 + sag^2) / (2 * sag). A refined model that keeps a
//  spherical-sag sheet keeps exact input mapping with no code change.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeskSceneModel::BuildGlassSurface()
{
    HRESULT   hr         = S_OK;
    float     lo[3]      = { FLT_MAX, FLT_MAX, FLT_MAX };
    float     hi[3]      = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    float     halfW      = 0.0f;
    float     halfH      = 0.0f;
    float     halfDiag   = 0.0f;
    float     sag        = 0.0f;
    bool      glassFound = !m_glass.empty();
    bool      valid      = false;



    CBRA (glassFound);

    for (const Dxui3DRenderer::Vertex & v : m_glass)
    {
        lo[0] = std::min (lo[0], v.x);  hi[0] = std::max (hi[0], v.x);
        lo[1] = std::min (lo[1], v.y);  hi[1] = std::max (hi[1], v.y);
        lo[2] = std::min (lo[2], v.z);  hi[2] = std::max (hi[2], v.z);
    }

    halfW    = (hi[0] - lo[0]) * 0.5f;
    halfH    = (hi[2] - lo[2]) * 0.5f;
    halfDiag = std::sqrt (halfW * halfW + halfH * halfH);
    sag      = hi[1] - lo[1];

    // A flat or degenerate sheet cannot carry the display.
    CBRA (halfW > 0.0f && halfH > 0.0f && sag > 0.0f);

    m_surface.x0     = lo[0];
    m_surface.x1     = hi[0];
    m_surface.z0     = lo[2];
    m_surface.z1     = hi[2];
    m_surface.baseY  = hi[1];
    m_surface.radius = (halfDiag * halfDiag + sag * sag) / (2.0f * sag);

    valid = CurvedDisplayMath::IsValid (m_surface);
    CBRA (valid);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::AssignGlassUvs
//
//  Planar XZ projection over the glass rect -- exact for a sheet displaced
//  only along Y (research R4). The same mapping CurvedDisplayMath uses for
//  input, so what the texel shows and what a click hits cannot disagree.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::AssignGlassUvs()
{
    for (Dxui3DRenderer::Vertex & v : m_glass)
    {
        float   pt[3] = { v.x, v.y, v.z };

        CurvedDisplayMath::UvFromModelPoint (m_surface, pt, v.u, v.v);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::BuildBrandStamp
//
//  Stamps the rainbow cassowary as geometry, built from CassoBranding's own
//  silhouette bitmask -- the same mark, same stripes, same concavities as the
//  2D chrome, with no image asset. One quad per contiguous bit run for the
//  face, unlit so the brand colors stay exact. Placement is the caller's: the
//  monitor chin and the drive faceplate both carry it.
//
//  With a thickness the mark also gets SIDE WALLS and becomes a solid standing
//  in the generator's recess rather than a decal lying on the surface. Those
//  walls are lit -- they carry normals, so they shade with the room and are
//  what actually reads as depth; the face stays unlit so the stripes keep
//  their exact values. Walls go on the silhouette BOUNDARY only, found by
//  masking each row against its neighbors, so nothing is emitted inside the
//  mark where it would never be seen. No back face: it would land coplanar
//  with the pocket floor, and the scene draws with culling off.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::BuildBrandStamp (float leftMm, float topZMm, float heightMm, float frontY,
                                      float thicknessMm)
{
    float  rowH     = heightMm / (float) CassoBranding::kGridH;
    float  colW     = rowH;   // the silhouette grid is square-celled
    int    firstRow = CassoBranding::kGridH;
    int    lastRow  = -1;



    for (int row = 0; row < CassoBranding::kGridH; row++)
    {
        if (CassoBranding::SilhouetteRow (row) != 0)
        {
            firstRow = std::min (firstRow, row);
            lastRow  = std::max (lastRow, row);
        }
    }

    if (lastRow < firstRow)
    {
        return;
    }

    if (thicknessMm > 0.0f)
    {
        BuildBrandSolid (leftMm, topZMm, heightMm, frontY, thicknessMm, firstRow, lastRow);
        return;
    }

    {
        std::vector<uint8_t>  bits;
        std::vector<float>    rgb;

        BrandMask (bits, rgb, firstRow, lastRow);
        BuildSmoothMask (m_opaque, bits.data(), CassoBranding::kGridW, CassoBranding::kGridH,
                         leftMm, topZMm, heightMm, frontY, rgb.data(), false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::BrandMask
//
//  The cassowary's silhouette as bytes and its stripe colors as one rgb per
//  grid row -- the form both mark builders take.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::BrandMask (std::vector<uint8_t> & outMask, std::vector<float> & outRgb,
                                int firstRow, int lastRow)
{
    outMask.assign ((size_t) CassoBranding::kGridW * CassoBranding::kGridH, 0);
    outRgb.assign ((size_t) CassoBranding::kGridH * 3, 0.0f);

    for (int row = 0; row < CassoBranding::kGridH; row++)
    {
        uint64_t  bits   = CassoBranding::SilhouetteRow (row);
        int       banded = (std::min) (lastRow, (std::max) (firstRow, row));
        int       stripe = ((banded - firstRow) * CassoBranding::kStripeCount)
                           / (lastRow - firstRow + 1);
        uint32_t  argb   = CassoBranding::StripeColor (stripe);

        for (int col = 0; col < CassoBranding::kGridW; col++)
        {
            outMask[(size_t) row * CassoBranding::kGridW + col] =
                ((bits >> col) & 1ULL) ? (uint8_t) 1 : (uint8_t) 0;
        }

        outRgb[(size_t) row * 3 + 0] = (float) ((argb >> 16) & 0xFF) / 255.0f;
        outRgb[(size_t) row * 3 + 1] = (float) ((argb >> 8) & 0xFF) / 255.0f;
        outRgb[(size_t) row * 3 + 2] = (float) (argb & 0xFF) / 255.0f;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::BuildSmoothMask
//
//  A mask as a flat mark whose outline is the field's own half-level contour
//  rather than a set of filled cells.
//
//  WHERE THE SMOOTHING LANDS is the whole of it. Resampling a mask and then
//  filling whole fine cells leaves the boundary a staircase however smooth
//  the field underneath is -- finer steps than the mask's, but steps, and at
//  a zoom the scene now offers they are visible. Interpolating to find where
//  the field actually crosses its half level, and putting the edge there,
//  takes the quantization out of the outline entirely.
//
//  The filter is a smooth falloff, not a hard disc: a box filter has an edge
//  of its own, that edge puts kinks in the field, and a kink in the field is
//  a kink in the contour.
//
//  Consecutive rows join as TRAPEZOIDS wherever their spans correspond, so a
//  sloping edge is one straight run rather than a stack of rectangles. Where
//  the span counts disagree -- a limb separating from a body -- the two rows
//  meet halfway as rectangles instead, which is correct if less pretty and
//  happens over a fraction of a millimeter.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::BuildSmoothMask (std::vector<Dxui3DRenderer::Vertex> & out,
                                      const uint8_t * mask, int gridW, int gridH,
                                      float leftMm, float topZMm, float heightMm, float frontY,
                                      const float * rowRgb, bool lit,
                                      float smoothCells, int subdivide)
{
    const int    fw    = gridW * subdivide;
    const int    fh    = gridH * subdivide;
    const float  cell  = heightMm / (float) gridH;
    const float  fcell = cell / (float) subdivide;
    const int    span  = (int) std::ceil (smoothCells + 1.0f);
    const float  ny    = lit ? -1.0f : 0.0f;



    std::vector<float>                 field ((size_t) fh * fw, 0.0f);
    std::vector<std::vector<float>>    cross ((size_t) fh);



    for (int fy = 0; fy < fh; fy++)
    {
        float  oy = ((float) fy + 0.5f) / (float) subdivide;

        for (int fx = 0; fx < fw; fx++)
        {
            float  ox  = ((float) fx + 0.5f) / (float) subdivide;
            float  num = 0.0f;
            float  den = 0.0f;

            for (int dy = -span; dy <= span; dy++)
            {
                for (int dx = -span; dx <= span; dx++)
                {
                    int    sy = (int) std::floor (oy) + dy;
                    int    sx = (int) std::floor (ox) + dx;
                    float  ax = (float) sx + 0.5f - ox;
                    float  az = (float) sy + 0.5f - oy;
                    float  d2 = (ax * ax + az * az) / (smoothCells * smoothCells);
                    float  w  = 0.0f;

                    if (d2 >= 1.0f)
                    {
                        continue;
                    }

                    w    = (1.0f - d2) * (1.0f - d2);
                    den += w;

                    if (sy >= 0 && sy < gridH && sx >= 0 && sx < gridW &&
                        mask[(size_t) sy * gridW + sx] != 0)
                    {
                        num += w;
                    }
                }
            }

            field[(size_t) fy * fw + fx] = (den > 0.0f) ? num / den : 0.0f;
        }
    }

    // Where the field crosses half, to a fraction of a sample. These are the
    // mark's edges, and they alternate entering and leaving it.
    for (int fy = 0; fy < fh; fy++)
    {
        const float *  row = &field[(size_t) fy * fw];

        for (int fx = 0; fx + 1 < fw; fx++)
        {
            float  a = row[fx];
            float  b = row[fx + 1];

            if ((a < 0.5f) == (b < 0.5f))
            {
                continue;
            }

            {
                float  t = (0.5f - a) / (b - a);

                cross[(size_t) fy].push_back (leftMm + ((float) fx + 0.5f + t) * fcell);
            }
        }
    }

    {
        auto  pushQuad = [&out, frontY, ny] (float xa0, float xa1, float za,
                                             float xb0, float xb1, float zb, const float * rgb)
        {
            Dxui3DRenderer::Vertex   quad[6] = {};

            quad[0] = { xa0, frontY, za, 0, 0, rgb[0], rgb[1], rgb[2], 1.0f, 0.0f, ny, 0.0f };
            quad[1] = { xa1, frontY, za, 0, 0, rgb[0], rgb[1], rgb[2], 1.0f, 0.0f, ny, 0.0f };
            quad[2] = { xb1, frontY, zb, 0, 0, rgb[0], rgb[1], rgb[2], 1.0f, 0.0f, ny, 0.0f };
            quad[3] = { xa0, frontY, za, 0, 0, rgb[0], rgb[1], rgb[2], 1.0f, 0.0f, ny, 0.0f };
            quad[4] = { xb1, frontY, zb, 0, 0, rgb[0], rgb[1], rgb[2], 1.0f, 0.0f, ny, 0.0f };
            quad[5] = { xb0, frontY, zb, 0, 0, rgb[0], rgb[1], rgb[2], 1.0f, 0.0f, ny, 0.0f };

            out.insert (out.end(), quad, quad + 6);
        };

        auto  pushBand = [&pushQuad] (const std::vector<float> & xs, float za, float zb,
                                      const float * rgb)
        {
            for (size_t i = 0; i + 1 < xs.size(); i += 2)
            {
                pushQuad (xs[i], xs[i + 1], za, xs[i], xs[i + 1], zb, rgb);
            }
        };

        for (int fy = 0; fy < fh; fy++)
        {
            const std::vector<float> &  a    = cross[(size_t) fy];
            const float *               rgb  = rowRgb + (size_t) (fy / subdivide) * 3;
            float                       zMid = topZMm - ((float) fy + 0.5f) * fcell;

            // The half rows at the very top and bottom, which no pair spans.
            if (fy == 0)
            {
                pushBand (a, topZMm, zMid, rgb);
            }

            if (fy + 1 >= fh)
            {
                pushBand (a, zMid, topZMm - heightMm, rgb);
                continue;
            }

            {
                const std::vector<float> &  b     = cross[(size_t) fy + 1];
                const float *               rgbB  = rowRgb + (size_t) ((fy + 1) / subdivide) * 3;
                float                       zNext = zMid - fcell;

                if (a.size() == b.size() && (a.size() % 2) == 0)
                {
                    for (size_t i = 0; i + 1 < a.size(); i += 2)
                    {
                        pushQuad (a[i], a[i + 1], zMid, b[i], b[i + 1], zNext, rgb);
                    }

                    continue;
                }

                // The spans do not correspond, so there is nothing to join.
                // Each row keeps its own half of the gap.
                pushBand (a, zMid, (zMid + zNext) * 0.5f, rgb);
                pushBand (b, (zMid + zNext) * 0.5f, zNext, rgbB);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::BuildBrandSolid
//
//  The cassowary as a solid with a smoothed outline and a rounded top edge.
//  Its silhouette and stripe colors, handed to the general relief builder.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::BuildBrandSolid (float leftMm, float topZMm, float heightMm, float frontY,
                                      float thicknessMm, int firstRow, int lastRow)
{
    constexpr float  kRollMm = 0.14f;    // how far down the top edge rolls



    std::vector<uint8_t>  mask;
    std::vector<float>    rgb;

    BrandMask (mask, rgb, firstRow, lastRow);

    // The flat interior stays UNLIT: it is most of the mark, and the brand's
    // colors have to come out exact. Only the rolled band and the side walls
    // shade, which is the whole of what reads as depth.
    BuildRelief (m_opaque, mask.data(), CassoBranding::kGridW, CassoBranding::kGridH,
                 leftMm, topZMm, heightMm, frontY, thicknessMm, kRollMm, rgb.data(), false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::BuildRelief
//
//  A mask as a solid with a smoothed outline and a rounded top edge.
//
//  The mask's own resolution is the problem this works around. A brand or a
//  logotype is a few dozen cells across, so one cell lands on a screen pixel
//  or two, and both the staircase edges and any round-over worth having are
//  the same order as a cell -- rounding the raw silhouette by a case's own
//  0.35 mm would erase a one-cell feature outright, taking the cassowary's
//  beak serrations and its leg with it.
//
//  So the mask is RESAMPLED instead of offset. Coverage is measured over a
//  disc a little wider than a cell and thresholded at half, which rounds the
//  staircase off while holding the mark's area and its concavities, and the
//  same field then gives a distance to the outline: cap height ramps from the
//  full face down over the last fraction of a millimeter, so the top edge
//  rolls over rather than breaking square. Erosion never enters it, which is
//  why thin features survive.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::BuildRelief (std::vector<Dxui3DRenderer::Vertex> & out,
                                  const uint8_t * mask, int gridW, int gridH,
                                  float leftMm, float topZMm, float heightMm, float frontY,
                                  float thicknessMm, float rollMm,
                                  const float * rowRgb, bool litFace,
                                  float smoothCells, int superSample)
{
    const int    kSuper   = superSample;
    const float  kSmoothR = smoothCells;



    const int    fw     = gridW * kSuper;
    const int    fh     = gridH * kSuper;
    const int    vw     = fw + 1;
    const float  cell   = heightMm / (float) gridH;
    const float  fcell  = cell / (float) kSuper;
    const float  scale  = 2.0f * kSmoothR * cell;   // coverage units -> mm
    const float  backY  = frontY + thicknessMm;
    const int    span   = (int) std::ceil (kSmoothR) + 1;

    std::vector<float>    height ((size_t) vw * (fh + 1), 0.0f);
    std::vector<uint8_t>  inside ((size_t) fw * fh, 0);

    // Vertex pass: coverage -> signed distance to the outline -> cap height.
    for (int vy = 0; vy <= fh; vy++)
    {
        float  oy = (float) vy / (float) kSuper;

        for (int vx = 0; vx <= fw; vx++)
        {
            float  ox   = (float) vx / (float) kSuper;
            int    hits = 0;
            int    seen = 0;

            for (int dy = -span; dy <= span; dy++)
            {
                for (int dx = -span; dx <= span; dx++)
                {
                    int    sy = (int) std::floor (oy) + dy;
                    int    sx = (int) std::floor (ox) + dx;
                    float  ax = (float) sx + 0.5f - ox;
                    float  az = (float) sy + 0.5f - oy;

                    if (ax * ax + az * az > kSmoothR * kSmoothR)
                    {
                        continue;
                    }

                    seen++;

                    if (sy >= 0 && sy < gridH && sx >= 0 && sx < gridW &&
                        mask[(size_t) sy * gridW + sx] != 0)
                    {
                        hits++;
                    }
                }
            }

            {
                float  cov  = (seen > 0) ? (float) hits / (float) seen : 0.0f;
                float  dist = (cov - 0.5f) * scale;
                float  t    = (std::min) (1.0f, (std::max) (0.0f, dist / rollMm));

                height[(size_t) vy * vw + vx] = frontY + rollMm * (1.0f - t);
            }
        }
    }

    for (int fy = 0; fy < fh; fy++)
    {
        for (int fx = 0; fx < fw; fx++)
        {
            // A cell belongs to the mark when its own corners sit at or inside
            // the outline -- the height field is already the smoothed shape, so
            // "not fully rolled out" is the test.
            float  h00 = height[(size_t) fy       * vw + fx];
            float  h10 = height[(size_t) fy       * vw + fx + 1];
            float  h01 = height[(size_t) (fy + 1) * vw + fx];
            float  h11 = height[(size_t) (fy + 1) * vw + fx + 1];
            float  avg = (h00 + h10 + h01 + h11) * 0.25f;

            inside[(size_t) fy * fw + fx] = (avg < frontY + rollMm) ? 1 : 0;
        }
    }

    {
        auto  pushTri = [&out] (const float a[3], const float b[3], const float c[3],
                                const float n[3], const float rgb[3])
        {
            Dxui3DRenderer::Vertex   tri[3] = {};

            tri[0] = { a[0], a[1], a[2], 0, 0, rgb[0], rgb[1], rgb[2], 1.0f, n[0], n[1], n[2] };
            tri[1] = { b[0], b[1], b[2], 0, 0, rgb[0], rgb[1], rgb[2], 1.0f, n[0], n[1], n[2] };
            tri[2] = { c[0], c[1], c[2], 0, 0, rgb[0], rgb[1], rgb[2], 1.0f, n[0], n[1], n[2] };

            out.insert (out.end(), tri, tri + 3);
        };

        auto  pushQuad = [&pushTri] (const float p00[3], const float p10[3],
                                     const float p11[3], const float p01[3],
                                     const float n[3], const float rgb[3])
        {
            pushTri (p00, p10, p11, n, rgb);
            pushTri (p00, p11, p01, n, rgb);
        };

        // A zero normal is what means unlit, so the flat interior's normal is
        // the caller's choice of ink or metal.
        const float  kFaceN[3] = { 0.0f, litFace ? -1.0f : 0.0f, 0.0f };

        for (int fy = 0; fy < fh; fy++)
        {
            const float *  rgb  = rowRgb + (size_t) (fy / kSuper) * 3;
            float          zTop = topZMm - (float) fy * fcell;
            float          zBot = zTop - fcell;
            int            fx   = 0;

            // Flat interior, merged into runs.
            while (fx < fw)
            {
                int  runStart = fx;

                while (fx < fw && inside[(size_t) fy * fw + fx] != 0 &&
                       height[(size_t) fy       * vw + fx]     <= frontY + 1e-4f &&
                       height[(size_t) fy       * vw + fx + 1] <= frontY + 1e-4f &&
                       height[(size_t) (fy + 1) * vw + fx]     <= frontY + 1e-4f &&
                       height[(size_t) (fy + 1) * vw + fx + 1] <= frontY + 1e-4f)
                {
                    fx++;
                }

                if (fx > runStart)
                {
                    float  x0     = leftMm + (float) runStart * fcell;
                    float  x1     = leftMm + (float) fx * fcell;
                    float  p00[3] = { x0, frontY, zTop };
                    float  p10[3] = { x1, frontY, zTop };
                    float  p11[3] = { x1, frontY, zBot };
                    float  p01[3] = { x0, frontY, zBot };

                    pushQuad (p00, p10, p11, p01, kFaceN, rgb);
                    continue;
                }

                fx++;
            }

            // The rolled band and the walls, cell by cell.
            for (fx = 0; fx < fw; fx++)
            {
                float  h00 = height[(size_t) fy       * vw + fx];
                float  h10 = height[(size_t) fy       * vw + fx + 1];
                float  h01 = height[(size_t) (fy + 1) * vw + fx];
                float  h11 = height[(size_t) (fy + 1) * vw + fx + 1];
                float  x0  = leftMm + (float) fx * fcell;
                float  x1  = x0 + fcell;

                if (inside[(size_t) fy * fw + fx] == 0)
                {
                    continue;
                }

                if (h00 > frontY + 1e-4f || h10 > frontY + 1e-4f ||
                    h01 > frontY + 1e-4f || h11 > frontY + 1e-4f)
                {
                    float  p00[3] = { x0, h00, zTop };
                    float  p10[3] = { x1, h10, zTop };
                    float  p11[3] = { x1, h11, zBot };
                    float  p01[3] = { x0, h01, zBot };
                    float  n[3]   = { (h10 - h00) * fcell, -fcell * fcell, -(h01 - h00) * fcell };
                    float  len    = std::sqrt (n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);

                    if (len > 1e-6f)
                    {
                        n[0] /= len;
                        n[1] /= len;
                        n[2] /= len;
                        pushQuad (p00, p10, p11, p01, n, rgb);
                    }
                }

                // Side walls, wherever the neighbor is not part of the mark.
                if (fx == 0 || inside[(size_t) fy * fw + fx - 1] == 0)
                {
                    float  a[3] = { x0, h00,   zTop };
                    float  b[3] = { x0, h01,   zBot };
                    float  c[3] = { x0, backY, zBot };
                    float  d[3] = { x0, backY, zTop };
                    float  n[3] = { -1.0f, 0.0f, 0.0f };

                    pushQuad (a, b, c, d, n, rgb);
                }

                if (fx == fw - 1 || inside[(size_t) fy * fw + fx + 1] == 0)
                {
                    float  a[3] = { x1, h10,   zTop };
                    float  b[3] = { x1, h11,   zBot };
                    float  c[3] = { x1, backY, zBot };
                    float  d[3] = { x1, backY, zTop };
                    float  n[3] = { 1.0f, 0.0f, 0.0f };

                    pushQuad (a, b, c, d, n, rgb);
                }

                if (fy == 0 || inside[(size_t) (fy - 1) * fw + fx] == 0)
                {
                    float  a[3] = { x0, h00,   zTop };
                    float  b[3] = { x1, h10,   zTop };
                    float  c[3] = { x1, backY, zTop };
                    float  d[3] = { x0, backY, zTop };
                    float  n[3] = { 0.0f, 0.0f, 1.0f };

                    pushQuad (a, b, c, d, n, rgb);
                }

                if (fy == fh - 1 || inside[(size_t) (fy + 1) * fw + fx] == 0)
                {
                    float  a[3] = { x0, h01,   zBot };
                    float  b[3] = { x1, h11,   zBot };
                    float  c[3] = { x1, backY, zBot };
                    float  d[3] = { x0, backY, zBot };
                    float  n[3] = { 0.0f, 0.0f, -1.0f };

                    pushQuad (a, b, c, d, n, rgb);
                }
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::BuildPadlockStamp
//
//  The write-protect cue: a small brass padlock on the drive faceplate,
//  top-right of the plate -- the 2D widget's badge carried into the model.
//  Flat unlit quads (brand-stamp style) so the brass reads exactly; the
//  scene draws them only while the mounted disk is protected.
//
//  Every piece is an analytic SHAPE rasterized into horizontal runs, not a
//  quad standing in for one. The badge is ten millimeters across, so it has
//  no room for detail -- what it has instead is silhouette, and a padlock's
//  silhouette is round-topped and soft-cornered. Squared off, it read as a
//  briefcase.
//
//  Drawn back to front: outline, shackle, body, keyhole. The shackle's legs
//  run down INSIDE the body, which is why the body has to cover them rather
//  than meet them.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::BuildPadlockStamp (float rightX, float topZ, float frontY, float scale)
{
    // EVERY DIMENSION OF THE BADGE IS SCALED, not just its outline. The
    // constants below are the Disk II's, on a faceplate 90 mm tall; the //c's
    // is 46, and a badge sized for the one is a placard on the other. Scaling
    // the whole of it -- corners, shackle stock, keyhole bore -- is what keeps
    // it reading as the same mark rather than as a fatter one.
    //
    // The rasterization cell is NOT scaled. It is a resolution, not a
    // proportion, and a smaller badge simply gets relatively finer.
    const float  outlnMm = s_kPadlockOutlineMm * scale;
    const float  cornerR = s_kPadlockCornerR   * scale;
    const float  shackR  = s_kPadlockShackleR  * scale;
    const float  holeR   = s_kPadlockHoleR     * scale;
    const float  legTop  = s_kPadlockLegTopMm  * scale;
    const float  outY    = frontY - s_kPadlockOutlineDy;
    const float  bodyX1  = rightX;
    const float  bodyX0  = bodyX1 - s_kPadlockBodyW * scale;
    const float  archZ1  = topZ;
    const float  bodyZ1  = archZ1 - s_kPadlockArchH * scale;
    const float  bodyZ0  = bodyZ1 - s_kPadlockBodyH * scale;
    const float  holeCz  = bodyZ0 + s_kPadlockHoleCzUp * scale;
    const float  holeZ0  = bodyZ0 + s_kPadlockHoleZ0Up * scale;
    const float  cx      = (bodyX0 + bodyX1) * 0.5f;
    const float  archCz  = archZ1 - shackR;
    const float  innerR  = shackR - s_kPadlockShackleT * scale;



    // Recorded so the hit box follows the badge rather than being written out
    // a second time beside it -- which is how a badge and its tooltip target
    // come to disagree.
    m_padlockMin[0] = bodyX0;
    m_padlockMin[1] = frontY - s_kPadlockHoleDy;
    m_padlockMin[2] = bodyZ0;
    m_padlockMax[0] = bodyX1;
    m_padlockMax[1] = 0.0f;
    m_padlockMax[2] = archZ1;



    auto  pushRun = [this] (float x0, float x1, float z0, float z1, float y, const float rgb[3])
    {
        Dxui3DRenderer::Vertex   quad[6] = {};

        quad[0] = { x0, y, z1, 0, 0, rgb[0], rgb[1], rgb[2], 1.0f };
        quad[1] = { x1, y, z1, 0, 0, rgb[0], rgb[1], rgb[2], 1.0f };
        quad[2] = { x1, y, z0, 0, 0, rgb[0], rgb[1], rgb[2], 1.0f };
        quad[3] = { x0, y, z1, 0, 0, rgb[0], rgb[1], rgb[2], 1.0f };
        quad[4] = { x1, y, z0, 0, 0, rgb[0], rgb[1], rgb[2], 1.0f };
        quad[5] = { x0, y, z0, 0, 0, rgb[0], rgb[1], rgb[2], 1.0f };

        m_padlock.insert (m_padlock.end(), quad, quad + 6);
    };

    // Scans the badge's bounding box a cell at a time and merges each row's
    // covered span into one quad, which is the same trick the text and brand
    // stamps use -- a curve costs a few more runs, not a triangle per cell.
    auto  rasterize = [&pushRun, bodyX0, bodyX1, bodyZ0, archZ1, outlnMm]
                      (float y, const float rgb[3], auto && inside)
    {
        float  x0   = bodyX0 - outlnMm;
        float  x1   = bodyX1 + outlnMm;
        float  z0   = bodyZ0 - outlnMm;
        float  z1   = archZ1 + outlnMm;
        int    cols = (int) std::ceil ((x1 - x0) / s_kPadlockCellMm);
        int    rows = (int) std::ceil ((z1 - z0) / s_kPadlockCellMm);

        for (int row = 0; row < rows; row++)
        {
            float  zTop = z1 - (float) row * s_kPadlockCellMm;
            float  zMid = zTop - s_kPadlockCellMm * 0.5f;
            int    col  = 0;

            while (col < cols)
            {
                int  start = col;

                while (col < cols &&
                       inside (x0 + ((float) col + 0.5f) * s_kPadlockCellMm, zMid))
                {
                    col++;
                }

                if (col > start)
                {
                    pushRun (x0 + (float) start * s_kPadlockCellMm,
                             x0 + (float) col * s_kPadlockCellMm,
                             zTop - s_kPadlockCellMm, zTop, y, rgb);
                    continue;
                }

                col++;
            }
        }
    };

    // The U: an annulus above its center, two straight legs below it. The
    // legs run PAST the body's top edge, so the body covers their ends and
    // the shackle reads as passing into it rather than resting on it.
    auto  shackle = [&] (float x, float z, float grow)
    {
        float  dx = x - cx;

        if (z >= archCz)
        {
            float  r = std::sqrt (dx * dx + (z - archCz) * (z - archCz));

            return r >= innerR + grow && r <= shackR + grow;
        }

        return z >= bodyZ1 - legTop &&
               std::fabs (dx) >= innerR + grow && std::fabs (dx) <= shackR + grow;
    };

    // The body: a rounded rectangle, as the distance from the rect its
    // corner centers describe.
    auto  body = [bodyX0, bodyX1, bodyZ0, bodyZ1, cornerR] (float x, float z, float grow)
    {
        float  r  = cornerR;
        float  ix = (std::min) (bodyX1 - r, (std::max) (bodyX0 + r, x));
        float  iz = (std::min) (bodyZ1 - r, (std::max) (bodyZ0 + r, z));
        float  dx = x - ix;
        float  dz = z - iz;

        return dx * dx + dz * dz <= (r + grow) * (r + grow);
    };

    // The keyhole: a bore with a tapered ward slot hanging under it.
    auto  keyhole = [&] (float x, float z)
    {
        float  dx = x - cx;

        if (z >= holeCz)
        {
            return dx * dx + (z - holeCz) * (z - holeCz) <= holeR * holeR;
        }

        {
            // Barely tapered, and always well inside the bore. Flared it
            // read as an ARROWHEAD -- a disc over a widening triangle is
            // an arrow before it is a keyhole. What makes it one is the
            // bore overhanging a slot narrower than itself the whole way.
            float  t  = (holeCz - z) / (holeCz - holeZ0);
            float  hw = holeR * (0.46f + 0.10f * t);

            return z >= holeZ0 && std::fabs (dx) <= hw;
        }
    };

    rasterize (outY, s_kPadlockOutline, [&] (float x, float z)
    {
        return shackle (x, z, outlnMm) || body (x, z, outlnMm);
    });

    rasterize (frontY - s_kPadlockShackleDy, s_kPadlockShade, [&] (float x, float z)
    {
        return shackle (x, z, 0.0f);
    });

    rasterize (frontY - s_kPadlockBodyDy, s_kPadlockFill, [&] (float x, float z)
    {
        return body (x, z, 0.0f);
    });

    rasterize (frontY - s_kPadlockHoleDy, s_kPadlockHole, keyhole);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::PoseDoor
//
//  The door at `progress`, by whichever motion this drive has.
//
//  One entry point, because the alternative is every caller choosing -- and
//  the only choice a caller reaches for is the one it already knows, which is
//  how the //c's latch came to be posed as a rotation twice.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::PoseDoor (float progress, std::vector<Dxui3DRenderer::Vertex> & out) const
{
    if (m_doorMotion == DeskDoorMotion::InThenUp)
    {
        SlideDoorVerts (m_door, kDisk2cDoorInMm, kDisk2cDoorUpMm, progress, out);
        return;
    }

    RotateDoorVerts (m_door, m_doorPivotY, m_doorPivotZ, progress * m_doorOpenRad, out);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::GetDoorBoundsAt
//
//  The shut door's eight corners run through the real motion, and the box
//  around where they land.
//
//  THROUGH THE MOTION, not through a copy of it. Both mechanisms are pure
//  per-vertex transforms, so eight corners pose exactly as the door's
//  thousands do -- and the target cannot drift away from the part, which is
//  the failure a hand-written second box invites every time either one is
//  touched.
//
//  For the turning door the result is the box around the rotated corners
//  rather than the rotated box, so it is a little larger than the door at
//  intermediate angles. That is the harmless direction: this is a click
//  target, and generous beats a pixel hunt.
//
////////////////////////////////////////////////////////////////////////////////

bool DeskSceneModel::GetDoorBoundsAt (float progress, float outMin[3], float outMax[3]) const
{
    std::vector<Dxui3DRenderer::Vertex>  corners;
    std::vector<Dxui3DRenderer::Vertex>  posed;
    float                                lo[3] = {  FLT_MAX,  FLT_MAX,  FLT_MAX };
    float                                hi[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };



    if (m_door.empty())
    {
        return false;
    }

    for (int corner = 0; corner < 8; corner++)
    {
        Dxui3DRenderer::Vertex  v = {};

        v.x = (corner & 1) ? m_doorMax[0] : m_doorMin[0];
        v.y = (corner & 2) ? m_doorMax[1] : m_doorMin[1];
        v.z = (corner & 4) ? m_doorMax[2] : m_doorMin[2];

        corners.push_back (v);
    }

    if (m_doorMotion == DeskDoorMotion::InThenUp)
    {
        SlideDoorVerts (corners, kDisk2cDoorInMm, kDisk2cDoorUpMm,
                        std::clamp (progress, 0.0f, 1.0f), posed);
    }
    else
    {
        RotateDoorVerts (corners, m_doorPivotY, m_doorPivotZ,
                         std::clamp (progress, 0.0f, 1.0f) * m_doorOpenRad, posed);
    }

    for (const Dxui3DRenderer::Vertex & v : posed)
    {
        lo[0] = std::min (lo[0], v.x);  hi[0] = std::max (hi[0], v.x);
        lo[1] = std::min (lo[1], v.y);  hi[1] = std::max (hi[1], v.y);
        lo[2] = std::min (lo[2], v.z);  hi[2] = std::max (hi[2], v.z);
    }

    memcpy (outMin, lo, sizeof (lo));
    memcpy (outMax, hi, sizeof (hi));

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::SlideDoorVerts
//
//  Straight in, then straight up. No rotation anywhere in it.
//
//  TWO LEGS, not a diagonal: the latch has to be clear of the face before it
//  can rise, so it travels back and then up. A single interpolated move would
//  drag it up through the case front it is still sitting against.
//
//  BUT NOT HALF THE TIME EACH, which is what made the corner stutter. The
//  legs are 4 mm and 13 mm long; giving each half the animation ran the first
//  at a crawl and then trebled the speed at the turn. The corner has to be
//  split by DISTANCE, not by count -- then the latch travels at one speed the
//  whole way and the turn is a turn rather than a lurch.
//
//  So the motion is walked as a PATH AT CONSTANT SPEED rather than as two
//  interpolations sharing a timeline: straight leg, quarter arc, straight
//  leg, stepped along by arc length. The arc is there because even at one
//  speed a right-angle turn taken between two frames changes direction
//  instantaneously, and the eye reads that as a hitch whatever the speed is
//  doing. Overlapping two eased legs was tried first and still dipped thirty
//  percent through the corner -- the in-leg is coasting to rest exactly while
//  the up-leg is only starting to move. An arc has no such gap in it.
//
//  One smoothstep over the whole path eases the start and the stop, because
//  the caller's progress is raw elapsed-over-duration -- see
//  DriveWidgetState::TickDoorAnimation -- and nothing else will.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::SlideDoorVerts (const std::vector<Dxui3DRenderer::Vertex> & base,
                                     float                                       inMm,
                                     float                                       upMm,
                                     float                                       progress,
                                     std::vector<Dxui3DRenderer::Vertex>       & out)
{
    // Magnitudes here, signs at the end: the path is built in the first
    // quadrant and mirrored onto whichever way the caller's legs run.
    float   signY = (inMm < 0.0f) ? -1.0f : 1.0f;
    float   signZ = (upMm < 0.0f) ? -1.0f : 1.0f;
    float   legY  = std::fabs (inMm);
    float   legZ  = std::fabs (upMm);
    float   bend  = (std::min) (s_kDoorCornerMm, (std::min) (legY, legZ));
    float   runY  = legY - bend;
    float   arc   = 1.5707963f * bend;
    float   runZ  = legZ - bend;
    float   walk  = runY + arc + runZ;
    float   t     = std::clamp (progress, 0.0f, 1.0f);
    float   here  = walk * (t * t * (3.0f - 2.0f * t));
    float   dy    = 0.0f;
    float   dz    = 0.0f;



    if (here <= runY)
    {
        dy = here;
    }
    else if (here <= runY + arc)
    {
        float  turned = (here - runY) / (std::max) (bend, 1e-4f);

        dy = runY + bend * std::sin (turned);
        dz = bend * (1.0f - std::cos (turned));
    }
    else
    {
        dy = legY;
        dz = bend + (here - runY - arc);
    }

    dy *= signY;
    dz *= signZ;



    out = base;

    for (Dxui3DRenderer::Vertex & v : out)
    {
        v.y += dy;
        v.z += dz;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::RotateDoorVerts
//
//  Rigid rotation about the hinge -- the X-axis line at (pivotY, pivotZ).
//  The signs put a point below the hinge out toward the viewer (-Y) as the
//  angle grows: at 90 degrees the door lies flat, pointing at the camera.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::RotateDoorVerts (const std::vector<Dxui3DRenderer::Vertex> & base,
                                      float                                       pivotY,
                                      float                                       pivotZ,
                                      float                                       angleRad,
                                      std::vector<Dxui3DRenderer::Vertex>       & out)
{
    float   c = std::cos (angleRad);
    float   s = std::sin (angleRad);



    out = base;

    for (Dxui3DRenderer::Vertex & v : out)
    {
        float   dy = v.y - pivotY;
        float   dz = v.z - pivotZ;

        v.y = pivotY + dy * c + dz * s;
        v.z = pivotZ - dy * s + dz * c;
    }
}







// The "disk ][" logotype as it is set on the drive's lid: a SILHOUETTE, not
// type. The mark is a logotype rather than a string -- its lowercase d has a
// slab that no font of ours has, and the ][ is a pair of drawn bars, not two
// bracket characters -- so it is stored as the shape it is, the same way the
// cassowary is.
//
// Rows read front-to-back on the lid: row 0 is the mark's top, which is the
// edge AWAY from the viewer, so the label reads right way up to someone
// standing at the drive.
static const char * const  s_kDiskWordmark[] =
{
    "....................######....................................#######.#######",
    ".............######.######...............######...............#######.#######",
    ".............######.######...............######...............#######.#######",
    ".............######.######...............######...............#######.#######",
    ".............######......................######...............#######.#######",
    ".............######............########..######....#######......#####.#####..",
    ".....#######.######.######...###########.######...########......#####.#####..",
    "....########.######.######..############.######..#########......#####.#####..",
    "...#########.######.######.#############.################.......#####.#####..",
    "..##########.######.######.#############.###############........#####.#####..",
    ".###########.######.######.#############.##############.........#####.#####..",
    ".########....######.######.#####.........############...........#####.#####..",
    "#######......######.######.###########...###########............#####.#####..",
    "######.......######.######.############..##########.............#####.#####..",
    "######.......######.######..############.##########.............#####.#####..",
    "######.......######.######..############.############...........#####.#####..",
    "#######......######.######........######.#############..........#####.#####..",
    "########.....######.######........######.##############.........#####.#####..",
    ".##################.######.#############.###############......#######.#######",
    ".##################.######.#############.######.##########....#######.#######",
    "..#################.######.#############.######..#########....#######.#######",
    "...################.######.############..######....#######....#######.#######",
    ".....##############.######.##########.....#####.....######...................",
};

static constexpr int  s_kWordmarkRows = (int) (sizeof (s_kDiskWordmark) / sizeof (s_kDiskWordmark[0]));
static constexpr int  s_kWordmarkCols = 77;





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::BuildInUseStamp
//
//  "IN USE" and its pointer, set to the left margin and CENTERED ON THE LAMP
//  -- the legend labels the lamp, so its middle is the lamp's middle rather
//  than a z chosen to look level with it.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::BuildInUseStamp()
{
    float               cell = s_kInUseCapMm / (float) kInUseMaskRows;
    float               topZ = s_kInUseLampZ + (float) kInUseMaskRows * cell * 0.5f;
    std::vector<float>  ink ((size_t) kInUseMaskRows * 3);



    for (int row = 0; row < kInUseMaskRows; row++)
    {
        ink[(size_t) row * 3 + 0] = s_kFaceInkRgb[0];
        ink[(size_t) row * 3 + 1] = s_kFaceInkRgb[1];
        ink[(size_t) row * 3 + 2] = s_kFaceInkRgb[2];
    }

    StampFaceMask (m_opaque, s_kInUseMask, kInUseMaskRows, kInUseMaskCols,
                   s_kInUseLeftMm, topZ, cell, s_kInUseFrontY, ink.data(), true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::BuildWordmarkStamp
//
//  The "disk ][" logotype, EMBOSSED low on the faceplate below the IN-USE
//  legend and its lamp -- opposite the cassowary, which holds the other
//  corner. Bottom-aligned with the cassowary on the face margin, so the two
//  marks sit on one line.
//
//  A SILHOUETTE, not type: the mark's lowercase d carries a slab no font of
//  ours has and its ][ is a pair of drawn bars rather than two bracket
//  characters, so it is stored as the shape it is.
//
//  It goes through BuildRelief rather than being laid flat, for two reasons
//  that happen to want the same thing. It is MOLDED on the real drive, so it
//  needs a lit top edge and a shadow under its bottom. And at 23 rows over
//  12 mm its own cells land around a screen pixel, so a flat stamp shows the
//  staircase -- the same coverage resample that rounds the cassowary's
//  outline rounds this one.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::BuildWordmarkStamp()
{
    // The mask is REFINED before it is smoothed. Its letters stand one cell
    // apart, and the coverage disc that rounds a staircase is about a cell
    // wide, so applied to the mask as drawn it welded d-i-s-k into one blob
    // -- the smoothing cannot tell a gap it is meant to keep from a step it
    // is meant to erase. Splitting every cell three ways changes neither the
    // shape nor its size, but it makes the same disc a third as wide, which
    // is small enough to round the steps and leave the gaps.
    //
    // Supersampling drops to match, because the mask now arrives fine: the
    // relief costs the square of it in work and in triangles, and paying 4x
    // on top of 3x would be paying for the same resolution twice.
    constexpr int    kRefine = 3;
    constexpr int    kSuper  = 2;
    constexpr int    kErode  = 2;



    const int             cols     = s_kWordmarkCols * kRefine;
    const int             rows     = s_kWordmarkRows * kRefine;
    float                 heightMm = (float) s_kWordmarkRows * s_kWordCellMm;
    std::vector<uint8_t>  mask ((size_t) rows * cols, 0);
    std::vector<float>    ink ((size_t) rows * 3);

    for (int row = 0; row < rows; row++)
    {
        const char *  bits = s_kDiskWordmark[row / kRefine];

        for (int col = 0; col < cols; col++)
        {
            mask[(size_t) row * cols + col] = (bits[col / kRefine] == '#') ? (uint8_t) 1
                                                                           : (uint8_t) 0;
        }

        ink[(size_t) row * 3 + 0] = s_kFaceInkRgb[0];
        ink[(size_t) row * 3 + 1] = s_kFaceInkRgb[1];
        ink[(size_t) row * 3 + 2] = s_kFaceInkRgb[2];
    }

    // REFINED CELLS OFF EVERY SIDE, which is where the weight comes off. The
    // mark is drawn in whole cells, so it has no way to say "two thirds of a
    // cell thinner" -- but the refined grid does, and this takes a six-cell
    // stroke to about four and two thirds without touching what the letters
    // are. Done here rather than by lowering the relief's ink threshold: the
    // threshold erodes hardest exactly where a shape turns a corner, and this
    // mark's corners are what carry it.
    for (int pass = 0; pass < kErode; pass++)
    {
        std::vector<uint8_t>  thin (mask);

        for (int row = 0; row < rows; row++)
        {
            for (int col = 0; col < cols; col++)
            {
                bool  edge = (row == 0 || col == 0 || row == rows - 1 || col == cols - 1)
                             || mask[(size_t) (row - 1) * cols + col] == 0
                             || mask[(size_t) (row + 1) * cols + col] == 0
                             || mask[(size_t) row * cols + col - 1] == 0
                             || mask[(size_t) row * cols + col + 1] == 0;

                if (edge)
                {
                    thin[(size_t) row * cols + col] = 0;
                }
            }
        }

        mask.swap (thin);
    }

    // Wider than the cassowary's disc, because the steps being rounded here
    // are a whole drawn cell rather than a refined one. Held under the gaps'
    // three refined cells, which is what keeps the letters apart.
    constexpr float  kSmooth = 1.9f;

    BuildRelief (m_opaque, mask.data(), cols, rows,
                 s_kWordLeftMm, s_kFaceMarginMm + heightMm, heightMm, s_kWordFrontY,
                 s_kWordThickMm, s_kWordRollMm, ink.data(), true, kSmooth, kSuper);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::StampFaceMask
//
//  A silhouette laid on a front face, one merged quad per horizontal run.
//
//  `lit` is the difference between ink and metal. A printed mark wants it
//  off, so its colors come out exactly as specified and the surface under it
//  carries all the shading the eye needs. The faceplate legends want it on,
//  because they are silver: what reads as metal is a highlight that MOVES as
//  the view does, and an unlit quad can be the right lightness but can never
//  be shiny.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::StampFaceMask (std::vector<Dxui3DRenderer::Vertex> & out,
                                    const char * const                  * rows,
                                    int                                   rowCount,
                                    int                                   colCount,
                                    float                                 leftMm,
                                    float                                 topZMm,
                                    float                                 cellMm,
                                    float                                 frontY,
                                    const float                         * rowRgb,
                                    bool                                  lit)
{
    float  ny = lit ? -1.0f : 0.0f;   // a zero normal is what means unlit



    for (int row = 0; row < rowCount; row++)
    {
        const char *  bits = rows[row];
        float         zTop = topZMm - (float) row * cellMm;
        float         z1   = zTop - cellMm;
        float         r    = rowRgb[row * 3 + 0];
        float         g    = rowRgb[row * 3 + 1];
        float         b    = rowRgb[row * 3 + 2];
        int           col  = 0;

        while (col < colCount)
        {
            int    runStart = 0;
            float  x0       = 0.0f;
            float  x1       = 0.0f;

            if (bits[col] != '#')
            {
                col++;
                continue;
            }

            runStart = col;

            while (col < colCount && bits[col] == '#')
            {
                col++;
            }

            x0 = leftMm + (float) runStart * cellMm;
            x1 = leftMm + (float) col * cellMm;

            {
                Dxui3DRenderer::Vertex   quad[6] = {};

                quad[0] = { x0, frontY, zTop, 0, 0, r, g, b, 1.0f, 0.0f, ny, 0.0f };
                quad[1] = { x1, frontY, zTop, 0, 0, r, g, b, 1.0f, 0.0f, ny, 0.0f };
                quad[2] = { x1, frontY, z1,   0, 0, r, g, b, 1.0f, 0.0f, ny, 0.0f };
                quad[3] = { x0, frontY, zTop, 0, 0, r, g, b, 1.0f, 0.0f, ny, 0.0f };
                quad[4] = { x1, frontY, z1,   0, 0, r, g, b, 1.0f, 0.0f, ny, 0.0f };
                quad[5] = { x0, frontY, z1,   0, 0, r, g, b, 1.0f, 0.0f, ny, 0.0f };

                out.insert (out.end(), quad, quad + 6);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::StampDriveLabel
//
//  "DRIVE n" from its baked masks, set to the faceplate's top-left margin.
//
//  Sized by CAP HEIGHT rather than by cell: each mask is exactly one cap
//  tall, so the cell falls out of the size wanted instead of being a number
//  tuned until the label looked right. Rebaking at a different resolution
//  then changes nothing about how big the legend is.
//
//  The word and the number are separate masks because they are set at
//  different sizes, and they share a CENTERLINE rather than a baseline: the
//  number is what the legend is for, so it is the taller of the two and it is
//  what the top margin holds, with the word centered on it.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::StampDriveLabel (std::vector<Dxui3DRenderer::Vertex> & out, int driveNumber)
{
    const char * const *  digit     = (driveNumber >= 2) ? s_kDrive2Mask : s_kDrive1Mask;
    int                   digitRows = (driveNumber >= 2) ? kDrive2MaskRows : kDrive1MaskRows;
    int                   digitCols = (driveNumber >= 2) ? kDrive2MaskCols : kDrive1MaskCols;
    float                 digitCap  = s_kDriveLabelCapMm * s_kDriveNumberScale;
    float                 centerZ   = s_kDriveLabelTopZMm - digitCap * 0.5f;
    float                 wordCell  = s_kDriveLabelCapMm / (float) kDriveWordMaskRows;
    float                 wordWidth = (float) kDriveWordMaskCols * wordCell;
    std::vector<float>    ink ((size_t) (std::max) (kDriveWordMaskRows, digitRows) * 3);



    for (size_t row = 0; row < ink.size() / 3; row++)
    {
        ink[row * 3 + 0] = s_kFaceInkRgb[0];
        ink[row * 3 + 1] = s_kFaceInkRgb[1];
        ink[row * 3 + 2] = s_kFaceInkRgb[2];
    }

    StampFaceMask (out, s_kDriveWordMask, kDriveWordMaskRows, kDriveWordMaskCols,
                   s_kDriveLabelLeftMm, centerZ + s_kDriveLabelCapMm * 0.5f,
                   wordCell, s_kDriveLabelFrontY, ink.data(), true);

    StampFaceMask (out, digit, digitRows, digitCols,
                   s_kDriveLabelLeftMm + wordWidth + s_kDriveNumberGapMm,
                   centerZ + digitCap * 0.5f,
                   digitCap / (float) digitRows, s_kDriveLabelFrontY, ink.data(), true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::AddRegionBoxes
//
//  Declaration order is precedence: the eject region sits inside the body
//  box and must list first, mirroring DriveWidget::HitTest checking the
//  eject rect before the body rect.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::AddRegionBoxes()
{
    DeskRegionBox   box;



    if (!IsDriveKind (m_kind))
    {
        return;
    }

    // Per drive, because the two faces are laid out differently and a box
    // measured against the wrong one is a click that lands nowhere. The
    // //c's eject zone wraps its slot and the open notch beneath it.
    if (m_kind == DeskDeviceKind::Disk2c)
    {
        memcpy (box.boxMin, s_kDisk2cEjectMin, sizeof (box.boxMin));
        memcpy (box.boxMax, s_kDisk2cEjectMax, sizeof (box.boxMax));
    }
    else
    {
        memcpy (box.boxMin, s_kDiskIiEjectMin, sizeof (box.boxMin));
        memcpy (box.boxMax, s_kDiskIiEjectMax, sizeof (box.boxMax));
    }

    box.region = DriveWidgetRegion::Eject;
    m_regions.push_back (box);

    // The //c's second eject box: the notch column, which is the finger
    // recess and the latch and the latch's top all in one line.
    if (m_kind == DeskDeviceKind::Disk2c)
    {
        memcpy (box.boxMin, s_kDisk2cLatchMin, sizeof (box.boxMin));
        memcpy (box.boxMax, s_kDisk2cLatchMax, sizeof (box.boxMax));

        box.region = DriveWidgetRegion::Eject;
        m_regions.push_back (box);
    }

    // The padlock's box, IF THERE IS A PADLOCK. There is not, on either drive,
    // now that the badge hangs beside the disk's name instead of being stamped
    // on the case -- but the builder is still here and still correct, and a
    // model that calls it deserves the region that goes with it. Without this
    // guard an unstamped drive would push a degenerate box at the model's
    // origin, padded into a real target sitting on the case's bottom-left
    // corner, silently taking clicks that mean browse.
    //
    // It ranks BELOW eject and above body: its box can overlap the eject
    // zone, declaration order is precedence, and listing it first would take
    // clicks away from eject for the sake of a tooltip. Slack all round so
    // the badge is not a pixel hunt, and the near face reaches in front of
    // the plate it stands on.
    if (!m_padlock.empty())
    {
        box.boxMin[0] = m_padlockMin[0] - s_kPadlockHitPadMm;
        box.boxMin[1] = m_padlockMin[1] - 1.0f;
        box.boxMin[2] = m_padlockMin[2] - s_kPadlockHitPadMm;
        box.boxMax[0] = m_padlockMax[0] + s_kPadlockHitPadMm;
        box.boxMax[1] = 0.5f;
        box.boxMax[2] = m_padlockMax[2] + s_kPadlockHitPadMm;
        box.region    = DriveWidgetRegion::Padlock;
        m_regions.push_back (box);
    }

    if (m_kind == DeskDeviceKind::Disk2c)
    {
        memcpy (box.boxMin, s_kDisk2cBodyMin, sizeof (box.boxMin));
        memcpy (box.boxMax, s_kDisk2cBodyMax, sizeof (box.boxMax));
    }
    else
    {
        memcpy (box.boxMin, s_kDiskIiBodyMin, sizeof (box.boxMin));
        memcpy (box.boxMax, s_kDiskIiBodyMax, sizeof (box.boxMax));
    }

    box.region = DriveWidgetRegion::Body;
    m_regions.push_back (box);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::GrowTiltGrip
//
//  Grows the grip for one tilt mark to cover the triangles just appended.
//
//  The mark's own extent IS the grip. An invisible pad sized by hand would
//  drift the moment the glyph moved on the bezel, and the whole point of a
//  grabbable mark is that what you aim at is what you grab.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::GrowTiltGrip (int direction, size_t firstVert)
{
    DeskTiltGrip *  grip = nullptr;



    for (DeskTiltGrip & candidate : m_tiltGrips)
    {
        if (candidate.direction == direction)
        {
            grip = &candidate;
            break;
        }
    }

    if (grip == nullptr)
    {
        DeskTiltGrip  fresh;

        fresh.direction = direction;
        fresh.boxMin[0] = fresh.boxMin[1] = fresh.boxMin[2] =  FLT_MAX;
        fresh.boxMax[0] = fresh.boxMax[1] = fresh.boxMax[2] = -FLT_MAX;

        m_tiltGrips.push_back (fresh);
        grip = &m_tiltGrips.back();
    }

    for (size_t i = firstVert; i < m_tiltable.size(); i++)
    {
        const Dxui3DRenderer::Vertex &  v = m_tiltable[i];

        grip->boxMin[0] = std::min (grip->boxMin[0], v.x);  grip->boxMax[0] = std::max (grip->boxMax[0], v.x);
        grip->boxMin[1] = std::min (grip->boxMin[1], v.y);  grip->boxMax[1] = std::max (grip->boxMax[1], v.y);
        grip->boxMin[2] = std::min (grip->boxMin[2], v.z);  grip->boxMax[2] = std::max (grip->boxMax[2], v.z);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::ComputeTiltTravel
//
//  Where the assembly pivots, and how far it may go.
//
//  THE LIMIT IS FLUSH, and it is measured rather than chosen: the bezel
//  stands proud of the frame around it, and the tilt stops at the angle where
//  the leading edge has swung back level with that frame. Past there the two
//  would interpenetrate, which is exactly what a real bezel's travel is
//  bounded by.
//
//  Rotation is about the assembly's own center, so the edge going back and
//  the edge coming forward move by the same amount and one number bounds
//  both directions.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::ComputeTiltTravel()
{
    float   lo[3]  = { FLT_MAX, FLT_MAX, FLT_MAX };
    float   hi[3]  = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    float   half   = 0.0f;
    float   proud  = 0.0f;



    m_maxTiltRad = 0.0f;

    if (m_tiltable.empty())
    {
        return;
    }

    for (const Dxui3DRenderer::Vertex & v : m_tiltable)
    {
        lo[0] = std::min (lo[0], v.x);  hi[0] = std::max (hi[0], v.x);
        lo[1] = std::min (lo[1], v.y);  hi[1] = std::max (hi[1], v.y);
        lo[2] = std::min (lo[2], v.z);  hi[2] = std::max (hi[2], v.z);
    }

    m_tiltPivotY = (lo[1] + hi[1]) * 0.5f;
    m_tiltPivotZ = (lo[2] + hi[2]) * 0.5f;

    // How far the leading edge has to travel to come level with the frame,
    // and how much leverage the assembly's half-height gives it. -Y is toward
    // the viewer, so the bezel's front is the SMALLER y.
    half  = (hi[2] - lo[2]) * 0.5f;
    proud = m_frontPlaneY - lo[1];

    if (half <= 0.0f || proud <= 0.0f)
    {
        return;
    }

    m_maxTiltRad = std::asin (std::min (proud / half, 1.0f));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::ComputeBounds
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::ComputeBounds()
{
    float   lo[3] = { FLT_MAX, FLT_MAX, FLT_MAX };
    float   hi[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };



    for (const std::vector<Dxui3DRenderer::Vertex> * batch : { &m_opaque, &m_glass, &m_lamp, &m_door, &m_tiltable })
    {
        for (const Dxui3DRenderer::Vertex & v : *batch)
        {
            lo[0] = std::min (lo[0], v.x);  hi[0] = std::max (hi[0], v.x);
            lo[1] = std::min (lo[1], v.y);  hi[1] = std::max (hi[1], v.y);
            lo[2] = std::min (lo[2], v.z);  hi[2] = std::max (hi[2], v.z);
        }
    }

    memcpy (m_boundsMin, lo, sizeof (m_boundsMin));
    memcpy (m_boundsMax, hi, sizeof (m_boundsMax));

    ComputeGroundFootprint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::ComputeGroundFootprint
//
//  The XY extent of the geometry that actually TOUCHES the ground, not the
//  full bounding box: the monitor's bezel overhangs its shell by 20 mm and
//  the drives' lips overhang theirs, so a shadow sized to the box would jut
//  out in front of the object it belongs to. Anything within kGroundBandMm of
//  the model's lowest vertex counts as contact.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::ComputeGroundFootprint()
{
    float   height  = m_boundsMax[2] - m_boundsMin[2];
    float   band    = std::max (kGroundBandMm, height * kGroundBandFraction);
    float   ceiling = m_boundsMin[2] + band;
    float   lo[2]   = { FLT_MAX, FLT_MAX };
    float   hi[2]   = { -FLT_MAX, -FLT_MAX };



    for (const std::vector<Dxui3DRenderer::Vertex> * batch : { &m_opaque, &m_glass, &m_lamp, &m_door })
    {
        for (const Dxui3DRenderer::Vertex & v : *batch)
        {
            if (v.z <= ceiling)
            {
                lo[0] = std::min (lo[0], v.x);  hi[0] = std::max (hi[0], v.x);
                lo[1] = std::min (lo[1], v.y);  hi[1] = std::max (hi[1], v.y);
            }
        }
    }

    // No contact geometry, or a patch with no area on one axis (a case that
    // rests on a single edge), collapses to the box -- the honest fallback,
    // and the one that keeps a caller from having to special-case a
    // zero-width footprint.
    if (lo[0] >= hi[0] || lo[1] >= hi[1])
    {
        lo[0] = m_boundsMin[0];  hi[0] = m_boundsMax[0];
        lo[1] = m_boundsMin[1];  hi[1] = m_boundsMax[1];
    }

    m_footprintMin[0] = lo[0];  m_footprintMax[0] = hi[0];
    m_footprintMin[1] = lo[1];  m_footprintMax[1] = hi[1];
}
