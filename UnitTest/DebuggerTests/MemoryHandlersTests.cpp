#include "Pch.h"

#include "Debugger/Handlers/MemoryHandlers.h"
#include "Debugger/Handlers/SymbolHandlers.h"
#include "HandlerTestRig.h"
#include "Core/TextEncoding.h"

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



        //  A row that would cross from RAM into I/O, or from slot ROM into the
        //  language card, ends at the boundary, so every row's region is true of
        //  all its bytes.
        TEST_METHOD (D_RowEndsWhereTheRegionChanges)
        {
            MachineHandlerRig<MemoryHandlers>  rig;
            Reply                              reply;
            MemoryData                         data;



            reply = rig.Run ("D BFFC:C003");
            data  = std::get<MemoryData> (reply.data);

            Assert::AreEqual ((size_t) 2, data.rows.size());
            Assert::AreEqual ((Word) 0xBFFC, data.rows[0].address);
            Assert::AreEqual ((size_t) 4, data.rows[0].bytes.size());
            Assert::AreEqual ((int) MemoryRegion::MainRam, (int) data.rows[0].region);
            Assert::AreEqual ((Word) 0xC000, data.rows[1].address);
            Assert::AreEqual ((int) MemoryRegion::Io, (int) data.rows[1].region);

            reply = rig.Run ("D CFFC:D003");
            data  = std::get<MemoryData> (reply.data);

            Assert::AreEqual ((size_t) 2, data.rows.size());
            Assert::AreEqual ((Word) 0xD000, data.rows[1].address);
            Assert::AreNotEqual ((int) data.rows[0].region, (int) data.rows[1].region);
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



        //  PATCH is what a memory window sends for an edit: it reaches ROM, which
        //  MEB does not, and it refuses I/O by name, pointing at OUT.
        TEST_METHOD (Patch_RamAndRom_IoRefused)
        {
            Rig    rig;
            Reply  reply;



            reply = rig.RunOk ("PATCH 300 A9 41");
            Assert::AreEqual ((Byte) 0xA9, rig.target.memory[0x0300]);
            Assert::AreEqual ((Byte) 0x41, rig.target.memory[0x0301]);
            Assert::AreEqual (std::string ("0300: A9 41                    )A"), reply.text.at (0), L"replies with the rows, as MEB does");

            rig.RunOk ("PATCH F800 EA");
            Assert::AreEqual ((Byte) 0xEA, rig.target.memory[0xF800], L"ROM takes the patch");

            reply = rig.RunFails ("PATCH C030 00", "memory not writable");
            Assert::IsTrue   (reply.error.detail.find ("OUT") != std::string::npos, L"the refusal says how to write I/O");
            Assert::AreEqual ((size_t) 0, rig.target.ioWrites.size(), L"and nothing reached the device");

            rig.RunFails ("PATCH 300", "invalid arguments");
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



        //  The debugger cannot read $C000-$C0FF, so nothing there is a hit.
        TEST_METHOD (Search_NeverFindsAnUnreadableByte)
        {
            Rig  rig;



            Assert::AreEqual (std::string ("Not found."), rig.RunOk ("S C000:C0FF 00").text.at (0));
            Assert::AreEqual (std::string ("Found 1: $C100"), rig.RunOk ("S C0FE:C101 ? 00").text.at (0), L"nor does a wildcard");
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

            //  @n reaches a result by its number; one past 32 bits is an error
            //  reply, never an exception.
            (void) rig.RunOk ("S 300:3FF AD ? C0");
            Assert::IsTrue (rig.RunOk ("D @2").text.at (0).starts_with ("0310"), L"@2 is the second result");
            Assert::AreNotEqual ((int) CommandStatus::Ok, (int) rig.Run ("D @99999999999999999999").status);
        }



        TEST_METHOD (BLOAD_BSAVE_RawWithAddress)
        {
            Rig    rig;
            Reply  reply;



            rig.files.WriteAllText (L"C:\\Work\\prog.bin", std::string ("\xA9\x41\x60", 3));

            reply = rig.RunOk ("BLOAD prog.bin 300");
            Assert::AreEqual ((Byte) 0xA9, rig.target.memory[0x0300]);
            Assert::AreEqual ((Byte) 0x60, rig.target.memory[0x0302]);
            Assert::AreEqual (std::string ("Loaded 3 bytes at $0300 from C:\\Work\\prog.bin"), reply.text.at (0));

            reply = rig.RunOk ("BLOAD prog.bin 400,2");
            Assert::AreEqual ((Byte) 0x41, rig.target.memory[0x0401]);
            Assert::AreEqual ((Byte) 0x00, rig.target.memory[0x0402], L"the length caps the load");
            Assert::AreEqual (std::string ("Loaded 2 bytes at $0400 from C:\\Work\\prog.bin"), reply.text.at (0));

            rig.RunFails ("BLOAD missing.bin 300", "file not found");
            rig.RunFails ("BLOAD prog.bin",        "file not loadable");
            rig.RunFails ("BLOAD prog.bin D000",   "memory not writable");

            Assert::AreEqual (std::string ("Saved 3 bytes from $0300 to C:\\Work\\out.bin"), rig.RunOk ("BSAVE out.bin 300:302").text.at (0));
            Assert::AreEqual (std::string ("\xA9\x41\x60", 3), rig.files.PeekContent (L"C:\\Work\\out.bin"));
            rig.RunFails ("BSAVE out.bin", "invalid arguments");

            rig.session.SetFileSystem (nullptr);
            rig.RunFails ("BLOAD prog.bin 300", "no file access");
        }



        //  The reply gives the address loaded at, and the path only when it
        //  was typed relative -- as the absolute path it resolved to.
        TEST_METHOD (BLOAD_RootedPath_IsNotEchoed)
        {
            Rig  rig;



            rig.files.WriteAllText (L"D:\\Bin\\prog.bin", std::string ("\xA9\x41\x60", 3));

            Assert::AreEqual (std::string ("Loaded 3 bytes at $0300"),           rig.RunOk ("BLOAD D:\\Bin\\prog.bin 300").text.at (0));
            Assert::AreEqual (std::string ("Saved 3 bytes from $0300"),          rig.RunOk ("BSAVE D:\\Bin\\out.bin 300:302").text.at (0));
            Assert::AreEqual (std::string ("Saved the text screen."),            rig.RunOk ("TSAVE D:\\Bin\\screen.txt").text.at (0));
        }



        //  A load that would stop short writes nothing: a file smaller than
        //  the length asked for, or one that runs past $FFFF.
        TEST_METHOD (BLOAD_ShortLoad_IsAnError)
        {
            Rig  rig;



            rig.files.WriteAllText (L"C:\\Work\\prog.bin", std::string ("\xA9\x41\x60", 3));

            rig.RunFails ("BLOAD prog.bin 300,8", "file too short");
            Assert::AreEqual ((Byte) 0x00, rig.target.memory[0x0300], L"nothing is written");

            rig.RunFails ("BLOAD prog.bin FFFE", "file too long");
            Assert::AreEqual ((Byte) 0x00, rig.target.memory[0xFFFE], L"nothing is written");
        }



        //  A debug file, or failing that a symbol file, beside the binary
        //  under the same base name loads with it, in one more line.
        TEST_METHOD (BLOAD_LoadsTheDebugOrSymbolFileBeside)
        {
            Rig             rig;
            SymbolHandlers  symbols;
            Reply           reply;



            rig.session.AddHandler (&symbols);
            rig.files.WriteAllText (L"C:\\Work\\prog.bin", std::string ("\xA9\x41\x60", 3));
            rig.files.WriteAllText (L"C:\\Work\\prog.sym", "0300 START\n");

            reply = rig.RunOk ("BLOAD prog.bin 300");
            Assert::AreEqual ((size_t) 2, reply.text.size());
            Assert::AreEqual (std::string ("Loaded 1 symbols into user from C:\\Work\\prog.sym."), reply.text.at (1));
            Assert::AreEqual (std::string ("$0300 START (user)"), rig.RunOk ("SYM START").text.at (0));

            rig.files.WriteAllText (L"C:\\Work\\prog.dbg",
                "version\tmajor=2,minor=0\n"
                "file\tid=0,name=\"prog.a65\",size=10,mtime=0,mod=0\n"
                "seg\tid=0,name=\"CODE\",start=0x0300,size=3,addrsize=absolute,type=rw\n"
                "span\tid=0,seg=0,start=0,size=3\n"
                "line\tid=0,file=0,line=4,span=0\n"
                "sym\tid=0,name=\"start\",addrsize=absolute,scope=0,val=0x0300,seg=0,type=lab\n"
                "scope\tid=0,name=\"\",mod=0\n");

            reply = rig.RunOk ("BLOAD prog.bin 300");
            Assert::AreEqual ((size_t) 2, reply.text.size(), L"the debug file, not both");
            Assert::AreEqual (std::string ("Loaded 1 symbols into user and 1 source lines from C:\\Work\\prog.dbg."), reply.text.at (1));
            Assert::IsTrue   (rig.session.HasDebugFile());

            rig.files.WriteAllText (L"C:\\Work\\lone.bin", std::string ("\x60", 1));
            Assert::AreEqual ((size_t) 1, rig.RunOk ("BLOAD lone.bin 300").text.size(), L"nothing beside it, nothing more");
        }



        //  The quotes around a file name are not part of it, so the reply
        //  gives the bare name. GSSquared's load and save always quote it.
        TEST_METHOD (BLOAD_BSAVE_QuotedName_ReportsTheBareName)
        {
            Rig    rig;
            Reply  reply;



            rig.files.WriteAllText (L"C:\\Work\\prog.bin", std::string ("\xA9\x41\x60", 3));

            Assert::AreEqual (std::string ("Loaded 3 bytes at $0300 from C:\\Work\\prog.bin"), rig.RunOk ("BLOAD \"prog.bin\" 300").text.at (0));
            Assert::AreEqual (std::string ("Loaded 3 bytes at $0300 from C:\\Work\\prog.bin"), rig.RunOk ("BLOAD \"prog.bin\",RAW 300").text.at (0));
            Assert::AreEqual (std::string ("Saved 3 bytes from $0300 to C:\\Work\\out.bin"),         rig.RunOk ("BSAVE \"out.bin\" 300:302").text.at (0));

            reply = rig.session.ExecuteLine ("save \"out.bin\" 300.302", CommandMode::GSSquared);
            Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status);
            Assert::AreEqual (std::string ("out.bin"), std::get<FileIoData> (reply.data).path);

            reply = rig.session.ExecuteLine ("load \"prog.bin\" 300", CommandMode::GSSquared);
            Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status);
            Assert::AreEqual (std::string ("prog.bin (raw)"), std::get<FileIoData> (reply.data).path);
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
            Assert::AreEqual (std::string ("Loaded 4 bytes at $0300 from C:\\Work\\prog.hex"), reply.text.at (0));

            Assert::AreEqual (std::string ("Loaded 3 bytes at $0500 from C:\\Work\\prog.s19"), rig.RunOk ("BLOAD prog.s19").text.at (0));
            Assert::AreEqual ((Byte) 0x41, rig.target.memory[0x0501]);

            Assert::AreEqual (std::string ("Loaded 2 bytes at $0700 from C:\\Work\\prog.dos"), rig.RunOk ("BLOAD prog.dos,DOS").text.at (0));
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

            Assert::AreEqual (std::string ("Saved the text screen to C:\\Work\\screen.txt."), rig.RunOk ("TSAVE screen.txt").text.at (0));

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



        TEST_METHOD (EnterAndPatch_PastFFFF_FailWithoutWrapping)
        {
            Rig  rig;



            rig.RunFails ("ME FFFF 11 22",    "value out of range");
            rig.RunFails ("PATCH FFFF 11 22", "value out of range");
            Assert::AreEqual ((Byte) 0x00, rig.target.memory[0x0000], L"nothing wraps to $0000");
        }



        TEST_METHOD (OneByteReplies_SaySingularByte)
        {
            Rig  rig;



            Assert::AreEqual (std::string ("Filled 1 byte at $0300-$0300."), rig.RunOk ("F 300:300 AA").text.at (0));
            Assert::IsTrue   (rig.RunOk ("MC 500 300:300").text.at (0).starts_with ("Compared 1 byte,"));
        }



        //  A format word after a GSSquared load's file name stays outside the
        //  quotes GSSquared puts around the name, so it still picks the format.
        TEST_METHOD (GSSquaredLoad_FormatWord_PicksFormat)
        {
            Rig    rig;
            Reply  reply;



            rig.files.WriteAllText (L"C:\\Work\\prog.dos", std::string ("\x00\x07\x02\x00\xEA\x60", 6));

            reply = rig.session.ExecuteLine ("load prog.dos,DOS 900", CommandMode::GSSquared);
            Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, TextEncoding::NarrowToWide (reply.error.detail).c_str());
            Assert::AreEqual ((Byte) 0xEA, rig.target.memory[0x0900], L"the header is not loaded as data");
        }



        TEST_METHOD (BLOAD_ReversedRange_Fails)
        {
            Rig  rig;



            rig.files.WriteAllText (L"C:\\Work\\prog.bin", std::string ("\xA9\x41\x60", 3));

            rig.RunFails ("BLOAD prog.bin 300:2FF", "invalid arguments");
            Assert::AreEqual ((Byte) 0x00, rig.target.memory[0x0300]);
        }



        //  A search item's width comes from its value, as ME's does, not from
        //  how many characters it was typed with.
        TEST_METHOD (Search_ByteValueTypedLong_IsOneByte)
        {
            Rig  rig;



            Store (rig, 0x0310, { 0x41, 0x42 });

            Assert::AreEqual (std::string ("Found 1: $0310"), rig.RunOk ("S 300:3FF $41").text.at (0));
            Assert::AreEqual (std::string ("Found 1: $0310"), rig.RunOk ("S 300:3FF #65").text.at (0));
            Assert::AreEqual (std::string ("Found 1: $0310"), rig.RunOk ("S 300:3FF 041").text.at (0));
        }



        TEST_METHOD (OUT_OutsideIo_Fails)
        {
            Rig  rig;



            rig.RunFails ("OUT 300 5A",      "invalid arguments");
            rig.RunFails ("OUT C0FF 5A 5B",  "invalid arguments");
            Assert::AreEqual ((size_t) 0, rig.target.ioWrites.size(), L"nothing is written when any address is not I/O");
        }



        //  I/O cannot be read without side effects, so a range that covers it
        //  fails rather than reading it as zeros.
        TEST_METHOD (UnreadableSource_FailsRatherThanReadingZeros)
        {
            Rig    rig;
            Reply  reply;



            rig.target.memory[0x0300] = 0x55;

            reply = rig.RunFails ("M 300 C000:C00F", "unreadable memory");
            Assert::AreEqual (std::string ("$C000 cannot be read."), reply.error.detail);
            Assert::AreEqual ((Byte) 0x55, rig.target.memory[0x0300], L"M wrote nothing");

            rig.RunFails ("MC 300 C000:C00F",      "unreadable memory");
            rig.RunFails ("MC C000 300:30F",       "unreadable memory");
            rig.RunFails ("BSAVE f BFF0:C00F",     "unreadable memory");
            Assert::IsFalse (rig.files.Exists (L"C:\\Work\\f"), L"BSAVE wrote no file");
        }


        TEST_METHOD (ReversedRange_FailsWithoutTouchingMemory)
        {
            Rig  rig;



            rig.RunFails ("F 30F:300 AA",      "invalid arguments");
            rig.RunFails ("M 500 30F:300",     "invalid arguments");
            rig.RunFails ("MC 500 30F:300",    "invalid arguments");
            rig.RunFails ("S 3FF:300 A9",      "invalid arguments");
            rig.RunFails ("BSAVE f 30F:300",   "invalid arguments");
            rig.RunFails ("D 300:200",         "invalid arguments");
            Assert::AreEqual ((Byte) 0x00, rig.target.memory[0x030F], L"F did not write the start byte");
            Assert::AreEqual ((Byte) 0x00, rig.target.memory[0x0500], L"M did not write the destination");
        }



        TEST_METHOD (LengthPastFFFF_FailsRatherThanWrapping)
        {
            Rig    rig;
            Reply  reply;



            reply = rig.RunFails ("D FFF0,20", "invalid arguments");
            Assert::IsFalse  (reply.error.detail.empty(), L"the reply gives the reason");
            rig.RunFails ("F FFF0,20 AA", "invalid arguments");
            Assert::AreEqual ((Byte) 0x00, rig.target.memory[0xFFF0], L"F did not write one byte");
            reply = rig.RunOk ("D FFF0,10");
            Assert::AreEqual ((size_t) 2, reply.text.size(), L"a range ending at $FFFF is fine");
        }



        TEST_METHOD (MissingValues_ReportTheRule)
        {
            Rig    rig;
            Reply  reply;



            reply = rig.RunFails ("ME 300", "invalid arguments");
            Assert::AreEqual (std::string ("ME needs an address and one or more values."), reply.error.detail);
            reply = rig.RunFails ("PATCH 300", "invalid arguments");
            Assert::AreEqual (std::string ("PATCH needs an address and one or more values."), reply.error.detail);
            reply = rig.RunFails ("F 300:30F", "invalid arguments");
            Assert::AreEqual (std::string ("F needs a range and one or more values."), reply.error.detail);
            reply = rig.RunFails ("OUT", "invalid arguments");
            Assert::AreEqual (std::string ("OUT needs an address and one or more values."), reply.error.detail);
            reply = rig.RunFails ("300:", "invalid arguments");
            Assert::AreEqual (std::string ("A deposit needs one or more values after the colon."), reply.error.detail);
        }
    };
}
