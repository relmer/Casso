#pragma once

#include "Pch.h"
#include "Core/MemoryDevice.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RomDevice
//
////////////////////////////////////////////////////////////////////////////////

class RomDevice : public MemoryDevice
{
public:
    RomDevice (Word start, Word end, std::vector<Byte> && data);

    Byte Read     (Word address) override;
    void Write    (Word address, Byte value) override;
    Word GetStart () const override { return m_start; }
    Word GetEnd   () const override { return m_end; }
    void Reset    () override;

    const Byte * GetData () const { return m_data.data (); }

    static std::unique_ptr<MemoryDevice> CreateFromFile (
        Word start, Word end, const std::string & filePath, std::string & outError);

    static std::unique_ptr<MemoryDevice> CreateFromData (
        Word start, Word end, const Byte * data, size_t size);

private:
    Word                m_start;
    Word                m_end;
    std::vector<Byte>   m_data;
};
