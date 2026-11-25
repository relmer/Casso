#include "Pch.h"

#include "ScratchImages.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "CassoExplorer/Model/DiskOperations.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchImages::GetDos33
//
////////////////////////////////////////////////////////////////////////////////

const vector<Byte> & ScratchImages::GetDos33 (bool hasPictures)
{
    static vector<Byte>  s_images[2];
    vector<Byte> &       image = s_images[hasPictures ? 1 : 0];



    if (image.empty())
    {
        image = Build (false, hasPictures);
    }

    return image;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchImages::GetProDos
//
////////////////////////////////////////////////////////////////////////////////

const vector<Byte> & ScratchImages::GetProDos (bool hasPictures)
{
    static vector<Byte>  s_images[2];
    vector<Byte> &       image = s_images[hasPictures ? 1 : 0];



    if (image.empty())
    {
        image = Build (true, hasPictures);
    }

    return image;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchImages::Build
//
//  The host files are the sources in Fixtures/CassoExplorer/Host.
//
////////////////////////////////////////////////////////////////////////////////

vector<Byte> ScratchImages::Build (bool isProDos, bool hasPictures)
{
    static constexpr Byte  kApplesoft[] =
    {
        0x31, 0x30, 0x20, 0x50, 0x52, 0x49, 0x4E, 0x54, 0x20, 0x22, 0x48, 0x45, 0x4C, 0x4C, 0x4F, 0x2C, 0x20, 0x43, 0x41, 0x53, 0x53, 0x51, 0x55, 0x45, 0x22, 0x0D, 0x0A,
        0x32, 0x30, 0x20, 0x46, 0x4F, 0x52, 0x20, 0x49, 0x20, 0x3D, 0x20, 0x31, 0x20, 0x54, 0x4F, 0x20, 0x33, 0x0D, 0x0A,
        0x33, 0x30, 0x20, 0x50, 0x52, 0x49, 0x4E, 0x54, 0x20, 0x49, 0x20, 0x2A, 0x20, 0x32, 0x0D, 0x0A,
        0x34, 0x30, 0x20, 0x4E, 0x45, 0x58, 0x54, 0x20, 0x49, 0x0D, 0x0A,
        0x35, 0x30, 0x20, 0x52, 0x45, 0x4D, 0x20, 0x20, 0x41, 0x20, 0x52, 0x45, 0x4D, 0x41, 0x52, 0x4B, 0x20, 0x57, 0x49, 0x54, 0x48, 0x20, 0x20, 0x20, 0x53, 0x50, 0x41, 0x43, 0x45, 0x53, 0x0D, 0x0A,
        0x36, 0x30, 0x20, 0x45, 0x4E, 0x44, 0x0D, 0x0A,
    };
    static constexpr Byte  kIntegerTokens[] =
    {
        0x0C, 0x0A, 0x00, 0x62, 0x28, 0xC8, 0xC5, 0xCC, 0xCC, 0xCF, 0x29, 0x01, 0x06, 0x0F, 0x00, 0x7B, 0xC9, 0x01, 0x0E, 0x14, 0x00, 0x55, 0xC9, 0x56, 0xB1,
        0x01, 0x00, 0x57, 0xB3, 0x03, 0x00, 0x01, 0x06, 0x1E, 0x00, 0x61, 0xC9, 0x01, 0x06, 0x28, 0x00, 0x59, 0xC9, 0x01, 0x05, 0x32, 0x00, 0x51, 0x01,
    };
    static const char  kNotes[] = "CASSO TEST NOTES\r\n\r\nThis file is ordinary printable text.\r\nIt has a tab\there and ends with a newline.\r\n";
    FakeDiskFileIo                  io;
    DiskOperations                  ops     (io);
    DiskOperations::NewDiskRequest  request;
    std::string                     path    = isProDos ? "C:\\Disks\\prodos.po" : "C:\\Disks\\dos33.dsk";



    io.files["C:\\Host\\applesoft.txt"] = vector<Byte> (std::begin (kApplesoft), std::end (kApplesoft));
    io.files["C:\\Host\\notes.txt"]     = vector<Byte> (kNotes, kNotes + sizeof (kNotes) - 1);
    io.files["C:\\Host\\hires.bin"]     = MakePattern (8192,  1);
    io.files["C:\\Host\\lores.bin"]     = MakePattern (1024,  2);
    io.files["C:\\Host\\dhires.bin"]    = MakePattern (16384, 3);
    io.files["C:\\Host\\odd.bin"]       = MakePattern (777,   4);
    io.files["C:\\Host\\integer.tok"]   = vector<Byte> (std::begin (kIntegerTokens), std::end (kIntegerTokens));

    request.formatName = isProDos ? "prodos" : "dos33";
    request.volumeName = isProDos ? "CASSO" : "77";

    Assert::IsTrue (ops.Create (path, request).Succeeded());
    Assert::IsTrue (ops.Put (path, "C:\\Host\\applesoft.txt", "HELLO", "", false, 0, DiskOperations::Encoding::Basic).Succeeded());
    Assert::IsTrue (ops.Put (path, "C:\\Host\\notes.txt",     "NOTES", "", false, 0, DiskOperations::Encoding::Text).Succeeded());

    if (hasPictures)
    {
        Assert::IsTrue (ops.Put (path, "C:\\Host\\hires.bin",  "PICTURE", "B", true, 0x2000, DiskOperations::Encoding::Verbatim).Succeeded());
        Assert::IsTrue (ops.Put (path, "C:\\Host\\lores.bin",  "LORES",   "B", true, 0x0400, DiskOperations::Encoding::Verbatim).Succeeded());
        Assert::IsTrue (ops.Put (path, "C:\\Host\\dhires.bin", "DHIRES",  "B", true, 0x2000, DiskOperations::Encoding::Verbatim).Succeeded());
    }

    Assert::IsTrue (ops.Put (path, "C:\\Host\\odd.bin", "ODD", "B", true, 0x0803, DiskOperations::Encoding::Verbatim).Succeeded());

    if (isProDos)
    {
        Assert::IsTrue (ops.Mkdir (path, "SUBDIR").Succeeded());
    }
    else
    {
        Assert::IsTrue (ops.Put (path, "C:\\Host\\integer.tok", "INTPROG", "I", false, 0, DiskOperations::Encoding::Verbatim).Succeeded());
    }

    return io.files[path];
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchImages::MakePattern
//
//  A deterministic pattern of bytes, as MakeFixtures.ps1 makes them. The
//  script's first product to overflow a PowerShell int is computed as a
//  double and rounded, and the masked result is an Int64, so every later
//  step is exact.
//
////////////////////////////////////////////////////////////////////////////////

vector<Byte> ScratchImages::MakePattern (size_t length, int64_t seed)
{
    static constexpr int64_t  kMultiplier = 1103515245;
    static constexpr int64_t  kIncrement  = 12345;
    vector<Byte>  bytes   (length, 0);
    int64_t       state   = seed;
    int64_t       product = 0;
    bool          isWide  = false;



    for (Byte & b : bytes)
    {
        product = state * kMultiplier;

        if (!isWide && (product > INT32_MAX || product + kIncrement > INT32_MAX))
        {
            product = (product > INT32_MAX) ? std::llrint (static_cast<double> (state) * kMultiplier + kIncrement) - kIncrement : product;
            isWide  = true;
        }

        state = (product + kIncrement) & 0x7FFFFFFF;
        b     = static_cast<Byte> ((state >> 16) & 0xFF);
    }

    return bytes;
}
