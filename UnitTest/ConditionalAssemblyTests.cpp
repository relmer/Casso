#include "Pch.h"

#include "Assembler.h"
#include "TestHelpers.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace ConditionalAssemblyTests
{
    TEST_CLASS (ConditionalAssemblyTests)
    {
    public:

        TEST_METHOD (Placeholder)
        {
            Assert::IsTrue (true);
        }
    };
}
