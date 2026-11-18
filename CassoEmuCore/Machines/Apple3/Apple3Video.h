#pragma once

#include "Pch.h"

#include "Apple380ColTextMode.h"
#include "Apple3Color140Mode.h"
#include "Apple3ColorHiResMode.h"
#include "Apple3ColorTextMode.h"
#include "Apple3HiRes560Mode.h"
#include "Apple3HiResMode.h"
#include "Apple3TextMode.h"
#include "Core/IMachineState.h"

class Apple3CharacterSet;
class Apple3Screen;
class Apple3SystemVias;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3Video
//
//  The Apple ///'s display control. Any access to $C050-$C057 sets or
//  clears a switch: black and white or color, 40 or 80 columns, page 1 or
//  2, text or graphics, selecting one of seven modes. The graphics buffers
//  are in bank 0 at $0000-$7FFF, read as $2000-$9FFF. $C0D8 and
//  $C0D9 turn smooth scroll off and on, which offsets each 8-line group by
//  the disk register's first three phase latches, and
//  $C0DA and $C0DB the character download. Environment bit 5 turns the
//  video on.
//
//  At the start of each vertical blanking period the character download
//  runs, and the vertical blanking signal goes high on the E VIA's CB1 and
//  CB2 lines until the display starts again. Flashing characters change
//  phase every 15 frames, about twice a second.
//
//  Under the Apple II lockout the display has the Apple II's modes, and the
//  switches act as an Apple II's: $C050/$C051 graphics or text, $C052/$C053
//  full screen or mixed, $C054/$C055 page 1 or 2, $C056/$C057 lo-res or
//  hi-res. Text is black and white.
//
////////////////////////////////////////////////////////////////////////////////

class Apple3Video : public IMachineState
{
public:
    static constexpr Word      kFirstSwitch     = 0xC050;
    static constexpr Word      kLastSwitch      = 0xC057;
    static constexpr Word      kSmoothScrollOff = 0xC0D8;
    static constexpr Word      kSmoothScrollOn  = 0xC0D9;
    static constexpr uint32_t  kDisplayedLines  = 192;
    static constexpr uint32_t  kFlashFrames     = 15;

    static constexpr uint32_t  kStateTag     = IMachineState::MakeTag ('A', '3', 'V', 'D');
    static constexpr uint16_t  kStateVersion = 1;

    using SystemReader = std::function<Byte (Word)>;

    explicit Apple3Video (Apple3CharacterSet & characterSet);

    void     AttachVias        (Apple3SystemVias * vias)   { m_vias = vias; }
    void     SetSystemReader   (SystemReader reader)       { m_readSystem   = std::move (reader); }
    void     SetGraphicsReader (SystemReader reader)       { m_readGraphics = std::move (reader); }
    void     SetScrollSource   (std::function<int ()> source) { m_scrollSource = std::move (source); }
    void     SetScreenOn       (bool on)                   { m_isScreenOn = on; }

    bool     IsSwitchAddress   (Word address) const;
    void     HandleSwitch      (Word address);

    //  The beam's line, 0-261, from the time base.
    void     OnBeamLine        (uint32_t line);

    void     Render            (Apple3Screen & screen) const;

    const IApple3VideoMode  * GetMode () const;

    bool     IsColor           () const { return m_isColor; }
    bool     Is80Columns       () const { return m_is80Columns; }
    bool     IsPage2           () const { return m_isPage2; }
    bool     IsGraphics        () const { return m_isGraphics; }
    bool     IsSmoothScroll    () const { return m_isSmoothScroll; }
    bool     IsScreenOn        () const { return m_isScreenOn; }
    bool     IsVerticalBlank   () const { return m_isVerticalBlank; }
    bool     IsFlashOn         () const { return m_isFlashOn; }

    //  The Apple II lockout, and the switches as an Apple II's.
    bool     IsEmulationMode   () const;
    bool     IsApple2Text      () const { return m_isColor; }
    bool     IsApple2Mixed     () const { return m_is80Columns; }
    bool     IsApple2HiRes     () const { return m_isGraphics; }

    //  A byte of video memory: the system bank below $2000, the graphics
    //  buffers from $2000.
    Byte     PeekDisplay       (Word address) const;

    HRESULT  SaveState         (StateWriter & writer) const override;
    HRESULT  LoadState         (StateReader & reader) override;

private:
    void     SetVerticalBlank  (bool isBlank);

    Apple3CharacterSet   & m_characterSet;
    Apple3SystemVias     * m_vias            = nullptr;
    SystemReader           m_readSystem;
    SystemReader           m_readGraphics;
    std::function<int ()>  m_scrollSource;
    Apple3TextMode         m_text40;
    Apple3ColorTextMode    m_color40;
    Apple380ColTextMode    m_text80;
    Apple3HiResMode        m_hires;
    Apple3ColorHiResMode   m_colorHires;
    Apple3HiRes560Mode     m_hires560;
    Apple3Color140Mode     m_color140;
    bool                   m_isColor         = false;
    bool                   m_is80Columns     = false;
    bool                   m_isPage2         = false;
    bool                   m_isGraphics      = false;
    bool                   m_isSmoothScroll  = false;
    bool                   m_isScreenOn      = false;
    bool                   m_isVerticalBlank = false;
    bool                   m_isFlashOn       = false;
    uint32_t               m_flashFrames     = 0;
};
