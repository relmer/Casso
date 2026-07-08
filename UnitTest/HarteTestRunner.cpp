#include "Pch.h"

#include "TestHelpers.h"
#include "TestCpu65C02.h"
#include "HarteTestRunner.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ReadByte
//
////////////////////////////////////////////////////////////////////////////////

static bool ReadByte (std::ifstream & f, Byte & out)
{
    HRESULT hr = S_OK;

    char    c;
    bool    fRead;



    fRead = (bool) f.get (c);
    CBRA (fRead);

    out = static_cast<Byte> (c);

Error:
    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadWord
//
////////////////////////////////////////////////////////////////////////////////

static bool ReadWord (std::ifstream & f, Word & out)
{
    HRESULT hr = S_OK;

    Byte    lo;
    Byte    hi;
    bool    fReadLo;
    bool    fReadHi;



    fReadLo = ReadByte (f, lo);
    CBR (fReadLo);

    fReadHi = ReadByte (f, hi);
    CBR (fReadHi);

    out = (Word) hi << 8 | lo;

Error:
    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadCpuState
//
////////////////////////////////////////////////////////////////////////////////

static bool ReadCpuState (std::ifstream & f, HarteCpuState & state)
{
    HRESULT hr = S_OK;

    Byte    ramCount;
    bool    fOk;



    fOk = ReadWord (f, state.pc);   CBR (fOk);
    fOk = ReadByte (f, state.s);    CBR (fOk);
    fOk = ReadByte (f, state.a);    CBR (fOk);
    fOk = ReadByte (f, state.x);    CBR (fOk);
    fOk = ReadByte (f, state.y);    CBR (fOk);
    fOk = ReadByte (f, state.p);    CBR (fOk);

    fOk = ReadByte (f, ramCount);   CBR (fOk);

    state.ramCount = ramCount;

    CBRA (state.ramCount <= HARTE_MAX_RAM_ENTRIES);

    for (int i = 0; i < state.ramCount; i++)
    {
        fOk = ReadWord (f, state.ram[i].address);   CBR (fOk);
        fOk = ReadByte (f, state.ram[i].value);     CBR (fOk);
    }

Error:
    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadHarteTestFile
//
////////////////////////////////////////////////////////////////////////////////

bool LoadHarteTestFile (const std::string & path, HarteTestFile & outFile)
{
    HRESULT       hr = S_OK;

    std::ifstream f (path, std::ios::binary);
    Word          count;
    Byte          reserved;
    bool          fOk;



    CBR (f.is_open());

    // Header: vector_count (uint16), opcode (uint8), reserved (uint8)
    fOk = ReadWord (f, count);             CBR (fOk);
    fOk = ReadByte (f, outFile.opcode);    CBR (fOk);
    fOk = ReadByte (f, reserved);          CBR (fOk);

    outFile.vectorCount = count;
    outFile.vectors.resize (count);

    for (int i = 0; i < count; i++)
    {
        HarteTestVector & v = outFile.vectors[i];

        // Name: length byte + ASCII chars
        Byte nameLen;

        fOk = ReadByte (f, nameLen);
        CBR (fOk);

        CBRA (nameLen < sizeof (v.name));

        fOk = (bool) f.read (v.name, nameLen);
        CBRA (fOk);

        v.name[nameLen] = '\0';

        // Initial and final state
        fOk = ReadCpuState (f, v.initial);    CBR (fOk);
        fOk = ReadCpuState (f, v.final);      CBR (fOk);
    }

Error:
    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetHarteTestDataDir
//
////////////////////////////////////////////////////////////////////////////////

std::string GetHarteTestDataDir (const char * cpuType)
{
    // __FILE__ resolves to the source file path at compile time.
    // Navigate from UnitTest/ to UnitTest/<cpuType>/
    std::string thisFile (__FILE__);
    std::string dir = thisFile.substr (0, thisFile.find_last_of ("\\/"));



    return dir + "\\" + cpuType;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatFailure
//
////////////////////////////////////////////////////////////////////////////////

static std::wstring FormatFailure (
    const char *    testName,
    Byte            opcode,
    const char *    field,
    int             expected,
    int             actual)
{
    wchar_t buf[256];



    swprintf_s (buf, L"[%hs] opcode $%02X: %hs expected $%02X got $%02X",
        testName, opcode, field, expected, actual);

    return buf;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatWordFailure
//
////////////////////////////////////////////////////////////////////////////////

static std::wstring FormatWordFailure (
    const char *    testName,
    Byte            opcode,
    const char *    field,
    int             expected,
    int             actual)
{
    wchar_t buf[256];



    swprintf_s (buf, L"[%hs] opcode $%02X: %hs expected $%04X got $%04X",
        testName, opcode, field, expected, actual);

    return buf;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatRamFailure
//
////////////////////////////////////////////////////////////////////////////////

static std::wstring FormatRamFailure (
    const char *    testName,
    Byte            opcode,
    Word            address,
    int             expected,
    int             actual)
{
    wchar_t buf[256];



    swprintf_s (buf, L"[%hs] opcode $%02X: RAM[$%04X] expected $%02X got $%02X",
        testName, opcode, address, expected, actual);

    return buf;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunHarteTestFile
//
//  Executes all vectors in a HarteTestFile against CpuT::Step(). CpuT is a
//  flat-memory test CPU (TestCpu for NMOS, TestCpu65C02 for CMOS) exposing the
//  same InitForTest / register / Poke / Peek / Step interface.
//  Returns the number of failures.
//
////////////////////////////////////////////////////////////////////////////////

template <class CpuT>
static int RunHarteTestFile (const HarteTestFile & file, std::wstring & firstFailure)
{
    int failures = 0;



    for (int i = 0; i < file.vectorCount; i++)
    {
        const HarteTestVector & v = file.vectors[i];

        CpuT cpu;
        cpu.InitForTest (v.initial.pc);

        // Set initial registers
        cpu.RegA()  = v.initial.a;
        cpu.RegX()  = v.initial.x;
        cpu.RegY()  = v.initial.y;
        cpu.RegSP() = v.initial.s;
        cpu.Status().status = v.initial.p;

        // Poke initial RAM
        for (int r = 0; r < v.initial.ramCount; r++)
        {
            cpu.Poke (v.initial.ram[r].address, v.initial.ram[r].value);
        }

        // Execute one instruction
        cpu.Step();

        // Compare final state
        bool failed = false;

        // PC
        if (cpu.RegPC() != v.final.pc)
        {
            if (failures == 0)
            {
                firstFailure = FormatWordFailure (v.name, file.opcode, "PC", v.final.pc, cpu.RegPC());
            }

            failed = true;
        }

        // A
        if (!failed && cpu.RegA() != v.final.a)
        {
            if (failures == 0)
            {
                firstFailure = FormatFailure (v.name, file.opcode, "A", v.final.a, cpu.RegA());
            }

            failed = true;
        }

        // X
        if (!failed && cpu.RegX() != v.final.x)
        {
            if (failures == 0)
            {
                firstFailure = FormatFailure (v.name, file.opcode, "X", v.final.x, cpu.RegX());
            }

            failed = true;
        }

        // Y
        if (!failed && cpu.RegY() != v.final.y)
        {
            if (failures == 0)
            {
                firstFailure = FormatFailure (v.name, file.opcode, "Y", v.final.y, cpu.RegY());
            }

            failed = true;
        }

        // SP
        if (!failed && cpu.RegSP() != v.final.s)
        {
            if (failures == 0)
            {
                firstFailure = FormatFailure (v.name, file.opcode, "S", v.final.s, cpu.RegSP());
            }

            failed = true;
        }

        // P (status) — mask bits 4 (B) and 5 (unused) for comparison
        Byte expectedP = v.final.p & 0xCF;
        Byte actualP   = cpu.Status().status & 0xCF;

        if (!failed && actualP != expectedP)
        {
            if (failures == 0)
            {
                firstFailure = FormatFailure (v.name, file.opcode, "P", expectedP, actualP);
            }

            failed = true;
        }

        // Final RAM
        if (!failed)
        {
            for (int r = 0; r < v.final.ramCount; r++)
            {
                Byte actual = cpu.Peek (v.final.ram[r].address);

                if (actual != v.final.ram[r].value)
                {
                    if (failures == 0)
                    {
                        firstFailure = FormatRamFailure (
                            v.name, file.opcode,
                            v.final.ram[r].address,
                            v.final.ram[r].value, actual);
                    }

                    failed = true;
                    break;
                }
            }
        }

        if (failed)
        {
            failures++;
        }
    }

    return failures;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunHarteOpcode
//
//  Loads the vector file for one opcode from UnitTest/<cpuDir>/ and runs it
//  against CpuT. Missing files are skipped (opcode not generated).
//
////////////////////////////////////////////////////////////////////////////////

template <class CpuT>
static void RunHarteOpcode (const char * cpuDir, Byte opcode)
{
    std::string dir = GetHarteTestDataDir (cpuDir);
    char        hex[8];



    sprintf_s (hex, "%02x", opcode);

    std::string   path = dir + "\\" + hex + ".bin";
    HarteTestFile file;

    if (!LoadHarteTestFile (path, file))
    {
        // Skip if the file doesn't exist (opcode not generated).
        return;
    }

    std::wstring firstFailure;
    int          failures = RunHarteTestFile<CpuT> (file, firstFailure);

    if (failures > 0)
    {
        wchar_t summary[512];

        swprintf_s (summary, L"%d/%d vectors failed. First: %s",
            failures, file.vectorCount, firstFailure.c_str());

        Assert::Fail (summary);
    }
}




namespace HarteTests
{


    ////////////////////////////////////////////////////////////////////////////////
    //
    //  HarteTestRunner
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (HarteTestRunner)
    {
    private:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  RunOpcodeTest
        //
        ////////////////////////////////////////////////////////////////////////////////

        void RunOpcodeTest (Byte opcode)
        {
            RunHarteOpcode<TestCpu> ("6502", opcode);
        }


    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Test methods — one per opcode
        //
        //  Each TEST_METHOD calls RunOpcodeTest with the opcode byte.
        //  Tests are named by hex opcode for easy identification.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Opcode_00_BRK)       { RunOpcodeTest (0x00); }
        TEST_METHOD (Opcode_01_ORA_IndX)  { RunOpcodeTest (0x01); }
        TEST_METHOD (Opcode_05_ORA_ZP)    { RunOpcodeTest (0x05); }
        TEST_METHOD (Opcode_06_ASL_ZP)    { RunOpcodeTest (0x06); }
        TEST_METHOD (Opcode_08_PHP)       { RunOpcodeTest (0x08); }
        TEST_METHOD (Opcode_09_ORA_Imm)   { RunOpcodeTest (0x09); }
        TEST_METHOD (Opcode_0A_ASL_Acc)   { RunOpcodeTest (0x0A); }
        TEST_METHOD (Opcode_0D_ORA_Abs)   { RunOpcodeTest (0x0D); }
        TEST_METHOD (Opcode_0E_ASL_Abs)   { RunOpcodeTest (0x0E); }

        TEST_METHOD (Opcode_10_BPL)       { RunOpcodeTest (0x10); }
        TEST_METHOD (Opcode_11_ORA_IndY)  { RunOpcodeTest (0x11); }
        TEST_METHOD (Opcode_15_ORA_ZPX)   { RunOpcodeTest (0x15); }
        TEST_METHOD (Opcode_16_ASL_ZPX)   { RunOpcodeTest (0x16); }
        TEST_METHOD (Opcode_18_CLC)       { RunOpcodeTest (0x18); }
        TEST_METHOD (Opcode_19_ORA_AbsY)  { RunOpcodeTest (0x19); }
        TEST_METHOD (Opcode_1D_ORA_AbsX)  { RunOpcodeTest (0x1D); }
        TEST_METHOD (Opcode_1E_ASL_AbsX)  { RunOpcodeTest (0x1E); }

        TEST_METHOD (Opcode_20_JSR)       { RunOpcodeTest (0x20); }
        TEST_METHOD (Opcode_21_AND_IndX)  { RunOpcodeTest (0x21); }
        TEST_METHOD (Opcode_24_BIT_ZP)    { RunOpcodeTest (0x24); }
        TEST_METHOD (Opcode_25_AND_ZP)    { RunOpcodeTest (0x25); }
        TEST_METHOD (Opcode_26_ROL_ZP)    { RunOpcodeTest (0x26); }
        TEST_METHOD (Opcode_28_PLP)       { RunOpcodeTest (0x28); }
        TEST_METHOD (Opcode_29_AND_Imm)   { RunOpcodeTest (0x29); }
        TEST_METHOD (Opcode_2A_ROL_Acc)   { RunOpcodeTest (0x2A); }
        TEST_METHOD (Opcode_2C_BIT_Abs)   { RunOpcodeTest (0x2C); }
        TEST_METHOD (Opcode_2D_AND_Abs)   { RunOpcodeTest (0x2D); }
        TEST_METHOD (Opcode_2E_ROL_Abs)   { RunOpcodeTest (0x2E); }

        TEST_METHOD (Opcode_30_BMI)       { RunOpcodeTest (0x30); }
        TEST_METHOD (Opcode_31_AND_IndY)  { RunOpcodeTest (0x31); }
        TEST_METHOD (Opcode_35_AND_ZPX)   { RunOpcodeTest (0x35); }
        TEST_METHOD (Opcode_36_ROL_ZPX)   { RunOpcodeTest (0x36); }
        TEST_METHOD (Opcode_38_SEC)       { RunOpcodeTest (0x38); }
        TEST_METHOD (Opcode_39_AND_AbsY)  { RunOpcodeTest (0x39); }
        TEST_METHOD (Opcode_3D_AND_AbsX)  { RunOpcodeTest (0x3D); }
        TEST_METHOD (Opcode_3E_ROL_AbsX)  { RunOpcodeTest (0x3E); }

        TEST_METHOD (Opcode_40_RTI)       { RunOpcodeTest (0x40); }
        TEST_METHOD (Opcode_41_EOR_IndX)  { RunOpcodeTest (0x41); }
        TEST_METHOD (Opcode_45_EOR_ZP)    { RunOpcodeTest (0x45); }
        TEST_METHOD (Opcode_46_LSR_ZP)    { RunOpcodeTest (0x46); }
        TEST_METHOD (Opcode_48_PHA)       { RunOpcodeTest (0x48); }
        TEST_METHOD (Opcode_49_EOR_Imm)   { RunOpcodeTest (0x49); }
        TEST_METHOD (Opcode_4A_LSR_Acc)   { RunOpcodeTest (0x4A); }
        TEST_METHOD (Opcode_4C_JMP_Abs)   { RunOpcodeTest (0x4C); }
        TEST_METHOD (Opcode_4D_EOR_Abs)   { RunOpcodeTest (0x4D); }
        TEST_METHOD (Opcode_4E_LSR_Abs)   { RunOpcodeTest (0x4E); }

        TEST_METHOD (Opcode_50_BVC)       { RunOpcodeTest (0x50); }
        TEST_METHOD (Opcode_51_EOR_IndY)  { RunOpcodeTest (0x51); }
        TEST_METHOD (Opcode_55_EOR_ZPX)   { RunOpcodeTest (0x55); }
        TEST_METHOD (Opcode_56_LSR_ZPX)   { RunOpcodeTest (0x56); }
        TEST_METHOD (Opcode_58_CLI)       { RunOpcodeTest (0x58); }
        TEST_METHOD (Opcode_59_EOR_AbsY)  { RunOpcodeTest (0x59); }
        TEST_METHOD (Opcode_5D_EOR_AbsX)  { RunOpcodeTest (0x5D); }
        TEST_METHOD (Opcode_5E_LSR_AbsX)  { RunOpcodeTest (0x5E); }

        TEST_METHOD (Opcode_60_RTS)       { RunOpcodeTest (0x60); }
        TEST_METHOD (Opcode_61_ADC_IndX)  { RunOpcodeTest (0x61); }
        TEST_METHOD (Opcode_65_ADC_ZP)    { RunOpcodeTest (0x65); }
        TEST_METHOD (Opcode_66_ROR_ZP)    { RunOpcodeTest (0x66); }
        TEST_METHOD (Opcode_68_PLA)       { RunOpcodeTest (0x68); }
        TEST_METHOD (Opcode_69_ADC_Imm)   { RunOpcodeTest (0x69); }
        TEST_METHOD (Opcode_6A_ROR_Acc)   { RunOpcodeTest (0x6A); }
        TEST_METHOD (Opcode_6C_JMP_Ind)   { RunOpcodeTest (0x6C); }
        TEST_METHOD (Opcode_6D_ADC_Abs)   { RunOpcodeTest (0x6D); }
        TEST_METHOD (Opcode_6E_ROR_Abs)   { RunOpcodeTest (0x6E); }

        TEST_METHOD (Opcode_70_BVS)       { RunOpcodeTest (0x70); }
        TEST_METHOD (Opcode_71_ADC_IndY)  { RunOpcodeTest (0x71); }
        TEST_METHOD (Opcode_75_ADC_ZPX)   { RunOpcodeTest (0x75); }
        TEST_METHOD (Opcode_76_ROR_ZPX)   { RunOpcodeTest (0x76); }
        TEST_METHOD (Opcode_78_SEI)       { RunOpcodeTest (0x78); }
        TEST_METHOD (Opcode_79_ADC_AbsY)  { RunOpcodeTest (0x79); }
        TEST_METHOD (Opcode_7D_ADC_AbsX)  { RunOpcodeTest (0x7D); }
        TEST_METHOD (Opcode_7E_ROR_AbsX)  { RunOpcodeTest (0x7E); }

        TEST_METHOD (Opcode_81_STA_IndX)  { RunOpcodeTest (0x81); }
        TEST_METHOD (Opcode_84_STY_ZP)    { RunOpcodeTest (0x84); }
        TEST_METHOD (Opcode_85_STA_ZP)    { RunOpcodeTest (0x85); }
        TEST_METHOD (Opcode_86_STX_ZP)    { RunOpcodeTest (0x86); }
        TEST_METHOD (Opcode_88_DEY)       { RunOpcodeTest (0x88); }
        TEST_METHOD (Opcode_8A_TXA)       { RunOpcodeTest (0x8A); }
        TEST_METHOD (Opcode_8C_STY_Abs)   { RunOpcodeTest (0x8C); }
        TEST_METHOD (Opcode_8D_STA_Abs)   { RunOpcodeTest (0x8D); }
        TEST_METHOD (Opcode_8E_STX_Abs)   { RunOpcodeTest (0x8E); }

        TEST_METHOD (Opcode_90_BCC)       { RunOpcodeTest (0x90); }
        TEST_METHOD (Opcode_91_STA_IndY)  { RunOpcodeTest (0x91); }
        TEST_METHOD (Opcode_94_STY_ZPX)   { RunOpcodeTest (0x94); }
        TEST_METHOD (Opcode_95_STA_ZPX)   { RunOpcodeTest (0x95); }
        TEST_METHOD (Opcode_96_STX_ZPY)   { RunOpcodeTest (0x96); }
        TEST_METHOD (Opcode_98_TYA)       { RunOpcodeTest (0x98); }
        TEST_METHOD (Opcode_99_STA_AbsY)  { RunOpcodeTest (0x99); }
        TEST_METHOD (Opcode_9A_TXS)       { RunOpcodeTest (0x9A); }
        TEST_METHOD (Opcode_9D_STA_AbsX)  { RunOpcodeTest (0x9D); }

        TEST_METHOD (Opcode_A0_LDY_Imm)   { RunOpcodeTest (0xA0); }
        TEST_METHOD (Opcode_A1_LDA_IndX)   { RunOpcodeTest (0xA1); }
        TEST_METHOD (Opcode_A2_LDX_Imm)   { RunOpcodeTest (0xA2); }
        TEST_METHOD (Opcode_A4_LDY_ZP)    { RunOpcodeTest (0xA4); }
        TEST_METHOD (Opcode_A5_LDA_ZP)    { RunOpcodeTest (0xA5); }
        TEST_METHOD (Opcode_A6_LDX_ZP)    { RunOpcodeTest (0xA6); }
        TEST_METHOD (Opcode_A8_TAY)       { RunOpcodeTest (0xA8); }
        TEST_METHOD (Opcode_A9_LDA_Imm)   { RunOpcodeTest (0xA9); }
        TEST_METHOD (Opcode_AA_TAX)       { RunOpcodeTest (0xAA); }
        TEST_METHOD (Opcode_AC_LDY_Abs)   { RunOpcodeTest (0xAC); }
        TEST_METHOD (Opcode_AD_LDA_Abs)   { RunOpcodeTest (0xAD); }
        TEST_METHOD (Opcode_AE_LDX_Abs)   { RunOpcodeTest (0xAE); }

        TEST_METHOD (Opcode_B0_BCS)       { RunOpcodeTest (0xB0); }
        TEST_METHOD (Opcode_B1_LDA_IndY)  { RunOpcodeTest (0xB1); }
        TEST_METHOD (Opcode_B4_LDY_ZPX)   { RunOpcodeTest (0xB4); }
        TEST_METHOD (Opcode_B5_LDA_ZPX)   { RunOpcodeTest (0xB5); }
        TEST_METHOD (Opcode_B6_LDX_ZPY)   { RunOpcodeTest (0xB6); }
        TEST_METHOD (Opcode_B8_CLV)       { RunOpcodeTest (0xB8); }
        TEST_METHOD (Opcode_B9_LDA_AbsY)  { RunOpcodeTest (0xB9); }
        TEST_METHOD (Opcode_BA_TSX)       { RunOpcodeTest (0xBA); }
        TEST_METHOD (Opcode_BC_LDY_AbsX)  { RunOpcodeTest (0xBC); }
        TEST_METHOD (Opcode_BD_LDA_AbsX)  { RunOpcodeTest (0xBD); }
        TEST_METHOD (Opcode_BE_LDX_AbsY)  { RunOpcodeTest (0xBE); }

        TEST_METHOD (Opcode_C0_CPY_Imm)   { RunOpcodeTest (0xC0); }
        TEST_METHOD (Opcode_C1_CMP_IndX)  { RunOpcodeTest (0xC1); }
        TEST_METHOD (Opcode_C4_CPY_ZP)    { RunOpcodeTest (0xC4); }
        TEST_METHOD (Opcode_C5_CMP_ZP)    { RunOpcodeTest (0xC5); }
        TEST_METHOD (Opcode_C6_DEC_ZP)    { RunOpcodeTest (0xC6); }
        TEST_METHOD (Opcode_C8_INY)       { RunOpcodeTest (0xC8); }
        TEST_METHOD (Opcode_C9_CMP_Imm)   { RunOpcodeTest (0xC9); }
        TEST_METHOD (Opcode_CA_DEX)       { RunOpcodeTest (0xCA); }
        TEST_METHOD (Opcode_CC_CPY_Abs)   { RunOpcodeTest (0xCC); }
        TEST_METHOD (Opcode_CD_CMP_Abs)   { RunOpcodeTest (0xCD); }
        TEST_METHOD (Opcode_CE_DEC_Abs)   { RunOpcodeTest (0xCE); }

        TEST_METHOD (Opcode_D0_BNE)       { RunOpcodeTest (0xD0); }
        TEST_METHOD (Opcode_D1_CMP_IndY)  { RunOpcodeTest (0xD1); }
        TEST_METHOD (Opcode_D5_CMP_ZPX)   { RunOpcodeTest (0xD5); }
        TEST_METHOD (Opcode_D6_DEC_ZPX)   { RunOpcodeTest (0xD6); }
        TEST_METHOD (Opcode_D8_CLD)       { RunOpcodeTest (0xD8); }
        TEST_METHOD (Opcode_D9_CMP_AbsY)  { RunOpcodeTest (0xD9); }
        TEST_METHOD (Opcode_DD_CMP_AbsX)  { RunOpcodeTest (0xDD); }
        TEST_METHOD (Opcode_DE_DEC_AbsX)  { RunOpcodeTest (0xDE); }

        TEST_METHOD (Opcode_E0_CPX_Imm)   { RunOpcodeTest (0xE0); }
        TEST_METHOD (Opcode_E1_SBC_IndX)  { RunOpcodeTest (0xE1); }
        TEST_METHOD (Opcode_E4_CPX_ZP)    { RunOpcodeTest (0xE4); }
        TEST_METHOD (Opcode_E5_SBC_ZP)    { RunOpcodeTest (0xE5); }
        TEST_METHOD (Opcode_E6_INC_ZP)    { RunOpcodeTest (0xE6); }
        TEST_METHOD (Opcode_E8_INX)       { RunOpcodeTest (0xE8); }
        TEST_METHOD (Opcode_E9_SBC_Imm)   { RunOpcodeTest (0xE9); }
        TEST_METHOD (Opcode_EA_NOP)       { RunOpcodeTest (0xEA); }
        TEST_METHOD (Opcode_EC_CPX_Abs)   { RunOpcodeTest (0xEC); }
        TEST_METHOD (Opcode_ED_SBC_Abs)   { RunOpcodeTest (0xED); }
        TEST_METHOD (Opcode_EE_INC_Abs)   { RunOpcodeTest (0xEE); }

        TEST_METHOD (Opcode_F0_BEQ)       { RunOpcodeTest (0xF0); }
        TEST_METHOD (Opcode_F1_SBC_IndY)  { RunOpcodeTest (0xF1); }
        TEST_METHOD (Opcode_F5_SBC_ZPX)   { RunOpcodeTest (0xF5); }
        TEST_METHOD (Opcode_F6_INC_ZPX)   { RunOpcodeTest (0xF6); }
        TEST_METHOD (Opcode_F8_SED)       { RunOpcodeTest (0xF8); }
        TEST_METHOD (Opcode_F9_SBC_AbsY)  { RunOpcodeTest (0xF9); }
        TEST_METHOD (Opcode_FD_SBC_AbsX)  { RunOpcodeTest (0xFD); }
        TEST_METHOD (Opcode_FE_INC_AbsX)  { RunOpcodeTest (0xFE); }


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Undocumented opcodes
        //
        //  One TEST_METHOD per illegal NMOS opcode Casso implements in
        //  CassoCore Cpu::InitializeUndocumented(). RunOpcodeTest skips
        //  silently when the matching .bin is absent, so these stay green
        //  under a --legal-only generation. When InitializeUndocumented()
        //  gains an opcode, add it here AND to the
        //  IMPLEMENTED_ILLEGAL_6502_OPCODES list in
        //  scripts/GenerateHarteTests.py.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Opcode_04_DOP_ZP)    { RunOpcodeTest (0x04); }
        TEST_METHOD (Opcode_CF_DCP_Abs)   { RunOpcodeTest (0xCF); }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  HarteSynertek65C02
    //
    //  Tom Harte SingleStepTests for the base-tier (Synertek) 65C02 -- the exact
    //  match for Casso Cpu65C02 (no Rockwell bit ops, no WAI/STP). One method per
    //  opcode byte; RunHarteOpcode skips silently when a .bin is absent, so the
    //  suite stays green before the synertek65c02 vectors are generated.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (HarteSynertek65C02)
    {
    public:

        // Casso models the $x7/$xF/$DB reserved slots as 1-byte, 1-cycle NOPs --
        // the canonical base-tier behavior (6502.org / AppleWin / Klaus Dormann).
        // Harte's synertek65c02 captures a specific Synertek part that consumes
        // 2-3 bytes in those slots; Apple shipped Rockwell/NCR/GTE/WDC parts, not
        // Synertek, so that quirk is intentionally not emulated. For those opcodes
        // we assert our 1-byte NOP behavior instead of the divergent vector; every
        // other opcode runs the Harte vector unchanged.
        static bool IsReservedNopDivergence (Byte opcode)
        {
            return (opcode & 0x0F) == 0x07 || (opcode & 0x0F) == 0x0F || opcode == 0xDB;
        }

        void RunSynertekOpcode (Byte opcode)
        {
            if (IsReservedNopDivergence (opcode))
            {
                TestCpu65C02 cpu;

                cpu.InitForTest (0x0400);
                cpu.Poke (0x0400, opcode);
                cpu.Poke (0x0401, 0xEA);            // sentinel operand byte
                cpu.Step ();

                Assert::AreEqual<Word> (0x0401, cpu.RegPC ());   // consumed exactly 1 byte
                return;
            }

            RunHarteOpcode<TestCpu65C02> ("synertek65c02", opcode);
        }

        TEST_METHOD (Op65_00) { RunSynertekOpcode (0x00); }
        TEST_METHOD (Op65_01) { RunSynertekOpcode (0x01); }
        TEST_METHOD (Op65_02) { RunSynertekOpcode (0x02); }
        TEST_METHOD (Op65_03) { RunSynertekOpcode (0x03); }
        TEST_METHOD (Op65_04) { RunSynertekOpcode (0x04); }
        TEST_METHOD (Op65_05) { RunSynertekOpcode (0x05); }
        TEST_METHOD (Op65_06) { RunSynertekOpcode (0x06); }
        TEST_METHOD (Op65_07) { RunSynertekOpcode (0x07); }
        TEST_METHOD (Op65_08) { RunSynertekOpcode (0x08); }
        TEST_METHOD (Op65_09) { RunSynertekOpcode (0x09); }
        TEST_METHOD (Op65_0A) { RunSynertekOpcode (0x0A); }
        TEST_METHOD (Op65_0B) { RunSynertekOpcode (0x0B); }
        TEST_METHOD (Op65_0C) { RunSynertekOpcode (0x0C); }
        TEST_METHOD (Op65_0D) { RunSynertekOpcode (0x0D); }
        TEST_METHOD (Op65_0E) { RunSynertekOpcode (0x0E); }
        TEST_METHOD (Op65_0F) { RunSynertekOpcode (0x0F); }
        TEST_METHOD (Op65_10) { RunSynertekOpcode (0x10); }
        TEST_METHOD (Op65_11) { RunSynertekOpcode (0x11); }
        TEST_METHOD (Op65_12) { RunSynertekOpcode (0x12); }
        TEST_METHOD (Op65_13) { RunSynertekOpcode (0x13); }
        TEST_METHOD (Op65_14) { RunSynertekOpcode (0x14); }
        TEST_METHOD (Op65_15) { RunSynertekOpcode (0x15); }
        TEST_METHOD (Op65_16) { RunSynertekOpcode (0x16); }
        TEST_METHOD (Op65_17) { RunSynertekOpcode (0x17); }
        TEST_METHOD (Op65_18) { RunSynertekOpcode (0x18); }
        TEST_METHOD (Op65_19) { RunSynertekOpcode (0x19); }
        TEST_METHOD (Op65_1A) { RunSynertekOpcode (0x1A); }
        TEST_METHOD (Op65_1B) { RunSynertekOpcode (0x1B); }
        TEST_METHOD (Op65_1C) { RunSynertekOpcode (0x1C); }
        TEST_METHOD (Op65_1D) { RunSynertekOpcode (0x1D); }
        TEST_METHOD (Op65_1E) { RunSynertekOpcode (0x1E); }
        TEST_METHOD (Op65_1F) { RunSynertekOpcode (0x1F); }
        TEST_METHOD (Op65_20) { RunSynertekOpcode (0x20); }
        TEST_METHOD (Op65_21) { RunSynertekOpcode (0x21); }
        TEST_METHOD (Op65_22) { RunSynertekOpcode (0x22); }
        TEST_METHOD (Op65_23) { RunSynertekOpcode (0x23); }
        TEST_METHOD (Op65_24) { RunSynertekOpcode (0x24); }
        TEST_METHOD (Op65_25) { RunSynertekOpcode (0x25); }
        TEST_METHOD (Op65_26) { RunSynertekOpcode (0x26); }
        TEST_METHOD (Op65_27) { RunSynertekOpcode (0x27); }
        TEST_METHOD (Op65_28) { RunSynertekOpcode (0x28); }
        TEST_METHOD (Op65_29) { RunSynertekOpcode (0x29); }
        TEST_METHOD (Op65_2A) { RunSynertekOpcode (0x2A); }
        TEST_METHOD (Op65_2B) { RunSynertekOpcode (0x2B); }
        TEST_METHOD (Op65_2C) { RunSynertekOpcode (0x2C); }
        TEST_METHOD (Op65_2D) { RunSynertekOpcode (0x2D); }
        TEST_METHOD (Op65_2E) { RunSynertekOpcode (0x2E); }
        TEST_METHOD (Op65_2F) { RunSynertekOpcode (0x2F); }
        TEST_METHOD (Op65_30) { RunSynertekOpcode (0x30); }
        TEST_METHOD (Op65_31) { RunSynertekOpcode (0x31); }
        TEST_METHOD (Op65_32) { RunSynertekOpcode (0x32); }
        TEST_METHOD (Op65_33) { RunSynertekOpcode (0x33); }
        TEST_METHOD (Op65_34) { RunSynertekOpcode (0x34); }
        TEST_METHOD (Op65_35) { RunSynertekOpcode (0x35); }
        TEST_METHOD (Op65_36) { RunSynertekOpcode (0x36); }
        TEST_METHOD (Op65_37) { RunSynertekOpcode (0x37); }
        TEST_METHOD (Op65_38) { RunSynertekOpcode (0x38); }
        TEST_METHOD (Op65_39) { RunSynertekOpcode (0x39); }
        TEST_METHOD (Op65_3A) { RunSynertekOpcode (0x3A); }
        TEST_METHOD (Op65_3B) { RunSynertekOpcode (0x3B); }
        TEST_METHOD (Op65_3C) { RunSynertekOpcode (0x3C); }
        TEST_METHOD (Op65_3D) { RunSynertekOpcode (0x3D); }
        TEST_METHOD (Op65_3E) { RunSynertekOpcode (0x3E); }
        TEST_METHOD (Op65_3F) { RunSynertekOpcode (0x3F); }
        TEST_METHOD (Op65_40) { RunSynertekOpcode (0x40); }
        TEST_METHOD (Op65_41) { RunSynertekOpcode (0x41); }
        TEST_METHOD (Op65_42) { RunSynertekOpcode (0x42); }
        TEST_METHOD (Op65_43) { RunSynertekOpcode (0x43); }
        TEST_METHOD (Op65_44) { RunSynertekOpcode (0x44); }
        TEST_METHOD (Op65_45) { RunSynertekOpcode (0x45); }
        TEST_METHOD (Op65_46) { RunSynertekOpcode (0x46); }
        TEST_METHOD (Op65_47) { RunSynertekOpcode (0x47); }
        TEST_METHOD (Op65_48) { RunSynertekOpcode (0x48); }
        TEST_METHOD (Op65_49) { RunSynertekOpcode (0x49); }
        TEST_METHOD (Op65_4A) { RunSynertekOpcode (0x4A); }
        TEST_METHOD (Op65_4B) { RunSynertekOpcode (0x4B); }
        TEST_METHOD (Op65_4C) { RunSynertekOpcode (0x4C); }
        TEST_METHOD (Op65_4D) { RunSynertekOpcode (0x4D); }
        TEST_METHOD (Op65_4E) { RunSynertekOpcode (0x4E); }
        TEST_METHOD (Op65_4F) { RunSynertekOpcode (0x4F); }
        TEST_METHOD (Op65_50) { RunSynertekOpcode (0x50); }
        TEST_METHOD (Op65_51) { RunSynertekOpcode (0x51); }
        TEST_METHOD (Op65_52) { RunSynertekOpcode (0x52); }
        TEST_METHOD (Op65_53) { RunSynertekOpcode (0x53); }
        TEST_METHOD (Op65_54) { RunSynertekOpcode (0x54); }
        TEST_METHOD (Op65_55) { RunSynertekOpcode (0x55); }
        TEST_METHOD (Op65_56) { RunSynertekOpcode (0x56); }
        TEST_METHOD (Op65_57) { RunSynertekOpcode (0x57); }
        TEST_METHOD (Op65_58) { RunSynertekOpcode (0x58); }
        TEST_METHOD (Op65_59) { RunSynertekOpcode (0x59); }
        TEST_METHOD (Op65_5A) { RunSynertekOpcode (0x5A); }
        TEST_METHOD (Op65_5B) { RunSynertekOpcode (0x5B); }
        TEST_METHOD (Op65_5C) { RunSynertekOpcode (0x5C); }
        TEST_METHOD (Op65_5D) { RunSynertekOpcode (0x5D); }
        TEST_METHOD (Op65_5E) { RunSynertekOpcode (0x5E); }
        TEST_METHOD (Op65_5F) { RunSynertekOpcode (0x5F); }
        TEST_METHOD (Op65_60) { RunSynertekOpcode (0x60); }
        TEST_METHOD (Op65_61) { RunSynertekOpcode (0x61); }
        TEST_METHOD (Op65_62) { RunSynertekOpcode (0x62); }
        TEST_METHOD (Op65_63) { RunSynertekOpcode (0x63); }
        TEST_METHOD (Op65_64) { RunSynertekOpcode (0x64); }
        TEST_METHOD (Op65_65) { RunSynertekOpcode (0x65); }
        TEST_METHOD (Op65_66) { RunSynertekOpcode (0x66); }
        TEST_METHOD (Op65_67) { RunSynertekOpcode (0x67); }
        TEST_METHOD (Op65_68) { RunSynertekOpcode (0x68); }
        TEST_METHOD (Op65_69) { RunSynertekOpcode (0x69); }
        TEST_METHOD (Op65_6A) { RunSynertekOpcode (0x6A); }
        TEST_METHOD (Op65_6B) { RunSynertekOpcode (0x6B); }
        TEST_METHOD (Op65_6C) { RunSynertekOpcode (0x6C); }
        TEST_METHOD (Op65_6D) { RunSynertekOpcode (0x6D); }
        TEST_METHOD (Op65_6E) { RunSynertekOpcode (0x6E); }
        TEST_METHOD (Op65_6F) { RunSynertekOpcode (0x6F); }
        TEST_METHOD (Op65_70) { RunSynertekOpcode (0x70); }
        TEST_METHOD (Op65_71) { RunSynertekOpcode (0x71); }
        TEST_METHOD (Op65_72) { RunSynertekOpcode (0x72); }
        TEST_METHOD (Op65_73) { RunSynertekOpcode (0x73); }
        TEST_METHOD (Op65_74) { RunSynertekOpcode (0x74); }
        TEST_METHOD (Op65_75) { RunSynertekOpcode (0x75); }
        TEST_METHOD (Op65_76) { RunSynertekOpcode (0x76); }
        TEST_METHOD (Op65_77) { RunSynertekOpcode (0x77); }
        TEST_METHOD (Op65_78) { RunSynertekOpcode (0x78); }
        TEST_METHOD (Op65_79) { RunSynertekOpcode (0x79); }
        TEST_METHOD (Op65_7A) { RunSynertekOpcode (0x7A); }
        TEST_METHOD (Op65_7B) { RunSynertekOpcode (0x7B); }
        TEST_METHOD (Op65_7C) { RunSynertekOpcode (0x7C); }
        TEST_METHOD (Op65_7D) { RunSynertekOpcode (0x7D); }
        TEST_METHOD (Op65_7E) { RunSynertekOpcode (0x7E); }
        TEST_METHOD (Op65_7F) { RunSynertekOpcode (0x7F); }
        TEST_METHOD (Op65_80) { RunSynertekOpcode (0x80); }
        TEST_METHOD (Op65_81) { RunSynertekOpcode (0x81); }
        TEST_METHOD (Op65_82) { RunSynertekOpcode (0x82); }
        TEST_METHOD (Op65_83) { RunSynertekOpcode (0x83); }
        TEST_METHOD (Op65_84) { RunSynertekOpcode (0x84); }
        TEST_METHOD (Op65_85) { RunSynertekOpcode (0x85); }
        TEST_METHOD (Op65_86) { RunSynertekOpcode (0x86); }
        TEST_METHOD (Op65_87) { RunSynertekOpcode (0x87); }
        TEST_METHOD (Op65_88) { RunSynertekOpcode (0x88); }
        TEST_METHOD (Op65_89) { RunSynertekOpcode (0x89); }
        TEST_METHOD (Op65_8A) { RunSynertekOpcode (0x8A); }
        TEST_METHOD (Op65_8B) { RunSynertekOpcode (0x8B); }
        TEST_METHOD (Op65_8C) { RunSynertekOpcode (0x8C); }
        TEST_METHOD (Op65_8D) { RunSynertekOpcode (0x8D); }
        TEST_METHOD (Op65_8E) { RunSynertekOpcode (0x8E); }
        TEST_METHOD (Op65_8F) { RunSynertekOpcode (0x8F); }
        TEST_METHOD (Op65_90) { RunSynertekOpcode (0x90); }
        TEST_METHOD (Op65_91) { RunSynertekOpcode (0x91); }
        TEST_METHOD (Op65_92) { RunSynertekOpcode (0x92); }
        TEST_METHOD (Op65_93) { RunSynertekOpcode (0x93); }
        TEST_METHOD (Op65_94) { RunSynertekOpcode (0x94); }
        TEST_METHOD (Op65_95) { RunSynertekOpcode (0x95); }
        TEST_METHOD (Op65_96) { RunSynertekOpcode (0x96); }
        TEST_METHOD (Op65_97) { RunSynertekOpcode (0x97); }
        TEST_METHOD (Op65_98) { RunSynertekOpcode (0x98); }
        TEST_METHOD (Op65_99) { RunSynertekOpcode (0x99); }
        TEST_METHOD (Op65_9A) { RunSynertekOpcode (0x9A); }
        TEST_METHOD (Op65_9B) { RunSynertekOpcode (0x9B); }
        TEST_METHOD (Op65_9C) { RunSynertekOpcode (0x9C); }
        TEST_METHOD (Op65_9D) { RunSynertekOpcode (0x9D); }
        TEST_METHOD (Op65_9E) { RunSynertekOpcode (0x9E); }
        TEST_METHOD (Op65_9F) { RunSynertekOpcode (0x9F); }
        TEST_METHOD (Op65_A0) { RunSynertekOpcode (0xA0); }
        TEST_METHOD (Op65_A1) { RunSynertekOpcode (0xA1); }
        TEST_METHOD (Op65_A2) { RunSynertekOpcode (0xA2); }
        TEST_METHOD (Op65_A3) { RunSynertekOpcode (0xA3); }
        TEST_METHOD (Op65_A4) { RunSynertekOpcode (0xA4); }
        TEST_METHOD (Op65_A5) { RunSynertekOpcode (0xA5); }
        TEST_METHOD (Op65_A6) { RunSynertekOpcode (0xA6); }
        TEST_METHOD (Op65_A7) { RunSynertekOpcode (0xA7); }
        TEST_METHOD (Op65_A8) { RunSynertekOpcode (0xA8); }
        TEST_METHOD (Op65_A9) { RunSynertekOpcode (0xA9); }
        TEST_METHOD (Op65_AA) { RunSynertekOpcode (0xAA); }
        TEST_METHOD (Op65_AB) { RunSynertekOpcode (0xAB); }
        TEST_METHOD (Op65_AC) { RunSynertekOpcode (0xAC); }
        TEST_METHOD (Op65_AD) { RunSynertekOpcode (0xAD); }
        TEST_METHOD (Op65_AE) { RunSynertekOpcode (0xAE); }
        TEST_METHOD (Op65_AF) { RunSynertekOpcode (0xAF); }
        TEST_METHOD (Op65_B0) { RunSynertekOpcode (0xB0); }
        TEST_METHOD (Op65_B1) { RunSynertekOpcode (0xB1); }
        TEST_METHOD (Op65_B2) { RunSynertekOpcode (0xB2); }
        TEST_METHOD (Op65_B3) { RunSynertekOpcode (0xB3); }
        TEST_METHOD (Op65_B4) { RunSynertekOpcode (0xB4); }
        TEST_METHOD (Op65_B5) { RunSynertekOpcode (0xB5); }
        TEST_METHOD (Op65_B6) { RunSynertekOpcode (0xB6); }
        TEST_METHOD (Op65_B7) { RunSynertekOpcode (0xB7); }
        TEST_METHOD (Op65_B8) { RunSynertekOpcode (0xB8); }
        TEST_METHOD (Op65_B9) { RunSynertekOpcode (0xB9); }
        TEST_METHOD (Op65_BA) { RunSynertekOpcode (0xBA); }
        TEST_METHOD (Op65_BB) { RunSynertekOpcode (0xBB); }
        TEST_METHOD (Op65_BC) { RunSynertekOpcode (0xBC); }
        TEST_METHOD (Op65_BD) { RunSynertekOpcode (0xBD); }
        TEST_METHOD (Op65_BE) { RunSynertekOpcode (0xBE); }
        TEST_METHOD (Op65_BF) { RunSynertekOpcode (0xBF); }
        TEST_METHOD (Op65_C0) { RunSynertekOpcode (0xC0); }
        TEST_METHOD (Op65_C1) { RunSynertekOpcode (0xC1); }
        TEST_METHOD (Op65_C2) { RunSynertekOpcode (0xC2); }
        TEST_METHOD (Op65_C3) { RunSynertekOpcode (0xC3); }
        TEST_METHOD (Op65_C4) { RunSynertekOpcode (0xC4); }
        TEST_METHOD (Op65_C5) { RunSynertekOpcode (0xC5); }
        TEST_METHOD (Op65_C6) { RunSynertekOpcode (0xC6); }
        TEST_METHOD (Op65_C7) { RunSynertekOpcode (0xC7); }
        TEST_METHOD (Op65_C8) { RunSynertekOpcode (0xC8); }
        TEST_METHOD (Op65_C9) { RunSynertekOpcode (0xC9); }
        TEST_METHOD (Op65_CA) { RunSynertekOpcode (0xCA); }
        TEST_METHOD (Op65_CB) { RunSynertekOpcode (0xCB); }
        TEST_METHOD (Op65_CC) { RunSynertekOpcode (0xCC); }
        TEST_METHOD (Op65_CD) { RunSynertekOpcode (0xCD); }
        TEST_METHOD (Op65_CE) { RunSynertekOpcode (0xCE); }
        TEST_METHOD (Op65_CF) { RunSynertekOpcode (0xCF); }
        TEST_METHOD (Op65_D0) { RunSynertekOpcode (0xD0); }
        TEST_METHOD (Op65_D1) { RunSynertekOpcode (0xD1); }
        TEST_METHOD (Op65_D2) { RunSynertekOpcode (0xD2); }
        TEST_METHOD (Op65_D3) { RunSynertekOpcode (0xD3); }
        TEST_METHOD (Op65_D4) { RunSynertekOpcode (0xD4); }
        TEST_METHOD (Op65_D5) { RunSynertekOpcode (0xD5); }
        TEST_METHOD (Op65_D6) { RunSynertekOpcode (0xD6); }
        TEST_METHOD (Op65_D7) { RunSynertekOpcode (0xD7); }
        TEST_METHOD (Op65_D8) { RunSynertekOpcode (0xD8); }
        TEST_METHOD (Op65_D9) { RunSynertekOpcode (0xD9); }
        TEST_METHOD (Op65_DA) { RunSynertekOpcode (0xDA); }
        TEST_METHOD (Op65_DB) { RunSynertekOpcode (0xDB); }
        TEST_METHOD (Op65_DC) { RunSynertekOpcode (0xDC); }
        TEST_METHOD (Op65_DD) { RunSynertekOpcode (0xDD); }
        TEST_METHOD (Op65_DE) { RunSynertekOpcode (0xDE); }
        TEST_METHOD (Op65_DF) { RunSynertekOpcode (0xDF); }
        TEST_METHOD (Op65_E0) { RunSynertekOpcode (0xE0); }
        TEST_METHOD (Op65_E1) { RunSynertekOpcode (0xE1); }
        TEST_METHOD (Op65_E2) { RunSynertekOpcode (0xE2); }
        TEST_METHOD (Op65_E3) { RunSynertekOpcode (0xE3); }
        TEST_METHOD (Op65_E4) { RunSynertekOpcode (0xE4); }
        TEST_METHOD (Op65_E5) { RunSynertekOpcode (0xE5); }
        TEST_METHOD (Op65_E6) { RunSynertekOpcode (0xE6); }
        TEST_METHOD (Op65_E7) { RunSynertekOpcode (0xE7); }
        TEST_METHOD (Op65_E8) { RunSynertekOpcode (0xE8); }
        TEST_METHOD (Op65_E9) { RunSynertekOpcode (0xE9); }
        TEST_METHOD (Op65_EA) { RunSynertekOpcode (0xEA); }
        TEST_METHOD (Op65_EB) { RunSynertekOpcode (0xEB); }
        TEST_METHOD (Op65_EC) { RunSynertekOpcode (0xEC); }
        TEST_METHOD (Op65_ED) { RunSynertekOpcode (0xED); }
        TEST_METHOD (Op65_EE) { RunSynertekOpcode (0xEE); }
        TEST_METHOD (Op65_EF) { RunSynertekOpcode (0xEF); }
        TEST_METHOD (Op65_F0) { RunSynertekOpcode (0xF0); }
        TEST_METHOD (Op65_F1) { RunSynertekOpcode (0xF1); }
        TEST_METHOD (Op65_F2) { RunSynertekOpcode (0xF2); }
        TEST_METHOD (Op65_F3) { RunSynertekOpcode (0xF3); }
        TEST_METHOD (Op65_F4) { RunSynertekOpcode (0xF4); }
        TEST_METHOD (Op65_F5) { RunSynertekOpcode (0xF5); }
        TEST_METHOD (Op65_F6) { RunSynertekOpcode (0xF6); }
        TEST_METHOD (Op65_F7) { RunSynertekOpcode (0xF7); }
        TEST_METHOD (Op65_F8) { RunSynertekOpcode (0xF8); }
        TEST_METHOD (Op65_F9) { RunSynertekOpcode (0xF9); }
        TEST_METHOD (Op65_FA) { RunSynertekOpcode (0xFA); }
        TEST_METHOD (Op65_FB) { RunSynertekOpcode (0xFB); }
        TEST_METHOD (Op65_FC) { RunSynertekOpcode (0xFC); }
        TEST_METHOD (Op65_FD) { RunSynertekOpcode (0xFD); }
        TEST_METHOD (Op65_FE) { RunSynertekOpcode (0xFE); }
        TEST_METHOD (Op65_FF) { RunSynertekOpcode (0xFF); }

    };


}
