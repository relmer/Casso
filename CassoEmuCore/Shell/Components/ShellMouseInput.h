#pragma once

#include "Pch.h"

#include "Controllers/GamePortInputMixer.h"

class ShellInput;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellMouseInput
//
//  The host mouse as the guest's input: in Paddle mode, a captured pointer
//  whose relative motion drives the paddle axes and whose buttons drive the
//  fire buttons; in Mouse mode, the //c mouse, tracking the host pointer
//  over the picture. The mode and the mouse's connection are ShellInput's.
//
////////////////////////////////////////////////////////////////////////////////

class ShellMouseInput
{
public:
    explicit ShellMouseInput (ShellInput & input) : m_input (input) {}

    // The //c mouse. Active while Mouse mode is selected and the machine has
    // a connected IOU mouse; live once guest software has turned it on, which
    // the firmware's SETMOUSE does through a hardware sequence garbage RAM
    // cannot fake, and which gates hiding and capturing the host pointer.
    // UpdateGuestMouseFromHost maps the host pointer onto the picture and
    // hands the fraction to the device.
    bool  IsGuestMouseActive       () const;
    bool  IsGuestMouseLive         () const;
    void  UpdateGuestMouseFromHost (int xPx, int yPx);

    // Paddle-mode mouse capture. Start hides and confines the cursor and
    // begins relative tracking (only in Paddle mode with the window focused);
    // Stop restores the cursor and releases the clip. ClipPaddleCursorToClient
    // confines the pointer and parks it in the middle, shared by the grab and
    // the re-take after a cancel the layout caused. UpdatePaddleFromMouse
    // turns one mouse move into the held paddle axes, and PushPaddleButton
    // one mouse button into a fire button.
    void  StartPaddleCapture       ();
    void  StopPaddleCapture        ();
    void  ClipPaddleCursorToClient ();
    void  UpdatePaddleFromMouse    (int xClient, int yClient);
    void  PushPaddleButton         (int index, bool pressed);
    bool  IsPaddleCaptured         () const { return m_paddleCaptured; }

    // A left-button press over the picture, as the //c guest mouse's button,
    // and any left-button release.
    void  PressGuestMouseButton    (int x, int y);
    void  ReleaseGuestMouseButton  ();

private:
    friend class ShellInput;

    void  PushPaddlePosition ();

    ShellInput  & m_input;

    // While captured, the cursor is hidden and confined, relative motion
    // drives the paddle axes (held, no recenter), and the mouse buttons drive
    // the fire buttons. m_paddleAxis* are float accumulators (0..255) so
    // sub-unit motion isn't lost between events.
    bool              m_paddleCaptured = false;
    float             m_paddleAxisX    = 127.0f;
    float             m_paddleAxisY    = 127.0f;

    // What the captured mouse asks of the game port.
    GamePortContribution  m_mousePaddleContribution;
};
