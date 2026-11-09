#pragma once

#include "Pch.h"

#include "CassoLink/LinkFault.h"
#include "Seams/IByteStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FaultInjectingByteStream
//
//  Another stream, with a byte fault applied to what is written to it:
//  the byte at the fault's offset, counted from the first write, is flipped,
//  left out, or written after a stall. Reads pass through. The stream wrapped
//  is not owned, and outlives this one.
//
////////////////////////////////////////////////////////////////////////////////

class FaultInjectingByteStream : public IByteStream
{
public:
    using ReportFn = std::function<void (const std::string & text)>;

    FaultInjectingByteStream (IByteStream & inner, const LinkFault & fault, ReportFn report);

    HRESULT  Write            (const Byte * data, size_t count) override;
    HRESULT  Read             (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead) override { return m_inner.Read (buffer, capacity, timeoutMs, outRead); }
    size_t   GetReadableCount () override                                                                 { return m_inner.GetReadableCount(); }
    bool     IsClosed         () const override                                                           { return m_inner.IsClosed(); }
    void     Close            () override                                                                 { m_inner.Close(); }
    bool     TryGetPeerReady  (bool & outReady) override                                                  { return m_inner.TryGetPeerReady (outReady); }

    //  The bytes written to `inner`, with `fault` applied: the write shared by
    //  this stream and the link's host.
    static HRESULT  WriteWithFault (IByteStream & inner, LinkByteFault & fault, std::span<const Byte> data, const ReportFn & report);

private:
    IByteStream &   m_inner;
    LinkByteFault   m_fault;
    ReportFn        m_report;
};
