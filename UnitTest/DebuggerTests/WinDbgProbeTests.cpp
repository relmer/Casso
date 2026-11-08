#include "Pch.h"

#include "ControllerRig.h"
#include "Debugger/ReplyJson.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace DebuggerTests
{
    TEST_CLASS (WinDbgProbeTests)
    {
    public:
        static void Dump (std::ofstream & out, const std::string & line, const Reply & reply, DebugSession & session)
        {
            Cpu6502Registers  r = session.GetTarget().GetRegisters();

            out << "=== [" << line << "] status=" << (int) reply.status << " label=[" << reply.error.label << "] detail=[" << reply.error.detail << "]\n";

            for (const std::string & text : reply.text)
            {
                out << "  |" << text << "|\n";
            }

            out << std::format ("  regs a={:02x} x={:02x} y={:02x} sp={:02x} pc={:04x} p={:02x} state={}\n", r.a, r.x, r.y, r.sp, r.pc, r.p, (int) session.GetRunState());
        }

        static void Finish (ControllerRig & rig, std::ofstream & out)
        {
            int  slices = 0;

            while (rig.controller.GetRunDriver().IsRunning() && slices < 2000)
            {
                uint64_t  ran = rig.machine.RunCycles (1000);

                rig.controller.GetRunDriver().OnSliceExecuted ((uint32_t) ran);
                slices++;
            }

            out << "  finished after " << slices << " slices; running=" << rig.controller.GetRunDriver().IsRunning() << "\n";
        }

        static void RunAll (const std::string & file, const std::vector<std::string> & lines, bool program)
        {
            std::ofstream  out (file);
            ControllerRig  rig;
            DebugSession & session = rig.controller.GetSession();
            HRESULT        hr      = rig.controller.Open();

            (void) hr;
            ChannelConnectionId  client = rig.transport.Connect();
            rig.controller.Pump();

            if (program)
            {
                // JSR $0310 / RTS at 300, LDA #$41 / STA $0400 / RTS at 310
                const Byte  code[] = { 0x20, 0x10, 0x03, 0x60 };
                const Byte  sub[]  = { 0xA9, 0x41, 0x8D, 0x00, 0x04, 0x60 };

                for (Word i = 0; i < 0x40; i++) { rig.machine.GetMemoryBus().WriteByte ((Word) (0x0300 + i), 0xEA); }
                for (Word i = 0; i < 4; i++) { rig.machine.GetMemoryBus().WriteByte ((Word) (0x0300 + i), code[i]); }
                for (Word i = 0; i < 6; i++) { rig.machine.GetMemoryBus().WriteByte ((Word) (0x0310 + i), sub[i]); }

                Cpu6502Registers  r = session.GetTarget().GetRegisters();
                r.pc = 0x0300; r.sp = 0xFF; r.p = 0x34; r.a = 0; r.x = 0; r.y = 0;
                session.GetTarget().SetRegisters (r);
            }

            session.ExecuteLine ("MODE WINDBG");

            for (const std::string & line : lines)
            {
                if (line == "@finish")
                {
                    Finish (rig, out);
                    rig.controller.Pump();
                    for (const std::string & w : rig.transport.Written (client)) { out << "  pipe: " << w << "\n"; }
                    continue;
                }

                Reply  reply = rig.view.ExecuteWindowLine (session, line, session.GetMode());
                Dump (out, line, reply, session);
            }
        }

        TEST_METHOD (Probe_Lines)
        {
            RunAll ("C:\\Users\\relmer\\AppData\\Local\\Temp\\claude\\C--Users-relmer-source-repos-relmer-Casso-worktrees-035-debugger\\1c1cd0a8-d304-442e-8b5d-62af4ccf9e22\\scratchpad\\probe_lines.txt",
            {
                "", "   ", "DB 300 l4", "  bl", "?300+10", ".FORMATS 41",
                "frobnicate", "FOO", "bu 300", "bm 300", "dc 300", "du 300", "ed 300 1", "a 300", "q", ".cls", "l+t", "l-t", "n 10",
                "* comment", "$$ comment", "; comment", "r; t", "bp 300; g", "kn", "ub 300", "uf 300", "ln 300", "gc", "pc", "tc", ".echo hi", ".detach",
                "dps 300", "dds 300", "dqs 300", "dpa 300", "dpu 300",
                "db 0x300 l4", "db 0X300 l4", "? 0n10", "db 0n768 l4", "? a0x10", "? 0xg", "? 0y1010", "? 0t17", "db 0`0300", "db 2000 l0x20",
                "db 2000 l0n16", "f 2000 l0n16 00", "m 300 l0x10 2000", "db 0x10000", "? 0n70000", "ea 0x400 \"0x\"", "db 400 l4",
                "~0s", "~*k", "|1s", "sxn av", "sxi", "!ext.help", "$t1", "$t19", "$t", "$ta", "lm vm", "!Analyze -v", ".reload /f", "DT foo",
                "??", "?? 1+1", "??x", "?1",
                "!mode", "!SWITCHES", "!bp 300", "!d 0x300", "!skip cout", "! mode", "!frob", "!FROB", "!code", "!hgr", "!", "!   ",
                "!output", "!output applewin", "db 300 l4", "!output windbg", "!mode foo", "!echo 0x41", "!echo \"0x41\"", "!budget 0n1000", "!budget 0x100", "!budget 0",
                "!db 300", "u 300", "!x 300", "!peb", "!heap", "! analyze -v", "!r", "!calc 41", "!bpl", "!bpc *",
                "t zzz", "t =300", "g =300", "p =300", "t 10000", "g 310 320 330",
                "bp", "bp zzz", "bp 300 5", "bp >300", "bp =300", "bp 300:303", "bp 300,4", "bp 300 IF", "bp 300 IF a==41", "bp 300 if x>2", "bp 300 IF zzz((", "bl",
                "bp start + 3", "bp 300 \"r; g\"", "bp /1 300", "bp0 300", "bp `my file.s:12`", "bp `:12`", "bp `prog!main.s:12`", "bp main.s:12", "bp a:12",
                "bc *",
                "ba r1 c000", "ba w1 400", "ba w4 400", "ba e1 300", "ba e4 300", "ba w4 fffe", "ba e4 fffe", "ba w10 400", "ba w16 400", "ba w01 400", "ba w8 400", "bl",
                "ba x1 300", "ba w0 300", "ba w1", "ba r 300", "ba rw1 300", "ba i1 300", "BA R1 C000", "ba r1 c000 IF a==1", "ba w1 400 before", "ba w1 400 junk", "bl",
                "bc", "bc x", "bc 0x1", "bc 99", "bc 1 2", "bc 1-3", "bd", "be", "bd 1", "be 1", "bd *", "be *", "bc 0", "bl", "bc *", "bl",
                "db 2000", "db", "dw 2000 l10", "dd 2000 l8", "dw 2000", "da 400", "da 400 l20", "dw 2000 2010", "db 2000 2010", "db 2010 2000",
                "db ffc0 l100", "db 2000 12345", "dw ffff l80000000", "dd 100 l40000000", "db 2000 l0", "db -5", "db 2000 l-1", "db zzz_nope", "db 2000 l20 x",
                "db 10000", "db 2000 L", "db c000 l10", "dw c000 l4", "dd c000 l2", "da c000", "da 2010 2000", "dw 2000 2000", "dd 2000 2002",
                "eb 300 1234", "eb 300 12345", "ew 300 1234", "ea 400 \"HI\"", "ea 0x400 \"a b\"", "ea 400 \"HI\" extra", "ea", "ea 400 HI", "ea 400 \"H", "ea 400 \"\"", "ea \"HI\"",
                "eb 300", "ew 300", "eb", "eb d000 00", "f bff0 l20 00", "f 2000 l20 00", "f 2000 l20 01 02", "f 2000 l4 100", "f 2000 00", "f 2000 2010 00", "f 2000 l0 00",
                "s 300 l100 a9 41", "s 300 l100 \"HI\"", "s 300 l100 a9 ?", "s 300 l10", "s -a 300 l10 \"x\"", "s -b 300 l10 41", "s 300 l100 0x41", "s 300 l100 0n65", "s 300 l100 \"A  B\"",
                "m 300 l10 2000", "m 300 l10", "m 300 l10 2000 x", "f fff0 l20 00", "s fff0 l20 a9", "m fff0 l20 2000",
                "r", "r a=41", "r a 42", "r a = 43", "r A=0x44", "r x=1", "r y=2", "r sp=f0", "r pc=300", "r fl=30", "r p=31", "r q=1", "r s=1", "r $t0=1",
                "r a", "r sp", "r fl", "r a=100", "r sp=1f0", "r a=1, x=2", "r @a=1", "? @pc", "? @a", "bp @pc",
                "eb 300 a", "? a", "? @1",
                "u 300", "u", "u 300:310", "u 300,10", "u 300 310", "u 300 l10", "u fded",
                "x home*", "x", "x cout", "x fded", "x 0xfded", "x zzz", "x main!home", "x *!cout",
                "k", "k 3", "k mode",
                "?", ".formats", "? 10000", "? -1", ".formats 0n65",
                "l+s", "l-s", "L+S", "lsa", "lsa main.s:3",
                ".help", ".help ba", ".help bpm", ".help hgr", ".help frob", ".help !mode", ".help dd", ".help l-s", ".help bp", ".help r", ".help p", ".help l+s",
                "wt", "wt -l 2",
                "!panel disk",
            }, true);
        }

        TEST_METHOD (Probe_FreeRun)
        {
            std::ofstream  out ("C:\\Users\\relmer\\AppData\\Local\\Temp\\claude\\C--Users-relmer-source-repos-relmer-Casso-worktrees-035-debugger\\1c1cd0a8-d304-442e-8b5d-62af4ccf9e22\\scratchpad\\probe_free.txt");
            ControllerRig  rig;
            DebugSession & session = rig.controller.GetSession();
            HRESULT        hr      = rig.controller.Open();
            (void) hr;
            ChannelConnectionId  client = rig.transport.Connect();
            rig.controller.Pump();

            const Byte  code[] = { 0x20, 0x10, 0x03, 0xEA, 0xEA, 0x4C, 0x05, 0x03 };
            const Byte  sub[]  = { 0xA9, 0x41, 0x8D, 0x00, 0x04, 0xAD, 0x01, 0x04, 0x60 };
            for (Word i = 0; i < sizeof (code); i++) { rig.machine.GetMemoryBus().WriteByte ((Word) (0x0300 + i), code[i]); }
            for (Word i = 0; i < sizeof (sub); i++) { rig.machine.GetMemoryBus().WriteByte ((Word) (0x0310 + i), sub[i]); }
            Cpu6502Registers  r = session.GetTarget().GetRegisters();
            r.pc = 0x0300; r.sp = 0xFF; r.p = 0x24;
            session.GetTarget().SetRegisters (r);

            session.ExecuteLine ("MODE WINDBG");
            Dump (out, "bp 312", rig.view.ExecuteWindowLine (session, "bp 312", session.GetMode()), session);
            rig.cpuManager.SetPaused (false);
            session.OnUserResumed();
            out << "state after resume " << (int) session.GetRunState() << "\n";
            for (int i = 0; i < 3; i++)
            {
                uint64_t ran = rig.machine.RunCycles (1000);
                bool ended = rig.controller.GetRunDriver().OnSliceExecuted ((uint32_t) ran);
                out << "slice ran " << ran << " ended " << ended << " pc " << std::format ("{:04x}", session.GetTarget().GetRegisters().pc) << " paused " << rig.cpuManager.IsPaused() << " state " << (int) session.GetRunState() << "\n";
            }
            rig.controller.Pump();
            for (const std::string & w : rig.transport.Written (client)) { out << "  pipe: " << w << "\n"; }
            Dump (out, "!lbr", rig.view.ExecuteWindowLine (session, "!lbr", session.GetMode()), session);
            Dump (out, "eb 400 1", rig.view.ExecuteWindowLine (session, "eb 400 1", session.GetMode()), session);
            Dump (out, "g", rig.view.ExecuteWindowLine (session, "g", session.GetMode()), session);
            Finish (rig, out);
            rig.controller.Pump();
            for (const std::string & w : rig.transport.Written (client)) { out << "  pipe: " << w << "\n"; }
            Dump (out, "!lbr", rig.view.ExecuteWindowLine (session, "!lbr", session.GetMode()), session);
            Dump (out, "bc *", rig.view.ExecuteWindowLine (session, "bc *", session.GetMode()), session);
            r = session.GetTarget().GetRegisters(); r.pc = 0x0300; r.sp = 0xFF; session.GetTarget().SetRegisters (r);
            Dump (out, "!budget 200", rig.view.ExecuteWindowLine (session, "!budget 200", session.GetMode()), session);
            Dump (out, "g", rig.view.ExecuteWindowLine (session, "g", session.GetMode()), session);
            Finish (rig, out);
            Dump (out, "!lbr", rig.view.ExecuteWindowLine (session, "!lbr", session.GetMode()), session);
            Dump (out, "!budget 0", rig.view.ExecuteWindowLine (session, "!budget 0", session.GetMode()), session);
            out << "speed before gg " << (int) rig.cpuManager.GetSpeedMode() << "\n";
            r = session.GetTarget().GetRegisters(); r.pc = 0x0300; r.sp = 0xFF; session.GetTarget().SetRegisters (r);
            Dump (out, "!gg 304", rig.view.ExecuteWindowLine (session, "!gg 304", session.GetMode()), session);
            out << "speed during gg " << (int) rig.cpuManager.GetSpeedMode() << "\n";
            Finish (rig, out);
            out << "speed after gg " << (int) rig.cpuManager.GetSpeedMode() << "\n";
            Dump (out, "r", rig.view.ExecuteWindowLine (session, "r", session.GetMode()), session);
            Dump (out, ".help", rig.controller.RunLine (".help", CommandMode::WinDbg, {}), session);
        }

        TEST_METHOD (Probe_Kinds)
        {
            RunAll ("C:\\Users\\relmer\\AppData\\Local\\Temp\\claude\\C--Users-relmer-source-repos-relmer-Casso-worktrees-035-debugger\\1c1cd0a8-d304-442e-8b5d-62af4ccf9e22\\scratchpad\\probe_kinds.txt",
            {
                "!bpr a=41", "!bpr a 41", "!brkop ea", "!brk", "!brkint", "!bpmv 400 41", "!bpm 400", "!bpio c030", "ba w1 400 before", "bl",
                "bd 0", "be 0", "bd *", "!bpchange 1 T", "!bpchange 2 s", "bl", "bl 1", "bl 0 2", "bl *", "bl 01", "bl 99", "bl x", "!bpl",
                "bc *", "r pc=300", "r sp=ff", "bp 312", "g", "@finish", "bl", "!bpl", "bc *",
                "r pc=2000", "eb 2000 a9 55 8d ff ff ea ea ea", "ba w4 fffe", "bl", "g 2006", "@finish", "r",
                "bc *", "ba w10 3f6", "bl", "r pc=300", "r sp=ff", "g 304", "@finish",
                "bc *", "r pc=300", "r sp=ff", "ba w1 400 before", "g", "@finish", "db 400 l1", "r", "bl",
                "bc *", "r pc=300", "r sp=ff", "ba w1 400 IF a==41", "g", "@finish", "r",
                "!sym load \"C:\\Work\\none.dbg\"", "bp `my prog.a65:3`",
                "!mode applewin", "BPL", "OUTPUT WINDBG", "BPL", "D 300 l4", "D 300,4", "R", "MODE MONITOR", "DA", "300.303", "DD",
            }, true);
        }

        TEST_METHOD (Probe_Runs)
        {
            RunAll ("C:\\Users\\relmer\\AppData\\Local\\Temp\\claude\\C--Users-relmer-source-repos-relmer-Casso-worktrees-035-debugger\\1c1cd0a8-d304-442e-8b5d-62af4ccf9e22\\scratchpad\\probe_runs.txt",
            {
                "t", "@finish", "r", "k", "gu", "@finish", "r",
                "r pc=300", "r sp=ff", "p", "@finish", "r",
                "r pc=300", "r sp=ff", "p 3", "@finish", "r",
                "r pc=300", "r sp=ff", "t 3", "@finish", "r",
                "r pc=300", "r sp=ff", "t 0", "@finish", "r",
                "r pc=300", "r sp=ff", "t 0n2", "@finish", "r",
                "r pc=300", "r sp=ff", "bp 312", "g", "@finish", "r", "bl",
                "r pc=300", "r sp=ff", "bd 0", "g 303", "@finish", "r",
                "r pc=300", "r sp=ff", "bc *", "ba w1 400", "g", "@finish", "r", "bl",
                "r pc=300", "r sp=ff", "bc *", "ba r1 400", "g 303", "@finish", "r",
                "r pc=300", "r sp=ff", "bc *", "ba e1 310", "g", "@finish", "r",
                "r pc=300", "r sp=ff", "bc *", "pa 310", "@finish", "r",
                "r pc=300", "r sp=ff", "ta 312", "@finish", "r",
                "r pc=300", "r sp=ff", "g 310 320", "@finish", "r",
                "r pc=300", "r sp=ff", "p", "eb 400 99", "db 400 l1", "t", "@finish", "r",
                "!budget 10", "r pc=300", "r sp=ff", "g", "@finish", "r", "!budget 0",
            }, true);
        }
    };
}
