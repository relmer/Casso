#include "Pch.h"

#include "Assembler.h"
#include "TestHelpers.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace MacroTests
{
    TEST_CLASS (MacroTests)
    {
    public:

        TEST_METHOD (Placeholder)
        {
            Assert::IsTrue (true);
        }
    };
}
