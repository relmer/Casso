#pragma once

#include "Pch.h"

#include "Controllers/ControllerTypes.h"
#include "Core/IDxuiControl.h"
#include "Render/IDxuiTextRenderer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  JoyportStickMarker / JoyportStickArt
//
//  The drawing of an Atari CX40 joystick seen from above, as coordinates
//  within a square of the given side, so the shapes can be checked without a
//  renderer. A marker is one of the cardinal chevrons, two pieces either side
//  of its direction; the up direction has the word TOP instead.
//
////////////////////////////////////////////////////////////////////////////////

struct JoyportStickMarker
{
    JoystickSwitch                           direction = JoystickSwitch::Left;
    std::array<std::vector<DxuiPointF>, 2>   pieces;
    DxuiPointF                               center;
};

struct JoyportStickArt
{
    std::vector<DxuiPointF>                body;
    std::vector<DxuiPointF>                bodyTop;
    std::vector<DxuiPointF>                plate;
    float                                  plateInner  = 0.0f;
    DxuiPointF                             ringCenter;
    float                                  ringInner   = 0.0f;
    float                                  ringOuter   = 0.0f;
    std::vector<std::vector<DxuiPointF>>   dashes;
    std::vector<JoyportStickMarker>        markers;
    DxuiPointF                             topCenter;
    float                                  topHeight   = 0.0f;
    DxuiPointF                             fireCenter;
    float                                  fireRadius  = 0.0f;
    float                                  bootRadius  = 0.0f;
    float                                  shaftRadius = 0.0f;
    float                                  shaftTravel = 0.0f;
};





////////////////////////////////////////////////////////////////////////////////
//
//  JoyportSwitchView
//
//  The Joyport's switches as an Atari joystick seen from above: the dark
//  base, the red fire button in its top-left corner, the stick's
//  rounded-hexagon top, and the orange ring of dashes with a chevron at left, right and down and TOP at up. A marker, or
//  TOP, lights while its direction's switch reads closed, the fire button
//  while fire does, and the stick leans the way it is pushed.
//
////////////////////////////////////////////////////////////////////////////////

class JoyportSwitchView : public IDxuiControl
{
public:

    // Dashes between two cardinal markers, as on the real stick.
    static constexpr int  kDashesPerQuadrant = 7;

    void  SetSwitches (JoystickSwitches switches) { m_switches = switches; }
    void  SetActive   (bool isActive)             { m_isActive = isActive; }

    void  Layout      (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint       (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

    static JoyportStickArt  BuildArt (float left, float top, float side);

private:

    static std::vector<DxuiPointF>  BuildRoundedSquare  (float left, float top, float side, float radius);
    static std::vector<DxuiPointF>  BuildRoundedHexagon (DxuiPointF center, float radius, float cornerRadius);
    static std::vector<DxuiPointF>  BuildPlate          (DxuiPointF ringCenter, float ringRadius, DxuiPointF fireCenter, float fireRadius, float filletRadius);
    static void                     AppendArc           (std::vector<DxuiPointF> & points, DxuiPointF center, float radius, float fromDeg, float toDeg, bool isLongWay);
    static float                    AngleOf             (DxuiPointF center, DxuiPointF point);
    static std::vector<DxuiPointF>  BuildDash           (DxuiPointF center, float inner, float outer, float fromDeg, float toDeg);
    static std::vector<DxuiPointF>  BuildMarkerPiece    (DxuiPointF center, float inner, float outer, float cardinalDeg, float sign);
    static DxuiPointF               PointAt             (DxuiPointF center, float radius, float degrees);

    bool  IsLit          (JoystickSwitch sw) const { return m_isActive && m_switches.test (static_cast<size_t> (sw)); }
    void  PaintMarkers   (IDxuiTextRenderer & text, const JoyportStickArt & art) const;
    void  PaintStick     (IDxuiTextRenderer & text, const JoyportStickArt & art) const;
    void  PaintFire      (IDxuiTextRenderer & text, const JoyportStickArt & art) const;

    DxuiDpiScaler     m_scaler;
    JoystickSwitches  m_switches;
    bool              m_isActive = false;
};
