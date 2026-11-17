#include "Pch.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Devices/Floppy35/Apple2cSmartPort.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OnIwmAccess
//
////////////////////////////////////////////////////////////////////////////////

bool Apple2cSmartPort::OnIwmAccess (int offset, bool isWrite, Byte value, Byte & outValue)
{
    Byte  result     = m_port.Access (offset, isWrite, value);
    bool  isDriveTwo = m_port.GetIwm().GetDrive() == kDriveTwo;
    bool  hasDrives  = m_port.GetDriveCount() > 0;
    bool  hasBus     = !m_hasFloppyAfter || m_port.GetBus().IsEnabledAt (0) || m_port.GetBus().IsReset();
    bool  isPorts    = isDriveTwo && hasDrives && hasBus;



    if (isPorts)
    {
        outValue = result;
    }

    return isPorts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2cSmartPort::SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple2cSmartPort::SaveState (StateWriter & writer) const
{
    HRESULT  hr = S_OK;



    writer.BeginSection (kStateTag, kStateVersion);

    hr = m_port.SaveState (writer);
    CHR (hr);

    hr = writer.EndSection();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2cSmartPort::LoadState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple2cSmartPort::LoadState (StateReader & reader)
{
    HRESULT   hr      = S_OK;
    uint16_t  version = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    hr = m_port.LoadState (reader);
    CHR (hr);

    hr = reader.EndSection();

Error:
    return hr;
}
