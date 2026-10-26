#include "Pch.h"

#include "Config/MacAddress.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MacAddressTests
//
////////////////////////////////////////////////////////////////////////////////

namespace MacAddressTests
{
    TEST_CLASS (MacAddressTests)
    {
    public:

        TEST_METHOD (ParsesColonAndHyphenForms)
        {
            MacAddress  colon;
            MacAddress  hyphen;

            Assert::IsTrue (MacAddress::TryParse ("02:CA:55:12:34:56", colon));
            Assert::IsTrue (MacAddress::TryParse ("02-ca-55-12-34-56", hyphen));
            Assert::IsTrue (colon == hyphen);
            Assert::AreEqual (std::string ("02:CA:55:12:34:56"), colon.Format());
            Assert::IsTrue (colon.IsSet());
        }


        TEST_METHOD (RejectsMalformedText)
        {
            MacAddress  parsed;

            for (const char * bad : { "", "02:CA:55:12:34", "02:CA:55:12:34:56:78", "02:CA:55:12:34:5G",
                                      "02CA55123456", "02:CA-55:12:34:56", "02:CA:55:12:34:+6", " 02:CA:55:12:34:5" })
            {
                Assert::IsFalse (MacAddress::TryParse (bad, parsed), std::wstring (bad, bad + strlen (bad)).c_str());
            }

            Assert::IsFalse (parsed.IsSet());
        }


        TEST_METHOD (MulticastIsNotUnicast)
        {
            MacAddress  multicast;
            MacAddress  unicast;

            Assert::IsTrue (MacAddress::TryParse ("01:00:5E:00:00:01", multicast));
            Assert::IsTrue (MacAddress::TryParse ("00:08:DC:01:02:03", unicast));

            Assert::IsFalse (multicast.IsUnicast());
            Assert::IsTrue  (unicast.IsUnicast());
        }


        TEST_METHOD (RandomLocalSetsTheLocalBitAndClearsMulticast)
        {
            for (Byte first : { (Byte) 0x00, (Byte) 0x01, (Byte) 0xFF, (Byte) 0xFC })
            {
                MacAddress  mac = MacAddress::MakeRandomLocal ({ first, 1, 2, 3, 4, 5 });

                Assert::AreEqual ((Byte) 0x02, (Byte) (mac.GetOctets()[0] & 0x03));
                Assert::AreEqual ((Byte) 5,    mac.GetOctets()[5]);
            }
        }
    };
}
