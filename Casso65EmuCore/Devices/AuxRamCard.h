#pragma once

#include "Pch.h"
#include "Core/MemoryDevice.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AuxRamCard
//
//  IIe auxiliary 64KB RAM bank.
//  Soft switches at $C003/$C004 (read main/aux) and $C005/$C006 (write).
//
////////////////////////////////////////////////////////////////////////////////

class AuxRamCard : public MemoryDevice
{
public:
    AuxRamCard ();

    Byte Read     (Word address) override;
    void Write    (Word address, Byte value) override;
    Word GetStart () const override { return 0xC003; }
    Word GetEnd   () const override { return 0xC006; }
    void Reset    () override;

    bool IsReadAux  () const { return m_readAux; }
    bool IsWriteAux () const { return m_writeAux; }

    Byte ReadAuxMem  (Word address) const;
    void WriteAuxMem (Word address, Byte value);

    static std::unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

private:
    std::vector<Byte>   m_auxRam;
    bool                m_readAux  = false;
    bool                m_writeAux = false;
};
