#include "Pch.h"

#include "Seams/Win32HostSerialPorts.h"
#include "Seams/Win32HostSerialPort.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSerialPorts::EnumeratePorts
//
//  The key is absent on a PC with no serial ports, which is an empty list.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostSerialPorts::EnumeratePorts (std::vector<std::string> & outNames)
{
    constexpr DWORD                kMaxName  = 256;
    HRESULT                        hr        = S_OK;
    HKEY                           key       = nullptr;
    LSTATUS                        status    = ERROR_SUCCESS;
    DWORD                          index     = 0;
    DWORD                          type      = 0;
    DWORD                          nameChars = 0;
    DWORD                          dataBytes = 0;
    std::array<wchar_t, kMaxName>  valueName = {};
    std::array<wchar_t, kMaxName>  data      = {};



    outNames.clear();

    status = RegOpenKeyExW (HKEY_LOCAL_MACHINE, L"HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &key);
    BAIL_OUT_IF (status == ERROR_FILE_NOT_FOUND, S_OK);
    CBREx (status == ERROR_SUCCESS, HRESULT_FROM_WIN32 (status));

    for (index = 0; ; index++)
    {
        nameChars = (DWORD) valueName.size();
        dataBytes = (DWORD) (data.size() * sizeof (wchar_t)) - sizeof (wchar_t);
        data      = {};

        status = RegEnumValueW (key, index, valueName.data(), &nameChars, nullptr, &type, (LPBYTE) data.data(), &dataBytes);

        if (status != ERROR_SUCCESS)
        {
            break;
        }

        if (type == REG_SZ)
        {
            outNames.push_back (TextEncoding::WideToNarrow (data.data()));
        }
    }

    std::sort (outNames.begin(), outNames.end(), IsBefore);

Error:
    if (key != nullptr)
    {
        RegCloseKey (key);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSerialPorts::Open
//
//  Windows opens a COM port for one handle at a time, so a port another
//  program has open fails with ERROR_ACCESS_DENIED.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostSerialPorts::Open (
    const std::string                 & name,
    std::unique_ptr<IHostSerialPort>  & outPort)
{
    HRESULT       hr     = S_OK;
    std::wstring  path   = L"\\\\.\\" + TextEncoding::NarrowToWide (name);
    HANDLE        handle = INVALID_HANDLE_VALUE;



    handle = CreateFileW (path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    CWR (handle != INVALID_HANDLE_VALUE);

    hr = Win32HostSerialPort::Prepare (handle);
    CHR (hr);

    outPort = std::make_unique<Win32HostSerialPort> (handle);
    handle  = INVALID_HANDLE_VALUE;

Error:
    if (handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle (handle);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSerialPorts::IsBefore
//
//  By the number after the letters, then by text.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32HostSerialPorts::IsBefore (
    const std::string  & a,
    const std::string  & b)
{
    size_t  aDigits = a.find_first_of ("0123456789");
    size_t  bDigits = b.find_first_of ("0123456789");
    int     aNumber = (aDigits == std::string::npos) ? 0 : atoi (a.c_str() + aDigits);
    int     bNumber = (bDigits == std::string::npos) ? 0 : atoi (b.c_str() + bDigits);



    return (aNumber != bNumber) ? aNumber < bNumber : a < b;
}
