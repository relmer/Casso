#include "Pch.h"

#include "CassoExplorer/Model/AppleTextFont.h"
#include "Config/IFileSystem.h"


static constexpr size_t  s_kSmallRomBytes = 2048;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleTextFont::Load
//
//  A file of any size but the two a video ROM has is passed over, as one that
//  cannot be read is.
//
////////////////////////////////////////////////////////////////////////////////

void AppleTextFont::Load (IFileSystem & fs, const std::wstring & assetDir)
{
    std::string   bytes;
    std::wstring  path;
    HRESULT       hr     = S_OK;
    bool          loaded = false;



    m_rom.LoadEmbeddedFallback();
    m_lowercase = false;
    m_source.clear();

    for (const wchar_t * relative : kRomPaths)
    {
        path = assetDir;

        if (!path.empty() && path.back() != L'\\')
        {
            path += L'\\';
        }

        path += relative;

        if (loaded || !fs.Exists (path))
        {
            continue;
        }

        hr = fs.ReadAllText (path, bytes);

        if (FAILED (hr) || (bytes.size() != s_kSmallRomBytes && bytes.size() != s_kLowercaseRomBytes))
        {
            continue;
        }

        hr = m_rom.LoadFromMemory ((const Byte *) bytes.data(), bytes.size());

        if (SUCCEEDED (hr))
        {
            loaded      = true;
            m_lowercase = bytes.size() == s_kLowercaseRomBytes;
            m_source    = path;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleTextFont::GetCode
//
//  Without lowercase, the five characters above it fold too: `{|}~ become
//  @[\]^, as a ][+ draws them.
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleTextFont::GetCode (wchar_t ch) const
{
    wchar_t  drawn = (ch < L' ' || ch > L'~') ? L'?' : ch;



    if (!m_lowercase && drawn >= L'`')
    {
        drawn = (wchar_t) (drawn - 0x20);
    }

    return (Byte) (drawn | 0x80);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleTextFont::Render
//
////////////////////////////////////////////////////////////////////////////////

void AppleTextFont::Render (
    const std::vector<std::wstring>  & rows,
    size_t                             first,
    size_t                             count,
    int                                columns,
    std::vector<uint32_t>            & outBgra,
    int                              & outWidth,
    int                              & outHeight) const
{
    size_t  last = (std::min) (rows.size(), first + count);



    outWidth  = (std::max) (columns, 0) * kCellWidth;
    outHeight = (int) count * kCellHeight;

    outBgra.assign ((size_t) outWidth * (size_t) outHeight, kUnlitArgb);

    for (size_t row = first; row < last; row++)
    {
        const std::wstring &  text = rows[row];
        int                   top  = (int) (row - first) * kCellHeight;

        for (int column = 0; column < columns && (size_t) column < text.size(); column++)
        {
            Byte  code = GetCode (text[(size_t) column]);

            for (int dotY = 0; dotY < kDotsHigh; dotY++)
            {
                Byte  dots = m_rom.GetGlyphRow (code, dotY);

                for (int dotX = 0; dotX < kDotsWide; dotX++)
                {
                    if ((dots & (1 << dotX)) == 0)
                    {
                        continue;
                    }

                    for (int sub = 0; sub < 4; sub++)
                    {
                        int  x = column * kCellWidth + dotX * 2 + (sub & 1);
                        int  y = top + dotY * 2 + (sub >> 1);

                        outBgra[(size_t) y * (size_t) outWidth + (size_t) x] = kLitArgb;
                    }
                }
            }
        }
    }
}
