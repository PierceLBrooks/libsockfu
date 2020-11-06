
// Author: Pierce Brooks

#include <fu/MonoSock.hpp>
#include <fu/PolySock.hpp>
#include <iostream>
#include <cstring>

fu::MonoSock::MonoSock(Role role, Protocol protocol, int port) :
  role(role),
  protocol(protocol),
  port(port),
  isConnected(false),
  isIdle(false)
{
  threads = new ThreadPool(4);
  loop = nullptr;
  tcp = nullptr;
  connection = nullptr;
  owner = nullptr;
  peer = "127.0.0.1";
  connectCallback = [](const std::string& address){return true;};
  disconnectCallback = [](){};
  receiveCallback = [](const uint8_t* bytes, size_t length){};
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
  isIdle = false;
  conditionRead.notify_all();
  conditionWrite.notify_all();
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
  uv_ip4_addr(peer.c_str(), port, (struct sockaddr_in*)(&address));
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
    disconnect();
    return false;
  }
  std::cout << "connect" << std::endl;
  threads->enqueue(0, "Connect", [](uv_loop_t* loop){uv_run(loop, UV_RUN_DEFAULT);}, loop);
  return true;
}

bool fu::MonoSock::listen()
{
  if (isConnected)
  {
    return false;
  }
  if (role != Role::Listener)
  {
    return false;
  }
  isConnected = true;
  peer = "0.0.0.0";
  loop = uv_default_loop();
  struct sockaddr address;
  uv_ip4_addr(peer.c_str(), port, (struct sockaddr_in*)(&address));
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
    disconnect();
    return false;
  }
  std::cout << "listen" << std::endl;
  threads->enqueue(0, "Listen", [](uv_loop_t* loop){uv_run(loop, UV_RUN_DEFAULT);}, loop);
  return true;
}

bool fu::MonoSock::accept(int status)
{
  std::cout << "accept" << std::endl;
  /*if (status != 0)
  {
    return false;
  }*/
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
  struct sockaddr_storage address;
  int length = sizeof(struct sockaddr_storage);
  memset(&address, 0, sizeof(struct sockaddr_storage));
  result = uv_tcp_getpeername(acceptance, (struct sockaddr*)(&address), &length);
  if (result != 0)
  {
    uv_close((uv_handle_t*)acceptance, nullptr);
    delete acceptance;
    return false;
  }
  bool success = establish(&address, status);
  if (success)
  {
    if (owner != nullptr)
    {
      std::cout << "child" << std::endl;
      int i = 0;
      MonoSock* child = new MonoSock(Role::Server, protocol, port);
      child->isConnected = true;
      child->owner = owner;
      child->peer = peer;
      child->parent = this;
      child->connectCallback = connectCallback;
      child->disconnectCallback = disconnectCallback;
      child->receiveCallback = receiveCallback;
      children.push_back(child);
      for (;;)
      {
        i++;
        if (owner->push(tag+i, child))
        {
          break;
        }
        else
        {
          if (child->owner == nullptr)
          {
            break;
          }
        }
      }
    }
  }
  else
  {
    std::cout << "reject" << std::endl;
  }
  peer = "0.0.0.0";
  return success;
}

bool fu::MonoSock::establish(int status)
{
  return establish(peer, status);
}

bool fu::MonoSock::establish(const std::string& address, int status)
{
  std::cout << "establish" << std::endl;
  /*if (status != 0)
  {
    return false;
  }*/
  peer = address;
  std::cout << address << std::endl;
  if (connectCallback(address))
  {
    if (role != Role::Listener)
    {
      if (idle())
      {
        return true;
      }
    }
    else
    {
      return true;
    }
  }
  return false;
}

bool fu::MonoSock::establish(const struct sockaddr_storage* address, int status)
{
  if (address == nullptr)
  {
    return false;
  }
  char ip[45] = {0};
  int port = 0;
  if (address->ss_family == AF_INET)
  {
    struct sockaddr_in* ip4 = (struct sockaddr_in*)address;
    uv_ip4_name(ip4, ip, sizeof(ip));
    port = ip4->sin_port;
  }
  else if (address->ss_family == AF_INET6)
  {
    struct sockaddr_in6* ip6 = (struct sockaddr_in6*)address;
    uv_ip6_name(ip6, ip, sizeof(ip));
    port = ip6->sin6_port;
  }
  else
  {
    std::cout << "address" << address->ss_family << std::endl;
    return false;
  }
  return establish(std::string(ip), status);
}

bool fu::MonoSock::idle()
{
  if (!isConnected)
  {
    return false;
  }
  if (isIdle)
  {
    return false;
  }
  isIdle = true;
  threads->enqueue(1, "Read", [this](){read();});
  threads->enqueue(2, "Write", [this](){write();});
  std::cout << "idle" << std::endl;
  return true;
}

void fu::MonoSock::read()
{
  std::cout << "read" << std::endl;
  while (isIdle)
  {
    std::chrono::milliseconds timespan(100);
    std::unique_lock<std::mutex> lock(mutexRead);
    conditionRead.wait(lock, [this]{return !this->isIdle;});
    if (!isIdle)
    {
      break;
    }
    //std::this_thread::sleep_for(timespan);
  }
  std::cout << "done" << std::endl;
}

void fu::MonoSock::write()
{
  std::cout << "write" << std::endl;
  while (isIdle)
  {
    std::chrono::milliseconds timespan(100);
    std::unique_lock<std::mutex> lock(mutexWrite);
    conditionWrite.wait(lock, [this]{return !this->isIdle;});
    if (!isIdle)
    {
      break;
    }
    //std::this_thread::sleep_for(timespan);
  }
}

bool fu::MonoSock::send(const uint8_t* bytes, size_t length)
{
  std::cout << std::string(reinterpret_cast<const char*>(bytes), length) << std::endl;
  return true;
}

void fu::MonoSock::setDisconnectCallback(DisconnectCallback callback)
{
  disconnectCallback = callback;
}

void fu::MonoSock::setConnectCallback(ConnectCallback callback)
{
  connectCallback = callback;
}

void fu::MonoSock::setReceiveCallback(ReceiveCallback callback)
{
  receiveCallback = callback;
}
