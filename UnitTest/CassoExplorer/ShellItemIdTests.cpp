#include "Pch.h"

#include "Seams/ShellItemId.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellItemIdTests
//
//  An encoded id is read back from a saved file, so one whose hex is not hex
//  is invalid data rather than an exception.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ShellItemIdTests)
{
public:

    TEST_METHOD (AnIdWhoseHexIsNotHexIsInvalidData)
    {
        PIDLIST_ABSOLUTE  pidl = nullptr;



        for (const wchar_t * hex : { L"ZZ", L"0G", L"1 " })
        {
            std::wstring  id = std::wstring (ShellItemId::kPrefix) + hex + L"\\name.dsk";

            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), ShellItemId::ToPidl (id, &pidl), hex);
            Assert::IsNull   (pidl);
        }
    }
};
