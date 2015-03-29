#pragma once

#include "conversion/cast.h"
#include "log/log.h"

#include "net/details/persistent_queue.hpp"
#include "net/details/channel_base.hpp"
#include "net/details/channel.hpp"
#include "net/exception.hpp"
#include "net/details/allocator.hpp"
#include "net/settings.hpp"

#include <boost/asio.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/bind.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

namespace net
{
namespace udp
{

template
<
    template<typename> class Channel, 
    typename A = CrtAllocator,
    typename S = DefaultSettings
>
class Transport : public boost::enable_shared_from_this<Transport<Channel, A, S>>
{
    typedef boost::asio::ip::udp::socket Socket;
    typedef boost::enable_shared_from_this<Transport<Channel, A, S>> Shared;

public:
    typedef A Allocator;
    typedef S Settings;
    typedef details::PersistentQueue<Allocator, Settings> Queue;
    typedef boost::shared_ptr<Socket> Handle;
    typedef std::string Endpoint;
    typedef Channel<Transport> ChannelImpl;
    typedef boost::function<void(const typename ChannelImpl::Ptr& connection, const boost::system::error_code& e)> Callback;

    Transport(boost::asio::io_service& svc)
		: m_Service(svc)
	{
	}

    ~Transport()
	{
        Close();
	}

    void Receive(const Endpoint& endpoint, const Callback& callback)
    {
        m_ClientConnectedCallback = callback;

        const boost::asio::ip::udp::endpoint ep(ParseEP(endpoint));

        const auto socket = boost::make_shared<Socket>(m_Service, ep);
        {
            boost::unique_lock<boost::mutex> lock(m_Mutex);
            m_Sockets.push_back(socket);
        }

        const auto connection = boost::make_shared<Channel>(m_Service, ep, Shared::shared_from_this(), socket);
        m_ClientConnectedCallback(connection, boost::system::error_code());
    }

    void ConnectionClosed(const typename ChannelImpl::Ptr& connection, const boost::system::error_code& e)
    {
        if (m_ClientConnectedCallback)
            m_ClientConnectedCallback(connection, e);

        boost::unique_lock<boost::mutex> lock(m_Mutex);
        const auto socket = static_cast<const ChannelImpl&>(*connection).GetSocket();
        const auto it = boost::find_if(m_Sockets, [&socket](const boost::weak_ptr<Socket>& s){
            if (const auto locked = s.lock())
                return locked == socket;
            return false;
        });

        if (it != m_Sockets.end())
            m_Sockets.erase(it);
    }

    typename IConnection<Allocator>::Ptr Connect(const std::string& endpoint)
    {
        const auto socket = boost::make_shared<Socket>(m_Service, boost::asio::ip::udp::v4());
        
        const auto ep = ParseEP(endpoint);
        return boost::make_shared<ChannelImpl>(m_Service, ep, Shared::shared_from_this(), socket);
    }

    void Close()
    {
        std::vector<boost::weak_ptr<Socket>> copy;
        {
            boost::unique_lock<boost::mutex> lock(m_Mutex);
            copy.swap(m_Sockets);
        }

        boost::system::error_code e;
        for (const auto& socket : copy)
        {
            if (const auto s = socket.lock())
                s->close(e);
        }
    }

private:

    boost::asio::ip::udp::endpoint ParseEP(const std::string& endpoint)
    {
        std::vector<std::string> parts;
        boost::algorithm::split(parts, endpoint, boost::algorithm::is_any_of(":"));
        if (parts.size() != 2)
            BOOST_THROW_EXCEPTION(Exception("Wrong endpoint: %s", endpoint));

        return boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string(parts.front()),
                                              conv::cast<short>(parts.back()));
    }

private:
    boost::asio::io_service& m_Service;
    Callback m_ClientConnectedCallback;
    std::vector<boost::weak_ptr<Socket>> m_Sockets;
    boost::mutex m_Mutex;
};

} // namespace udp
} // namespace net