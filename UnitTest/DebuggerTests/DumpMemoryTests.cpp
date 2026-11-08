#include "Pch.h"

#include "CommandPathRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DumpMemoryTests
//
//  D and MDB, and each dialect's dump, on a real //e through the window
//  console, the debug channel and a batch script: every argument form, the
//  continuation a bare D follows, the edges of the address space, I/O, the
//  banking a dump reads through, and each output format.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DumpMemoryTests)
    {
    public:

        static constexpr Word  kRamRdOff   = 0xC002;
        static constexpr Word  kRamRdOn    = 0xC003;
        static constexpr Word  kRamWrtOff  = 0xC004;
        static constexpr Word  kRamWrtOn   = 0xC005;
        static constexpr Word  kIntCxOff   = 0xC006;
        static constexpr Word  kIntCxOn    = 0xC007;
        static constexpr Word  kStore80On  = 0xC001;
        static constexpr Word  kPage2On    = 0xC055;
        static constexpr Word  kPage2Off   = 0xC054;
        static constexpr Word  kLcBank2    = 0xC080;
        static constexpr Word  kLcBank2Rw  = 0xC083;
        static constexpr Word  kLcRom      = 0xC082;
        static constexpr Word  kLcBank1    = 0xC088;



        //  $0300-$0347 hold the letters A to Z over and over with the high bit
        //  set, which a dump shows with it stripped.
        static void StoreLetters (CommandPathRig & rig)
        {
            for (int i = 0; i < 0x48; ++i)
            {
                rig.GetMachine().GetMemoryBus().WriteByte ((Word) (0x0300 + i), (Byte) (0xC1 + i % 26));
            }
        }



        //  64 bytes from an address that is not on a row boundary, in rows of
        //  eight from that address, under both names in any case.
        TEST_METHOD (D_ShowsSixtyFourBytes_InRowsOfEightFromTheAddress)
        {
            static constexpr const char * kForms[] = { "D 301", "d 301", "MDB 301", "mdb 301" };



            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                StoreLetters (rig);

                for (const char * form : kForms)
                {
                    rig.AssertText (rig.RunOk (form), { "0301: C2 C3 C4 C5 C6 C7 C8 C9  BCDEFGHI",
                                                        "0309: CA CB CC CD CE CF D0 D1  JKLMNOPQ",
                                                        "0311: D2 D3 D4 D5 D6 D7 D8 D9  RSTUVWXY",
                                                        "0319: DA C1 C2 C3 C4 C5 C6 C7  ZABCDEFG",
                                                        "0321: C8 C9 CA CB CC CD CE CF  HIJKLMNO",
                                                        "0329: D0 D1 D2 D3 D4 D5 D6 D7  PQRSTUVW",
                                                        "0331: D8 D9 DA C1 C2 C3 C4 C5  XYZABCDE",
                                                        "0339: C6 C7 C8 C9 CA CB CC CD  FGHIJKLM" }, form);
                }
            }
        }



        //  The character column strips the high bit and shows anything outside
        //  printable ASCII as a period.
        TEST_METHOD (D_CharacterColumn_StripsTheHighBit)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                rig.Store (0x0300, { 0x00, 0x1F, 0x20, 0x7E, 0x7F, 0x80, 0xA0, 0xFF });
                rig.AssertText (rig.RunOk ("D 300:307"), { "0300: 00 1F 20 7E 7F 80 A0 FF  .. ~.. ." }, "D 300:307");
            }
        }



        //  first:last is inclusive; addr,len is len bytes; a single address
        //  as a range is one byte, with the row padded so the characters line
        //  up; anything after the range is ignored.
        TEST_METHOD (D_RangeForms)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                StoreLetters (rig);

                rig.AssertText (rig.RunOk ("D 300:30F"), { "0300: C1 C2 C3 C4 C5 C6 C7 C8  ABCDEFGH",
                                                           "0308: C9 CA CB CC CD CE CF D0  IJKLMNOP" }, "D 300:30F");
                rig.AssertText (rig.RunOk ("D 300,0A"),  { "0300: C1 C2 C3 C4 C5 C6 C7 C8  ABCDEFGH",
                                                           "0308: C9 CA                    IJ" }, "D 300,0A");

                rig.SetRegisters (0x0300, 0x02, 0x00, 0x00, 0x30, 0xFF);
                rig.AssertText (rig.RunOk ("D 300,A"),   { "0300: C1 C2                    AB" }, "a bare A is the register, even as a length");
                rig.AssertText (rig.RunOk ("D 300:300"), { "0300: C1                       A" }, "D 300:300");
                rig.AssertText (rig.RunOk ("D 300,1 junk"), { "0300: C1                       A" }, "D 300,1 junk");
                Assert::AreEqual ((size_t) 8, rig.RunOk ("D 300 junk").text.size(), rig.Where ("a word after the address is ignored").c_str());
            }
        }



        //  A bare D continues from where the last dump ended, whatever form
        //  that dump took; a session's first starts at $0000.
        TEST_METHOD (D_Bare_ContinuesFromTheLastDump)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);
                Reply           reply;



                StoreLetters (rig);

                reply = rig.RunOk ("D");
                Assert::AreEqual ((size_t) 8, reply.text.size());
                Assert::IsTrue   (reply.text[0].starts_with ("0000: "), rig.Where (reply.text[0]).c_str());

                rig.RunOk ("D 300");
                reply = rig.RunOk ("D");
                Assert::IsTrue (reply.text[0].starts_with ("0340: CD CE CF D0 D1 D2 D3 D4"), rig.Where (reply.text[0]).c_str());

                rig.RunOk ("D 300:30A");
                Assert::IsTrue (rig.RunOk ("D").text[0].starts_with ("030B: CC"), rig.Where ("after a range").c_str());

                rig.RunOk ("MDB 300,3");
                Assert::IsTrue (rig.RunOk ("MDB").text[0].starts_with ("0303: C4"), rig.Where ("after a length").c_str());
            }
        }



        //  Near the top the default dump stops at $FFFF, and the next bare D
        //  starts again at $0000.
        TEST_METHOD (D_NearTheTop_StopsAtFFFF_AndContinuesAtZero)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);
                Reply           reply;



                reply = rig.RunOk ("D FFE0");
                Assert::AreEqual ((size_t) 4, reply.text.size());
                Assert::IsTrue   (reply.text[3].starts_with ("FFF8: "), rig.Where (reply.text[3]).c_str());
                Assert::IsTrue   (rig.RunOk ("D").text[0].starts_with ("0000: "), rig.Where ("then $0000").c_str());

                reply = rig.RunOk ("D FFFF");
                Assert::AreEqual ((size_t) 1, reply.text.size());
                Assert::IsTrue   (reply.text[0].starts_with ("FFFF: "), rig.Where (reply.text[0]).c_str());
                Assert::AreEqual ((size_t) 32, reply.text[0].size(), rig.Where ("one byte, padded").c_str());
                Assert::IsTrue   (rig.RunOk ("D").text[0].starts_with ("0000: "), rig.Where ("then $0000").c_str());

                reply = rig.RunOk ("D FFF8:FFFF");
                Assert::AreEqual ((size_t) 1, reply.text.size(), rig.Where ("a range ending at $FFFF is inclusive").c_str());
                Assert::IsTrue   (reply.text[0].starts_with ("FFF8: "), rig.Where (reply.text[0]).c_str());
            }
        }



        //  A range that ends before it starts is refused and moves nothing,
        //  including where a bare D goes next.
        TEST_METHOD (D_ReversedRange_IsRefused)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);
                Reply           reply;



                StoreLetters (rig);
                rig.RunOk ("D 300");

                reply = rig.RunFails ("D 300:200", "invalid arguments",
                                      "$0300-$0200 ends before it starts. A range runs up from its first address to its last, and stops at $FFFF.");
                rig.AssertText (reply, { "Error: invalid arguments",
                                         "       $0300-$0200 ends before it starts. A range runs up from its first address to its last, and stops at $FFFF." },
                                "D 300:200");

                Assert::IsTrue (rig.RunOk ("D").text[0].starts_with ("0340: "), rig.Where ("the continuation is where it was").c_str());
            }
        }



        //  A length of zero is refused; so is a value outside the address
        //  space.
        TEST_METHOD (D_BadRange_IsRefused)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                rig.RunFails ("D 300,0",   "invalid arguments", "A range length must be at least 1.");
                rig.RunFails ("D NOSUCH",  "invalid arguments", "NOSUCH is not a symbol, register, or hex number.");
                rig.RunFails ("D 300:1G",  "invalid arguments", "1G is not a hex number.");
            }
        }



        //  $C000-$C0FF is never read: its bytes are ?? and the characters
        //  periods, the region is I/O, and no soft switch moves.
        TEST_METHOD (D_OverIo_ReadsNothingAndChangesNoSwitch)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);
                Reply           before;
                Reply           reply;



                before = rig.RunOk ("SWITCHES");

                rig.AssertText (rig.RunOk ("D C050:C057"), { "C050: ?? ?? ?? ?? ?? ?? ?? ??  ........" }, "D C050:C057");
                reply = rig.RunOk ("D C000:C0FF");
                Assert::AreEqual ((size_t) 32, reply.text.size());

                Assert::AreEqual (CommandPathRig::Join (before.text), CommandPathRig::Join (rig.RunOk ("SWITCHES").text),
                                  rig.Where ("the dump touched no switch").c_str());
            }
        }



        //  The channel's rows: bytes as numbers, an I/O byte as null, and each
        //  row's region.
        TEST_METHOD (Channel_D_CarriesBytesAndRegions)
        {
            CommandPathRig     rig (CommandPath::Channel);
            const JsonValue  * data  = nullptr;
            const JsonValue  * rows  = nullptr;
            const JsonValue  * bytes = nullptr;
            std::string        text;
            int                value = 0;



            rig.Store (0xBFF8, { 0x01, 0x02 });
            rig.RunOk ("D BFF8:C007");

            Assert::IsTrue   (rig.GetLastRecord().HasObject ("data", data));
            Assert::IsTrue   (data->HasString ("kind", text));
            Assert::AreEqual (std::string ("memory"), text);
            Assert::IsTrue   (data->HasArray ("rows", rows));
            Assert::AreEqual ((size_t) 2, rows->GetArraySize());

            Assert::IsTrue   (rows->GetArrayElement (0).HasInt ("address", value));
            Assert::AreEqual (0xBFF8, value);
            Assert::IsTrue   (rows->GetArrayElement (0).HasString ("region", text));
            Assert::AreEqual (std::string ("mainRam"), text);
            Assert::IsTrue   (rows->GetArrayElement (0).HasArray ("bytes", bytes));
            Assert::AreEqual ((size_t) 8, bytes->GetArraySize());
            Assert::AreEqual (0x01, bytes->GetArrayElement (0).GetInt());
            Assert::AreEqual (0x02, bytes->GetArrayElement (1).GetInt());

            Assert::IsTrue   (rows->GetArrayElement (1).HasString ("region", text));
            Assert::AreEqual (std::string ("io"), text);
            Assert::IsTrue   (rows->GetArrayElement (1).HasArray ("bytes", bytes));
            Assert::IsTrue   (bytes->GetArrayElement (0).GetType() == JsonType::Null, L"an I/O byte is null");
        }



        //  The region the channel gives a dump's first row.
        static std::string GetFirstRegion (CommandPathRig & rig, const std::string & line)
        {
            const JsonValue  * data = nullptr;
            const JsonValue  * rows = nullptr;
            std::string        name;



            rig.RunOk (line);
            Assert::IsTrue (rig.GetLastRecord().HasObject ("data", data));
            Assert::IsTrue (data->HasArray ("rows", rows));
            Assert::IsTrue (rows->GetArrayElement (0).HasString ("region", name));
            return name;
        }



        //  Each row carries the region of its first byte, under the banking
        //  in force: main or aux RAM, either language-card bank, the ROM, a
        //  slot's ROM, and I/O.
        TEST_METHOD (D_RowsCarryTheirRegion_UnderTheBanking)
        {
            CommandPathRig  rig (CommandPath::Channel);
            MemoryBus     & bus = rig.GetMachine().GetMemoryBus();



            Assert::AreEqual (std::string ("mainRam"), GetFirstRegion (rig, "D 300:307"));
            Assert::AreEqual (std::string ("io"),      GetFirstRegion (rig, "D C030:C037"));
            Assert::AreEqual (std::string ("rom"),     GetFirstRegion (rig, "D F800:F807"));

            bus.WriteByte (kIntCxOff, 0);
            Assert::AreEqual (std::string ("slotRom"), GetFirstRegion (rig, "D C600:C607"));
            bus.WriteByte (kIntCxOn, 0);
            Assert::AreEqual (std::string ("rom"),     GetFirstRegion (rig, "D C600:C607"));

            bus.ReadByte (kLcBank1);
            Assert::AreEqual (std::string ("lcBank1"), GetFirstRegion (rig, "D D000:D007"));
            bus.ReadByte (kLcBank2);
            Assert::AreEqual (std::string ("lcBank2"), GetFirstRegion (rig, "D D000:D007"));
            bus.ReadByte (kLcRom);
            Assert::AreEqual (std::string ("rom"),     GetFirstRegion (rig, "D D000:D007"));

            bus.WriteByte (kRamRdOn, 0);
            Assert::AreEqual (std::string ("auxRam"),  GetFirstRegion (rig, "D 300:307"));
            bus.WriteByte (kRamRdOff, 0);

            bus.WriteByte (kStore80On, 0);
            bus.ReadByte  (kPage2On);
            Assert::AreEqual (std::string ("auxRam"),  GetFirstRegion (rig, "D 400:407"));
            Assert::AreEqual (std::string ("mainRam"), GetFirstRegion (rig, "D 800:807"));
            bus.ReadByte  (kPage2Off);
            Assert::AreEqual (std::string ("mainRam"), GetFirstRegion (rig, "D 400:407"));

            Assert::AreEqual (std::string ("mainRam"), GetFirstRegion (rig, "D BFFC:C003"), L"a row spanning $C000 takes its first byte's region");
        }



        //  A dump reads memory as the CPU would under the banking in force:
        //  aux RAM under RAMRD, and under 80STORE with PAGE2 for the text
        //  page; language-card RAM while the card reads RAM.
        TEST_METHOD (D_ReadsThroughTheBanking)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);
                MemoryBus     & bus = rig.GetMachine().GetMemoryBus();



                bus.WriteByte (kRamWrtOn, 0);
                rig.Store (0x0300, { 0xAA });
                rig.Store (0x0400, { 0xAB });
                bus.WriteByte (kRamWrtOff, 0);
                rig.Store (0x0300, { 0x11 });
                rig.Store (0x0400, { 0x12 });

                Assert::IsTrue (rig.RunOk ("D 300:300").text[0].starts_with ("0300: 11"), rig.Where ("main").c_str());

                bus.WriteByte (kRamRdOn, 0);
                Assert::IsTrue (rig.RunOk ("D 300:300").text[0].starts_with ("0300: AA"), rig.Where ("aux under RAMRD").c_str());
                bus.WriteByte (kRamRdOff, 0);

                bus.WriteByte (kStore80On, 0);
                bus.ReadByte  (kPage2On);
                Assert::IsTrue (rig.RunOk ("D 400:400").text[0].starts_with ("0400: AB"), rig.Where ("aux under 80STORE and PAGE2").c_str());
                Assert::IsTrue (rig.RunOk ("D 300:300").text[0].starts_with ("0300: 11"), rig.Where ("which moves only the text page").c_str());
                bus.ReadByte  (kPage2Off);

                bus.ReadByte (kLcBank2);
                bus.ReadByte (kLcBank2Rw);
                bus.ReadByte (kLcBank2Rw);
                rig.Store (0xD000, { 0x5A });
                bus.ReadByte (kLcRom);
                Assert::IsFalse (rig.RunOk ("D D000:D000").text[0].starts_with ("D000: 5A"), rig.Where ("the ROM while the card reads ROM").c_str());
                bus.ReadByte (kLcBank2);
                Assert::IsTrue  (rig.RunOk ("D D000:D000").text[0].starts_with ("D000: 5A"), rig.Where ("its RAM while it reads RAM").c_str());
            }
        }



        //  The address is an expression: a symbol, a register, a search
        //  result, a byte read through *.
        TEST_METHOD (D_AddressIsAnExpression)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                StoreLetters (rig);
                rig.SetRegisters (0x0310, 0x00, 0x00, 0x00, 0x30, 0xFF);

                Assert::IsTrue (rig.RunOk ("D COUT").text[0].starts_with ("FDED: "), rig.Where ("a symbol").c_str());
                Assert::IsTrue (rig.RunOk ("D PC").text[0].starts_with ("0310: D1"), rig.Where ("PC").c_str());
                Assert::IsTrue (rig.RunOk ("D PC+2").text[0].starts_with ("0312: D3"), rig.Where ("arithmetic").c_str());

                rig.RunOk ("S 300:347 C5");
                Assert::IsTrue (rig.RunOk ("D @2").text[0].starts_with ("031E: C5"), rig.Where ("a search result").c_str());

                rig.Store (0x0000, { 0x05 });
                Assert::IsTrue (rig.RunOk ("D *0").text[0].starts_with ("0005: "), rig.Where ("* reads one byte, not a word").c_str());
            }
        }



        //  Each dialect's own dump layout, and the `/` and `!` forms of D.
        TEST_METHOD (Dialects_DumpInTheirOwnLayouts)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);
                Reply           reply;



                StoreLetters (rig);

                rig.AssertText (rig.RunOk ("/D 300:30F", CommandMode::Monitor), { "0300- C1 C2 C3 C4 C5 C6 C7 C8",
                                                                                  "0308- C9 CA CB CC CD CE CF D0" }, "/D");
                Assert::AreEqual ((size_t) 8, rig.RunOk ("/D 300", CommandMode::Monitor).text.size(), rig.Where ("/D addr").c_str());

                rig.AssertText (rig.RunOk ("300", CommandMode::GSSquared), { "00/0300: C1" }, "300");
                rig.AssertText (rig.RunOk ("00/301", CommandMode::GSSquared), { "00/0301: C2" }, "00/301");
                rig.AssertText (rig.RunOk ("300.311", CommandMode::GSSquared),
                                { "00/0300: C1 C2 C3 C4 C5 C6 C7 C8 C9 CA CB CC CD CE CF D0  ABCDEFGHIJKLMNOP",
                                  "00/0310: D1 D2" + std::string (44, ' ') + "QR",
                                  "" }, "300.311");
                reply = rig.RunOk ("MDB 300", CommandMode::GSSquared);
                Assert::AreEqual ((size_t) 5, reply.text.size(), rig.Where ("MDB: 64 bytes, 16 a line, and a blank line").c_str());
                Assert::IsTrue   (reply.text[0].starts_with ("00/0300: C1 C2"), rig.Where (reply.text[0]).c_str());

                rig.Store (0x000D, { 0x7D });
                rig.Store (0x000F, { 0x7F });
                rig.AssertText (rig.RunOk ("D", CommandMode::GSSquared), { "00/000D: 7D" }, "GSSquared's D is address $000D");
                rig.AssertText (rig.RunOk ("F", CommandMode::GSSquared), { "00/000F: 7F" }, "and F is $000F");

                reply = rig.RunOk ("!D 300", CommandMode::WinDbg);
                Assert::AreEqual ((size_t) 4, reply.text.size(), rig.Where ("!D in db's layout").c_str());
                Assert::AreEqual (std::string ("0300  c1 c2 c3 c4 c5 c6 c7 c8-c9 ca cb cc cd ce cf d0  ................"), reply.text[0]);
            }
        }



        //  GSSquared's errors for its examine and dump forms.
        TEST_METHOD (GSSquared_DumpErrors)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);
                Reply           reply;



                rig.RunFails ("300.311 400", "invalid arguments", "A dump takes one range, first.last.", CommandMode::GSSquared);
                rig.RunFails ("311.300",     "invalid arguments", "311.300 ends before it starts.", CommandMode::GSSquared);
                rig.RunFails ("300.3G0",     "invalid arguments", "300.3G0 is not a range. A range is first.last in hex.", CommandMode::GSSquared);
                rig.RunFails ("300 1",       "invalid arguments", "An address to examine takes nothing after it.", CommandMode::GSSquared);
                rig.RunFails ("12345",       "invalid arguments", "12345 is not an address. An address is one to four hex digits.", CommandMode::GSSquared);
                rig.RunFails ("F 300 00",    "invalid arguments", "An address to examine takes nothing after it.", CommandMode::GSSquared);

                reply = rig.Run ("E1/0300", CommandMode::GSSquared);
                Assert::AreEqual ((int) CommandStatus::NotAvailable, (int) reply.status);
                Assert::AreEqual (std::string ("command not available"), reply.error.label);
                Assert::AreEqual (std::string ("E1/0300 is in bank E1. Only bank 00 exists on this machine."), reply.error.detail);
            }
        }



        //  WinDbg's db, dw, dd and da: 128 bytes unless a length in the
        //  command's units is given, words and double words little-endian,
        //  and a string to its first zero with the high bit cleared.
        TEST_METHOD (WinDbg_DumpsInItsUnits)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);
                Reply           reply;



                rig.Store (0x0300, { 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x00, 0x7F, 0x80 });
                rig.Store (0x0400, { 0xC8, 0xC5, 0xCC, 0xCC, 0xCF, 0x00, 0xC1 });

                reply = rig.RunOk ("db 300", CommandMode::WinDbg);
                Assert::AreEqual ((size_t) 8, reply.text.size(), rig.Where ("db: 128 bytes, 16 a line").c_str());
                Assert::IsTrue   (reply.text[0].starts_with ("0300  48 65 6c 6c 6f 00 7f 80-"), rig.Where (reply.text[0]).c_str());

                rig.AssertText (rig.RunOk ("db 300 l8", CommandMode::WinDbg),   { "0300  48 65 6c 6c 6f 00 7f 80" + std::string (26, ' ') + "Hello..." }, "db l8");
                rig.AssertText (rig.RunOk ("db 0x300 l4", CommandMode::WinDbg), { "0300  48 65 6c 6c" + std::string (38, ' ') + "Hell" }, "db l4");
                rig.AssertText (rig.RunOk ("db 300 303", CommandMode::WinDbg),  { "0300  48 65 6c 6c" + std::string (38, ' ') + "Hell" }, "db start end");
                rig.AssertText (rig.RunOk ("dw 300 l2", CommandMode::WinDbg),   { "0300  6548 6c6c" }, "dw l2");
                rig.AssertText (rig.RunOk ("dd 300 l2", CommandMode::WinDbg),   { "0300  6c6c6548 807f006f" }, "dd l2");
                rig.AssertText (rig.RunOk ("da 400", CommandMode::WinDbg),      { "0400  \"HELLO\"" }, "da");

                reply = rig.RunOk ("db fff0", CommandMode::WinDbg);
                Assert::AreEqual ((size_t) 1, reply.text.size(), rig.Where ("a default length stops at $FFFF").c_str());
            }
        }



        //  WinDbg's dump errors.
        TEST_METHOD (WinDbg_DumpErrors)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                rig.RunFails ("db 2000 l0",  "invalid arguments", "A length must be at least 1.", CommandMode::WinDbg);
                rig.RunFails ("db 300 l4 5", "invalid arguments", "db takes an address and a length, as db 2000 l20.", CommandMode::WinDbg);
                rig.RunFails ("dw 300 l2 9", "invalid arguments", "dw takes an address and a length, as dw 2000 l20.", CommandMode::WinDbg);
                rig.RunFails ("db 10000",    "invalid arguments", "$10000 is outside $0000-$FFFF.", CommandMode::WinDbg);
                rig.RunFails ("db 300 200",  "invalid arguments",
                              "$0300-$0200 ends before it starts. A range runs up from its first address to its last, and stops at $FFFF.", CommandMode::WinDbg);
            }
        }



        //  OUTPUT changes how a dump is written without changing the mode.
        TEST_METHOD (D_UnderEachOutputFormat)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);



                StoreLetters (rig);

                rig.RunOk ("OUTPUT MONITOR");
                rig.AssertText (rig.RunOk ("D 300:307"), { "0300- C1 C2 C3 C4 C5 C6 C7 C8" }, "OUTPUT MONITOR");

                rig.RunOk ("OUTPUT GSSQUARED");
                rig.AssertText (rig.RunOk ("D 300:301"), { "00/0300: C1 C2" + std::string (44, ' ') + "AB", "" }, "OUTPUT GSSQUARED");

                rig.RunOk ("OUTPUT WINDBG");
                rig.AssertText (rig.RunOk ("D 300:301"), { "0300  c1 c2" + std::string (44, ' ') + ".." }, "OUTPUT WINDBG");

                rig.RunOk ("OUTPUT APPLEWIN");
                rig.AssertText (rig.RunOk ("D 300:301"), { "0300: C1 C2                    AB" }, "OUTPUT APPLEWIN");
            }
        }



        //  A whole-memory dump is 8,192 rows, and nothing caps it.
        TEST_METHOD (D_WholeAddressSpace)
        {
            for (CommandPath path : CommandPathRig::kAllPaths)
            {
                CommandPathRig  rig (path);
                Reply           reply = rig.RunOk ("D 0:FFFF");



                Assert::AreEqual ((size_t) 8192, reply.text.size(), rig.Where ("D 0:FFFF").c_str());
                Assert::IsTrue   (reply.text.back().starts_with ("FFF8: "), rig.Where (reply.text.back()).c_str());
            }
        }



        //  The memory pane and the memory windows read D in AppleWin mode
        //  whatever the dialect, so each shows the bytes in every one.
        TEST_METHOD (MemoryPanes_ShowTheBytes_InEveryDialect)
        {
            CommandPathRig  rig (CommandPath::WindowConsole);
            size_t          checked = 0;



            StoreLetters (rig);
            rig.GetWindow().view.SetMemoryAddress (0x0300);
            rig.GetWindow().view.OpenMemoryWindow (2, 0x0310);

            for (CommandMode mode : { CommandMode::AppleWin, CommandMode::Monitor, CommandMode::GSSquared, CommandMode::WinDbg, CommandMode::Casso })
            {
                DebuggerViewSnapshot  snapshot;
                std::wstring          where = rig.Where (CommandModeNames::GetName (mode));



                rig.SetMode (mode);
                snapshot = rig.GetWindow().view.Build (rig.GetSession());

                Assert::IsFalse  (snapshot.memory.empty(), where.c_str());
                Assert::AreEqual ((Word) 0x0300, snapshot.memory[0].address, where.c_str());
                Assert::AreEqual (std::string ("C1 C2 C3 C4 C5 C6 C7 C8"), snapshot.memory[0].bytes, where.c_str());
                Assert::AreEqual (std::string ("ABCDEFGH"), snapshot.memory[0].characters, where.c_str());
                Assert::AreEqual ((size_t) 2, snapshot.memoryWindows.size(), where.c_str());
                Assert::AreEqual ((Word) 0x0310, snapshot.memoryWindows[1].first, where.c_str());
                Assert::IsTrue   (snapshot.memoryWindows[1].bytes[0] == std::optional<Byte> ((Byte) 0xD1), where.c_str());
                ++checked;
            }

            Assert::AreEqual ((size_t) 5, checked);
        }
    };
}
