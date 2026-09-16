#include "Pch.h"

#include "Debugger/Handlers/MemoryHandlers.h"
#include "HandlerTestRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryHandlersTests
//
//  Viewing, entering, moving, comparing, filling and searching memory, the
//  binary and text files, and I/O, against the mock target.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (MemoryHandlersTests)
    {
    public:

        using Rig = HandlerRig<MemoryHandlers>;

        static void Store (Rig & rig, Word address, std::initializer_list<Byte> bytes)
        {
            for (Byte b : bytes)
            {
                rig.target.memory[address++] = b;
            }
        }



        TEST_METHOD (D_RowsOfEight_ContinuesAndShowsRegion)
        {
            Rig    rig;
            Reply  reply;



            Store (rig, 0x0300, { 0xC8, 0xC5, 0xCC, 0xCC, 0xCF, 0x00, 0x41, 0x7F, 0x01 });

            reply = rig.RunOk ("D 300:30F");
            Assert::AreEqual ((size_t) 2, reply.text.size());
            Assert::AreEqual (std::string ("0300: C8 C5 CC CC CF 00 41 7F  HELLO.A."), reply.text[0]);
            Assert::AreEqual (std::string ("0308: 01 00 00 00 00 00 00 00  ........"), reply.text[1]);

            reply = rig.RunOk ("D");
            Assert::AreEqual ((size_t) 8, reply.text.size(), L"D alone shows 64 bytes");
            Assert::IsTrue   (reply.text[0].starts_with ("0310:"), L"from where the last dump ended");

            reply = rig.RunOk ("mdb 300");
            Assert::AreEqual ((size_t) 8, reply.text.size());
            Assert::IsTrue   (reply.text[0].starts_with ("0300:"));

            reply = rig.RunOk ("D C000:C001");
            Assert::AreEqual (std::string ("C000: ?? ??                    .."), reply.text.at (0));
            Assert::AreEqual ((int) MemoryRegion::Io, (int) std::get<MemoryData> (reply.data).rows.at (0).region);
        }



        TEST_METHOD (Enter_BytesWordsAndShorthand_RomRejected)
        {
            Rig    rig;
            Reply  reply;



            reply = rig.RunOk ("ME 300 A9 41 60");
            Assert::AreEqual ((Byte) 0xA9, rig.target.memory[0x0300]);
            Assert::AreEqual ((Byte) 0x60, rig.target.memory[0x0302]);
            Assert::AreEqual (std::string ("0300: A9 41 60                 )A`"), reply.text.at (0));

            rig.RunOk ("MEB 300 1234");
            Assert::AreEqual ((Byte) 0x34, rig.target.memory[0x0300], L"a value above $FF is two bytes, low first");
            Assert::AreEqual ((Byte) 0x12, rig.target.memory[0x0301]);

            rig.RunOk ("MEW 310 1234 5");
            Assert::AreEqual ((Byte) 0x34, rig.target.memory[0x0310]);
            Assert::AreEqual ((Byte) 0x12, rig.target.memory[0x0311]);
            Assert::AreEqual ((Byte) 0x05, rig.target.memory[0x0312]);
            Assert::AreEqual ((Byte) 0x00, rig.target.memory[0x0313], L"MEW always writes words");

            rig.RunOk ("ME16 320 ABCD");
            Assert::AreEqual ((Byte) 0xAB, rig.target.memory[0x0321]);

            rig.RunOk ("330:60 EA");
            Assert::AreEqual ((Byte) 0xEA, rig.target.memory[0x0331]);

            rig.RunFails ("ME D000 1", "memory not writable");
            rig.RunFails ("ME 300",    "invalid arguments");
        }



        TEST_METHOD (Move_Compare_Fill)
        {
            Rig    rig;
            Reply  reply;



            Assert::AreEqual (std::string ("Filled 16 bytes at $0300-$030F."), rig.RunOk ("F 300:30F 41").text.at (0));
            Assert::AreEqual ((Byte) 0x41, rig.target.memory[0x030F]);
            Assert::AreEqual ((Byte) 0x00, rig.target.memory[0x0310]);

            rig.RunOk ("F 400 40F AA BB");
            Assert::AreEqual ((Byte) 0xAA, rig.target.memory[0x0400]);
            Assert::AreEqual ((Byte) 0xBB, rig.target.memory[0x0401]);
            Assert::AreEqual ((Byte) 0xBB, rig.target.memory[0x040F], L"the pattern repeats");

            Assert::AreEqual (std::string ("Moved 16 bytes from $0300 to $0500."), rig.RunOk ("M 500 300:30F").text.at (0));
            Assert::AreEqual ((Byte) 0x41, rig.target.memory[0x050F]);

            Assert::AreEqual (std::string ("Compared 16 bytes, 0 differ."), rig.RunOk ("MC 500 300,10").text.at (0));
            rig.target.memory[0x0505] = 0;
            reply = rig.RunOk ("MC 500 300,10");
            Assert::AreEqual (std::string ("Compared 16 bytes, 1 differ."), reply.text.at (0));
            Assert::AreEqual (std::string ("0305: 41  0505: 00"),           reply.text.at (1));

            Store (rig, 0x0600, { 0, 1, 2, 3, 4, 5, 6, 7 });
            rig.RunOk ("M 601 600:607");
            Assert::AreEqual ((Byte) 0, rig.target.memory[0x0601], L"an overlapping move copies the source as it was");
            Assert::AreEqual ((Byte) 7, rig.target.memory[0x0608]);

            rig.RunOk ("610<600.607M");
            Assert::AreEqual ((Byte) 0, rig.target.memory[0x0611], L"the Monitor move form copies what the range holds now");
            Assert::AreEqual ((Byte) 1, rig.target.memory[0x0612]);

            rig.RunFails ("F 300:30F", "invalid arguments");
            rig.RunFails ("M D000 300:30F", "memory not writable");
        }



        TEST_METHOD (Search_WildcardsAndResults)
        {
            Rig    rig;
            Reply  reply;



            Store (rig, 0x0300, { 0xAD, 0x30, 0xC0 });
            Store (rig, 0x0310, { 0xAD, 0x10, 0xC0 });
            Store (rig, 0x0320, { 0x41, 0x42 });

            Assert::AreEqual (std::string ("Found 2: $0300 $0310"), rig.RunOk ("S 300:3FF AD ? C0").text.at (0));
            Assert::AreEqual (std::string ("Found 2: $0300 $0310"), rig.RunOk ("@").text.at (0));
            Assert::IsTrue   (rig.RunOk ("D @2").text.at (0).starts_with ("0310: AD 10 C0"), L"@n reads as a symbol");

            Assert::AreEqual (std::string ("Found 1: $0311"), rig.RunOk ("SH 300:3FF 1? C0").text.at (0));
            Assert::AreEqual (std::string ("Found 1: $0301"), rig.RunOk ("S 300:3FF C030").text.at (0), L"a 16-bit value matches low byte first, so $30 $C0 at $0301");
            Assert::AreEqual (std::string ("Found 1: $0320"), rig.RunOk ("S 300:3FF \"AB\"").text.at (0));
            Assert::AreEqual (std::string ("Not found."),     rig.RunOk ("S 300:3FF 'AB'").text.at (0), L"quoted with the high bit set");
            Assert::AreEqual (std::string ("Not found."),     rig.RunOk ("@").text.at (0), L"the results are the last search's");
            rig.RunFails ("S 300:3FF", "invalid arguments");
        }



        TEST_METHOD (BLOAD_BSAVE_RawWithAddress)
        {
            Rig    rig;
            Reply  reply;



            rig.files.WriteAllText (L"C:\\Work\\prog.bin", std::string ("\xA9\x41\x60", 3));

            reply = rig.RunOk ("BLOAD prog.bin 300");
            Assert::AreEqual ((Byte) 0xA9, rig.target.memory[0x0300]);
            Assert::AreEqual ((Byte) 0x60, rig.target.memory[0x0302]);
            Assert::AreEqual (std::string ("prog.bin (raw): 3 of 3 bytes"), reply.text.at (0));

            reply = rig.RunOk ("BLOAD prog.bin 400,2");
            Assert::AreEqual ((Byte) 0x41, rig.target.memory[0x0401]);
            Assert::AreEqual ((Byte) 0x00, rig.target.memory[0x0402], L"the length caps the load");
            Assert::AreEqual (std::string ("prog.bin (raw): 2 of 3 bytes; the file and the range differ in size"), reply.text.at (0));

            rig.RunFails ("BLOAD missing.bin 300", "file not found");
            rig.RunFails ("BLOAD prog.bin",        "file not loadable");
            rig.RunFails ("BLOAD prog.bin D000",   "memory not writable");

            Assert::AreEqual (std::string ("out.bin: 3 of 3 bytes"), rig.RunOk ("BSAVE out.bin 300:302").text.at (0));
            Assert::AreEqual (std::string ("\xA9\x41\x60", 3), rig.files.PeekContent (L"C:\\Work\\out.bin"));
            rig.RunFails ("BSAVE out.bin", "invalid arguments");

            rig.session.SetFileSystem (nullptr);
            rig.RunFails ("BLOAD prog.bin 300", "no file access");
        }



        TEST_METHOD (BLOAD_FormatsFromContentOrWord)
        {
            Rig    rig;
            Reply  reply;



            rig.files.WriteAllText (L"C:\\Work\\prog.hex", ":03030000A94160B0\n:0103030060" "99" "\n:00000001FF\n");
            rig.files.WriteAllText (L"C:\\Work\\prog.s19", "S1060500A94160AA\nS9030500F7\n");
            rig.files.WriteAllText (L"C:\\Work\\prog.dos", std::string ("\x00\x07\x02\x00\xEA\x60", 6));

            reply = rig.RunOk ("BLOAD prog.hex");
            Assert::AreEqual ((Byte) 0xA9, rig.target.memory[0x0300]);
            Assert::AreEqual ((Byte) 0x60, rig.target.memory[0x0303]);
            Assert::AreEqual (std::string ("prog.hex (Intel HEX): 4 of 4 bytes"), reply.text.at (0));

            Assert::AreEqual (std::string ("prog.s19 (S-record): 3 of 3 bytes"), rig.RunOk ("BLOAD prog.s19").text.at (0));
            Assert::AreEqual ((Byte) 0x41, rig.target.memory[0x0501]);

            Assert::AreEqual (std::string ("prog.dos (DOS 3.3 binary): 2 of 2 bytes"), rig.RunOk ("BLOAD prog.dos,DOS").text.at (0));
            Assert::AreEqual ((Byte) 0xEA, rig.target.memory[0x0700], L"the header's address");
            Assert::AreEqual ((Byte) 0x60, rig.target.memory[0x0701]);

            rig.RunOk ("BLOAD prog.dos,raw 800");
            Assert::AreEqual ((Byte) 0x00, rig.target.memory[0x0800], L"the header loads as data when raw is chosen");
            Assert::AreEqual ((Byte) 0x07, rig.target.memory[0x0801]);

            rig.RunFails ("BLOAD prog.hex 300", "file not loadable");
            rig.RunFails ("BLOAD prog.dos",     "file not loadable");
        }



        TEST_METHOD (TSAVE_WritesTextPageInScreenOrder)
        {
            Rig                       rig;
            std::string               content;
            std::vector<std::string>  lines;



            Store (rig, 0x0400, { 0xC8, 0xC5, 0xCC, 0xCC, 0xCF });
            Store (rig, 0x0480, { 0xD4, 0xD7, 0xCF });
            Store (rig, 0x0428, { 0xC5, 0xC9, 0xC7, 0xC8, 0xD4 });
            rig.target.memory[0x0405] = 0x8D;

            Assert::AreEqual (std::string ("Saved the text screen to screen.txt."), rig.RunOk ("TSAVE screen.txt").text.at (0));

            content = rig.files.PeekContent (L"C:\\Work\\screen.txt");

            for (size_t start = 0, end = content.find ('\n'); end != std::string::npos; start = end + 1, end = content.find ('\n', start))
            {
                lines.push_back (content.substr (start, end - start));
            }

            Assert::AreEqual ((size_t) 24, lines.size());
            Assert::AreEqual ((size_t) 40, lines[0].size());
            Assert::IsTrue   (lines[0].starts_with ("HELLO."), L"line 0 from $0400, with a control character as a period");
            Assert::IsTrue   (lines[1].starts_with ("TWO"),    L"line 1 from $0480");
            Assert::IsTrue   (lines[8].starts_with ("EIGHT"),  L"line 8 from $0428");
            rig.RunFails ("TSAVE", "invalid arguments");
        }



        TEST_METHOD (IN_OUT_SWITCHES)
        {
            Rig    rig;
            Reply  reply;



            rig.target.softSwitches = { { "RAMRD", true }, { "TEXT", false } };

            reply = rig.RunOk ("IN C030");
            Assert::AreEqual ((size_t) 1, rig.target.ioReads.size(), L"IN is a real bus read");
            Assert::AreEqual ((Word) 0xC030, rig.target.ioReads[0]);
            Assert::AreEqual (std::string ("C030: 00                       ."), reply.text.at (0));

            Assert::AreEqual (std::string ("Wrote 2 bytes at $C030."), rig.RunOk ("OUT C030 5A 5B").text.at (0));
            Assert::AreEqual ((size_t) 2, rig.target.ioWrites.size());
            Assert::AreEqual ((Word) 0xC031, rig.target.ioWrites[1]);

            reply = rig.RunOk ("SWITCHES");
            Assert::AreEqual (std::string ("RAMRD      on"),  reply.text.at (0));
            Assert::AreEqual (std::string ("TEXT       off"), reply.text.at (1));

            rig.RunFails ("IN",       "invalid arguments");
            rig.RunFails ("OUT C030", "invalid arguments");
        }
    };
}
