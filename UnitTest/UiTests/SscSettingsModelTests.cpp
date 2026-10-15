#include "Pch.h"

#include "Ui/Settings/SscSettingsModel.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




namespace SscSettingsModelTestNs
{
    static constexpr SscSetting  s_kAllSettings[] =
    {
        SscSetting::Mode,     SscSetting::BaudRate, SscSetting::DataFormat, SscSetting::StopBits,
        SscSetting::Parity,   SscSetting::CrDelay,  SscSetting::LineWidth,  SscSetting::LineFeed,
        SscSetting::Interrupts, SscSetting::ClearToSend,
    };




    ////////////////////////////////////////////////////////////////////////////
    //
    //  SscSettingsModelTests
    //
    //  The Serial card page's model against Apple's switch tables. The sweep
    //  applies every choice of every setting over a patterned set of switches
    //  and checks both that the choice reads back and that no switch outside
    //  the setting moved.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SscSettingsModelTests)
    {
    public:
        TEST_METHOD (DefaultSwitchesReadAsTheDefaultChoices)
        {
            SscSwitches  sw = SscSwitches::MakeDefault();



            Assert::AreEqual (0,  SscSettingsModel::GetChoiceIndex (SscSetting::Mode,        sw), L"communications");
            Assert::AreEqual (13, SscSettingsModel::GetChoiceIndex (SscSetting::BaudRate,    sw), L"9,600");
            Assert::AreEqual (0,  SscSettingsModel::GetChoiceIndex (SscSetting::DataFormat,  sw), L"8 data, 1 stop");
            Assert::AreEqual (0,  SscSettingsModel::GetChoiceIndex (SscSetting::Parity,      sw), L"none");
            Assert::AreEqual (0,  SscSettingsModel::GetChoiceIndex (SscSetting::LineFeed,    sw), L"no");
            Assert::AreEqual (0,  SscSettingsModel::GetChoiceIndex (SscSetting::Interrupts,  sw), L"off");
            Assert::AreEqual (0,  SscSettingsModel::GetChoiceIndex (SscSetting::ClearToSend, sw), L"normal");
        }


        TEST_METHOD (EveryChoiceReadsBackAndTouchesOnlyItsSwitches)
        {
            int  checked = 0;



            for (SscSetting setting : s_kAllSettings)
            {
                vector<wstring>  choices = SscSettingsModel::GetChoices (setting);

                Assert::IsFalse (choices.empty(), SscSettingsModel::GetLabel (setting).c_str());

                for (int c = 0; c < static_cast<int> (choices.size()); ++c)
                {
                    SscSwitches  before = MakePattern();
                    SscSwitches  after  = before;

                    SscSettingsModel::ApplyChoice (setting, c, after);

                    Assert::AreEqual (c, SscSettingsModel::GetChoiceIndex (setting, after),
                                      format (L"{}: {}", SscSettingsModel::GetLabel (setting), choices[c]).c_str());

                    for (SscSetting other : s_kAllSettings)
                    {
                        if (!SharesSwitches (setting, other))
                        {
                            Assert::AreEqual (SscSettingsModel::GetChoiceIndex (other, before),
                                              SscSettingsModel::GetChoiceIndex (other, after),
                                              format (L"{} disturbed {}", SscSettingsModel::GetLabel (setting),
                                                      SscSettingsModel::GetLabel (other)).c_str());
                        }
                    }

                    ++checked;
                }
            }

            Assert::IsTrue (checked > 0, L"the sweep checked something");
        }


        TEST_METHOD (BaudChoiceIsTheBaudCodeLessOne)
        {
            for (int c = 0; c < 15; ++c)
            {
                SscSwitches  sw;

                SscSettingsModel::ApplyChoice (SscSetting::BaudRate, c, sw);
                Assert::AreEqual (c + 1, sw.GetBaudCode());
            }
        }


        TEST_METHOD (UndefinedBaudSettingIsCustom)
        {
            SscSwitches  sw;



            for (int n = 1; n <= 4; ++n)
            {
                sw.SetOn (SscBank::Sw1, n, true);
            }

            Assert::AreEqual (SscSettingsModel::kCustom, SscSettingsModel::GetChoiceIndex (SscSetting::BaudRate, sw));
        }


        TEST_METHOD (ModeChangesTheSettingsOffered)
        {
            vector<SscSetting>  comm    = SscSettingsModel::GetSettings (SscMode::Communications);
            vector<SscSetting>  printer = SscSettingsModel::GetSettings (SscMode::Printer);
            vector<SscSetting>  sicP8   = SscSettingsModel::GetSettings (SscMode::SicP8);



            Assert::IsTrue (find (comm.begin(),    comm.end(),    SscSetting::Parity)    != comm.end());
            Assert::IsTrue (find (comm.begin(),    comm.end(),    SscSetting::LineWidth) == comm.end());
            Assert::IsTrue (find (printer.begin(), printer.end(), SscSetting::LineWidth) != printer.end());
            Assert::IsTrue (find (printer.begin(), printer.end(), SscSetting::Parity)    == printer.end());
            Assert::IsTrue (printer == sicP8, L"the SIC emulations use the printer switch table");
        }


        TEST_METHOD (ReadoutListsEveryPosition)
        {
            SscSwitches  sw = SscSwitches::MakeDefault();



            Assert::AreEqual (wstring (L"SW1: OFF OFF OFF ON ON ON ON"), SscSettingsModel::GetReadout (sw, SscBank::Sw1));
            Assert::AreEqual (wstring (L"SW2: ON ON OFF ON OFF OFF OFF"), SscSettingsModel::GetReadout (sw, SscBank::Sw2));
        }

    private:
        //  Alternating positions, so a setting that writes a neighbor's switch
        //  changes something the check can see.
        static SscSwitches MakePattern()
        {
            SscSwitches  sw;



            for (int n = 1; n <= SscSwitches::kSwitchesPerBank; ++n)
            {
                sw.SetOn (SscBank::Sw1, n, (n % 2) == 0);
                sw.SetOn (SscBank::Sw2, n, (n % 2) == 1);
            }

            return sw;
        }

        //  Settings that read the same switches in different modes (data
        //  format and stop bits, parity and line width, data format and the
        //  carriage-return delay) legitimately move each other.
        static bool SharesSwitches (SscSetting a, SscSetting b)
        {
            static const vector<pair<SscSetting, SscSetting>>  kShared =
            {
                { SscSetting::DataFormat, SscSetting::StopBits },
                { SscSetting::DataFormat, SscSetting::CrDelay  },
                { SscSetting::Parity,     SscSetting::LineWidth },
            };
            bool  shared = (a == b);



            for (const auto & pairing : kShared)
            {
                shared = shared || (pairing.first == a && pairing.second == b)
                                || (pairing.first == b && pairing.second == a);
            }

            return shared;
        }
    };
}
