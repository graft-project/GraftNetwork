#pragma once

#include "net/http_client.h"

#include <memory>
#include <unordered_map>

namespace tools
{

//forwards
class HttpConnectionImpl;
class HttpClientHolder;

typedef std::shared_ptr<HttpConnectionImpl> HttpConnectionImplPtr;
typedef std::shared_ptr<HttpClientHolder> HttpClientHolderPtr;

class HttpConnectionManager
{
public:
  HttpConnectionManager();
  ~HttpConnectionManager();

  HttpConnectionManager(const HttpConnectionManager&) = delete;
  HttpConnectionManager(HttpConnectionManager&&) = delete;
  HttpConnectionManager& operator = (const HttpConnectionManager&) = delete;

  HttpConnectionImplPtr get_connection(const std::string& host, const std::string& port, boost::optional<epee::net_utils::http::login> user);

  static HttpConnectionManager& get_instance();

private:
  typedef std::unordered_map<std::string, HttpConnectionImplPtr> ConnectionMap;

  std::mutex m_connections_mutex;
  ConnectionMap m_connections;
};

class HttpConnection
{
public:
  HttpConnection() = default;
  HttpConnection(HttpConnectionManager& manager, const std::string& host, const std::string& port, boost::optional<epee::net_utils::http::login> user);
  ~HttpConnection() = default;

  HttpClientHolderPtr get_client_holder();

private:
  HttpConnectionImplPtr m_impl;
};

class HttpInvoker
{
public:
  HttpInvoker(HttpConnection&);
  ~HttpInvoker();

  HttpInvoker(const HttpInvoker&) = delete;
  HttpInvoker(HttpInvoker&&) = delete;
  HttpInvoker& operator = (const HttpInvoker&) = delete;

  epee::net_utils::http::http_simple_client* get_client();

private:
  HttpClientHolderPtr m_client_holder;
};

template<class t_request, class t_response>
bool invoke_http_json
 (const boost::string_ref uri,
  const t_request& out_struct,
  t_response& result_struct,
  HttpConnection& connection,
  std::chrono::milliseconds timeout = std::chrono::seconds(15),
  const boost::string_ref method = "GET")
{
  HttpInvoker invoker(connection);
  epee::net_utils::http::http_simple_client* http_client = invoker.get_client();

  if (!http_client)
    return false;

  return invoke_http_json(uri, out_struct, result_struct, *http_client, timeout, method);
}

template<class t_request, class t_response>
bool invoke_http_bin
 (const boost::string_ref uri,
  const t_request& out_struct,
  t_response& result_struct,
  HttpConnection& connection,
  std::chrono::milliseconds timeout = std::chrono::seconds(15),
  const boost::string_ref method = "GET")
{
  HttpInvoker invoker(connection);
  epee::net_utils::http::http_simple_client* http_client = invoker.get_client();

  if (!http_client)
    return false;

  return invoke_http_bin(uri, out_struct, result_struct, *http_client, timeout, method);
}

}
