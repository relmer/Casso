#pragma once

#include "Pch.h"

#include "Seams/ILinkAnnouncements.h"
#include "Seams/INetworkChanges.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32LinkAnnouncements
//
//  ILinkAnnouncements over a UDP socket bound to the announcement port on
//  every interface, read on a thread of its own. The port is bound for
//  reuse, so a second Casso Explorer on the PC receives the announcements
//  too. With a port that cannot be bound, the list is empty.
//
////////////////////////////////////////////////////////////////////////////////

class Win32LinkAnnouncements : public ILinkAnnouncements
{
public:
    //  With changes, the PC's addresses are listed again after a network
    //  change.
    explicit Win32LinkAnnouncements (uint16_t port, const INetworkChanges * changes = nullptr) : m_port (port), m_networkChanges (changes) {}
    ~Win32LinkAnnouncements () override;

    HRESULT                        SetListening   (bool listening) override;
    std::vector<LinkAnnouncement>  GetAnnounced   () const override;
    uint32_t                       GetChangeCount () const override { return m_changes; }

private:
    //  How long a receive waits, so Stop is seen.
    static constexpr int  s_kWaitMs = 250;

    HRESULT  Start              ();
    void     Stop               ();
    void     Run                ();
    void     ListLocalAddresses ();
    void     Record             (LinkAnnouncement announcement, const sockaddr_in & from);

    uint16_t                       m_port           = 0;
    const INetworkChanges        * m_networkChanges = nullptr;
    uint32_t                       m_seenNetwork    = 0;
    SOCKET                         m_socket         = INVALID_SOCKET;
    bool                           m_winsock        = false;
    std::thread                    m_thread;
    std::atomic<bool>              m_stop           { false };
    std::atomic<uint32_t>          m_changes        { 0 };
    mutable std::mutex             m_lock;
    std::vector<LinkAnnouncement>  m_announced;
    std::vector<uint32_t>          m_local;
};
