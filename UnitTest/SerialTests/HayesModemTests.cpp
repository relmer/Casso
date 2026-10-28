#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Devices/Serial/HayesModem.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace HayesModemTestNs
{
    static constexpr uint32_t  kClockHz   = 1000000;
    static constexpr uint64_t  kCharGap   = 1000;            // about 9600 baud at this clock
    static constexpr uint64_t  kSecond    = kClockHz;




    ////////////////////////////////////////////////////////////////////////////
    //
    //  ModemDriver
    //
    //  A HayesModem with a clock: the guest's text typed a character time
    //  apart, silence, and the modem's output to the guest as text.
    //
    ////////////////////////////////////////////////////////////////////////////

    class ModemDriver
    {
    public:
        HayesModem  modem { kClockHz };
        uint64_t    now   = kSecond * 10;

        void  Type (const std::string & text)
        {
            for (char each : text)
            {
                now += kCharGap;
                modem.ReceiveFromGuest ((Byte) each, now);
                modem.Tick (now);
            }
        }

        void  Wait (uint64_t cycles)
        {
            uint64_t  end = now + cycles;



            while (now < end)
            {
                now = std::min (end, now + kCharGap * 10);
                modem.Tick (now);
            }
        }

        std::string  Output()
        {
            std::vector<Byte>  bytes = modem.TakeToGuest();



            return std::string (bytes.begin(), bytes.end());
        }

        std::string  Remote()
        {
            std::vector<Byte>  bytes = modem.TakeToRemote();



            return std::string (bytes.begin(), bytes.end());
        }

        //  Dials and connects, with the output so far discarded.
        void  Connect()
        {
            Type ("ATDT bbs.example.org\r");
            modem.TakeLinkRequests();
            modem.OnConnected (now);
            Output();
        }
    };




    ////////////////////////////////////////////////////////////////////////////
    //
    //  HayesModemTests
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (HayesModemTests)
    {
    public:
        TEST_METHOD (AtReturnsOk)
        {
            ModemDriver  driver;



            driver.Type ("AT\r");

            Assert::AreEqual (std::string ("AT\r\r\nOK\r\n"), driver.Output(), L"the echo, then the result");
        }


        TEST_METHOD (LowerCaseAndLeadingNoiseAreAccepted)
        {
            ModemDriver  driver;



            driver.modem.ReceiveFromGuest ('x', driver.now);
            driver.Type ("at\r");

            Assert::IsTrue (driver.Output().ends_with ("\r\nOK\r\n"));
        }


        TEST_METHOD (DialPassesTheHostAndPortToTheLink)
        {
            ModemDriver                            driver;
            std::vector<HayesModem::LinkRequest>   requests;



            driver.Type ("ATDT bbs.example.org:2323\r");
            requests = driver.modem.TakeLinkRequests();

            Assert::AreEqual<size_t> (1, requests.size());
            Assert::IsTrue (requests[0].kind == HayesModem::LinkRequestKind::Dial);
            Assert::AreEqual (std::string ("bbs.example.org"), requests[0].host);
            Assert::AreEqual<uint16_t> (2323, requests[0].port);
            Assert::IsTrue (driver.modem.GetMode() == HayesModem::Mode::Dialing);
            Assert::AreEqual (std::string ("ATDT bbs.example.org:2323\r"), driver.Output(), L"no result until the call is answered or fails");
        }


        TEST_METHOD (DialWithoutAPortUses23AndPulseWorksToo)
        {
            ModemDriver                            driver;
            std::vector<HayesModem::LinkRequest>   requests;



            driver.Type ("ATDPbbs.example.org\r");
            requests = driver.modem.TakeLinkRequests();

            Assert::AreEqual (std::string ("bbs.example.org"), requests[0].host);
            Assert::AreEqual<uint16_t> (23, requests[0].port);
        }


        TEST_METHOD (AConnectionReturnsConnectAndTurnsDcdOn)
        {
            ModemDriver  driver;



            driver.Type ("ATD host\r");
            Assert::IsFalse (driver.modem.GetDcd());
            driver.Output();

            driver.modem.OnConnected (driver.now);

            Assert::AreEqual (std::string ("\r\nCONNECT\r\n"), driver.Output());
            Assert::IsTrue (driver.modem.GetDcd());
            Assert::IsTrue (driver.modem.IsDataMode());
        }


        TEST_METHOD (AFailedDialReturnsNoCarrier)
        {
            ModemDriver  driver;



            driver.Type ("ATD offline.example.org\r");
            driver.Output();
            driver.modem.OnDialFailed();

            Assert::AreEqual (std::string ("\r\nNO CARRIER\r\n"), driver.Output());
            Assert::IsTrue (driver.modem.GetMode() == HayesModem::Mode::Command);
        }


        TEST_METHOD (ADialThatNeverConnectsTimesOutAfterS7)
        {
            ModemDriver                            driver;
            std::vector<HayesModem::LinkRequest>   requests;



            driver.Type ("ATS7=5D slow.example.org\r");
            driver.modem.TakeLinkRequests();
            driver.Output();

            driver.Wait (kSecond * 4);
            Assert::IsTrue (driver.modem.GetMode() == HayesModem::Mode::Dialing);

            driver.Wait (kSecond * 2);
            requests = driver.modem.TakeLinkRequests();
            Assert::AreEqual (std::string ("\r\nNO CARRIER\r\n"), driver.Output());
            Assert::IsTrue (requests.size() == 1 && requests[0].kind == HayesModem::LinkRequestKind::HangUp);
        }


        TEST_METHOD (AKeyWhileDialingAbandonsTheCall)
        {
            ModemDriver  driver;



            driver.Type ("ATD host\r");
            driver.modem.TakeLinkRequests();
            driver.Output();

            driver.Type (" ");

            Assert::IsTrue (driver.Output().ends_with ("\r\nNO CARRIER\r\n"));
            Assert::IsTrue (driver.modem.TakeLinkRequests()[0].kind == HayesModem::LinkRequestKind::HangUp);
        }


        TEST_METHOD (DataGoesToTheRemoteEndPlusesIncluded)
        {
            ModemDriver  driver;



            driver.Connect();
            driver.Type ("a+++b");
            driver.Wait (kSecond * 3);

            Assert::AreEqual (std::string ("a+++b"), driver.Remote());
            Assert::IsTrue (driver.modem.IsDataMode(), L"pluses between other characters are data");
            Assert::AreEqual (std::string(), driver.Output(), L"no echo in data mode");
        }


        TEST_METHOD (TheEscapeNeedsSilenceThenThreePlusesThenSilence)
        {
            ModemDriver  driver;



            driver.Connect();
            driver.Wait (kSecond + kCharGap);
            driver.Type ("+++");
            Assert::IsTrue (driver.modem.IsDataMode(), L"not before the closing guard time");

            driver.Wait (kSecond + kCharGap);

            Assert::IsTrue (driver.modem.GetMode() == HayesModem::Mode::OnlineCommand);
            Assert::AreEqual (std::string ("\r\nOK\r\n"), driver.Output());
            Assert::IsTrue (driver.modem.GetDcd(), L"the connection is still open");
            Assert::AreEqual (std::string ("+++"), driver.Remote(), L"the pluses were sent, as a Hayes sends them");
        }


        TEST_METHOD (TooLittleSilenceBeforeIsData)
        {
            ModemDriver  driver;



            driver.Connect();
            driver.Wait (kSecond / 2);
            driver.Type ("+++");
            driver.Wait (kSecond * 2);

            Assert::IsTrue (driver.modem.IsDataMode());
        }


        TEST_METHOD (PlusesTooFarApartAreData)
        {
            ModemDriver  driver;



            driver.Connect();
            driver.Wait (kSecond * 2);
            driver.Type ("+");
            driver.Wait (kSecond + kCharGap);
            driver.Type ("+");
            driver.Wait (kSecond + kCharGap);
            driver.Type ("+");
            driver.Wait (kSecond * 2);

            Assert::IsTrue (driver.modem.IsDataMode(), L"each must be within the guard time of the one before");
        }


        TEST_METHOD (ACharacterAfterThePlusesIsData)
        {
            ModemDriver  driver;



            driver.Connect();
            driver.Wait (kSecond * 2);
            driver.Type ("+++x");
            driver.Wait (kSecond * 2);

            Assert::IsTrue (driver.modem.IsDataMode());
        }


        TEST_METHOD (TheGuardTimeFollowsS12)
        {
            ModemDriver  driver;



            driver.Type ("ATS12=10\r");
            driver.Connect();
            driver.Wait (kSecond / 5 + kCharGap);
            driver.Type ("+++");
            driver.Wait (kSecond / 5 + kCharGap);

            Assert::IsTrue (driver.modem.GetMode() == HayesModem::Mode::OnlineCommand, L"S12=10 is a fifth of a second");
        }


        TEST_METHOD (TheGuardTimeIsInEmulatedCycles)
        {
            HayesModem  fast (kClockHz * 2);
            uint64_t    now = kSecond * 10;
            int         i   = 0;



            for (char each : std::string ("ATD h\r"))
            {
                now += kCharGap;
                fast.ReceiveFromGuest ((Byte) each, now);
            }

            fast.OnConnected (now);
            now += kSecond * 2 + 1;

            for (i = 0; i < 3; i++)
            {
                now += kCharGap;
                fast.ReceiveFromGuest ('+', now);
            }

            now += kSecond + kCharGap;
            fast.Tick (now);
            Assert::IsTrue (fast.IsDataMode(), L"at twice the clock, a second of cycles is half the guard time");

            now += kSecond + kCharGap;
            fast.Tick (now);
            Assert::IsTrue (fast.GetMode() == HayesModem::Mode::OnlineCommand);
        }


        TEST_METHOD (S2Above127OrS12ZeroTurnsTheEscapeOff)
        {
            for (const char * setting : { "ATS2=200\r", "ATS12=0\r" })
            {
                ModemDriver  driver;



                driver.Type (setting);
                driver.Connect();
                driver.Wait (kSecond * 3);
                driver.Type ("+++");
                driver.Wait (kSecond * 3);

                Assert::IsTrue (driver.modem.IsDataMode(), TextEncoding::NarrowToWide (setting).c_str());
            }
        }


        TEST_METHOD (AnotherEscapeCharacterWorks)
        {
            ModemDriver  driver;



            driver.Type ("ATS2=42\r");
            driver.Connect();
            driver.Wait (kSecond * 2);
            driver.Type ("***");
            driver.Wait (kSecond * 2);

            Assert::IsTrue (driver.modem.GetMode() == HayesModem::Mode::OnlineCommand);
        }


        //  SC-004: a thousand bursts of text, each with +++ somewhere, typed
        //  without pause, never leave data mode.
        TEST_METHOD (PlusesInsideABurstNeverEscape)
        {
            ModemDriver                   driver;
            std::mt19937                  random (47);
            std::uniform_int_distribution<int>  length (1, 40);
            std::uniform_int_distribution<int>  letter ('a', 'z');
            int                           trial  = 0;
            int                           before = 0;
            int                           after  = 0;
            std::string                   burst;



            driver.Connect();

            for (trial = 0; trial < 1000; trial++)
            {
                before = (trial % 3 == 0) ? 0 : length (random);
                after  = (before == 0)    ? length (random) : ((trial % 3 == 1) ? 0 : length (random));
                burst.clear();

                while ((int) burst.size() < before)
                {
                    burst += (char) letter (random);
                }

                burst += "+++";

                while ((int) burst.size() < before + 3 + after)
                {
                    burst += (char) letter (random);
                }

                driver.Wait (kSecond * 2);
                driver.Type (burst);
                driver.Wait (kSecond * 2);

                Assert::IsTrue (driver.modem.IsDataMode(), TextEncoding::NarrowToWide (burst).c_str());
            }

            driver.Remote();
        }


        TEST_METHOD (HangUpInCommandModeClosesTheConnection)
        {
            ModemDriver                            driver;
            std::vector<HayesModem::LinkRequest>   requests;



            driver.Connect();
            driver.Wait (kSecond * 2);
            driver.Type ("+++");
            driver.Wait (kSecond * 2);
            driver.Output();

            driver.Type ("ATH\r");
            requests = driver.modem.TakeLinkRequests();

            Assert::IsTrue (driver.Output().ends_with ("\r\nOK\r\n"));
            Assert::IsTrue (requests.size() == 1 && requests[0].kind == HayesModem::LinkRequestKind::HangUp);
            Assert::IsFalse (driver.modem.GetDcd());
            Assert::IsTrue (driver.modem.GetMode() == HayesModem::Mode::Command);
        }


        TEST_METHOD (OnlineReturnsToDataMode)
        {
            ModemDriver  driver;



            driver.Connect();
            driver.Wait (kSecond * 2);
            driver.Type ("+++");
            driver.Wait (kSecond * 2);
            driver.Output();

            driver.Type ("ATO\r");

            Assert::IsTrue (driver.Output().ends_with ("\r\nCONNECT\r\n"));
            Assert::IsTrue (driver.modem.IsDataMode());
        }


        TEST_METHOD (OnlineWithNoCallReturnsNoCarrier)
        {
            ModemDriver  driver;



            driver.Type ("ATO\r");

            Assert::IsTrue (driver.Output().ends_with ("\r\nNO CARRIER\r\n"));
        }


        TEST_METHOD (ARemoteCloseReturnsNoCarrier)
        {
            ModemDriver  driver;



            driver.Connect();
            driver.modem.OnRemoteClosed();

            Assert::AreEqual (std::string ("\r\nNO CARRIER\r\n"), driver.Output());
            Assert::IsFalse (driver.modem.GetDcd());
        }


        TEST_METHOD (DroppingTheCarrierReturnsNoCarrierOnce)
        {
            ModemDriver  driver;



            driver.Connect();
            driver.modem.DropCarrier (true);

            Assert::AreEqual (std::string ("\r\nNO CARRIER\r\n"), driver.Output());
            Assert::IsTrue (driver.modem.TakeLinkRequests()[0].kind == HayesModem::LinkRequestKind::HangUp);

            driver.modem.DropCarrier (false);
            Assert::AreEqual (std::string(), driver.Output(), L"no call and no carrier for the guest: nothing to drop");

            driver.modem.DropCarrier (true);
            Assert::AreEqual (std::string ("\r\nNO CARRIER\r\n"), driver.Output(), L"the guest had carrier at that point, though the call ended since");
        }


        TEST_METHOD (AnIncomingCallRingsAndAtaAnswers)
        {
            ModemDriver                            driver;
            std::vector<HayesModem::LinkRequest>   requests;



            driver.modem.OnIncomingCall (driver.now);
            Assert::AreEqual (std::string ("\r\nRING\r\n"), driver.Output());

            driver.Wait (kSecond * HayesModem::kRingSeconds + kCharGap);
            Assert::AreEqual (std::string ("\r\nRING\r\n"), driver.Output(), L"it rings again");
            Assert::AreEqual<Byte> (2, driver.modem.GetRegister (HayesModem::kRegRingCount));

            driver.Type ("ATA\r");
            requests = driver.modem.TakeLinkRequests();

            Assert::IsTrue (driver.Output().ends_with ("\r\nCONNECT\r\n"));
            Assert::IsTrue (requests.size() == 1 && requests[0].kind == HayesModem::LinkRequestKind::Answer);
            Assert::IsTrue (driver.modem.IsDataMode());
        }


        TEST_METHOD (WithS0TheCallIsAnsweredAfterThatManyRings)
        {
            ModemDriver  driver;



            driver.Type ("ATS0=2\r");
            driver.Output();

            driver.modem.OnIncomingCall (driver.now);
            Assert::IsFalse (driver.modem.IsDataMode(), L"one ring");

            driver.Wait (kSecond * HayesModem::kRingSeconds + kCharGap);

            Assert::IsTrue (driver.modem.IsDataMode(), L"answered on the second");
            Assert::IsTrue (driver.Output().ends_with ("\r\nCONNECT\r\n"));
        }


        TEST_METHOD (ACallerHangingUpStopsTheRinging)
        {
            ModemDriver  driver;



            driver.modem.OnIncomingCall (driver.now);
            driver.Output();
            driver.modem.OnIncomingGone();
            driver.Wait (kSecond * HayesModem::kRingSeconds * 2);

            Assert::AreEqual (std::string(), driver.Output());
            Assert::IsFalse (driver.modem.IsRinging());
        }


        TEST_METHOD (AnswerWithNothingRingingReturnsNoCarrier)
        {
            ModemDriver  driver;



            driver.Type ("ATA\r");

            Assert::IsTrue (driver.Output().ends_with ("\r\nNO CARRIER\r\n"));
        }


        TEST_METHOD (NumericResultsFollowV0)
        {
            ModemDriver  driver;



            driver.Type ("ATV0\r");
            Assert::IsTrue (driver.Output().ends_with ("0\r"), L"V0's own OK is numeric");

            driver.Type ("ATX\r");
            Assert::IsTrue (driver.Output().ends_with ("0\r"));

            driver.Type ("ATQ\r");
            driver.Type ("ATD h\r");
            driver.Output();
            driver.modem.OnDialFailed();
            Assert::AreEqual (std::string ("3\r"), driver.Output());
        }


        TEST_METHOD (QuietSendsNoResults)
        {
            ModemDriver  driver;



            driver.Type ("ATQ1\r");

            Assert::AreEqual (std::string ("ATQ1\r"), driver.Output(), L"the echo alone");
        }


        TEST_METHOD (EchoOffSendsNoEcho)
        {
            ModemDriver  driver;



            driver.Type ("ATE0\r");
            driver.Output();
            driver.Type ("AT\r");

            Assert::AreEqual (std::string ("\r\nOK\r\n"), driver.Output());
        }


        TEST_METHOD (RegistersAreSetAndRead)
        {
            ModemDriver  driver;



            driver.Type ("ATS0=3S0?\r");

            Assert::AreEqual (std::string ("ATS0=3S0?\r\r\n003\r\n\r\nOK\r\n"), driver.Output());
            Assert::AreEqual<Byte> (3, driver.modem.GetRegister (HayesModem::kRegAutoAnswer));
        }


        TEST_METHOD (RegisterRangesAreErrors)
        {
            for (const char * command : { "ATS38=1\r", "ATS0=256\r", "ATS=1\r" })
            {
                ModemDriver  driver;



                driver.Type (command);

                Assert::IsTrue (driver.Output().ends_with ("\r\nERROR\r\n"), TextEncoding::NarrowToWide (command).c_str());
            }
        }


        TEST_METHOD (ResetAndFactoryDefaultsRestoreTheSettings)
        {
            for (const char * reset : { "ATZ\r", "AT&F\r" })
            {
                ModemDriver  driver;



                driver.Type ("ATE0V0S0=1\r");
                driver.Type (reset);
                driver.Output();
                driver.Type ("AT\r");

                Assert::AreEqual (std::string ("AT\r\r\nOK\r\n"), driver.Output(), TextEncoding::NarrowToWide (reset).c_str());
                Assert::AreEqual<Byte> (0, driver.modem.GetRegister (HayesModem::kRegAutoAnswer));
            }
        }


        TEST_METHOD (ResetHangsUp)
        {
            ModemDriver  driver;



            driver.Connect();
            driver.Wait (kSecond * 2);
            driver.Type ("+++");
            driver.Wait (kSecond * 2);
            driver.Type ("ATZ\r");

            Assert::IsFalse (driver.modem.IsOnline());
            Assert::IsTrue (driver.modem.TakeLinkRequests().back().kind == HayesModem::LinkRequestKind::HangUp);
        }


        TEST_METHOD (UnknownCommandsAreErrors)
        {
            ModemDriver  driver;



            driver.Type ("ATK\r");

            Assert::IsTrue (driver.Output().ends_with ("\r\nERROR\r\n"));
        }


        TEST_METHOD (PeriodInitializationStringsAreAccepted)
        {
            ModemDriver  driver;



            driver.Type ("AT&C1&D2X4M0L1B0&K0&W\r");

            Assert::IsTrue (driver.Output().ends_with ("\r\nOK\r\n"));
        }


        TEST_METHOD (ALineLongerThanTheBufferIsAnError)
        {
            ModemDriver  driver;



            driver.Type ("AT" + std::string (HayesModem::kCommandBufferSize + 1, 'M') + "\r");

            Assert::IsTrue (driver.Output().ends_with ("\r\nERROR\r\n"));
        }


        TEST_METHOD (BackspaceRemovesACharacter)
        {
            ModemDriver  driver;



            driver.Type ("ATK\bE1\r");

            Assert::IsTrue (driver.Output().ends_with ("\r\nOK\r\n"));
        }


        TEST_METHOD (RepeatRunsTheLastLine)
        {
            ModemDriver  driver;



            driver.Type ("ATS0?\r");
            driver.Output();
            driver.Type ("A/");

            Assert::IsTrue (driver.Output().ends_with ("\r\n000\r\n\r\nOK\r\n"));
        }


        TEST_METHOD (IdentifyReturnsTheModemsName)
        {
            ModemDriver  driver;



            driver.Type ("ATI\r");

            Assert::IsTrue (driver.Output().ends_with ("\r\nCasso modem\r\n\r\nOK\r\n"));
        }


        TEST_METHOD (X1ReportsTheBaudRate)
        {
            ModemDriver  driver;



            driver.modem.SetBaudRate (2400.0);
            driver.Type ("ATX1D h\r");
            driver.Output();
            driver.modem.OnConnected (driver.now);
            Assert::AreEqual (std::string ("\r\nCONNECT 2400\r\n"), driver.Output());

            driver.modem.OnRemoteClosed();
            driver.Output();
            driver.Type ("ATV0D h\r");
            driver.Output();
            driver.modem.OnConnected (driver.now);
            Assert::AreEqual (std::string ("10\r"), driver.Output(), L"the 2400 code");
        }


        TEST_METHOD (DtrDroppingHangsUpUnderAmpD2)
        {
            ModemDriver  driver;



            driver.Connect();
            driver.modem.OnDtrChanged (false, driver.now);

            Assert::IsFalse (driver.modem.IsOnline());
            Assert::IsTrue (driver.modem.TakeLinkRequests().back().kind == HayesModem::LinkRequestKind::HangUp);
        }


        TEST_METHOD (DtrIsIgnoredUnderAmpD0)
        {
            ModemDriver  driver;



            driver.Type ("AT&D0\r");
            driver.Connect();
            driver.modem.OnDtrChanged (false, driver.now);

            Assert::IsTrue (driver.modem.IsOnline());
        }


        TEST_METHOD (WithAmpC0DcdIsAlwaysOn)
        {
            ModemDriver  driver;



            driver.Type ("AT&C0\r");

            Assert::IsTrue (driver.modem.GetDcd());
        }
    };
}
