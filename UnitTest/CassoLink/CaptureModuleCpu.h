#pragma once

#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../TestHelpers.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/LinkProgram.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureModuleCpu
//
//  A 6502 with flat memory, the link program and the capture module loaded,
//  and a 6551 in slot 2 that is always ready to send. Each byte stored to the
//  6551's data register goes into `sent`, and the CPU's cycle count into
//  `sentAt`, so a test can check both the bytes of a read and how fast they go out.
//
//  Encode runs the module's encoder over CAPBUF as a test fills it, as the
//  module does once its read ends.
//
////////////////////////////////////////////////////////////////////////////////

class CaptureModuleCpu : public TestCpu
{
public:
    using Assert = Microsoft::VisualStudio::CppUnitTestFramework::Assert;

    static constexpr Word      kAciaData   = 0xC0A8;
    static constexpr Word      kAciaStatus = 0xC0A9;
    static constexpr Byte      kTdre       = 0x10;
    static constexpr Byte      kSlotIndex  = 0x20;
    static constexpr Word      kStub       = 0x0300;
    static constexpr Word      kCapBuf     = 0x5900;
    static constexpr Word      kCapEnd     = 0x9000;
    static constexpr size_t    kReadBytes  = kCapEnd - kCapBuf;
    static constexpr uint64_t  kMaxCycles  = 40'000'000;

    std::vector<Byte>                       sent;
    std::vector<uint64_t>                   sentAt;
    std::unordered_map<std::string, Word>   programSymbols;
    std::unordered_map<std::string, Word>   moduleSymbols;
    std::vector<Byte>                       module;
    uint64_t                                cycles = 0;

    Byte  ReadByteSlow (Word address) override
    {
        return address == kAciaStatus ? kTdre : memory[address];
    }

    void  WriteByte (Word address, Byte value) override
    {
        if (address == kAciaData)
        {
            sent.push_back (value);
            sentAt.push_back (cycles);
        }
        else
        {
            memory[address] = value;
        }
    }

    //  The program and the module in memory, the 6551's addresses set in the
    //  program's code as its startup sets them, and the CRC tables built.
    void  Load()
    {
        std::vector<Byte>  program;
        std::string        errors;
        HRESULT            hr = S_OK;



        InitForTest (kStub);

        hr = LinkProgram::AssembleProgram (program, errors, LinkChip::Acia);
        Assert::IsTrue (SUCCEEDED (hr), TextEncoding::NarrowToWide (errors).c_str());

        hr = LinkProgram::GetProgramSymbols (LinkChip::Acia, programSymbols);
        Assert::IsTrue (SUCCEEDED (hr), L"the program's symbols");

        hr = LinkProgram::AssembleModule (LinkProgram::GetCaptureModuleSource(), module, errors, LinkChip::Acia, false, &moduleSymbols);
        Assert::IsTrue (SUCCEEDED (hr), TextEncoding::NarrowToWide (errors).c_str());

        for (size_t at = 0; at < program.size(); at++)
        {
            Poke ((Word) (LinkProgram::kProgramAddress + at), program[at]);
        }

        for (size_t at = 0; at < module.size(); at++)
        {
            Poke ((Word) (LinkProgram::kModuleAddress + at), module[at]);
        }

        Poke (GetProgramSymbol ("slotx"),               kSlotIndex);
        Poke ((Word) (GetProgramSymbol ("pbsta")  + 1), (Byte) kAciaData);
        Poke ((Word) (GetProgramSymbol ("sdsta")  + 1), (Byte) kAciaData);
        Poke ((Word) (GetProgramSymbol ("sdwait") + 1), (Byte) kAciaStatus);
        Poke ((Word) (GetProgramSymbol ("pgsta")  + 1), (Byte) kAciaData);
        Poke ((Word) (GetProgramSymbol ("pgwait") + 1), (Byte) kAciaStatus);

        Call (GetProgramSymbol ("mktables"));
    }

    Word  GetProgramSymbol (const char * name) const
    {
        auto  found = programSymbols.find (name);



        Assert::IsTrue (found != programSymbols.end(), TextEncoding::NarrowToWide (name).c_str());
        return found->second;
    }

    Word  GetModuleSymbol (const char * name) const
    {
        auto  found = moduleSymbols.find (name);



        Assert::IsTrue (found != moduleSymbols.end(), TextEncoding::NarrowToWide (name).c_str());
        return found->second;
    }

    //  A JSR to `entry`, run until it returns.
    void  Call (Word entry)
    {
        uint64_t  start = cycles;



        Poke (kStub, 0x20);
        PokeWord ((Word) (kStub + 1), entry);
        RegPC() = kStub;
        RegSP() = 0xFF;

        while (RegPC() != kStub + 3 && cycles - start < kMaxCycles)
        {
            cycles += StepAndCountCycles();
        }

        Assert::AreEqual ((int) (kStub + 3), (int) RegPC(), L"the call returns");
    }

    //  `read`, as the read loop stores it, in CAPBUF, and the module's
    //  encoder run over it.
    void  Encode (const std::vector<Byte> & read)
    {
        Assert::AreEqual (kReadBytes, read.size());

        for (size_t at = 0; at < read.size(); at++)
        {
            Poke ((Word) (kCapBuf + at), read[at]);
        }

        sent.clear();
        sentAt.clear();

        Call (GetModuleSymbol ("encode"));
    }
};
