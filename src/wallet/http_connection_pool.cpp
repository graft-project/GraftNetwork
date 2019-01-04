#include "http_connection_pool.h"

using namespace tools;

const int HTTP_CONNECTION_TIMEOUT_MS = 20000;

namespace tools
{

/// HttpClientHolder
class HttpClientHolder
{
public:
  HttpClientHolder(const std::string& host, const std::string& port, boost::optional<epee::net_utils::http::login> user)
    : m_locked(false)
  {
    m_http_client.set_server(host, port, user);
  }

  epee::net_utils::http::http_simple_client& get_client() { return m_http_client; }

  bool tryLock()
  {
    bool expected = false;

    if (!m_locked.compare_exchange_strong(expected, true, std::memory_order::memory_order_seq_cst))
      return false;

    return true;
  }

  void unlock()
  {
    m_locked.store(false, std::memory_order::memory_order_seq_cst);
  }

private:
  std::atomic<bool> m_locked;
  epee::net_utils::http::http_simple_client m_http_client;
};

/// HttpConnectionImpl
class HttpConnectionImpl
{
public:
  HttpConnectionImpl(const std::string& host, const std::string& port, boost::optional<epee::net_utils::http::login> user)
    : m_host(host)
    , m_port(port)
    , m_user(user)
  {
  }

  HttpClientHolderPtr get_client_holder()
  {
    try
    {
      {
        std::lock_guard<std::mutex> lock(m_clients_mutex);

        for (HttpClientHolderPtr& client : m_clients)
        {
          if (client->tryLock())
            return client;
        }
      }

      HttpClientHolderPtr new_client = std::make_shared<HttpClientHolder>(m_host, m_port, m_user);

      bool r = new_client->tryLock();

      assert(r && "tryLock should always succeed for new created clients");

      std::lock_guard<std::mutex> lock(m_clients_mutex);

      m_clients.push_back(new_client);

      return new_client;
    }
    catch (...)
    {
      return nullptr;
    }
  }

private:
  typedef std::list<HttpClientHolderPtr> HttpClientList;

  std::string m_host;
  std::string m_port;
  boost::optional<epee::net_utils::http::login> m_user;
  std::mutex m_clients_mutex;
  HttpClientList m_clients;
};

}

/// HttpInvoker
HttpInvoker::HttpInvoker(HttpConnection& connection)
  : m_client_holder(connection.get_client_holder())
{
  //already locked by HttpConectionImpl::get_client_holder or null
}

HttpInvoker::~HttpInvoker()
{
  if (m_client_holder)
    m_client_holder->unlock();
}

epee::net_utils::http::http_simple_client* HttpInvoker::get_client()
{
  if (!m_client_holder)
    return nullptr;

  return &m_client_holder->get_client();
}

/// HttpConnection

HttpConnection::HttpConnection(HttpConnectionManager& manager, const std::string& host, const std::string& port, boost::optional<epee::net_utils::http::login> user)
  : m_impl(manager.get_connection(host, port, user))
{
}

HttpClientHolderPtr HttpConnection::get_client_holder()
{
  if (!m_impl)
    return HttpClientHolderPtr();

  return m_impl->get_client_holder();
}

/// HttpConnectionManager

HttpConnectionManager::HttpConnectionManager()
{
}

HttpConnectionManager::~HttpConnectionManager()
{
}

HttpConnectionManager& HttpConnectionManager::get_instance()
{
  static HttpConnectionManager instance;
  return instance;
}

HttpConnectionImplPtr HttpConnectionManager::get_connection(const std::string& host, const std::string& port, boost::optional<epee::net_utils::http::login> user)
{
  std::string key = host + ":" + port;

  if (user)
  {
    key += "@";
    key += user->username;
  }

  std::lock_guard<std::mutex> lock(m_connections_mutex);

  ConnectionMap::iterator it = m_connections.find(key);

  if (it != m_connections.end())
    return it->second;

  HttpConnectionImplPtr new_connection = std::make_shared<HttpConnectionImpl>(host, port, user);

  m_connections[key] = new_connection;

  return new_connection;  
}
