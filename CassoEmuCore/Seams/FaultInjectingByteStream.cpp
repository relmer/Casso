#include "Pch.h"

#include "Seams/FaultInjectingByteStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FaultInjectingByteStream::FaultInjectingByteStream
//
////////////////////////////////////////////////////////////////////////////////

FaultInjectingByteStream::FaultInjectingByteStream (IByteStream & inner, const LinkFault & fault, ReportFn report)
    : m_inner  (inner),
      m_report (std::move (report))
{
    m_fault.Arm (fault);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FaultInjectingByteStream::Write
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FaultInjectingByteStream::Write (const Byte * data, size_t count)
{
    return WriteWithFault (m_inner, m_fault, std::span<const Byte> (data, count), m_report);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FaultInjectingByteStream::WriteWithFault
//
//  A stall writes the bytes before the fault's, waits, then writes the rest.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FaultInjectingByteStream::WriteWithFault (IByteStream & inner, LinkByteFault & fault, std::span<const Byte> data, const ReportFn & report)
{
    HRESULT            hr      = S_OK;
    std::vector<Byte>  bytes;
    size_t             stallAt = LinkByteFault::kNoStall;
    DWORD              stallMs = 0;
    std::string        text;



    fault.Apply (data, bytes, stallAt, stallMs, text);

    if (!text.empty() && report)
    {
        report (text);
    }

    if (stallAt == LinkByteFault::kNoStall)
    {
        hr = inner.Write (bytes.data(), bytes.size());
        CHR (hr);
    }
    else
    {
        hr = inner.Write (bytes.data(), stallAt);
        CHR (hr);

        Sleep (stallMs);

        hr = inner.Write (bytes.data() + stallAt, bytes.size() - stallAt);
        CHR (hr);
    }

Error:
    return hr;
}
