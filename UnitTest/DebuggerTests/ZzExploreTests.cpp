#include "Pch.h"

#include "SessionRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace DebuggerTests
{
    TEST_CLASS (ZzExploreTests)
    {
    public:

        static const char * ModeName (CommandMode mode)
        {
            switch (mode)
            {
            case CommandMode::Monitor:   return "MON";
            case CommandMode::GSSquared: return "GSS";
            case CommandMode::WinDbg:    return "WDB";
            case CommandMode::Casso:     return "CAS";
            default:                     return "AWN";
            }
        }

        static void Dump (std::ostream & out, const Reply & reply, const std::string & line, CommandMode mode)
        {
            out << "[" << ModeName (mode) << "] " << line << "\n";
            out << "   status=" << (int) reply.status << " label=<" << reply.error.label << "> detail=<" << reply.error.detail << ">\n";

            for (const std::string & text : reply.text)
            {
                out << "   |" << text << "|\n";
            }
        }

        static void D (SessionRig & rig, std::ostream & out, const std::string & line, CommandMode mode = CommandMode::AppleWin)
        {
            Reply  reply = rig.Run (line, mode);

            Dump (out, reply, line, mode);
        }

        static std::ofstream Open (const char * name)
        {
            std::string  path = std::string ("C:\\Users\\relmer\\AppData\\Local\\Temp\\claude\\C--Users-relmer-source-repos-relmer-Casso-worktrees-035-debugger\\1c1cd0a8-d304-442e-8b5d-62af4ccf9e22\\scratchpad\\") + name;

            return std::ofstream (path, std::ios::binary);
        }

        static void LoadProgram (SessionRig & rig)
        {
            //  $0300: LDA #$41 / STA $0400 / JSR $FDED / RTS / LDA $C000 / STA $C000 / INC $C000 / LDA $C07E / STA $C07E / LDA $C05E / STA $C05E / JSR $FC58
            rig.Poke (0x0300, { 0xA9, 0x41, 0x8D, 0x00, 0x04, 0x20, 0xED, 0xFD, 0x60,
                                0xAD, 0x00, 0xC0, 0x8D, 0x00, 0xC0, 0xEE, 0x00, 0xC0,
                                0xAD, 0x7E, 0xC0, 0x8D, 0x7E, 0xC0, 0xAD, 0x5E, 0xC0, 0x8D, 0x5E, 0xC0, 0x20, 0x58, 0xFC });
        }

        TEST_METHOD (Explore_U)
        {
            std::ofstream  out = Open ("explore_u.txt");
            SessionRig     rig;

            LoadProgram (rig);
            D (rig, out, "U");
            D (rig, out, "U 300");
            D (rig, out, "U");
            D (rig, out, "U 300", CommandMode::Casso);
            D (rig, out, "U 300", CommandMode::GSSquared);
            D (rig, out, "!U 300", CommandMode::WinDbg);
            D (rig, out, "/U 300", CommandMode::Monitor);
            D (rig, out, "U 300:30A");
            D (rig, out, "U 300:309");
            D (rig, out, "U 300,5");
            D (rig, out, "U 300,0");
            D (rig, out, "300L");
            D (rig, out, "300l");
            D (rig, out, "COUTL");
            D (rig, out, "FROGL");
            D (rig, out, "R A 12");
            D (rig, out, "AL");
            D (rig, out, "U A");
            D (rig, out, "U S");
            D (rig, out, "U B");
            D (rig, out, "U FFFD");
            D (rig, out, "U");
            D (rig, out, "U FFF0:FFFF");
            D (rig, out, "U");
            D (rig, out, "U 400:300");
            D (rig, out, "U");
            D (rig, out, "U FFF0,20");
            D (rig, out, "U ZZZ");
            D (rig, out, "U 12G");
            D (rig, out, "U 10000");
            D (rig, out, "U 300 400");
            D (rig, out, "U C000");
            D (rig, out, "U BFFE");
            D (rig, out, "U 309:31D");
            D (rig, out, "SYMMAIN OFF");
            D (rig, out, "U 300:309");
            D (rig, out, "U 309:31D");
            D (rig, out, "SYMMAIN ON");
            D (rig, out, "u 300", CommandMode::WinDbg);
            D (rig, out, "u 0x300", CommandMode::WinDbg);
            D (rig, out, "u $300", CommandMode::WinDbg);
            D (rig, out, "u", CommandMode::WinDbg);
            D (rig, out, "u 300 310", CommandMode::WinDbg);
            D (rig, out, "u 300 l8", CommandMode::WinDbg);
            D (rig, out, "u 300,8", CommandMode::WinDbg);
            D (rig, out, "u 300:310", CommandMode::WinDbg);
            D (rig, out, "ub 300", CommandMode::WinDbg);
            D (rig, out, "uf 300", CommandMode::WinDbg);
            D (rig, out, "ln 300", CommandMode::WinDbg);
            D (rig, out, "l", CommandMode::GSSquared);
            D (rig, out, "list", CommandMode::GSSquared);
            D (rig, out, "list 300", CommandMode::GSSquared);
            D (rig, out, "300l", CommandMode::GSSquared);
            D (rig, out, "300L", CommandMode::GSSquared);
            D (rig, out, "00/300l", CommandMode::GSSquared);
            D (rig, out, "l 300 400", CommandMode::GSSquared);
            D (rig, out, "300l 5", CommandMode::GSSquared);
            D (rig, out, "l zz", CommandMode::GSSquared);
            D (rig, out, "l e1/300", CommandMode::GSSquared);
            D (rig, out, "300L", CommandMode::Monitor);
            D (rig, out, "L", CommandMode::Monitor);
            D (rig, out, "U 300:30A", CommandMode::WinDbg);
            D (rig, out, "SYM LOOP = 300");
            D (rig, out, "SYM PTR = 6");
            D (rig, out, "U 300:309");
            D (rig, out, "u 300", CommandMode::WinDbg);
            D (rig, out, "/U 300:309", CommandMode::Monitor);
            D (rig, out, "300L", CommandMode::Monitor);
            D (rig, out, "l 300", CommandMode::GSSquared);
        }

        TEST_METHOD (Explore_U_65C02)
        {
            std::ofstream  out = Open ("explore_u65c02.txt");
            SessionRig     rig ("Apple2eEnhanced");
            SessionRig     nmos;

            rig.Poke (0x0300, { 0x80, 0x02, 0x64, 0x3E, 0x0F, 0x3E, 0x10, 0xA7, 0x53, 0x02, 0x9C, 0x00, 0x04 });
            nmos.Poke (0x0300, { 0x80, 0x02, 0x64, 0x3E, 0x0F, 0x3E, 0x10, 0xA7, 0x53, 0x02, 0x9C, 0x00, 0x04 });
            D (rig, out, "U 300:30C");
            D (nmos, out, "U 300:30C");
        }

        TEST_METHOD (Explore_Data)
        {
            std::ofstream  out = Open ("explore_data.txt");
            SessionRig     rig;

            rig.Poke (0x0300, { 0xA9, 0x41, 0x01, 0x02, 0x03, 0x04, 0x60, 0x34, 0x12, 0x78, 0x56, 0xC8, 0xC9, 0x8D, 0xED, 0xFD, 0x81, 0x40, 0x00, 0x00, 0x00 });
            D (rig, out, "B");
            D (rig, out, "DB 300");
            D (rig, out, "Z 2F0");
            D (rig, out, "DW 3A0");
            D (rig, out, "DA 330");
            D (rig, out, "DF 340");
            D (rig, out, "ASC 320");
            D (rig, out, "DB2 350");
            D (rig, out, "DB4 354");
            D (rig, out, "DB8 358");
            D (rig, out, "DW2 380");
            D (rig, out, "DW4 384");
            D (rig, out, "B");
            D (rig, out, "X 200:3FF");
            D (rig, out, "DB 300:303");
            D (rig, out, "ASC 320,5");
            D (rig, out, "DW TABLE 310:313");
            D (rig, out, "DB8 NAME = 360");
            D (rig, out, "X 200:3FF");
            D (rig, out, "DB COLOR 300");
            D (rig, out, "DB A 300");
            D (rig, out, "DB FACE 300");
            D (rig, out, "DB 300 400");
            D (rig, out, "DB NAME=300");
            D (rig, out, "DB NAME= 300");
            D (rig, out, "DB");
            D (rig, out, "DB NAME");
            D (rig, out, "X 0:FFFF");
            D (rig, out, "DB 310:300");
            D (rig, out, "B");
            D (rig, out, "X 0:FFFF");
            D (rig, out, "DW FFFF");
            D (rig, out, "DA FFFF");
            D (rig, out, "DF FFFC");
            D (rig, out, "B");
            D (rig, out, "X 0:FFFF");
            D (rig, out, "DB 300,0");
            D (rig, out, "DW 10000");
            D (rig, out, "ASC 30G");
            D (rig, out, "DB 300:ZZZ");
            D (rig, out, "DB NAME 300,0");
            D (rig, out, "B");
            D (rig, out, "X");
            D (rig, out, "DB 300:30F");
            D (rig, out, "X B_0300");
            D (rig, out, "X 300,0");
            D (rig, out, "X 10000");
            D (rig, out, "X 300 310");
            D (rig, out, "X 310:300");
            D (rig, out, "B");
            D (rig, out, "X 0:FFFF");
            D (rig, out, "DB FDE0:FDFF");
            D (rig, out, "X COUT");
            D (rig, out, "/B", CommandMode::Monitor);
            D (rig, out, "!B", CommandMode::WinDbg);
            D (rig, out, "B", CommandMode::GSSquared);
            D (rig, out, "dw 300", CommandMode::GSSquared);
            D (rig, out, "DB 300", CommandMode::GSSquared);
            D (rig, out, "DB", CommandMode::GSSquared);
            D (rig, out, "dw2 308", CommandMode::GSSquared);
            D (rig, out, "z 30C", CommandMode::GSSquared);
            D (rig, out, "asc 30B:30D", CommandMode::GSSquared);
            D (rig, out, "!db4 302", CommandMode::WinDbg);
            D (rig, out, "/db2 330", CommandMode::Monitor);
            D (rig, out, "x 300", CommandMode::GSSquared);
            D (rig, out, "/X 300", CommandMode::Monitor);
            D (rig, out, "!X 330", CommandMode::WinDbg);
            D (rig, out, "B");
            D (rig, out, "X 0:FFFF");
            D (rig, out, "DB 302:305");
            D (rig, out, "DW2 307:30A");
            D (rig, out, "ASC 30B:30D");
            D (rig, out, "DA 30E");
            D (rig, out, "DF 310");
            D (rig, out, "U 300:314");
            D (rig, out, "U 300:314", CommandMode::WinDbg);
            D (rig, out, "/U 300:314", CommandMode::Monitor);
            D (rig, out, "l 300", CommandMode::GSSquared);
            D (rig, out, "300L", CommandMode::Monitor);
            rig.Poke (0x0400, { 0x7D, 0x4C, 0xCC, 0xCC, 0xCD, 0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 0x80, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x12, 0x34, 0x56, 0x78, 0x81, 0x40 });
            D (rig, out, "DF 400:417");
            D (rig, out, "U 400:417");
            D (rig, out, "DW 420:424");
            rig.Poke (0x0420, { 0x01, 0x02, 0x03, 0x04, 0x05 });
            D (rig, out, "U 420:424");
            rig.Poke (0x0430, { 0x41, 0x7F, 0x00, 0xC1, 0x20, 0xFF });
            D (rig, out, "ASC 430:435");
            D (rig, out, "U 430:435");
            D (rig, out, "ASC 440:47F");
            D (rig, out, "U 440:47F");
            D (rig, out, "SYM HERE = 302");
            D (rig, out, "U 300:306");
            rig.Poke (0x02FF, { 0x20 });
            D (rig, out, "U 2FF:306");
            D (rig, out, "DW 500:507");
            D (rig, out, "U 501:507");
            D (rig, out, "X 500");
            D (rig, out, "U 500:507");
            D (rig, out, "DB 600:60F");
            D (rig, out, "X 608:609");
            D (rig, out, "U 600:60F");
        }

        TEST_METHOD (Explore_A)
        {
            std::ofstream  out = Open ("explore_a.txt");
            SessionRig     rig;

            D (rig, out, "A 300");
            D (rig, out, "LDA #41");
            D (rig, out, "FROB");
            D (rig, out, "LDA #$1234");
            D (rig, out, "STA #$41");
            D (rig, out, "BCC $0382");
            D (rig, out, "LDA ( $3E ) , Y");
            D (rig, out, "JSR COUT");
            D (rig, out, "LDA #");
            D (rig, out, "LOOP LDA #0");
            D (rig, out, "LDA #41 ; load");
            D (rig, out, "LDA A");
            D (rig, out, "LDA ($003E,X)");
            D (rig, out, "LDA 003E,X");
            D (rig, out, "LDA $12345");
            D (rig, out, "STC");
            D (rig, out, "DISABLE");
            D (rig, out, "ENABLE");
            D (rig, out, "STI");
            D (rig, out, "STD");
            D (rig, out, "LAX $10");
            D (rig, out, "NOP");
            D (rig, out, "NOP #$41");
            D (rig, out, "STZ $3E");
            D (rig, out, "CLC");
            D (rig, out, "BRK");
            D (rig, out, "JSR FDED");
            D (rig, out, "R");
            D (rig, out, "");
            D (rig, out, "U 300:330");
            D (rig, out, "A");
            D (rig, out, "");
            D (rig, out, "A COUT");
            D (rig, out, "");
            D (rig, out, "A ZZZ");
            D (rig, out, "A 10000");
            D (rig, out, "A 300,0");
            D (rig, out, "A 300:200");
            D (rig, out, "");
            D (rig, out, "R A 42");
            D (rig, out, "A A");
            D (rig, out, "");
            D (rig, out, "A F800");
            D (rig, out, "LDA #0");
            D (rig, out, "");
            D (rig, out, "A BFFF");
            D (rig, out, "JMP $1234");
            D (rig, out, "");
            D (rig, out, "U BFFF");
            D (rig, out, "A FFFE");
            D (rig, out, "NOP");
            D (rig, out, "NOP");
            D (rig, out, "NOP");
            D (rig, out, "");
            D (rig, out, "A FFF0");
            D (rig, out, "BNE $0005");
            D (rig, out, "");
            D (rig, out, "MODE MONITOR");
            D (rig, out, "!", CommandMode::Monitor);
            D (rig, out, "LDA #41", CommandMode::Monitor);
            D (rig, out, "300:LDA #0", CommandMode::Monitor);
            D (rig, out, "$300L", CommandMode::Monitor);
            D (rig, out, "", CommandMode::Monitor);
            D (rig, out, "F666G", CommandMode::Monitor);
            D (rig, out, "", CommandMode::Monitor);
            D (rig, out, "300!", CommandMode::Monitor);
            D (rig, out, "", CommandMode::Monitor);
            D (rig, out, "/MODE APPLEWIN", CommandMode::Monitor);
            D (rig, out, "!A 300", CommandMode::WinDbg);
            D (rig, out, "lda #41", CommandMode::WinDbg);
            D (rig, out, "", CommandMode::WinDbg);
            D (rig, out, "A", CommandMode::GSSquared);
            D (rig, out, "A 300", CommandMode::GSSquared);
        }

        TEST_METHOD (Explore_Sym)
        {
            std::ofstream  out = Open ("explore_sym.txt");
            SessionRig     rig;
            SessionRig     plus ("Apple2Plus");

            rig.CopyFixture ("Debugger/DebugFiles/hello.dbg", L"hello.dbg");
            rig.CopyFixture ("Debugger/Merlin/PI.ADD.LST", L"PI.ADD.LST");
            rig.files.WriteAllText (L"C:\\Work\\labels.sym", "0300 START\n0310 LOOP\n");
            rig.files.WriteAllText (L"C:\\Work\\casso.sym", "; by address\nSTART=$0300\nLOOP=$0310\n");
            rig.files.WriteAllText (L"C:\\Work\\vice.lbl", "al 0300 .start\nal 0310 .loop\n");
            rig.files.WriteAllText (L"C:\\Work\\vice2.lbl", "al C:0300 .start\nal C:0310 .loop\n");
            rig.files.WriteAllText (L"C:\\Work\\ld65.lbl", "al 000300 .start\nal 000310 .loop\n");
            rig.files.WriteAllText (L"C:\\Work\\comment.sym", "; symbol table for the game\nSTART=$0300\n");
            rig.files.WriteAllText (L"C:\\Work\\comment2.sym", "; symbol table\n0300 START\n");
            rig.files.WriteAllText (L"C:\\Work\\bad.txt", "nothing here\n");
            rig.files.WriteAllText (L"C:\\Work\\a,b.sym", "0300 COMMA\n");
            rig.files.WriteAllText (L"C:\\Work\\mixed.sym", "Loop=$0300\nLOOP=$0310\n");
            rig.files.WriteAllText (L"C:\\Work\\mixfmt.sym", "START=$0300\n0310 LOOP\n");
            rig.files.WriteAllText (L"C:\\Work\\v3.dbg", "version\tmajor=3,minor=0\n");
            rig.files.WriteAllText (L"C:\\Work\\bad.dbg", "version\tmajor=2,minor=0\nline\tid=0,file=5,line=1,span=0\n");
            rig.files.WriteAllText (L"C:\\Work\\my file.sym", "0300 SPACED\n");

            D (rig, out, "SYM");
            D (rig, out, "SYMINFO");
            D (plus, out, "SYM");
            D (rig, out, "SYM cout");
            D (rig, out, "SYM FDED");
            D (rig, out, "SYM $FDED");
            D (rig, out, "SYM C000");
            D (rig, out, "SYMMAIN C000");
            D (rig, out, "SYM CO*");
            D (rig, out, "SYMMAIN H?ME");
            D (rig, out, "SYM ZZ*");
            D (rig, out, "SYM NOSUCH");
            D (rig, out, "SYMBASIC COUT");
            D (rig, out, "SYMPRODOS MLI");
            D (rig, out, "SYMPRO MLI");
            D (rig, out, "SYMDOS DOSWARM");
            D (rig, out, "SYMDOS33 3D0");
            D (rig, out, "SYM LIFE = 300");
            D (rig, out, "SYM life=310");
            D (rig, out, "SYM LIFE");
            D (rig, out, "SYM = 300");
            D (rig, out, "SYM X = ZZZ");
            D (rig, out, "SYM A B = 300");
            D (rig, out, "SYM ABC = 300");
            D (rig, out, "SYM ! LIFE");
            D (rig, out, "SYM ~ A");
            D (rig, out, "SYM ! LIFE");
            D (rig, out, "SYM !");
            D (rig, out, "SYM !LIFE");
            D (rig, out, "SYMUSER OFF");
            D (rig, out, "SYM N = 1");
            D (rig, out, "SYM N");
            D (rig, out, "SYMUSER N");
            D (rig, out, "SYMUSER ON");
            D (rig, out, "SYMLIST");
            D (rig, out, "SYMLIST src");
            D (rig, out, "SYMLIST SYMUSER");
            D (rig, out, "SYMLIST bogus");
            D (rig, out, "SYMLIST", CommandMode::GSSquared);
            D (rig, out, "SYMLIST SRC", CommandMode::GSSquared);
            D (rig, out, "SYMMAIN");
            D (rig, out, "SYM CLEAR = 300");
            D (rig, out, "SYM LOAD = 300");
            D (rig, out, "SYM LOAD labels.sym");
            D (rig, out, "SYM LOAD \"casso.sym\"");
            D (rig, out, "SYM LOAD vice.lbl");
            D (rig, out, "SYM LOAD vice2.lbl");
            D (rig, out, "SYM LOAD ld65.lbl");
            D (rig, out, "SYM LOAD comment.sym");
            D (rig, out, "SYM LOAD comment2.sym");
            D (rig, out, "SYM LOAD bad.txt");
            D (rig, out, "SYM LOAD \"a,b.sym\"");
            D (rig, out, "SYM LOAD mixed.sym");
            D (rig, out, "SYMINFO");
            D (rig, out, "SYM LOAD mixfmt.sym");
            D (rig, out, "SYMLIST");
            D (rig, out, "SYM LOAD v3.dbg");
            D (rig, out, "SYM LOAD bad.dbg");
            D (rig, out, "SYM LOAD missing.dbg");
            D (rig, out, "SYM LOAD \"\"");
            D (rig, out, "SYM LOAD ,1000");
            D (rig, out, "SYM LOAD labels.sym,zz");
            D (rig, out, "SYMUSER LOAD");
            D (rig, out, "SYM LOAD \"my  file.sym\"");
            D (rig, out, "SYM LOAD \"my file.sym\"");
            D (rig, out, "SYM LOAD labels.sym,FF00");
            D (rig, out, "SYMLIST");
            D (rig, out, "SYM LOAD hello.dbg");
            D (rig, out, "SYMLIST");
            D (rig, out, "SRC");
            D (rig, out, "SYM LOAD hello.dbg,1000");
            D (rig, out, "SYMLIST");
            D (rig, out, "SYM LOAD PI.ADD.LST");
            D (rig, out, "SYMLIST");
            D (rig, out, "SYM SAVE out.sym");
            D (rig, out, "SYM SAVE");
            D (rig, out, "SYM SAVE out.sym,1000");
            D (rig, out, "SYM CLEAR");
            D (rig, out, "SRC");
            D (rig, out, "sload \"labels.sym\"", CommandMode::GSSquared);
            D (rig, out, "sload \"my file.sym\"", CommandMode::GSSquared);
            D (rig, out, "sload labels.sym,1000", CommandMode::GSSquared);
            D (rig, out, "slookup FDED", CommandMode::GSSquared);
            D (rig, out, "slookup 00/FDED", CommandMode::GSSquared);
            D (rig, out, "slookup", CommandMode::GSSquared);
            D (rig, out, "slookup 300", CommandMode::GSSquared);
            D (rig, out, "slookup C000", CommandMode::GSSquared);
            D (rig, out, "sclear", CommandMode::GSSquared);
            D (rig, out, "sclear x", CommandMode::GSSquared);
            D (rig, out, "syminfo", CommandMode::GSSquared);
            D (rig, out, "SYM cout", CommandMode::GSSquared);
            D (rig, out, "x cout", CommandMode::WinDbg);
            D (rig, out, "x home*", CommandMode::WinDbg);
            D (rig, out, "x", CommandMode::WinDbg);
            D (rig, out, "x LIFE = 300", CommandMode::WinDbg);
            D (rig, out, "x load labels.sym", CommandMode::WinDbg);
            D (rig, out, "x save out2.sym", CommandMode::WinDbg);
            D (rig, out, "x clear", CommandMode::WinDbg);
            D (rig, out, "x off", CommandMode::WinDbg);
            D (rig, out, "x on", CommandMode::WinDbg);
            D (rig, out, "x c000", CommandMode::WinDbg);
            D (rig, out, "x hello!start", CommandMode::WinDbg);
            D (rig, out, "x *!cout", CommandMode::WinDbg);
            D (rig, out, "!SYM cout", CommandMode::WinDbg);
            D (rig, out, "!SYMINFO", CommandMode::WinDbg);
            D (rig, out, ".sympath", CommandMode::WinDbg);
            D (rig, out, ".srcpath", CommandMode::WinDbg);
            D (rig, out, "ld hello", CommandMode::WinDbg);
            D (rig, out, "/SYM cout", CommandMode::Monitor);
            D (rig, out, "/SYMINFO", CommandMode::Monitor);
            D (rig, out, "/SYM LIFE = 300", CommandMode::Monitor);
            D (rig, out, "/SYM LOAD labels.sym", CommandMode::Monitor);
            D (rig, out, "/SYM CLEAR", CommandMode::Monitor);
            D (rig, out, "/SYM OFF", CommandMode::Monitor);
            D (rig, out, "/SYM ON", CommandMode::Monitor);
            D (rig, out, "SYM cout", CommandMode::Casso);
            D (rig, out, "SYMMAIN CLEAR");
            D (rig, out, "SYM COUT");
            D (rig, out, "U FDED");
        }

        TEST_METHOD (Explore_Src)
        {
            std::ofstream  out = Open ("explore_src.txt");
            SessionRig     rig;

            rig.CopyFixture ("Debugger/DebugFiles/hello.dbg", L"hello.dbg");
            rig.CopyFixture ("Debugger/DebugFiles/hello.bin", L"hello.bin");

            D (rig, out, "SRC");
            D (rig, out, "SRC ON");
            D (rig, out, "SRC OFF");
            D (rig, out, "SRC SIDEWAYS");
            D (rig, out, "lsa", CommandMode::WinDbg);
            D (rig, out, "lsa 12", CommandMode::WinDbg);
            D (rig, out, "l+s", CommandMode::WinDbg);
            D (rig, out, "l-s", CommandMode::WinDbg);
            D (rig, out, "l+t", CommandMode::WinDbg);
            D (rig, out, "l-t", CommandMode::WinDbg);
            D (rig, out, "l+s foo", CommandMode::WinDbg);
            D (rig, out, "lsf hello.s", CommandMode::WinDbg);
            D (rig, out, ".lines", CommandMode::WinDbg);
            D (rig, out, "ls", CommandMode::WinDbg);
            D (rig, out, "src", CommandMode::GSSquared);
            D (rig, out, "src on", CommandMode::GSSquared);
            D (rig, out, "/SRC", CommandMode::Monitor);
            D (rig, out, "/SRC ON", CommandMode::Monitor);
            D (rig, out, "!SRC", CommandMode::WinDbg);
            D (rig, out, "!SRC OFF", CommandMode::WinDbg);
            D (rig, out, "SRC", CommandMode::Casso);
            D (rig, out, "BP hello.s:9");
            D (rig, out, "BLOAD hello.bin 300");
            D (rig, out, "SYM LOAD hello.dbg");
            D (rig, out, "R PC 300");
            D (rig, out, "SRC");
            D (rig, out, "SRC ON");
            D (rig, out, "T");
            D (rig, out, "SRC");
            D (rig, out, "T");
            D (rig, out, "T");
            D (rig, out, "T");
            D (rig, out, "T");
            D (rig, out, "R PC 300");
            D (rig, out, "P");
            D (rig, out, "P");
            D (rig, out, "P");
            D (rig, out, "P");
            D (rig, out, "R PC 300");
            D (rig, out, "T 3");
            D (rig, out, "R PC 300");
            D (rig, out, "t", CommandMode::WinDbg);
            D (rig, out, "p", CommandMode::WinDbg);
            D (rig, out, "s", CommandMode::GSSquared);
            D (rig, out, "o", CommandMode::GSSquared);
            D (rig, out, "R PC 300");
            D (rig, out, "S", CommandMode::Monitor);
            D (rig, out, "T", CommandMode::Monitor);
            D (rig, out, "R PC 300");
            D (rig, out, "/T", CommandMode::Monitor);
            D (rig, out, "R PC F800");
            D (rig, out, "SRC");
            D (rig, out, "BP hello.s:9");
            D (rig, out, "BP hello.s:3");
            D (rig, out, "BP hello.s:4");
            D (rig, out, "BP hello.s:1");
            D (rig, out, "BP hello.s:0");
            D (rig, out, "BP hello.s:999");
            D (rig, out, "BP other.s:3");
            D (rig, out, "BP other/hello.s:3");
            D (rig, out, "BP HELLO.S:11 IF A=3");
            D (rig, out, "BPX hello.s:10");
            D (rig, out, "BP hello.s:3000000000");
            D (rig, out, "BP add:12");
            D (rig, out, "BP start:12");
            D (rig, out, "BPA hello.s:9");
            D (rig, out, "bp hello.s:9", CommandMode::WinDbg);
            D (rig, out, "bp `hello.s:9`", CommandMode::WinDbg);
            D (rig, out, "/BP hello.s:9", CommandMode::Monitor);
            D (rig, out, "bp hello.s:9", CommandMode::GSSquared);
            D (rig, out, "BPX hello.s:9", CommandMode::GSSquared);
            D (rig, out, "BPL");
            D (rig, out, "BPC *");
            D (rig, out, "R PC 300");
            D (rig, out, "BP hello.s:11");
            D (rig, out, "G");
            D (rig, out, "BPL");
        }

        TEST_METHOD (Explore_Skip)
        {
            std::ofstream  out = Open ("explore_skip.txt");
            SessionRig     rig;

            D (rig, out, "SKIP");
            D (rig, out, "SKIP COUT");
            D (rig, out, "SKIP 0310");
            D (rig, out, "SKIP $0310.$0312");
            D (rig, out, "SKIP F800.FFFF");
            D (rig, out, "SKIP 0310");
            D (rig, out, "SKIP");
            D (rig, out, "SKIP -COUT");
            D (rig, out, "SKIP - 0310");
            D (rig, out, "SKIP -F800.FFFF");
            D (rig, out, "SKIP -");
            D (rig, out, "SKIP -NOSUCH");
            D (rig, out, "SKIP NOSUCH");
            D (rig, out, "SKIP FFFF.F800");
            D (rig, out, "SKIP A");
            D (rig, out, "SKIP X");
            D (rig, out, "SKIP PC");
            D (rig, out, "SKIP -X");
            D (rig, out, "SKIP HOME RDKEY");
            D (rig, out, "SKIP CLEAR");
            D (rig, out, "skip", CommandMode::GSSquared);
            D (rig, out, "skip cout", CommandMode::GSSquared);
            D (rig, out, "!skip cout", CommandMode::WinDbg);
            D (rig, out, "!skip", CommandMode::WinDbg);
            D (rig, out, "/skip home", CommandMode::Monitor);
            D (rig, out, "/skip", CommandMode::Monitor);
            D (rig, out, "SKIP", CommandMode::Casso);
            D (rig, out, "/skip clear", CommandMode::Monitor);
        }
    };
}
