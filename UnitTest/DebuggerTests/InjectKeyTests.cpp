#include "Pch.h"

#include "RunPathRigs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  InjectKeyTests
//
//  KEY on a real //e, through each way a user reaches it. A guest loop reads
//  three keys at $C000, clears the strobe and stores each at $0400, so the
//  test reads what the guest actually received. The queue feeds a key each
//  time the guest clears the strobe during a debugger run, on the batch,
//  window and channel paths alike, and waits while the machine runs freely.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (InjectKeyTests)
    {
    public:

        static constexpr Word  kReader = 0x0300;
        static constexpr Word  kDone   = 0x0310;
        static constexpr Word  kStored = 0x0400;

        //  $0300: LDA $C000 / BPL $0300 / STA $C010 / STA $0400,Y / INY /
        //         CPY #$03 / BNE $0300    $0310: JMP $0310
        static void LoadReader (EmulatorRig & rig)
        {
            rig.Load (kReader, { 0xAD, 0x00, 0xC0, 0x10, 0xFB, 0x8D, 0x10, 0xC0, 0x99, 0x00, 0x04,
                                 0xC8, 0xC0, 0x03, 0xD0, 0xF0, 0x4C, 0x10, 0x03 }, kReader);
        }

        static std::vector<std::string> ReaderScript()
        {
            return { "MEB 300 AD 00 C0 10 FB 8D 10 C0 99 00 04 C8 C0 03 D0 F0 4C 10 03", "R PC 300", "R Y 0" };
        }

        static void AssertStored (EmulatorRig & rig, std::initializer_list<Byte> keys)
        {
            Word  at = kStored;



            for (Byte key : keys)
            {
                Assert::AreEqual ((int) key, (int) rig.Peek (at), std::format (L"the guest's key at ${:04X}", at).c_str());
                at++;
            }
        }

        static std::vector<std::string> Error (const std::string & label, const std::string & detail)
        {
            return { "Error: " + label, "       " + detail };
        }



        //  The run paths.

        TEST_METHOD (Window_G_FeedsEachKeyAsTheGuestClearsTheStrobe)
        {
            EmulatorRig  rig;



            LoadReader (rig);
            Assert::AreEqual (std::string ("Queued 3 keys. Keys waiting: 2."), rig.WindowOk ("KEY 41 42 43").text.at (0));
            Assert::AreEqual (0xC1, (int) rig.ReadKeyboard(), L"the first key is latched at once, with the high bit set");

            rig.WindowRun ("G 310");
            Assert::AreEqual ((int) kDone, (int) rig.Session().GetTarget().GetRegisters().pc);
            AssertStored (rig, { 0xC1, 0xC2, 0xC3 });
            Assert::AreEqual (std::string ("Queued 1 key. Keys waiting: 0."), rig.WindowOk ("KEY 44").text.at (0),
                              L"the queue drained, and the guest cleared the last strobe");
        }



        TEST_METHOD (Window_T_FeedsKeysBetweenSteps)
        {
            EmulatorRig  rig;



            LoadReader (rig);
            rig.WindowOk  ("KEY 41 42 43");
            rig.WindowRun ("T 21");

            Assert::AreEqual ((int) kDone, (int) rig.Session().GetTarget().GetRegisters().pc, L"seven instructions a key");
            AssertStored (rig, { 0xC1, 0xC2, 0xC3 });
        }



        TEST_METHOD (Channel_G_FeedsKeysOnTheEmulatorPath)
        {
            EmulatorRig  rig;
            JsonValue    reply;



            LoadReader (rig);
            EmulatorRig::AssertLines ({ "Queued 3 keys. Keys waiting: 2." }, rig.PipeText ("KEY 41 42 43"), "KEY from a client");

            reply = rig.Pipe ("G 310");
            Assert::AreEqual (std::string ("ok"), EmulatorRig::GetString (reply, "status"));
            Assert::IsTrue   (rig.Slices(), L"the run the client started ended");
            AssertStored (rig, { 0xC1, 0xC2, 0xC3 });
        }



        TEST_METHOD (Batch_G_FeedsKeysOnTheSynchronousPath)
        {
            BatchScript               batch;
            std::vector<std::string>  lines = ReaderScript();



            lines.insert (lines.end(), { "KEY 41 42 43", "G 310", "KEY 44" });

            Assert::AreEqual (0, batch.Run (lines));
            BatchScript::AssertLines ({ "Queued 3 keys. Keys waiting: 2." }, batch.GetReplyText (3), "KEY 41 42 43");
            BatchScript::AssertLines ({ "Queued 1 key. Keys waiting: 0." },  batch.GetReplyText (5), "KEY 44");
            Assert::AreEqual (0xC1, (int) batch.Read (kStored));
            Assert::AreEqual (0xC2, (int) batch.Read (kStored + 1));
            Assert::AreEqual (0xC3, (int) batch.Read (kStored + 2));
        }



        //  Only a debugger run feeds the queue. The emulator running on its
        //  own delivers the key that was latched at once and no other; the
        //  rest wait for the next debugger run.
        TEST_METHOD (FreeRun_FeedsNothing_TheNextDebuggerRunDoes)
        {
            EmulatorRig  rig;
            Byte         untouched = 0;



            LoadReader (rig);
            untouched = rig.Peek (kStored + 1);

            rig.WindowOk ("KEY 41 42 43");
            rig.FreeRun  (20000);
            rig.UserPause();

            Assert::AreEqual (0xC1, (int) rig.Peek (kStored), L"the latched key reached the guest");
            Assert::AreEqual ((int) untouched, (int) rig.Peek (kStored + 1), L"no other key was fed while the machine ran freely");
            Assert::AreEqual ((int) kReader, (int) (rig.Session().GetTarget().GetRegisters().pc & 0xFFF0), L"the guest is still waiting in its loop");

            rig.WindowRun ("G 310");
            AssertStored (rig, { 0xC1, 0xC2, 0xC3 });
        }



        //  A key already pending, from an earlier KEY or from the keyboard,
        //  holds every queued key back. The queue is first in, first out.
        TEST_METHOD (APendingKey_HoldsTheQueue)
        {
            EmulatorRig  rig;



            Assert::AreEqual (std::string ("Queued 1 key. Keys waiting: 0."),  rig.WindowOk ("KEY 41").text.at (0));
            Assert::AreEqual (std::string ("Queued 2 keys. Keys waiting: 2."), rig.WindowOk ("KEY 42 43").text.at (0), L"the strobe is still set");
            Assert::AreEqual (0xC1, (int) rig.ReadKeyboard(), L"nothing replaced the pending key");

            rig.ClearStrobe();
            Assert::AreEqual (std::string ("Queued 1 key. Keys waiting: 2."), rig.WindowOk ("KEY 44").text.at (0), L"the oldest key went, and 44 joined the end");
            Assert::AreEqual (0xC2, (int) rig.ReadKeyboard());

            rig.ClearStrobe();
            rig.machine.GetRefs().keyboard->PressKey ('Z');
            Assert::AreEqual (std::string ("Queued 1 key. Keys waiting: 3."), rig.WindowOk ("KEY 45").text.at (0), L"a key the user typed holds the queue too");
            Assert::AreEqual (0xDA, (int) rig.ReadKeyboard());
        }



        //  What the guest reads is the value with its high bit set. The //e
        //  keeps lowercase, and nothing sets its any-key-down flag.
        TEST_METHOD (TheLatchHoldsTheValueWithItsHighBitSet)
        {
            EmulatorRig  rig;



            for (const auto & [line, latched] : std::vector<std::pair<std::string, int>> { { "KEY 00", 0x80 }, { "KEY FF", 0xFF },
                                                                                          { "KEY 61", 0xE1 }, { "KEY C1", 0xC1 },
                                                                                          { "KEY 41", 0xC1 } })
            {
                rig.WindowOk (line);
                Assert::AreEqual (latched, (int) rig.ReadKeyboard(), Widen (line).c_str());
                rig.ClearStrobe();
            }

            rig.WindowOk ("KEY 41");
            Assert::AreEqual (0, rig.machine.GetMemoryBus().ReadByte (EmulatorRig::kStrobe) & 0x80, L"no key is held down");
            Assert::AreEqual (0x41, (int) rig.ReadKeyboard(), L"and reading $C010 cleared the strobe");
        }



        //  The ][+ keyboard has no lowercase: $61-$7A arrive as capitals. A
        //  value above $7F passes as given.
        TEST_METHOD (TheIIPlusFoldsLowercase)
        {
            for (const auto & [line, latched] : std::vector<std::pair<std::string, int>> { { "KEY 61", 0xC1 }, { "KEY 7A", 0xDA },
                                                                                          { "KEY 7B", 0xFB }, { "KEY E1", 0xE1 },
                                                                                          { "KEY 41", 0xC1 } })
            {
                BatchScript  batch ("Apple2Plus");



                Assert::AreEqual (0, batch.Run ({ line }));
                Assert::AreEqual (latched, (int) batch.Read (EmulatorRig::kKeyboard), Widen (line).c_str());
            }
        }



        //  Every value is an expression: hex bare or with $, decimal with #,
        //  a character, a symbol, arithmetic, and a register's value. A
        //  register name is read before a hex digit, so A is the accumulator
        //  and $A or 0A is ten.
        TEST_METHOD (EveryValueForm)
        {
            EmulatorRig  rig;



            rig.SetPc    (kReader);
            rig.WindowOk ("SYM KEYA = 41");
            rig.WindowOk ("R A 5A");
            rig.WindowOk ("R X 31");
            rig.WindowOk ("R Y 32");

            for (const auto & [line, latched] : std::vector<std::pair<std::string, int>> { { "KEY 41",     0xC1 }, { "KEY $41",   0xC1 },
                                                                                          { "KEY #65",    0xC1 }, { "KEY 'A'",   0xC1 },
                                                                                          { "KEY KEYA",   0xC1 }, { "KEY 40+1",  0xC1 },
                                                                                          { "KEY 'A'+80", 0xC1 }, { "KEY A",     0xDA },
                                                                                          { "KEY X",      0xB1 }, { "KEY Y",     0xB2 },
                                                                                          { "KEY S",      0xFF }, { "KEY 0A",    0x8A },
                                                                                          { "KEY $A",     0x8A }, { "KEY B",     0x8B },
                                                                                          { "KEY C",      0x8C } })
            {
                rig.WindowOk (line);
                Assert::AreEqual (latched, (int) rig.ReadKeyboard(), Widen (line).c_str());
                rig.ClearStrobe();
            }
        }



        //  Each bad form fails whole: nothing is queued, even a valid value
        //  before the bad one.
        TEST_METHOD (BadValues_FailAndQueueNothing)
        {
            EmulatorRig  rig;



            for (const auto & [line, detail] : std::vector<std::pair<std::string, std::string>>
                                               {
                                                   { "KEY",           "KEY takes one or more key codes." },
                                                   { "KEY 100",       "100 is not a byte." },
                                                   { "KEY 41 100",    "100 is not a byte." },
                                                   { "KEY 10000",     "10000 is outside $0000-$FFFF." },
                                                   { "KEY -1",        "-1 is outside $0000-$FFFF." },
                                                   { "KEY PC",        "PC is not a byte." },
                                                   { "KEY ' '",       "A character value is written as 'c'." },
                                                   { "KEY 'AB'",      "A character value is written as 'c'." },
                                                   { "KEY NOSUCHSYM", "NOSUCHSYM is not a symbol, register, or hex number." },
                                               })
            {
                Reply  reply = rig.Window (line);



                Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status, Widen (line).c_str());
                EmulatorRig::AssertLines (Error ("invalid arguments", detail), reply.text, line);
                Assert::AreEqual (0, rig.ReadKeyboard() & 0x80, Widen (line + " latched a key").c_str());
            }

            Assert::AreEqual (std::string ("Queued 1 key. Keys waiting: 0."), rig.WindowOk ("KEY 41").text.at (0), L"nothing was left in the queue");
        }



        //  Each dialect's form, in that dialect's output.
        TEST_METHOD (EveryDialect)
        {
            EmulatorRig  rig;



            rig.WindowOk ("MODE MONITOR");
            EmulatorRig::AssertLines ({ "Queued 1 key. Keys waiting: 0." }, rig.WindowOk ("/KEY 41").text, "/KEY 41");
            Assert::AreEqual (0xC1, (int) rig.ReadKeyboard());
            EmulatorRig::AssertLines ({ "ERR", "Error: invalid arguments", "       KEY takes one or more key codes." }, rig.Window ("/KEY").text, "/KEY");
            rig.ClearStrobe();

            rig.WindowOk ("/MODE GSSQUARED");
            EmulatorRig::AssertLines ({ "Queued 1 key. Keys waiting: 0." }, rig.WindowOk ("key 42").text, "key 42");
            Assert::AreEqual (0xC2, (int) rig.ReadKeyboard());
            EmulatorRig::AssertLines (Error ("invalid arguments", "KEY takes one or more key codes."), rig.Window ("key").text, "key");
            rig.ClearStrobe();

            rig.WindowOk ("MODE WINDBG");
            EmulatorRig::AssertLines ({ "Queued 2 keys. Keys waiting: 1." }, rig.WindowOk ("!key 0x43 0n68").text, "!key 0x43 0n68");
            Assert::AreEqual (0xC3, (int) rig.ReadKeyboard());
            rig.ClearStrobe();
            rig.WindowOk ("!key 0x45");
            Assert::AreEqual (0xC4, (int) rig.ReadKeyboard(), L"0n68 is decimal 68, $44");
            EmulatorRig::AssertLines (Error ("invalid arguments", "KEY takes one or more key codes."), rig.Window ("!key").text, "!key");
            rig.ClearStrobe();
            rig.WindowOk ("!key 0x46");
            Assert::AreEqual (0xC5, (int) rig.ReadKeyboard(), L"45 was still queued");
            rig.ClearStrobe();

            rig.WindowOk ("!mode casso");
            rig.WindowOk ("KEY 0");
            Assert::AreEqual (0xC6, (int) rig.ReadKeyboard(), L"46 went first");
            rig.ClearStrobe();
            EmulatorRig::AssertLines ({ "Queued 1 key. Keys waiting: 1." }, rig.WindowOk ("KEY 'G'").text, "KEY 'G'");
            Assert::AreEqual (0x80, (int) rig.ReadKeyboard(), L"KEY 0 was fed ahead of G");
        }



        //  The queue belongs to the session: the window and every client
        //  share it, and it outlives runs, resets and a machine switch.
        TEST_METHOD (TheQueueIsTheSessionsAndPersists)
        {
            EmulatorRig  rig;



            EmulatorRig::AssertLines ({ "Queued 2 keys. Keys waiting: 1." }, rig.PipeText ("KEY 41 42"), "KEY 41 42 from a client");
            Assert::AreEqual (std::string ("Queued 1 key. Keys waiting: 2."), rig.WindowOk ("KEY 43").text.at (0), L"the window adds to the client's queue");
            EmulatorRig::AssertLines ({ "Queued 1 key. Keys waiting: 3." }, rig.PipeText ("!key 0x44", "windbg"), "!key 0x44 from a client in WinDbg mode");

            rig.Session().OnReset (false);
            rig.Session().OnMachineChanged ("Apple //e");

            //  $0300: LDA $C000 / BPL $0300 / STA $C010 / STA $0400 / JMP $030B
            rig.Load (kReader, { 0xAD, 0x00, 0xC0, 0x10, 0xFB, 0x8D, 0x10, 0xC0, 0x8D, 0x00, 0x04, 0x4C, 0x0B, 0x03 }, kReader);
            rig.WindowRun ("G 30B");
            Assert::AreEqual (0xC1, (int) rig.Peek (kStored));
            Assert::AreEqual (0xC2, (int) rig.ReadKeyboard(), L"the next key was fed when the guest cleared the strobe");

            rig.SetPc (kReader);
            rig.WindowRun ("G 30B");
            Assert::AreEqual (0xC2, (int) rig.Peek (kStored), L"a later run takes the next key");
            Assert::AreEqual (std::string ("Queued 1 key. Keys waiting: 2."), rig.WindowOk ("KEY 45").text.at (0), L"43 is latched, and 44 and 45 wait");
        }



        //  KEY changes no register or memory, so it runs while the machine
        //  runs: between the slices of a debugger run, the guest reads it in
        //  the same run, and while the machine runs freely.
        TEST_METHOD (KeyRunsWhileTheMachineRuns)
        {
            EmulatorRig  rig;



            LoadReader (rig);
            rig.WindowOk ("G 310");
            Assert::IsFalse  (rig.Slices (2), L"the guest waits for a key");
            Assert::AreEqual (std::string ("Queued 3 keys. Keys waiting: 2."), rig.WindowOk ("KEY 41 42 43").text.at (0));
            Assert::IsTrue   (rig.Slices(), L"the keys let the guest finish");
            AssertStored (rig, { 0xC1, 0xC2, 0xC3 });

            rig.FreeRun (100);
            Assert::AreEqual (std::string ("Queued 1 key. Keys waiting: 0."), rig.WindowOk ("KEY 44").text.at (0), L"accepted while running freely");
            rig.UserPause();
        }

    private:

        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
        }
    };
}
