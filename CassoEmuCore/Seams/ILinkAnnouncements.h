#pragma once

#include "Pch.h"

#include "CassoLink/LinkAnnouncement.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ILinkAnnouncements
//
//  The Apples universal boot disks' announcements have come from, each
//  once, by its MAC address, at the address in its latest announcement.
//
//  An announcement sent from this PC is from Casso's emulated card, whose
//  listening socket is on 127.0.0.1 rather than at the guest's address, so
//  its address is stored as 127.0.0.1.
//
////////////////////////////////////////////////////////////////////////////////

class ILinkAnnouncements
{
public:
    virtual ~ILinkAnnouncements () = default;

    virtual std::vector<LinkAnnouncement>  GetAnnounced   () const = 0;

    //  Changes with an Apple's first announcement, and with one that has a
    //  new address.
    virtual uint32_t                       GetChangeCount () const = 0;
};
