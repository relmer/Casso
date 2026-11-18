#include "Pch.h"

#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/LinkProgram.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ServeReady
//
//  A client of another version is sent the current program, and so is one
//  whose CRC differs from the image. A 6551 shim of another version is
//  sent the current shim first, whose Ready is then sent the program; the other
//  shims, of a network card, a cassette or SOS, are made again from the
//  connection's menu.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::ServeReady (const CassoLinkPacket & packet)
{
    HRESULT            hr          = S_OK;
    bool               whole       = IsWholeReady (packet);
    bool               current     = false;
    bool               intact      = false;
    bool               shimMatches = false;
    bool               updatable   = false;
    std::vector<Byte>  update;



    CBREx (whole, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    ReadClient (packet);

    current = m_client.programVersion == LinkProgram::kProgramVersion;
    intact  = current;

    if (current)
    {
        hr = CheckProgramCrc (intact);
        CHR (hr);
    }

    m_badProgramLoads = (current && !intact) ? m_badProgramLoads + 1 : 0;

    if (m_badProgramLoads > 0 && m_adtProServer.m_activity)
    {
        m_adtProServer.m_activity (std::format ("The program on the Apple failed its CRC check: it computed {:04X}, and the image sent is {:04X}",
                                                m_client.programCrc, CassoLinkCodec::ComputeCrc (m_program)));
    }

    //  A program that fails again after it is loaded again points to the
    //  Apple's memory, which another load would not fix. The cassette build
    //  came from the cassette input, so its audio is sent again instead.
    CBREx (m_badProgramLoads <= kMaxProgramReloads, HRESULT_FROM_WIN32 (ERROR_CRC));
    CBREx (intact || m_programChip != LinkChip::Cassette, HRESULT_FROM_WIN32 (ERROR_CRC));

    //  The SOS build is SOS.INTERP on the CassoLink SOS disk, with no shim
    //  to load another, so a disk from another version of Casso is made
    //  again.
    if (!intact && m_programChip == LinkChip::Sos && m_adtProServer.m_activity)
    {
        m_adtProServer.m_activity ("The CassoLink SOS disk doesn't match this version of Casso. Make the disk again from the connection's menu.");
    }

    CBREx (intact || m_programChip != LinkChip::Sos, HRESULT_FROM_WIN32 (ERROR_REVISION_MISMATCH));

    shimMatches = m_programChip == LinkChip::Sos || m_client.shimVersion == LinkProgram::kShimVersion;
    updatable   = !shimMatches && m_programChip == LinkChip::Acia;

    if (updatable)
    {
        if (m_adtProServer.m_activity)
        {
            m_adtProServer.m_activity ("Updating the CassoLink shim on the Apple to this version of Casso.");
        }

        hr = LinkProgram::BuildShimUpdate (update);
        CHR (hr);

        hr = LoadImage (update, true);
        CHR (hr);

        BAIL_OUT_IF (true, S_OK);
    }

    if (!shimMatches && m_adtProServer.m_activity)
    {
        m_adtProServer.m_activity ("The CassoLink boot disk on the Apple doesn't match this version of Casso. Make the disk again from the connection's menu.");
    }

    CBREx (intact || shimMatches, HRESULT_FROM_WIN32 (ERROR_REVISION_MISMATCH));

    if (intact)
    {
        hr = SendPacket (CassoLinkCodec::kIdle, {});
        CHR (hr);
    }
    else
    {
        hr = LoadProgram();
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::CheckProgramCrc
//
//  Whether the CRC in the client's Ready matches the image of the program
//  for its chip. A Ready with no CRC, from an earlier program, passes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::CheckProgramCrc (bool & outIntact)
{
    HRESULT  hr = S_OK;



    outIntact = true;

    BAIL_OUT_IF (!m_client.hasProgramCrc, S_OK);

    hr = AssembleProgram();
    CHR (hr);

    outIntact = CassoLinkCodec::ComputeCrc (m_program) == m_client.programCrc;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::LoadProgram
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::LoadProgram()
{
    HRESULT  hr = S_OK;



    hr = AssembleProgram();
    CHR (hr);

    hr = LoadImage (m_program);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::LoadImage
//
//  The image loaded, and loaded again while the shim restarts in the first
//  page with `restartOnReady`.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::LoadImage (const std::vector<Byte> & image, bool restartOnReady)
{
    HRESULT  hr      = S_OK;
    bool     restart = false;



    for (int start = 0; start == 0 || (restart && start <= kMaxResends); start++)
    {
        hr = LoadImageOnce (image, restartOnReady, restart);
        CHR (hr);
    }

    CBREx (!restart, HRESULT_FROM_WIN32 (ERROR_RETRY));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::LoadImageOnce
//
//  Load program and the image's pages, each followed by the shim's bare
//  ACK or NAK. Sent to a running program instead, Load program alone
//  returns it to the shim.
//
//  With `restartOnReady`, a Ready in place of the first page's reply sets
//  `outRestart`: after the Idle a connection sends first, a shim older than
//  version 12 draws its screen again and sends Ready again, and doesn't read
//  a Load program sent while it draws.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::LoadImageOnce (const std::vector<Byte> & image, bool restartOnReady, bool & outRestart)
{
    HRESULT            hr      = S_OK;
    std::vector<Byte>  page (kSectorSize, 0);
    std::vector<Byte>  encoded;
    Byte               count[1] = {};
    size_t             pages    = 0;
    size_t             from     = 0;
    Byte               reply    = 0;
    bool               filled   = false;
    bool               acked    = false;
    bool               running  = false;
    CassoLinkPacket    stray;



    outRestart = false;
    pages      = (image.size() + kSectorSize - 1) / kSectorSize;
    count[0]   = (Byte) pages;

    hr = SendPacket (CassoLinkCodec::kLoadProgram, count);
    CHR (hr);

    //  A running program goes back to the shim, whose Ready comes next.
    BAIL_OUT_IF (m_client.programVersion != 0, S_OK);

    for (size_t i = 0; i < pages && !outRestart; i++)
    {
        from = i * kSectorSize;

        std::fill (page.begin(), page.end(), (Byte) 0);
        std::copy_n (image.begin() + (ptrdiff_t) from, std::min (kSectorSize, image.size() - from), page.begin());

        encoded.clear();

        hr = CassoLinkCodec::EncodeSectorData ((Byte) i, (Byte) i, page, encoded);
        CHR (hr);

        acked = false;

        for (int attempt = 0; attempt <= kMaxResends && !acked && !outRestart; attempt++)
        {
            hr = Send (encoded);
            CHR (hr);

            do
            {
                hr = Fill (1, m_adtProServer.m_operationTimeoutMs, filled);
                CHR (hr);
                CBREx (filled, HRESULT_FROM_WIN32 (ERROR_TIMEOUT));

                reply = m_adtProServer.m_buffer[0];
                running = reply == CassoLinkCodec::kStart && i + 1 == pages;

                //  A packet in place of the last page's ACK is the program's
                //  Ready: the program is running, and its Ready is left for
                //  the next operation to read. A packet in place of another
                //  page's, such as a second Ready from the shim, is read whole
                //  and dropped, so that no byte of it is taken for the reply:
                //  a Ready's check byte can be an ACK's.
                if (reply == CassoLinkCodec::kStart && !running)
                {
                    hr = ReadPacket (m_adtProServer.m_operationTimeoutMs, stray);
                    hr = (hr == HRESULT_FROM_WIN32 (ERROR_INVALID_DATA)) ? S_OK : hr;
                    CHR (hr);

                    reply      = 0;
                    outRestart = restartOnReady && i == 0 && stray.command == CassoLinkCodec::kReady;
                }
                else if (!running)
                {
                    Consume (1);
                }
            }
            while (!running && !outRestart && reply != CassoLinkCodec::kAck && reply != CassoLinkCodec::kNak);

            acked = running || reply == CassoLinkCodec::kAck;
        }

        CBREx (acked || outRestart, HRESULT_FROM_WIN32 (ERROR_RETRY));
    }

Error:
    return hr;
}
