#pragma once

#include "Pch.h"

#include "Devices/Tape/TapeDeck.h"
#include "Ui/Scene/DeskScene.h"
#include "Ui/Scene/DeskSceneHitTester.h"
#include "Ui/Scene/FullscreenStripState.h"



class EmulatorShell;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDeskScene
//
//  The 3D desk: the scene renderer with its monitor, drives and recorder,
//  the fullscreen drive strip, the baked labels and the chrome labels that
//  stand in for them, the scene compass, the recorder's key and reel
//  animation, the bezel tilt and the volume wheel, and the user's own
//  framing of the scene.
//
//  The shell's mouse, wheel and gesture handlers still drive the framing and
//  the drag state directly, so the shell is a friend; the handlers move here
//  once the input routing they sit in has been taken apart.
//
////////////////////////////////////////////////////////////////////////////////

class ShellDeskScene
{
    friend class EmulatorShell;

public:
    explicit ShellDeskScene (EmulatorShell & shell);
    ~ShellDeskScene();

    // The scene renderer itself, for the Settings > Theme preview and the
    // recorder's drop target.
    DeskScene &  GetScene () { return m_deskScene; }

    // The fullscreen strip's hotkey: summons the strip on the next frame.
    void  RequestStripHotkey () { m_stripHotkeyPending = true; }
    // Settings > Theme antialiasing, in SAMPLES (1 / 2 / 4). Applies to the
    // next frame and persists to GlobalUserPrefs; ApplySceneAntiAliasing is
    // the startup half, which pushes the stored value without re-saving it.
    void    SetSceneAntiAliasing   (int samples);
    void    ApplySceneAntiAliasing ();

    // A left-button orbit that has not yet travelled far enough to BE one.
    // The press arms it over anything the scene shows; only movement past
    // the slop turns it into a rotation, and a release before that lets the
    // click chain run as though nothing had been armed at all.
    static constexpr int  s_kSceneOrbitSlopPx = 4;

    // The most drives the desk scene ever composes -- DeskSceneComposition
    // sizes its world matrices to the same number.
    static constexpr int  s_kSceneDriveMax = 2;

    // Each drive's door hit box, posed to the openness that drive is showing.
    // Filled for every slot, degenerate where there is no drive or no door,
    // which the hit tester reads as "no door target here".
    void  BuildDriveDoorBoxes (DeskRegionBox (& out)[s_kSceneDriveMax]) const;

    // Whether a point falls inside the scene's OWN rect -- the band between
    // the chrome bands, which is what the composition is solved into. A
    // press outside it is on the toolbar, the status bar or the menu strip,
    // and a gesture that begins on the scene must not begin there.
    bool    PointInSceneRect     (int x, int y) const
    {
        const RECT &  vp = m_deskScene.Composition().viewportPx;

        return x >= vp.left && x < vp.right && y >= vp.top && y < vp.bottom;
    }

    // Initializes the desk scene renderer against the host device and loads
    // the embedded device models. Failure leaves the scene off (asserting
    // in debug -- a broken embedded asset is a build defect) and hands off
    // to FallBackFromDeskScene.
    HRESULT InitializeDeskScene  ();

    // Records that the desk scene failed and swaps a skeuomorphic chrome
    // theme for a compact one, which has drives that do not need the scene.
    void    FallBackFromDeskScene ();

    // Builds and applies the chrome theme for a theme name, with the desk
    // scene's availability taken into account.
    void    ApplyChromeThemeByName (const std::string & themeName);

    static constexpr const wchar_t *  kpszDeskSceneFallbackNotice =
        L"The 3D desk could not be loaded, so Casso is using the Dark Modern theme.";

    // Loads the monitor + drive pair the active machine wore (//c gets its
    // own platinum set, everything else the beige Monitor II over Disk IIs).
    // Called again on a machine switch.
    HRESULT LoadDeskSceneModelsForMachine ();

    // Resolves a client-px position against the composed scene (glass /
    // drive region / nothing).
    SceneHitResult  DeskSceneHit (int xPx, int yPx) const;

    // Resolves against the fullscreen strip's drives-and-recorder
    // composition (glass excluded -- its monitor placement is meaningless).
    SceneHitResult  StripHit     (int xPx, int yPx) const;

    // Whichever of those two holds the recorder the pointer can reach: the
    // strip while it is up in fullscreen, the desk otherwise.
    SceneHitResult  RecorderHit  (int xPx, int yPx) const;

    // How many drives the scene composes: the machine's Disk II presence and
    // the //c external-drive connection, the same gates the 2D widgets use.
    int     DeskSceneDriveCount  () const;

    // Zoom by `factor` about a client point, so whatever is under the cursor
    // stays under it. Zooming about the viewport CENTER instead would push
    // the thing being inspected off toward an edge exactly as it got big
    // enough to look at.
    void    ZoomSceneAt          (POINT clientPt, float factor);
    DxuiMessageResult  PanSceneByNotch (float notch, bool horizontal);
    void    OrbitSceneBy         (float yawRad, float pitchRad);
    void    BeginSceneOrbit      (int x, int y);
    void    UpdateSceneOrbit     (int x, int y);
    float   OrbitRadPerPx        () const;

    // Put the framing back to the fitted composition.
    void    ResetSceneView       ();

    // Clamp pan so the scene cannot be dragged entirely off-screen, and drop
    // the pan to zero once zoomed back out -- at 1.0 the composition already
    // fits, so an offset there is only ever a way to lose it.
    void    ClampSceneView       ();

    // Re-solve the composition for the current client size and repaint. The
    // framing feeds the same solve the viewport does, so there is no separate
    // "just the camera" path to keep in step with it.
    void    InvalidateSceneComposition ();

    // How far in and out the framing goes. The far end is where the model
    // stops rewarding a closer look -- past roughly 8x the mesh's own facets
    // are the subject -- and the near end is 1, the fitted composition, since
    // zooming out past a view that already contains everything only shrinks
    // it into the middle of an empty viewport.
    // Below 1 the fitted composition shrinks into the window with margin
    // around it -- the step-back look. Pan slack stays zero down there (see
    // ClampSceneView), so zooming back in cannot strand the scene off-center.
    static constexpr float  s_kSceneZoomMin     = 0.5f;
    static constexpr float  s_kScenePanFloorNdc = 1.0f;   // pan room at any zoom: half the viewport
    static constexpr float  s_kSceneZoomMax     = 8.0f;

    // One wheel notch. Geometric, so the same flick covers the same visual
    // proportion at every zoom -- a fixed additive step feels fast when close
    // in and useless when far out.
    static constexpr float  s_kSceneZoomStep = 1.15f;

    // How far one wheel notch pans, in units of the pan range -- which runs
    // -1..1 across the viewport, the same units the touch pan works in.
    static constexpr float  s_kScenePanStep  = 0.12f;

    // The inspection orbit's feel: a drag across the full viewport sweeps
    // this many radians (per-pixel is DERIVED from the viewport, because the
    // pixel coordinates the handlers see are DPI-scaled -- see
    // OrbitRadPerPx), and a Shift+slide turns this much per wheel notch. The
    // stored pitch is clamped a shade past the layout's own elevation limit
    // -- the layout clamps the TOTAL, seat included, so this one only stops
    // the value winding up unboundedly while pinned.
    static constexpr float  s_kOrbitDragSweepRad = 3.6f;
    static constexpr float  s_kOrbitRadPerNotch  = 0.06f;
    static constexpr float  s_kOrbitPitchLimit   = 1.6f;

    // While the scene owns the drives, the 2D widgets stay hidden (they keep
    // mirroring state for the //c switch strip) and the drag-drop hit rects
    // come from the composition's projected drive bounds.
    void    SyncSceneDriveChrome ();

    // Re-hangs the mounted-image basename strip under each projected drive.
    // The desk's baked labels: the two drives' names, the recorder's tape
    // name, its counter, the name of the key under the pointer, the title on
    // the cassette, and the two drives' info icons.
    static constexpr size_t  s_kSceneLabelCount    = 8;
    static constexpr int     s_kSceneTapeNameCell  = 2;
    static constexpr int     s_kSceneCounterCell   = 3;
    static constexpr int     s_kSceneKeyCell       = DeskScene::kTipLabel;
    static constexpr int     s_kSceneCassetteCell  = 5;   // the title written on the cassette itself
    static constexpr int     kSceneInfoIconCell    = 6;   // drive 0's info icon; drive 1's is the next cell

    // Where a baked label's quad sits relative to its anchor, when it is not
    // the whole cell centered under it: a drive name sharing its strip with
    // the info icon, and the icon itself. A width of zero means the default.
    struct SceneLabelSpan
    {
        LONG  offsetPx = 0;
        LONG  widthPx  = 0;
    };

    // The cassette title's pen: a handwriting face that ships with Windows 10
    // and later, in ballpoint blue, with no halo -- it is ink on paper, not a
    // caption. The text is sized down to fit the label rather than cut.
    static constexpr const wchar_t *  s_kpszCassetteInkFace     = L"Ink Free";
    static constexpr uint32_t         s_kCassetteInkArgb        = 0xFF101A5C;
    static constexpr float            s_kCassetteInkHeightRatio = 0.85f;  // of a row's height
    static constexpr LONG             s_kCassetteCellTall       = 4;      // the title's cell, in name strips: two rows of writing

    // How far each written letter strays, as fractions: its size, its rise
    // off the line (of the letter height), its spacing, its lean (as the
    // tangent of the slant), and how much lighter the ink can come out; and
    // how often a letter is pressed harder, drawn heavier.
    static constexpr float            s_kInkSizeWobble          = 0.10f;
    static constexpr float            s_kInkRiseWobble          = 0.08f;
    static constexpr float            s_kInkSpaceWobble         = 0.18f;
    static constexpr float            s_kInkSlantWobble         = 0.14f;
    static constexpr float            s_kInkPressWobble         = 0.25f;
    static constexpr float            s_kInkHeavyShare          = 0.3f;
    static constexpr float            s_kInkHeavyOffset         = 0.03f;  // of the letter size, between a heavy letter's two strokes
    static constexpr float            s_kCassetteInkRoom        = 0.9f;   // of the width, before a title goes to two rows
    static constexpr float            s_kInkSpaceEm             = 0.4f;   // a space between words, at the least

    // One written letter, as PaintHandwritten lays it out.
    struct InkGlyph
    {
        wchar_t  ch      = 0;
        float    size    = 0.0f;
        float    rise    = 0.0f;
        float    advance = 0.0f;
        float    slant   = 0.0f;
        float    ink     = 1.0f;
        bool     isHeavy = false;
    };

    void    SyncSceneDriveLabels ();
    bool    UpdateSceneLabelHover (int x, int y, int64_t nowMs);
    float   GetSceneLabelScrollPx (int drive, int64_t nowMs);
    bool    SyncRecorderKeys      (int64_t nowMs);
    void    SyncSceneTapeLabel    ();
    bool    GetRecorderLabelAnchor (const DeskSceneComposition & comp, int key, float anchor[3]);
    void    SyncStripTapeLabels    (const DeskSceneComposition & comp, bool onStrip,
                                    const std::array<std::wstring, s_kSceneLabelCount> & names);
    void    SetStripLabelMarquee   (DxuiShadowedText & label, int cell, const std::wstring & name, const RECT & rc);
    int     GetSceneLabelHalfWidthPx (const DeskSceneComposition & comp);
    bool    TryMakeCassetteTitleQuad (const DeskSceneComposition & comp, const SIZE & cellPx, float corners[4][3]);

    // Splits a drive's name strip between the name and the info icon after
    // it, with the 2D widget's rule: centered together when the name fits,
    // otherwise the icon at the right and the name scrolling in what is left.
    // Gives where each is drawn, halo included, and the icon's hover target.
    void    PlaceSceneNameAndIcon  (IDxuiTextRenderer   & text,
                                    const std::wstring  & name,
                                    float                 fontPx,
                                    const RECT          & strip,
                                    RECT                & outName,
                                    RECT                & outIcon,
                                    RECT                & outIconTarget) const;

    // The recorder's volume wheel on screen, where the pointer can reach it
    // (the strip in fullscreen, the desk otherwise); empty when it is not.
    RECT    GetVolumeWheelRect     (float * widthPx = nullptr) const;   // and the wheel's own width on screen
    void    DragVolumeWheel        (int x, int64_t nowMs);
    void    PersistTapeVolume      ();

    static std::wstring  FormatTapeVolumeTip (float gain);

    // Hands each drive's name to the scene as a depth-tested quad: bakes the
    // two strings into one texture when either has changed, then re-solves
    // the quads, which move whenever the camera does.
    void    SyncSceneDiskLabelQuads (const std::array<std::wstring, s_kSceneLabelCount> & names,
                                     const SIZE                        & cellPx,
                                     int                                 gapPx);

    // Draws both names into a single off-screen texture, stacked, and keeps
    // the view. One texture because the text renderer has only one: a second
    // bake replaces the first, so baking per drive would leave both wearing
    // whichever name went last.
    bool    TryMakeSceneLabelQuad   (const DeskSceneComposition & comp, int cell, const SIZE & cellPx,
                                     int gapPx, float corners[4][3]);
    bool    TryBakeSceneDiskLabels  (const std::array<std::wstring, s_kSceneLabelCount> & names,
                                     const SIZE                        & cellPx);

    // Where a label's cell starts in that texture, with room between cells
    // for each name's glow.
    static LONG GetSceneLabelCellTopPx    (int cell, const SIZE & cellPx);
    static LONG GetSceneLabelCellHeightPx (int cell, const SIZE & cellPx);
    LONG        GetSceneLabelCellWidthPx  (int cell, const SIZE & cellPx) const;

    static void  PaintHandwritten (IDxuiTextRenderer & text, const std::wstring & name, float top,
                                   float width, float height);
    static void  LayOutInk        (IDxuiTextRenderer & text, const std::wstring & name, float basePx,
                                   uint32_t & seed, std::vector<InkGlyph> & glyphs);
    static float GetInkWidth      (const std::vector<InkGlyph> & glyphs);
    static void  SplitInk         (const std::vector<InkGlyph> & glyphs, std::vector<InkGlyph> & first,
                                   std::vector<InkGlyph> & second);
    static void  FillInk          (const std::vector<InkGlyph> & glyphs, float width, std::vector<InkGlyph> & first,
                                   std::vector<InkGlyph> & second);
    static void  FitInk           (IDxuiTextRenderer & text, std::vector<InkGlyph> & row, float width,
                                   float basePx, uint32_t & seed);
    static void  DrawInkRow       (IDxuiTextRenderer & text, const std::vector<InkGlyph> & row, float top,
                                   float width, float height);
    static float NextWobble       (uint32_t & seed);

    // Retires both quads, for a theme or a presentation that draws no scene
    // drives at all.
    void    ClearSceneDiskLabels    ();

    // The scene pose across the middle of the picture, so a screenshot of a
    // render fault carries the angle it was taken from.
    void    SyncSceneViewReadout ();

    // Builds/refreshes the CASSO_SCENE_DEBUG=2 texel-calibration texture.
    void  EnsureSceneCalibration (const RECT & fittedRect);

    static constexpr int    s_kVolumeWheelSlopDp   = 4;
    static constexpr int    s_kVolumeWheelLabelKey = (int) DeskSceneModel::kRecorderKeyCount;   // m_recorderHoverKey for the wheel
    static constexpr int    s_kVolumeWheelMinDp    = 28;   // the smallest target, either way, however small the wheel
    static constexpr float  s_kVolumeWheelTurnRad  = 2.0943951f;   // a third of a turn: the mark stays in the window

    // How much tilt a pixel of drag is worth. The assembly's whole travel is
    // about eleven degrees each way, so this spends it over a couple of
    // hundred pixels -- far enough that the limit is reached deliberately
    // rather than by flinching.
    static constexpr float  kBezelTiltRadPerPx = 0.0022f;

    // A compass arrow click's fixed turn. Yaw takes more than pitch for the
    // same reason the free orbit allows more of it: the interesting sides
    // of the machines are around them, not above.
    static constexpr float  kCompassStepYawRad   = 0.2618f;   // 15 degrees
    static constexpr float  kCompassStepPitchRad = 0.1745f;   // 10 degrees

    void  ApplySavedBezelTilt ();
    void  PersistBezelTilt    ();

    void  LayoutSceneCompass ();
    bool  StepCompassHint    (int64_t nowMs);
    void  PanSceneByCompass  (float dx, float dy);   // in pan units, down positive
    bool  PointOnCompass     (int x, int y) const;

    static_assert (s_kSceneLabelCount == (size_t) DeskScene::kLabelCount,
                   "every baked label the shell makes needs a slot in the scene");

private:
    EmulatorShell  & m_shell;

    bool  m_sceneOrbitMoved = false;

    // Dragging a bezel tilt mark. The gesture is the mark's, but the motion
    // is the pointer's: how far the mouse has travelled vertically since the
    // press is the whole input, so which mark started it only decides that a
    // drag started at all.
    bool       m_bezelTilting          = false;
    POINT      m_bezelTiltStartPx      = {};
    float      m_bezelTiltStartRad     = 0.0f;

    // Dragging the recorder's volume wheel: left is silent, right is full,
    // and the pointer's travel across the wheel's own width on screen is the
    // whole range, so the wheel stays under the pointer as it goes. The
    // wheel turns s_kVolumeWheelTurnRad over that range.
    bool       m_volumeDragging        = false;
    int        m_volumeDragStartX      = 0;
    float      m_volumeDragStartGain   = 0.0f;
    float      m_volumeDragSpanPx      = 0.0f;

    // The 3D desk scene (spec 018): Monitor //c + drives rendered from the
    // before-present hook on the host device, with the CRT chain's offscreen
    // output on the curved glass. Gated by the deskScene opt-out pref.
    DeskScene                  m_deskScene;
    bool                       m_deskSceneReady = false;

    // Set once scene initialization has failed. Distinct from !m_deskSceneReady,
    // which also holds before initialization has been attempted at all.
    bool                       m_deskSceneFailed = false;

    // Which machine family the loaded models belong to, so a switch that
    // does not cross the //c boundary skips the reload.
    bool  m_deskSceneMachineIsC = false;
    int   m_deskSceneDebug      = 0;   // CASSO_SCENE_DEBUG: 1=layout rects, 2=+calibration texture

    // Fullscreen drive overlay strip (FR-015): the pure FSM plus this
    // frame's composed band. The hotkey edge arrives via the accelerator;
    // m_stripBrowseOpen pins the strip while a browse it opened is up, and
    // m_stripSuppressGuestMouse is the "released capture" of a hotkey summon
    // in mouse mode (paddle mode releases its real capture instead).
    FullscreenStripState       m_stripState;
    DeskSceneComposition       m_stripComp;
    RECT                       m_stripRectPx             = {};
    bool                       m_stripHotkeyPending      = false;
    bool                       m_stripBrowseOpen         = false;
    bool                       m_stripSuppressGuestMouse = false;

    // CASSO_SCENE_DEBUG=2: a synthetic stripe pattern standing in for the
    // CRT output, to verify the glass texel mapping end to end.
    ComPtr<ID3D11Texture2D>           m_sceneCalibTex;
    ComPtr<ID3D11ShaderResourceView>  m_sceneCalibSrv;
    RECT                              m_sceneCalibRect = {};

    // The mounted image's basename under each 3D drive -- the label strip the
    // 2D widget carried below its body, kept on screen rather than demoted to
    // a hover tooltip. Positioned from the composition's projected drive
    // bounds; empty (and invisible) when that drive holds no disk.
    // The mounted image's name under each drive. CHROME, not scene geometry:
    // it is read at a fixed size wherever the desk is posed.
    std::array<DxuiShadowedText, 2>  m_sceneDriveLabel;

    // The info icon after each name, in the overlay strip where the names are
    // chrome. A control of its own because the glyph needs the icon font and
    // a label has one face. On the desk the icon is baked with the names.
    std::array<DxuiShadowedText, 2>  m_sceneDriveInfoIcon;

    // The recorder's tape name, counter and key name on the fullscreen strip,
    // chrome there for the same reason the strip's drive names are.
    std::array<DxuiShadowedText, 3>  m_stripTapeLabel;

    // Where the desk recorder's tape name and counter are, for clicks.
    RECT                      m_sceneTapeNameRect    = {};
    std::wstring              m_sceneTapeLabelShown;   // what the baked recorder labels last showed
    RECT                      m_sceneTapeCounterRect = {};

    // What the in-scene quads currently say and the cell they were baked at,
    // so the texture is rendered on a change rather than on every
    // composition pass. The view belongs to the text renderer and stays good
    // until the next bake, which is why nothing else may use that path.
    std::array<std::wstring, s_kSceneLabelCount>    m_sceneDiskLabelText;
    SIZE                                            m_sceneDiskLabelCell = {};
    ID3D11ShaderResourceView                      * m_sceneDiskLabelSrv  = nullptr;

    // Where each of those strips landed, empty when a drive shows no name.
    // The write-protect tooltip belongs to the strip now that the padlock
    // does -- see SyncSceneDriveLabels.
    std::array<RECT, 2>       m_sceneDriveLabelRect = {};

    // Where each name's info icon landed, in client pixels, and empty when
    // the drive shows none. Its own tooltip target, ahead of the name's.
    std::array<RECT, 2>       m_sceneInfoIconRect   = {};

    // Each baked label's span; see SceneLabelSpan.
    std::array<SceneLabelSpan, s_kSceneLabelCount>  m_sceneLabelSpan = {};

    // A desk name too long for its strip scrolls while the pointer is on its
    // drive or the name. The period is the name plus its gap in baked pixels,
    // zero for a name that fits.
    std::array<float, s_kSceneLabelCount>  m_sceneDiskLabelPeriod = {};
    int                                    m_sceneLabelHover      = -1;
    int64_t                                m_sceneLabelHoverMs    = 0;

    // When each of the desk recorder's keys was last clicked, for its dip.
    std::array<int64_t, 6>    m_recorderKeyDipMs      = {};

    // Where each key stands now, easing toward where the transport puts it,
    // and when that was last advanced.
    std::array<float, 6>      m_recorderKeyShownMm    = {};
    int64_t                   m_recorderKeyStepMs     = 0;
    std::array<int64_t, 6>    m_recorderKeyDownMs     = {};   // when each key's press began, 0 if not going down
    std::array<float, 6>      m_recorderKeyDownFrom   = {};   // and where it started from
    float                     m_recorderLidOpen       = 1.0f; // the door, 0 shut to 1 open
    int64_t                   m_recorderLidStepMs     = 0;
    float                     m_recorderReelRad       = 0.0f;  // how far the spindles have turned
    int64_t                   m_recorderReelStepMs    = 0;

    // The key under the pointer, whose name shows over it; -1 for none.
    int                       m_recorderHoverKey      = -1;
    int                       m_recorderHeldKey       = -1;   // the key the left button is holding down

    // The tape deck's resets and transport as of the last frame, which the
    // desk recorder's keys follow.
    uint32_t                  m_seenTapeResets        = 0;
    TapeTransport             m_seenTransport         = TapeTransport::Empty;

    // The source path each label was last built from, so mounts and ejects
    // re-hang it without a layout pass and an unchanged frame does no
    // filesystem parsing or text measurement.
    std::array<std::string, 2>  m_sceneLabelPath;

    DxuiShadowedText           m_sceneViewReadout;

    // The scene compass: the visible way to turn the scene, for everyone
    // who will never guess that dragging does it. Laid out into the scene
    // viewport's corner by SyncSceneDriveChrome, which already runs at
    // every moment the viewport moves.
    DxuiOrbitControl           m_sceneCompass;

    // "Hold CTRL to pan", under the compass while the pointer is over it,
    // fading in and out.
    DxuiShadowedText           m_compassHint;
    float                      m_compassHintOpacity = 0.0f;
    int64_t                    m_compassHintStepMs  = 0;

    // The user's own framing of the desk scene: how far they have zoomed in
    // and where they have dragged it to. Deliberately NOT persisted -- it is
    // a way of looking at the scene for a moment, like leaning toward a
    // screen, and a saved zoom would have people opening Casso to a view they
    // set once and forgot. Ctrl+0 puts it back.
    DeskSceneView            m_sceneView;

    // Set while a pan drag is in flight, with the anchor the drag started
    // from. The anchor is the SCENE's pan at mouse-down plus the cursor
    // position, so the scene tracks the cursor exactly however far it moves
    // and a slow drag cannot accumulate rounding drift.
    bool                     m_scenePanning     = false;
    POINT                    m_scenePanStartPx  = {};
    float                    m_scenePanStartX   = 0.0f;
    float                    m_scenePanStartY   = 0.0f;

    // The orbit drag mirrors the pan drag: anchored at the press, absolute
    // from there, one flag per button so a left-orbit (Shift+drag) and the
    // right-drag never fight over state.
    bool                     m_sceneOrbiting      = false;
    bool                     m_sceneOrbitLeftBtn  = false;
    POINT                    m_sceneOrbitStartPx  = {};
    float                    m_sceneOrbitStartYaw = 0.0f;
    float                    m_sceneOrbitStartPit = 0.0f;
    int64_t                  m_sceneOrbitTapMs    = 0;

    // Touch gesture tracking. Windows reports a pinch as an ABSOLUTE
    // separation between the two fingers and a pan as an ABSOLUTE point, so
    // both need their previous value kept to turn into a step -- there is no
    // delta in the message. Reset on GF_BEGIN, or the first step of a new
    // gesture would be measured against wherever the last one ended.
    ULONGLONG                m_gestureZoomLast  = 0;
    POINT                    m_gesturePanLastPx = {};
};
