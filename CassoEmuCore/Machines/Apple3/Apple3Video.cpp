#include "Pch.h"

#include "Apple3Video.h"
#include "Apple3CharacterSet.h"
#include "Apple3Screen.h"
#include "Apple3SystemVias.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"





static constexpr int  s_kColorSwitch    = 0;
static constexpr int  s_k80ColumnSwitch = 1;
static constexpr int  s_kPage2Switch    = 2;
static constexpr int  s_kGraphicsSwitch = 3;
static constexpr int  s_kScrollMask     = 0x07;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3Video
//
////////////////////////////////////////////////////////////////////////////////

Apple3Video::Apple3Video (Apple3CharacterSet & characterSet)
    : m_characterSet (characterSet)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsSwitchAddress
//
////////////////////////////////////////////////////////////////////////////////

bool Apple3Video::IsSwitchAddress (Word address) const
{
    bool  isDisplay = address >= kFirstSwitch && address <= kLastSwitch;
    bool  isControl = address >= kSmoothScrollOff && address <= Apple3CharacterSet::kDownloadOn;



    return isDisplay || isControl;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleSwitch
//
//  Each display switch has an even address that clears it and an odd one
//  that sets it.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3Video::HandleSwitch (Word address)
{
    bool  isOn = (address & 1) != 0;



    if (address >= kFirstSwitch && address <= kLastSwitch)
    {
        switch ((address - kFirstSwitch) >> 1)
        {
        case s_kColorSwitch:    m_isColor     = isOn; break;
        case s_k80ColumnSwitch: m_is80Columns = isOn; break;
        case s_kPage2Switch:    m_isPage2     = isOn; break;
        case s_kGraphicsSwitch: m_isGraphics  = isOn; break;
        default:                                      break;
        }
    }
    else if (address == kSmoothScrollOff || address == kSmoothScrollOn)
    {
        m_isSmoothScroll = isOn;
    }
    else if (address == Apple3CharacterSet::kDownloadOff || address == Apple3CharacterSet::kDownloadOn)
    {
        m_characterSet.SetDownloadEnabled (isOn);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnBeamLine
//
////////////////////////////////////////////////////////////////////////////////

void Apple3Video::OnBeamLine (uint32_t line)
{
    bool  isBlank = line >= kDisplayedLines;



    if (isBlank && !m_isVerticalBlank)
    {
        if (m_readSystem)
        {
            m_characterSet.OnVerticalBlank (m_readSystem);
        }

        m_flashFrames++;

        if (m_flashFrames >= kFlashFrames)
        {
            m_flashFrames = 0;
            m_isFlashOn   = !m_isFlashOn;
        }
    }

    if (isBlank != m_isVerticalBlank)
    {
        SetVerticalBlank (isBlank);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetVerticalBlank
//
////////////////////////////////////////////////////////////////////////////////

void Apple3Video::SetVerticalBlank (bool isBlank)
{
    m_isVerticalBlank = isBlank;

    if (m_vias != nullptr)
    {
        m_vias->GetE().SetCb1 (isBlank);
        m_vias->GetE().SetCb2 (isBlank);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMode
//
//  The text, graphics, 40/80 and color switches select one of seven modes;
//  the 80-column text mode ignores the color switch.
//
////////////////////////////////////////////////////////////////////////////////

const IApple3VideoMode * Apple3Video::GetMode() const
{
    const IApple3VideoMode  * mode = nullptr;



    if (!m_isGraphics)
    {
        mode = m_is80Columns ? static_cast<const IApple3VideoMode *> (&m_text80)
             : m_isColor     ? static_cast<const IApple3VideoMode *> (&m_color40)
             :                 static_cast<const IApple3VideoMode *> (&m_text40);
    }
    else
    {
        mode = (m_is80Columns && m_isColor) ? static_cast<const IApple3VideoMode *> (&m_color140)
             : m_is80Columns                ? static_cast<const IApple3VideoMode *> (&m_hires560)
             : m_isColor                    ? static_cast<const IApple3VideoMode *> (&m_colorHires)
             :                                static_cast<const IApple3VideoMode *> (&m_hires);
    }

    return mode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Render
//
//  With the video off the screen is black.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3Video::Render (Apple3Screen & screen) const
{
    const IApple3VideoMode  * mode   = GetMode();
    int                       scroll = (m_isSmoothScroll && m_scrollSource) ? m_scrollSource() & s_kScrollMask : 0;
    Apple3VideoInput          input  { m_readSystem, m_characterSet, m_isPage2, m_isFlashOn, m_readGraphics ? &m_readGraphics : nullptr, scroll };
    bool                      isOn   = m_isScreenOn && mode != nullptr && m_readSystem;



    if (isOn)
    {
        mode->Decode (input, screen);
    }
    else
    {
        screen.Clear();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple3Video::SaveState (StateWriter & writer) const
{
    writer.BeginSection (kStateTag, kStateVersion);

    writer.WriteBool   (m_isColor);
    writer.WriteBool   (m_is80Columns);
    writer.WriteBool   (m_isPage2);
    writer.WriteBool   (m_isGraphics);
    writer.WriteBool   (m_isSmoothScroll);
    writer.WriteBool   (m_isScreenOn);
    writer.WriteBool   (m_isVerticalBlank);
    writer.WriteBool   (m_isFlashOn);
    writer.WriteUInt32 (m_flashFrames);

    return writer.EndSection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple3Video::LoadState (StateReader & reader)
{
    HRESULT   hr      = S_OK;
    uint16_t  version = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadBool   (m_isColor);
    reader.ReadBool   (m_is80Columns);
    reader.ReadBool   (m_isPage2);
    reader.ReadBool   (m_isGraphics);
    reader.ReadBool   (m_isSmoothScroll);
    reader.ReadBool   (m_isScreenOn);
    reader.ReadBool   (m_isVerticalBlank);
    reader.ReadBool   (m_isFlashOn);
    reader.ReadUInt32 (m_flashFrames);

    hr = reader.EndSection();
    CHR (hr);

    CBREx (m_flashFrames < kFlashFrames, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

Error:
    return hr;
}
