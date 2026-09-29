#pragma once

#include "Pch.h"

#include "Core/IDxuiControl.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StickPositionView
//
//  Where the game port's joystick is, drawn as a dot inside a circle. The
//  horizontal axis is PDL0 and the vertical PDL1, each labeled with its
//  value, so a user moving a stick sees what the guest will read rather than
//  a pair of numbers to picture.
//
//  0 is left and up and 255 is right and down, the way an Apple II paddle
//  reads. An axis nothing drives rests the dot at center.
//
////////////////////////////////////////////////////////////////////////////////

class StickPositionView : public IDxuiControl
{
public:

    void  SetValues (Byte pdl0, Byte pdl1);
    void  SetActive (bool isActive);

    // What to call the two axes. They are the controller's own PDL0 and PDL1
    // until two people play, when a player's paddles are whichever the machine
    // gave their slot -- PDL2 and PDL3 for the second joystick. An empty
    // second label is an axis this player does not drive.
    void  SetAxisLabels (const std::wstring & horizontal, const std::wstring & vertical);

    void  Layout    (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint     (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

private:

    DxuiDpiScaler  m_scaler;
    Byte           m_pdl0       = 127;
    Byte           m_pdl1       = 127;
    bool           m_isActive   = false;
    std::wstring   m_horizontal = L"PDL0";
    std::wstring   m_vertical   = L"PDL1";
};





////////////////////////////////////////////////////////////////////////////////
//
//  ButtonLightView
//
//  One pushbutton's state: a ring that fills while the guest reads the
//  button pressed. A press, however brief, stays fully lit for at least
//  kMinLitMs so it can be seen; with animations on it fills over
//  kPressRampMs and fades over kFadeMs once released. The clock and the
//  animation setting are passed in, so all of it is testable without either.
//
//  Each press shows a short message picked at random from a fixed list to
//  its right, which stays while the light holds and then fades over
//  kMessageFadeMs; a new press replaces it and starts it over.
//
////////////////////////////////////////////////////////////////////////////////

class ButtonLightView : public IDxuiControl
{
public:

    static constexpr int64_t  kMinLitMs        = 90;
    static constexpr int64_t  kPressRampMs     = 40;
    static constexpr int64_t  kFadeMs          = 120;
    static constexpr int64_t  kMessageFadeMs   = 2000;
    static constexpr float    kPressStartLevel = 0.5f;

    // The button's state at `nowMs`: whether it read pressed at any time
    // since the last update, and whether it reads pressed now.
    void   Update               (bool wasPressed, bool isPressed, int64_t nowMs);
    void   Clear                ();
    void   SetAnimationsEnabled (bool isEnabled) { m_isAnimated = isEnabled; }

    // How lit the circle is, 0 dark to 1 fully lit, as of the last update.
    float  GetLevel             () const { return m_level; }
    bool   IsLit                () const { return m_level > 0.0f; }

    // The message beside the light, empty once it has faded, and where it
    // goes: a row to the light's right, which it is elided to fit.
    std::wstring  GetFunMessage     () const;
    void          SetMessageBounds  (const RECT & bounds) { m_messageBounds = bounds; }

    // The list the messages come from, and a pick from it given a random
    // number: never `previous`, so a press never repeats the one before.
    static size_t           GetFunMessageCount ();
    static const wchar_t  * GetFunMessageAt    (size_t index);
    static size_t           PickFunMessage     (std::optional<size_t> previous, uint32_t random);

    void   Layout               (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void   Paint                (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

private:

    static uint32_t  BlendColor (uint32_t from, uint32_t to, float amount);

    void             UpdateMessage   (int64_t nowMs);
    void             PaintFunMessage (IDxuiTextRenderer & text, const IDxuiTheme & theme);

    DxuiDpiScaler  m_scaler;
    bool           m_isAnimated   = true;
    bool           m_hasPress     = false;
    bool           m_isHeld       = false;
    int64_t        m_pressStartMs = 0;
    int64_t        m_lastSeenMs   = 0;
    float          m_level        = 0.0f;

    RECT                   m_messageBounds = {};
    std::optional<size_t>  m_message;
    int64_t                m_messageEnd    = 0;
    float                  m_messageLevel  = 0.0f;
    std::optional<size_t>  m_lastMessage;
    std::mt19937           m_random { std::random_device{}() };
};
