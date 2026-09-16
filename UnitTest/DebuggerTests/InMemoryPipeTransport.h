#pragma once

#include "Debugger/Channel/IPipeTransport.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InMemoryPipeTransport
//
//  A whole multi-client conversation with no pipe and no threads: the test
//  connects clients, types lines as them, pumps the server, and reads back
//  what each one was sent.
//
//  ARRIVAL ORDER IS THE TEST'S TO CHOOSE, because that is the property under
//  test. Lines from every client go into one queue in the order they were
//  typed, so a test can interleave two clients exactly and then assert that
//  the server ran their commands one at a time in that order.
//
//  What each client received is kept per connection rather than merged, so a
//  test can tell a reply that went to one client from a notification that
//  went to all of them -- which is the distinction the contract turns on.
//
////////////////////////////////////////////////////////////////////////////////

class InMemoryPipeTransport : public IPipeTransport
{
public:
    HRESULT Listen () override
    {
        m_isListening = true;
        return m_listenResult;
    }


    bool TryAccept (ChannelConnectionId & connection) override
    {
        if (m_pendingAccepts.empty())
        {
            return false;
        }

        connection = m_pendingAccepts.front();
        m_pendingAccepts.pop_front();
        return true;
    }


    bool TryReadLine (ChannelConnectionId & connection, std::string & line) override
    {
        if (m_incoming.empty())
        {
            return false;
        }

        connection = m_incoming.front().first;
        line       = m_incoming.front().second;
        m_incoming.pop_front();
        return true;
    }


    void WriteLine (ChannelConnectionId connection, const std::string & line) override
    {
        //  A write to a client that has gone is dropped, as the real one
        //  drops it: the client can disappear between the decision to answer
        //  and the answer.
        if (std::find (m_connections.begin(), m_connections.end(), connection) != m_connections.end())
        {
            m_written[connection].push_back (line);
        }
    }


    void Disconnect (ChannelConnectionId connection) override
    {
        m_connections.erase (std::remove (m_connections.begin(), m_connections.end(), connection), m_connections.end());
    }


    void Close () override
    {
        m_isListening = false;
        m_connections.clear();
    }


    std::vector<ChannelConnectionId> GetConnections () const override
    {
        return m_connections;
    }



    ////////////////////////////////////////////////////////////////////////////
    //
    //  What a test drives it with
    //
    ////////////////////////////////////////////////////////////////////////////

    //  A client arrives. The server picks it up on its next pump.
    ChannelConnectionId Connect ()
    {
        ChannelConnectionId  connection = m_nextConnection++;

        m_connections.push_back (connection);
        m_pendingAccepts.push_back (connection);
        return connection;
    }


    //  That client types a line. Order across clients is the order of these
    //  calls, which is what makes arrival order a thing a test can state.
    void Send (ChannelConnectionId connection, const std::string & line)
    {
        m_incoming.emplace_back (connection, line);
    }


    const std::vector<std::string> & Written (ChannelConnectionId connection) const
    {
        static const std::vector<std::string>  kNothing;
        auto                                   it = m_written.find (connection);

        return (it == m_written.end()) ? kNothing : it->second;
    }


    bool IsListening () const { return m_isListening; }

    void SetListenResult (HRESULT result) { m_listenResult = result; }

private:
    ChannelConnectionId                                            m_nextConnection = 1;
    bool                                                           m_isListening    = false;
    HRESULT                                                        m_listenResult   = S_OK;
    std::vector<ChannelConnectionId>                               m_connections;
    std::deque<ChannelConnectionId>                                m_pendingAccepts;
    std::deque<std::pair<ChannelConnectionId, std::string>>        m_incoming;
    std::map<ChannelConnectionId, std::vector<std::string>>        m_written;
};
