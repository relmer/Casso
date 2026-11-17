#include "Pch.h"

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
