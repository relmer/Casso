#include "Pch.h"

#include "CassoLink/FullSend/FullSendBootResults.h"

#include "CassoLink/FullSend/FullSendManifest.h"
#include "CassoLink/FullSend/FullSendStatus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendBootResults::FullSendBootResults
//
////////////////////////////////////////////////////////////////////////////////

FullSendBootResults::FullSendBootResults (FullSendStatus & status, FullSendManifest & manifest) :
    m_status   (status),
    m_manifest (manifest)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendBootResults::Apply
//
//  Each boot result that arrived since the last call is set in the manifest
//  in memory, for a write of it before the next reload. The thread that
//  booted the image writes the result to the manifest's file. An image that
//  did not boot is listed on the Apple at the next pause, by its number,
//  with the offer to read its disk again the first time.
//
////////////////////////////////////////////////////////////////////////////////

void FullSendBootResults::Apply()
{
    for (const std::pair<std::string, std::string> & boot : m_status.TakeBoots())
    {
        const std::vector<FullSendRow>  & rows  = m_manifest.GetRows();
        auto                              found = std::find_if (rows.begin(), rows.end(), [&boot] (const FullSendRow & row) { return _stricmp (row.file.c_str(), boot.first.c_str()) == 0; });

        m_manifest.SetBoot (boot.first, boot.second);
        m_latest = boot.second;

        if (boot.second == FullSendManifest::kBootDidNotBoot)
        {
            std::string  image   = found != rows.end() ? std::format ("{:04}", found->sequence) : boot.first.substr (0, boot.first.find_last_of ('.'));
            bool         isAgain = found != rows.end() && m_retried.contains (found->fingerprint);

            //  A disk is read again once. The image of one read again that
            //  did not boot either is as good as this session makes it.
            if (isAgain || found == rows.end())
            {
                m_lines.push_back (image + (isAgain ? " still does not boot." : " did not boot."));
            }
            else
            {
                m_retryPrint = found->fingerprint;

                m_lines.push_back (std::format ("{} did not boot. Capture it again", image));
                m_lines.push_back ("or put in a new disk, then press Space.");
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendBootResults::TakeRetryPrint
//
//  The fingerprint of the disk the pause offers to read again, or empty; the
//  offer is taken by the key that ends the pause.
//
////////////////////////////////////////////////////////////////////////////////

std::string FullSendBootResults::TakeRetryPrint()
{
    std::string  print = std::move (m_retryPrint);



    m_retryPrint.clear();

    return print;
}
