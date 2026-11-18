#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Debugger/Channel/HostAccessPolicy.h"
#include "Debugger/Channel/Win32HostPathResolver.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HostAccessJunctionTests
//
//  The debug channel's host file access policy over the real file system:
//  a scratch folder with an allowed folder, a folder outside it, and a
//  junction from the first into the second, which GetFinalPathNameByHandle
//  has to follow. Creating a junction doesn't require a privilege, where a
//  symbolic link requires Developer Mode, so the test runs on any machine.
//
//  The scratch folder is removed before anything is asserted, so nothing is
//  left behind by a failure.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (HostAccessJunctionTests)
{
public:
    //  A mount point reparse point at `link`, an empty folder, to `target`.
    static bool TryCreateJunction (const std::wstring & link, const std::wstring & target)
    {
        static constexpr size_t  kHeaderBytes = 8;
        static constexpr size_t  kNamesBytes  = 8;
        static constexpr size_t  kDataLength  = 4;
        static constexpr size_t  kSubLength   = 10;
        static constexpr size_t  kPrintStart  = 12;
        std::wstring             substitute   = L"\\??\\" + target;
        WORD                     subBytes     = (WORD) (substitute.size() * sizeof (wchar_t));
        WORD                     dataBytes    = (WORD) (kNamesBytes + subBytes + sizeof (wchar_t) * 2);
        std::vector<Byte>        buffer       (kHeaderBytes + dataBytes, 0);
        DWORD                    tag          = IO_REPARSE_TAG_MOUNT_POINT;
        WORD                     printOffset  = (WORD) (subBytes + sizeof (wchar_t));
        DWORD                    returned     = 0;
        HANDLE                   handle       = INVALID_HANDLE_VALUE;
        BOOL                     isSet        = FALSE;



        memcpy (buffer.data(),               &tag,         sizeof (tag));
        memcpy (buffer.data() + kDataLength, &dataBytes,   sizeof (dataBytes));
        memcpy (buffer.data() + kSubLength,  &subBytes,    sizeof (subBytes));
        memcpy (buffer.data() + kPrintStart, &printOffset, sizeof (printOffset));
        memcpy (buffer.data() + kHeaderBytes + kNamesBytes, substitute.data(), subBytes);

        handle = CreateFileW (link.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                              FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);

        if (handle == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        isSet = DeviceIoControl (handle, FSCTL_SET_REPARSE_POINT, buffer.data(), (DWORD) buffer.size(), nullptr, 0, &returned, nullptr);
        CloseHandle (handle);

        return isSet != FALSE;
    }



    TEST_METHOD (AJunctionOutOfAnAllowedFolder_IsFollowed)
    {
        std::error_code          ec;
        std::filesystem::path    root      = std::filesystem::temp_directory_path (ec)
                                             / std::format (L"casso-access-{}-{}", GetCurrentProcessId(), GetTickCount64());
        std::wstring             allowed   = (root / L"Allowed").wstring();
        std::wstring             outside   = (root / L"Outside").wstring();
        std::wstring             escape    = (root / L"Allowed" / L"Escape").wstring();
        Win32HostPathResolver    resolver;
        HostAccessVerdict        inside;
        HostAccessVerdict        through;
        HostAccessVerdict        dotDot;
        HostAccessVerdict        existing;
        HostAccessVerdict        replaced;
        HostAccessVerdict        viaLink;
        std::wstring             outsideFinal;
        bool                     isLinked  = false;



        std::filesystem::create_directories (escape, ec);
        std::filesystem::create_directories (outside, ec);
        std::ofstream (root / L"Allowed" / L"x.bin") << "x";

        isLinked = TryCreateJunction (escape, outside);

        {
            HostAccessPolicy  policy (resolver, { allowed });
            HostAccessPolicy  linked (resolver, { escape });



            inside   = policy.Check (allowed + L"\\new.bin",            HostAccess::Write, false);
            through  = policy.Check (escape  + L"\\new.bin",            HostAccess::Write, false);
            dotDot   = policy.Check (allowed + L"\\..\\Outside\\x.bin", HostAccess::Read,  false);
            existing = policy.Check (allowed + L"\\x.bin",              HostAccess::Write, false);
            replaced = policy.Check (allowed + L"\\x.bin",              HostAccess::Write, true);
            viaLink  = linked.Check (outside + L"\\new.bin",            HostAccess::Write, false);
        }

        (void) resolver.TryGetFinalPath (outside, outsideFinal);

        //  The junction is removed first, so remove_all doesn't delete its target.
        RemoveDirectoryW (escape.c_str());
        std::filesystem::remove_all (root, ec);

        Assert::IsTrue  (isLinked, L"the junction was created");
        Assert::IsTrue  (inside.isAllowed,   TextEncoding::NarrowToWide (inside.reason).c_str());
        Assert::IsFalse (through.isAllowed,  L"a write through the junction is outside");
        Assert::IsTrue  (through.reason.find ("is outside the allowed folders") != std::string::npos, TextEncoding::NarrowToWide (through.reason).c_str());
        Assert::IsTrue  (through.fullPath == outsideFinal + L"\\new.bin", through.fullPath.c_str());
        Assert::IsFalse (dotDot.isAllowed,   L"a path with .. out of the folder is outside");
        Assert::IsFalse (existing.isAllowed, L"a real file is not replaced without the option");
        Assert::IsTrue  (existing.reason.find ("already exists") != std::string::npos, TextEncoding::NarrowToWide (existing.reason).c_str());
        Assert::IsTrue  (replaced.isAllowed, TextEncoding::NarrowToWide (replaced.reason).c_str());
        Assert::IsTrue  (viaLink.isAllowed,  L"an allowed folder given through a junction is its target");
    }
};
