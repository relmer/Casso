#include "Pch.h"

#include "Seams/ByteStreamSettings.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ByteStreamSettings::ToDcb
//
////////////////////////////////////////////////////////////////////////////////

void ByteStreamSettings::ToDcb (const ComSettings & settings, DCB & outDcb)
{
    outDcb = DCB {};

    outDcb.DCBlength    = sizeof (DCB);
    outDcb.BaudRate     = settings.baud;
    outDcb.fBinary      = TRUE;
    outDcb.ByteSize     = settings.dataBits;
    outDcb.Parity       = NOPARITY;
    outDcb.StopBits     = ONESTOPBIT;
    outDcb.fDtrControl  = DTR_CONTROL_ENABLE;
    outDcb.fOutxCtsFlow = settings.hardwareHandshake ? TRUE : FALSE;
    outDcb.fRtsControl  = settings.hardwareHandshake ? RTS_CONTROL_HANDSHAKE : RTS_CONTROL_ENABLE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ByteStreamSettings::GetBindAddress
//
////////////////////////////////////////////////////////////////////////////////

const char * ByteStreamSettings::GetBindAddress (LinkTransport transport)
{
    return (transport == LinkTransport::WifiModem) ? kAnyAddress : kLoopbackAddress;
}
