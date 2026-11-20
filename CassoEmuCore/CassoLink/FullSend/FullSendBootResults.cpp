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
//  Each boot result and file-system verdict that arrived since the last call
//  is set in the manifest in memory, for a write of it before the next
//  reload. The thread that booted the image writes them to the manifest's
//  file. An image that did not boot is listed on the Apple at the next
//  pause, by its number. The latest one not read again yet has the offer to
//  read its disk again; an earlier one's offer is withdrawn, as the pause
//  has room for one.
//
////////////////////////////////////////////////////////////////////////////////

void FullSendBootResults::Apply()
{
    for (const FullSendBoot & boot : m_status.TakeBoots())
    {
        const std::vector<FullSendRow>  & rows  = m_manifest.GetRows();
        auto                              found = std::find_if (rows.begin(), rows.end(), [&boot] (const FullSendRow & row) { return _stricmp (row.file.c_str(), boot.file.c_str()) == 0; });

        m_manifest.SetBoot (boot.file, boot.boot, boot.fsCheck);
        m_latest      = boot.boot;
        m_latestCheck = boot.fsCheck;

        if (boot.boot == FullSendManifest::kBootDidNotBoot)
        {
            std::string  image   = found != rows.end() ? std::format ("{:04}", found->sequence) : boot.file.substr (0, boot.file.find_last_of ('.'));
            bool         isAgain = found != rows.end() && m_retried.contains (found->fingerprint);

            //  A disk is read again once. The image of one read again that
            //  did not boot either is as good as this session makes it.
            if (isAgain)
            {
                m_stillFailed.push_back (image);
            }
            else if (found == rows.end())
            {
                m_failed.push_back (image);
            }
            else
            {
                if (!m_retryImage.empty())
                {
                    m_failed.push_back (m_retryImage);
                }

                m_retryImage = image;
                m_retryPrint = found->fingerprint;
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendBootResults::GetLines
//
//  The next pause's lines about the images that did not boot: a line
//  listing those not offered again, a line listing those read again, and
//  the offer for the latest. The pause has the history's rows on the Apple,
//  so the lines stay few however many disks did not boot.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> FullSendBootResults::GetLines() const
{
    std::vector<std::string>  lines;



    if (!m_failed.empty())
    {
        lines.push_back (ListImages (m_failed) + " did not boot.");
    }

    if (!m_stillFailed.empty())
    {
        lines.push_back (ListImages (m_stillFailed) + " still does not boot.");
    }

    if (!m_retryImage.empty())
    {
        lines.push_back (std::format ("{} did not boot. Capture it again", m_retryImage));
        lines.push_back ("or put in a new disk, then press Space.");
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendBootResults::ListImages
//
//  0012, 0013 and 0014: the first s_kListedImages by number, and a count of
//  the rest.
//
////////////////////////////////////////////////////////////////////////////////

std::string FullSendBootResults::ListImages (const std::vector<std::string> & images)
{
    std::string  listed;
    size_t       count  = std::min (images.size(), s_kListedImages);
    bool         isMore = images.size() > count;



    for (size_t i = 0; i < count; i++)
    {
        bool  isLast = i + 1 == count && !isMore;

        listed += (i == 0) ? images[i] : (isLast ? " and " : ", ") + images[i];
    }

    if (isMore)
    {
        listed += std::format (" and {} more", images.size() - count);
    }

    return listed;
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
