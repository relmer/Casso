#pragma once

#include "Cpu.h"


// Maximum RAM entries per test vector (SingleStepTests typically has < 20)
static constexpr int HARTE_MAX_RAM_ENTRIES = 32;

// Packed-fixture format version. Version 2 added the per-vector cycle count;
// version 1 wrote a zero in the same header byte. Must match FORMAT_VERSION in
// scripts/GenerateHarteTests.py and scripts/ReduceHarteVectors.py.
//
// Reading a version 1 file as version 2 would not fail -- every field after the
// name would simply shift by one byte and the run would report hundreds of
// nonsense CPU failures -- so the loader refuses the file outright and the
// runner says which script regenerates it.
static constexpr Byte HARTE_FORMAT_VERSION = 2;





////////////////////////////////////////////////////////////////////////////////
//
//  HarteRamEntry
//
////////////////////////////////////////////////////////////////////////////////

struct HarteRamEntry
{
    Word    address;
    Byte    value;
};





////////////////////////////////////////////////////////////////////////////////
//
//  HarteCpuState
//
////////////////////////////////////////////////////////////////////////////////

struct HarteCpuState
{
    Word            pc;
    Byte            s;
    Byte            a;
    Byte            x;
    Byte            y;
    Byte            p;
    int             ramCount;
    HarteRamEntry   ram[HARTE_MAX_RAM_ENTRIES];
};





////////////////////////////////////////////////////////////////////////////////
//
//  HarteTestVector
//
////////////////////////////////////////////////////////////////////////////////

struct HarteTestVector
{
    char            name[16];
    Byte            cycles;
    HarteCpuState   initial;
    HarteCpuState   final;
};





////////////////////////////////////////////////////////////////////////////////
//
//  HarteTestFile
//
////////////////////////////////////////////////////////////////////////////////

struct HarteTestFile
{
    Byte                            opcode        = 0;
    Byte                            formatVersion = 0;
    int                             vectorCount   = 0;
    std::vector<HarteTestVector>    vectors;
};





////////////////////////////////////////////////////////////////////////////////
//
//  LoadHarteTestFile
//
//  Loads a binary test file produced by GenerateHarteTests.py.
//
//  The failure code says WHICH failure, because the caller treats them
//  differently: ERROR_FILE_NOT_FOUND is an opcode this CPU has no vectors for
//  and is skipped, ERROR_REVISION_MISMATCH is a fixture from an older format
//  and must be reported, and anything else is a truncated or corrupt file.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LoadHarteTestFile   (const std::string & path, HarteTestFile & outFile);
HRESULT ParseHarteTestBytes (std::span<const Byte> bytes, HarteTestFile & outFile);





////////////////////////////////////////////////////////////////////////////////
//
//  FindHarteSubset
//
//  The vectors compiled into the test DLL for one opcode, in the same packed
//  form as a vector file; empty for an opcode with none. Generated into
//  UnitTest/HarteVectorSubset/ by scripts/EmbedHarteVectors.py.
//
////////////////////////////////////////////////////////////////////////////////

struct HarteSubsetEntry
{
    const char  * cpu;
    Byte          opcode;
    const Byte  * bytes;
    size_t        size;
};

std::span<const Byte> FindHarteSubset (const char * cpuType, Byte opcode);





////////////////////////////////////////////////////////////////////////////////
//
//  GetHarteTestDataDir
//
//  Returns the directory of the given CPU type's vector files (e.g., "6502")
//  in CASSO_HARTE_DIR, for a full run; empty when it isn't set, and the
//  opcode tests then run the subset compiled into the DLL.
//
////////////////////////////////////////////////////////////////////////////////

std::string GetHarteTestDataDir (const char * cpuType);
