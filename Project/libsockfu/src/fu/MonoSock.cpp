
// Author: Pierce Brooks

#include <fu/MonoSock.hpp>
#include <iostream>

fu::MonoSock::MonoSock(Role role, Protocol protocol, int port) :
  role(role),
  protocol(protocol),
  port(port),
  isConnected(false)
{
  threads = new ThreadPool(4);
  loop = nullptr;
  tcp = nullptr;
  connection = nullptr;
}

fu::MonoSock::~MonoSock()
{
  disconnect();
  delete threads;
}

bool fu::MonoSock::disconnect()
{
  if (!isConnected)
  {
    return false;
  }
  isConnected = false;
  if (loop != nullptr)
  {
    uv_stop(loop);
    loop = nullptr;
  }
  for (auto i = tcpAccepts.begin(); i != tcpAccepts.end(); i++)
  {
    uv_close((uv_handle_t*)(*i), nullptr);
    std::chrono::milliseconds timespan(100);
    std::this_thread::sleep_for(timespan);
    delete *i;
  }
  tcpAccepts.clear();
  if (tcp != nullptr)
  {
    uv_close((uv_handle_t*)tcp, nullptr);
    std::chrono::milliseconds timespan(100);
    std::this_thread::sleep_for(timespan);
    delete tcp;
    tcp = nullptr;
  }
  if (connection != nullptr)
  {
    std::chrono::milliseconds timespan(100);
    std::this_thread::sleep_for(timespan);
    delete connection;
    connection = nullptr;
  }
  return true;
}

bool fu::MonoSock::connect()
{
  if (isConnected)
  {
    return false;
  }
  if (role != Role::Client)
  {
    return false;
  }
  isConnected = true;
  loop = uv_default_loop();
  struct sockaddr address;
  uv_ip4_addr("127.0.0.1", port, (struct sockaddr_in*)(&address));
  bool success = true;
  int result = 0;
  switch (protocol)
  {
  case Protocol::TCP:
    tcp = new uv_tcp_t();
    uv_tcp_init(loop, tcp);
    tcp->data = this;
    connection = new uv_connect_t();
    result = uv_tcp_connect(connection, tcp, (const struct sockaddr*)(&address), [](uv_connect_t* connection, int status){static_cast<MonoSock*>(connection->handle->data)->establish(status);});
    if (result != 0)
    {
      success = false;
      goto MonoSock_connect_END;
    }
    break;
  }
MonoSock_connect_END:
  if (!success)
  {
    isConnected = false;
    return false;
  }
  threads->enqueue(0, [](uv_loop_t* loop){uv_run(loop, UV_RUN_DEFAULT);}, loop);
  return true;
}

bool fu::MonoSock::listen()
{
  if (isConnected)
  {
    return false;
  }
  if (role != Role::Server)
  {
    return false;
  }
  isConnected = true;
  loop = uv_default_loop();
  struct sockaddr address;
  uv_ip4_addr("0.0.0.0", port, (struct sockaddr_in*)(&address));
  bool success = true;
  int result = 0;
  switch (protocol)
  {
  case Protocol::TCP:
    tcp = new uv_tcp_t();
    uv_tcp_init(loop, tcp);
    tcp->data = this;
    result = uv_tcp_bind(tcp, (const struct sockaddr*)(&address), 0);
    if (result != 0)
    {
      success = false;
      goto MonoSock_listen_END;
    }
    result = uv_listen((uv_stream_t*)(tcp), 128, [](uv_stream_t* stream, int status){static_cast<MonoSock*>(stream->data)->accept(status);});
    if (result != 0)
    {
      success = false;
      goto MonoSock_listen_END;
    }
    break;
  }
MonoSock_listen_END:
  if (!success)
  {
    isConnected = false;
    return false;
  }
  threads->enqueue(0, [](uv_loop_t* loop){uv_run(loop, UV_RUN_DEFAULT);}, loop);
  return true;
}

bool fu::MonoSock::accept(int status)
{
  uv_tcp_t* acceptance = new uv_tcp_t();
  uv_tcp_init(loop, acceptance);
  int result = uv_accept((uv_stream_t*)tcp, (uv_stream_t*)acceptance);
  if (result != 0)
  {
    uv_close((uv_handle_t*)acceptance, nullptr);
    delete acceptance;
    return false;
  }
  tcpAccepts.push_back(acceptance);
  bool success = establish(status);
  return success;
}

bool fu::MonoSock::establish(int status)
{
  std::cout << std::to_string(tag) << std::endl;
  connectCallback();
  return true;
}

bool fu::MonoSock::send(const uint8_t* bytes, size_t length)
{
  std::cout << std::string(reinterpret_cast<const char*>(bytes), length) << std::endl;
  return true;
}

void fu::MonoSock::setConnectCallback(ConnectCallback callback)
{
  connectCallback = callback;
}

void fu::MonoSock::setReceiveCallback(ReceiveCallback callback)
{
  receiveCallback = callback;
}
