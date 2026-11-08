#include "Pch.h"

#include "RunPathRigs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BranchRecordTests
//
//  LBR. The CPU keeps the record as it executes, so it is right after any
//  run: the emulator running freely with no hook installed, a debugger run
//  with nothing to stop on but its budget, a step. A record kept through the
//  per-instruction hook would pass the step tests here and fail the free
//  run, which is the run a person makes most.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (BranchRecordTests)
    {
    public:

        static constexpr Word  kStart = 0x0300;

        static std::string Lbr (EmulatorRig & rig)
        {
            std::vector<std::string>  text = rig.WindowOk ("LBR").text;



            Assert::AreEqual ((size_t) 1, text.size(), L"LBR is one line");
            return text[0];
        }

        //  $0300: INX / JMP $0300
        static void LoadLoop (EmulatorRig & rig)
        {
            rig.Load (kStart, { 0xE8, 0x4C, 0x00, 0x03 }, kStart);
        }



        //  The run paths.

        TEST_METHOD (AFreeRun_WithNoHookInstalled_IsRecorded)
        {
            EmulatorRig  rig;



            LoadLoop (rig);
            Assert::AreEqual (std::string ("No branch recorded."), Lbr (rig), L"a machine that has run nothing");

            rig.FreeRun (1000);
            Assert::IsNull (rig.machine.GetDebugHook(), L"nothing asks the debugger about any instruction");
            rig.UserPause();

            Assert::AreEqual (std::string ("Last branch at $0301"), Lbr (rig), L"the JMP, not where it went");
        }



        TEST_METHOD (ADebuggerRunWithOnlyABudget_IsRecorded_OnEveryPath)
        {
            EmulatorRig  window;
            EmulatorRig  channel;
            BatchScript  batch;
            JsonValue    reply;



            LoadLoop (window);
            window.WindowOk  ("BUDGET 50");
            window.WindowRun ("G");
            Assert::AreEqual (std::string ("budget"), EmulatorRig::GetString (window.GetLastStop(), "reason"));
            Assert::AreEqual (std::string ("Last branch at $0301"), Lbr (window), L"the window's run");

            LoadLoop (channel);
            reply = channel.Pipe ("G", nullptr, 50);
            Assert::IsTrue   (channel.Slices(), L"the client's run ended on its budget");
            EmulatorRig::AssertLines ({ "Last branch at $0301" }, channel.PipeText ("LBR"), "LBR from the client");

            Assert::AreEqual (3, batch.Run ({ "MEB 300 E8 4C 00 03", "R PC 300", "BUDGET 50", "G", "LBR" }), L"the last run ended on its budget");
            BatchScript::AssertLines ({ "Last branch at $0301" }, batch.GetReplyText (4), "LBR in batch");
        }



        //  Each transfer on the //e's 6502, one step each.
        TEST_METHOD (EveryTransfer_OnThe6502)
        {
            struct Case
            {
                const char                 * name;
                std::initializer_list<Byte>  code;
                Byte                         p;
            };

            static constexpr Byte  kZeroClear = 0x30;
            static constexpr Byte  kZeroSet   = 0x32;
            const Case             cases[]    =
            {
                { "JMP abs",   { 0x4C, 0x00, 0x04 }, kZeroClear },
                { "JMP (ind)", { 0x6C, 0x10, 0x00 }, kZeroClear },
                { "JSR",       { 0x20, 0x00, 0x04 }, kZeroClear },
                { "RTS",       { 0x60 },             kZeroClear },
                { "RTI",       { 0x40 },             kZeroClear },
                { "BRK",       { 0x00 },             kZeroClear },
                { "BNE taken", { 0xD0, 0x10 },       kZeroClear },
                { "BEQ taken", { 0xF0, 0xF0 },       kZeroSet   },
                { "BPL taken", { 0x10, 0x10 },       kZeroClear },
            };



            for (const Case & each : cases)
            {
                EmulatorRig  rig;



                rig.Load (kStart, each.code, kStart, each.p);
                rig.Poke (0x0010, { 0x00, 0x04 });
                rig.Poke (0x01FD, { 0x30, 0x00, 0x04 });

                if (each.code.begin()[0] == 0x60 || each.code.begin()[0] == 0x40)
                {
                    Cpu6502Registers  registers = rig.Session().GetTarget().GetRegisters();



                    registers.sp = (each.code.begin()[0] == 0x60) ? 0xFD : 0xFC;
                    rig.Session().GetTarget().SetRegisters (registers);
                }

                rig.WindowRun ("T");
                Assert::AreNotEqual ((int) 0x0300 + (int) each.code.size(), (int) rig.Session().GetTarget().GetRegisters().pc,
                                     Widen (std::string (each.name) + " transferred").c_str());
                Assert::AreEqual (std::string ("Last branch at $0300"), Lbr (rig), Widen (each.name).c_str());
            }
        }



        //  The 65C02's own transfers, on the enhanced //e.
        TEST_METHOD (EveryTransfer_OnThe65C02)
        {
            for (const auto & [name, code] : std::vector<std::pair<std::string, std::string>>
                                             {
                                                 { "BRA",          "80 10" },
                                                 { "JMP (abs,X)",  "7C 0F 00" },
                                                 { "JMP (ind)",    "6C 10 00" },
                                                 { "BBR0 taken",   "0F 20 10" },
                                                 { "BBS1 taken",   "9F 20 10" },
                                             })
            {
                BatchScript  batch ("Apple2eEnhanced");



                Assert::AreEqual (0, batch.Run ({ "MEB 10 00 04", "MEB 20 02", "MEB 300 " + code, "R PC 300", "R X 1", "T", "LBR" }), Widen (name).c_str());
                BatchScript::AssertLines ({ "Last branch at $0300" }, batch.GetReplyText (6), name);
            }
        }



        //  No transfer: a branch not taken, a branch or jump to the next
        //  instruction, an opcode the 6502 does not have, and an interrupt,
        //  which is no instruction at all. The last record stays.
        TEST_METHOD (NoTransfer_LeavesTheRecordAsItWas)
        {
            EmulatorRig  rig;



            //  $0300: JMP $0304    $0304: BNE $0308 (Z set) / NOP    $0307: BNE $0309 (+0) /
            //  JMP $030C    $030C: ANE / BRK / NOP
            rig.Load (kStart, { 0x4C, 0x04, 0x03, 0xEA, 0xD0, 0x02, 0xEA, 0xD0, 0x00, 0x4C, 0x0C, 0x03, 0x8B, 0x00, 0xEA }, kStart, 0x36);

            rig.WindowRun ("T");
            Assert::AreEqual (std::string ("Last branch at $0300"), Lbr (rig));

            rig.WindowRun ("T");
            Assert::AreEqual (0x0306, (int) rig.Session().GetTarget().GetRegisters().pc, L"not taken");
            Assert::AreEqual (std::string ("Last branch at $0300"), Lbr (rig), L"a branch not taken");

            rig.SetPc (0x0307, 0x34);
            rig.WindowRun ("T");
            Assert::AreEqual (0x0309, (int) rig.Session().GetTarget().GetRegisters().pc, L"taken, to the next instruction");
            Assert::AreEqual (std::string ("Last branch at $0300"), Lbr (rig), L"a branch taken to where it would have gone anyway");

            rig.WindowRun ("T");
            Assert::AreEqual (0x030C, (int) rig.Session().GetTarget().GetRegisters().pc);
            Assert::AreEqual (std::string ("Last branch at $0300"), Lbr (rig), L"a jump to the next instruction");

            rig.WindowRun ("T");
            Assert::AreEqual (0x030D, (int) rig.Session().GetTarget().GetRegisters().pc, L"an opcode the 6502 lacks is one byte");
            Assert::AreEqual (std::string ("Last branch at $0300"), Lbr (rig), L"an opcode the 6502 lacks");
        }



        TEST_METHOD (AnInterruptDispatch_IsNotATransfer)
        {
            EmulatorRig  rig;



            //  $0300: JMP $0304 / NOP    $0304: NOP / NOP    the handler at $0400: NOP
            rig.Load (kStart, { 0x4C, 0x04, 0x03, 0xEA, 0xEA, 0xEA }, kStart, 0x30);
            rig.Poke (0x0400, { 0xEA });
            rig.Poke (0x03FE, { 0x00, 0x04 });

            rig.WindowRun ("T");
            rig.machine.GetCpu()->SetInterruptLine (CpuInterruptKind::kMaskable, true);
            rig.WindowRun ("T");

            Assert::AreNotEqual (0x0305, (int) rig.Session().GetTarget().GetRegisters().pc, L"the interrupt was taken");
            Assert::AreEqual    (std::string ("Last branch at $0300"), Lbr (rig), L"the dispatch is not recorded");
        }



        //  Moving the program counter from the debugger is not a transfer:
        //  =, R PC, addrG and the Monitor's G. The JSR command's own push and
        //  jump are not either, but the RTS its routine runs is.
        TEST_METHOD (TheDebuggersOwnPcChanges_AreNotTransfers)
        {
            EmulatorRig  rig;



            //  $0400: INX / INX / NOP    $0410: INX / RTS
            rig.Load (0x0400, { 0xE8, 0xE8, 0xEA }, kStart);
            rig.Poke (0x0410, { 0xE8, 0x60 });
            rig.Poke (kStart, { 0xEA });

            rig.WindowOk ("= 400");
            rig.WindowOk ("R PC 300");
            rig.WindowOk ("BP 402");
            rig.WindowRun ("400G");
            Assert::AreEqual (0x0402, (int) rig.Session().GetTarget().GetRegisters().pc);
            Assert::AreEqual (std::string ("No branch recorded."), Lbr (rig), L"=, R PC and addrG");

            rig.WindowOk  ("MODE MONITOR");
            rig.WindowRun ("400G");
            Assert::AreEqual (0x0402, (int) rig.Session().GetTarget().GetRegisters().pc);
            rig.WindowOk  ("/MODE APPLEWIN");
            Assert::AreEqual (std::string ("No branch recorded."), Lbr (rig), L"the Monitor's G and the return it pushes");

            rig.WindowOk  ("BPC *");
            rig.WindowOk  ("R PC 300");
            rig.WindowRun ("JSR 410");
            Assert::AreEqual (0x0300, (int) rig.Session().GetTarget().GetRegisters().pc, L"the routine returned");
            Assert::AreEqual (std::string ("Last branch at $0411"), Lbr (rig), L"the RTS, not the JSR command");
        }



        //  The record outlives instructions that do not transfer, and a new
        //  machine starts with none.
        TEST_METHOD (TheRecordPersistsUntilTheNextTransfer)
        {
            EmulatorRig  rig;



            //  $0300: JMP $0304    $0304: INX / INX / NOP
            rig.Load (kStart, { 0x4C, 0x04, 0x03, 0xEA, 0xE8, 0xE8, 0xEA }, kStart);
            rig.WindowRun ("T 3");

            Assert::AreEqual (0x0306, (int) rig.Session().GetTarget().GetRegisters().pc);
            Assert::AreEqual (std::string ("Last branch at $0300"), Lbr (rig));
        }



        //  Every dialect's form; LBR takes no arguments and ignores any.
        TEST_METHOD (EveryDialect)
        {
            EmulatorRig  rig;



            LoadLoop (rig);
            rig.WindowRun ("T 2");

            EmulatorRig::AssertLines ({ "Last branch at $0301" }, rig.WindowOk ("LBR anything at all").text, "LBR with arguments");

            rig.WindowOk ("MODE MONITOR");
            EmulatorRig::AssertLines ({ "Last branch at $0301" }, rig.WindowOk ("/LBR").text, "/LBR");

            rig.WindowOk ("/MODE GSSQUARED");
            EmulatorRig::AssertLines ({ "Last branch at $0301" }, rig.WindowOk ("lbr").text, "lbr");

            rig.WindowOk ("mode windbg");
            EmulatorRig::AssertLines ({ "Last branch at $0301" }, rig.WindowOk ("!lbr").text, "!lbr");

            rig.WindowOk ("!mode casso");
            EmulatorRig::AssertLines ({ "Last branch at $0301" }, rig.WindowOk ("LBR").text, "LBR in Casso mode");
        }



        //  As JSON: the transferring instruction's address, or null.
        TEST_METHOD (Json_CarriesTheAddressOrNull)
        {
            EmulatorRig        rig;
            JsonValue          reply  = rig.Pipe ("LBR");
            const JsonValue  * data   = nullptr;
            const JsonValue  * member = nullptr;



            Assert::IsTrue   (reply.HasObject ("data", data));
            Assert::AreEqual (std::string ("branchRecord"), EmulatorRig::GetString (*data, "kind"));
            member = EmulatorRig::FindMember (*data, "address");
            Assert::IsNotNull (member);
            Assert::IsTrue    (member->GetType() == JsonType::Null, L"no branch yet");

            LoadLoop (rig);
            rig.WindowRun ("T 2");
            reply = rig.Pipe ("LBR");
            Assert::IsTrue   (reply.HasObject ("data", data));
            member = EmulatorRig::FindMember (*data, "address");
            Assert::IsNotNull (member);
            Assert::AreEqual  (0x0301, member->GetInt());
        }

    private:

        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
        }
    };
}
