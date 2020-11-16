
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
  isIdle(false),
  isWriting(false),
  isReading(false),
  isRunning(false)
{
  threads = new ThreadPool(4);
  loop = nullptr;
  tcp = nullptr;
  connection = nullptr;
  owner = nullptr;
  parent = nullptr;
  peer = "127.0.0.1";
  tag = 0;
  index = -1;
  connectCallback = [](MonoSock* sock, const std::string& address){return true;};
  disconnectCallback = [](MonoSock* sock){};
  receiveCallback = [](MonoSock* sock, const uint8_t* bytes, size_t length){};
}

fu::MonoSock::~MonoSock()
{
  disconnect();
  if (threads != nullptr)
  {
    threads->kill();
    delete threads;
    threads = nullptr;
  }
}

bool fu::MonoSock::getIsIdle() const
{
  bool isIdle;
  {
    std::unique_lock<std::mutex> lock(mutexGlobal);
    isIdle = this->isIdle;
  }
  return isIdle;
}

bool fu::MonoSock::getIsConnected() const
{
  bool isConnected;
  {
    std::unique_lock<std::mutex> lock(mutexGlobal);
    isConnected = this->isConnected;
  }
  return isConnected;
}

bool fu::MonoSock::disconnect()
{
  if (!getIsConnected())
  {
    return false;
  }
  std::cout << "disconnect" << tag << std::endl;
  {
    std::unique_lock<std::mutex> lock(mutexGlobal);
    isIdle = false;
    isConnected = false;
  }
  {
    std::unique_lock<std::mutex> lock(mutexRead);
    for (auto i = receives.begin(); i != receives.end(); i++)
    {
      free(i->base);
    }
    receives.clear();
    if (isReading)
    {
      conditionRead.notify_one();
    }
  }
  {
    std::unique_lock<std::mutex> lock(mutexWrite);
    for (auto i = sends.begin(); i != sends.end(); i++)
    {
      free(i->base);
    }
    sends.clear();
    if (isWriting)
    {
      conditionWrite.notify_one();
    }
  }
  for (auto i = tcpAccepts.begin(); i != tcpAccepts.end(); i++)
  {
    uv_close((uv_handle_t*)(*i), [](uv_handle_t* handle){static_cast<MonoSock*>(handle->data)->close(handle);});
  }
  tcpAccepts.clear();
  if (tcp != nullptr)
  {
    uv_close((uv_handle_t*)tcp, [](uv_handle_t* handle){static_cast<MonoSock*>(handle->data)->close(handle);});
    tcp = nullptr;
  }
  if (connection != nullptr)
  {
    delete connection;
    connection = nullptr;
  }
  if (loop != nullptr)
  {
    uv_stop(loop);
    for (;;)
    {
      if (uv_loop_alive(loop) == 0)
      {
        if (uv_loop_close(loop) != UV_EBUSY)
        {
          break;
        }
      }
      uv_walk(loop, [](uv_handle_t* handle, void* data){static_cast<MonoSock*>(handle->data)->walk(handle);}, nullptr);
      bool isRunning;
      {
        std::unique_lock<std::mutex> lock(mutexGlobal);
        isRunning = this->isRunning;
      }
      if (!isRunning)
      {
        int status = uv_run(loop, UV_RUN_NOWAIT);
      }
    }
    delete loop;
    loop = nullptr;
  }
  disconnectCallback(this);
  return true;
}

bool fu::MonoSock::connect()
{
  if (getIsConnected())
  {
    return false;
  }
  if (role != Role::Client)
  {
    return false;
  }
  {
    std::unique_lock<std::mutex> lock(mutexGlobal);
    isConnected = true;
  }
  bool success = true;
  int result = 0;
  loop = new uv_loop_t();
  result = uv_loop_init(loop);
  if (result != 0)
  {
    success = false;
    goto MonoSock_connect_END;
  }
  struct sockaddr address;
  result = uv_ip4_addr(peer.c_str(), port, (struct sockaddr_in*)(&address));
  if (result != 0)
  {
    success = false;
    goto MonoSock_connect_END;
  }
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
    {
      std::unique_lock<std::mutex> lock(mutexGlobal);
      isConnected = false;
    }
    disconnect();
    return false;
  }
  std::cout << "connect" << std::endl;
  threads->enqueue(0, "Connect", [this](bool* result){*result = true;this->run();return 0;});
  return true;
}

bool fu::MonoSock::listen()
{
  if (getIsConnected())
  {
    return false;
  }
  if (role != Role::Listener)
  {
    return false;
  }
  {
    std::unique_lock<std::mutex> lock(mutexGlobal);
    isConnected = true;
   }
  peer = "0.0.0.0";
  bool success = true;
  int result = 0;
  loop = new uv_loop_t();
  result = uv_loop_init(loop);
  if (result != 0)
  {
    success = false;
    goto MonoSock_listen_END;
  }
  struct sockaddr address;
  result = uv_ip4_addr(peer.c_str(), port, (struct sockaddr_in*)(&address));
  if (result != 0)
  {
    success = false;
    goto MonoSock_listen_END;
  }
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
    {
      std::unique_lock<std::mutex> lock(mutexGlobal);
      isConnected = false;
    }
    disconnect();
    return false;
  }
  std::cout << "listen" << std::endl;
  threads->enqueue(0, "Listen", [this](bool* result){*result = true;this->run();return 0;});
  return true;
}

uv_buf_t fu::MonoSock::allocate(size_t length)
{
  if (length == 0)
  {
    return uv_buf_init(nullptr, 0);
  }
  if (parent != nullptr)
  {
    return parent->allocate(length);
  }
  return uv_buf_init(static_cast<char*>(malloc(length)), length);
}

void fu::MonoSock::run()
{
  if (loop == nullptr)
  {
    return;
  }
  bool isRunning = true;
  {
    std::unique_lock<std::mutex> lock(mutexGlobal);
    this->isRunning = true;
    if (!isConnected)
    {
      isRunning = false;
    }
  }
  while (isRunning)
  {
    int status = uv_run(loop, UV_RUN_NOWAIT);
    {
      std::unique_lock<std::mutex> lock(mutexGlobal);
      if (!isConnected)
      {
        isRunning = false;
      }
    }
  }
  std::cout << "ran" << tag << std::endl;
  {
    std::unique_lock<std::mutex> lock(mutexGlobal);
    this->isRunning = false;
  }
}

void fu::MonoSock::walk(uv_handle_t* handle)
{
  if (uv_is_closing(handle) != 0)
  {
    return;
  }
  std::cout << "walk" << std::endl;
  uv_close(handle, [](uv_handle_t* handle){static_cast<MonoSock*>(handle->data)->close(handle);});
}

void fu::MonoSock::close(uv_handle_t* handle)
{
  std::cout << "close" << std::endl;
  switch (protocol)
  {
    case Protocol::TCP:
      uv_tcp_t* tcp = (uv_tcp_t*)handle;
      delete tcp;
      break;
  }
}

bool fu::MonoSock::wrote(uv_write_t* writer, int status)
{
  bool success = false;
  {
    std::unique_lock<std::mutex> lock(mutexWrite);
    for (auto i = writers.begin(); i != writers.end(); i++)
    {
      if (i->first == writer)
      {
        std::cout << "wrote" << std::endl;
        success = true;
        delete i->first;
        free(i->second.base);
        writers.erase(i);
        break;
      }
    }
  }
  if (status != 0)
  {
    return false;
  }
  if (!success)
  {
    return false;
  }
  return true;
}

bool fu::MonoSock::accept(int status)
{
  std::cout << "accept" << std::endl;
  if (status != 0)
  {
    return false;
  }
  uv_stream_t* acceptance = nullptr;
  switch (protocol)
  {
    case Protocol::TCP:
      acceptance = (uv_stream_t*)(new uv_tcp_t());
      uv_tcp_init(loop, (uv_tcp_t*)acceptance);
      break;
  }
  if (acceptance == nullptr)
  {
    return false;
  }
  acceptance->data = this;
  int result = uv_accept((uv_stream_t*)tcp, acceptance);
  if (result != 0)
  {
    uv_close((uv_handle_t*)acceptance, [](uv_handle_t* handle){static_cast<MonoSock*>(handle->data)->close(handle);});
    return false;
  }
  struct sockaddr_storage address;
  int length = sizeof(struct sockaddr_storage);
  memset(&address, 0, sizeof(struct sockaddr_storage));
  switch (protocol)
  {
  case Protocol::TCP:
    result = uv_tcp_getpeername((uv_tcp_t*)acceptance, (struct sockaddr*)(&address), &length);
    break;
  default:
    result = -1;
    break;
  }
  if (result != 0)
  {
    uv_close((uv_handle_t*)acceptance, [](uv_handle_t* handle){static_cast<MonoSock*>(handle->data)->close(handle);});
    return false;
  }
  bool success = establish(&address, status);
  MonoSock* child = nullptr;
  if (success)
  {
    if (owner != nullptr)
    {
      std::cout << "child" << std::endl;
      int i = 0;
      child = new MonoSock(Role::Server, protocol, port);
      child->isConnected = true;
      child->owner = owner;
      child->peer = peer;
      child->parent = this;
      child->connectCallback = connectCallback;
      child->disconnectCallback = disconnectCallback;
      child->receiveCallback = receiveCallback;
      for (;;)
      {
        std::cout << i << std::endl;
        i++;
        if (owner->push(tag+i, child))
        {
          break;
        }
        else
        {
          std::cout << "fail" << std::endl;
          if (child->owner == nullptr)
          {
            success = false;
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
  if (child != nullptr)
  {
    if (success)
    {
      acceptance->data = child;
    }
    else
    {
      acceptance->data = this;
      delete child;
      child = nullptr;
    }
  }
  if (success)
  {
    int result = 0;
    switch (protocol)
    {
    case Protocol::TCP:
      result = uv_read_start(acceptance,
                             [](uv_handle_t* handle, size_t length, uv_buf_t* buffer){*buffer = static_cast<MonoSock*>(handle->data)->allocate(length);},
                             [](uv_stream_t* stream, ssize_t length, const uv_buf_t* buffer){static_cast<MonoSock*>(stream->data)->receive(reinterpret_cast<const uint8_t*>(buffer->base), length);free(buffer->base);});
      break;
    default:
      result = -1;
      break;
    }
    if (result != 0)
    {
      return false;
    }
  }
  if (!success)
  {
    acceptance->data = this;
    delete child;
    child = nullptr;
    uv_close((uv_handle_t*)acceptance, [](uv_handle_t* handle){static_cast<MonoSock*>(handle->data)->close(handle);});
    return false;
  }
  children.push_back(child);
  switch (protocol)
  {
  case Protocol::TCP:
    child->index = tcpAccepts.size();
    tcpAccepts.push_back((uv_tcp_t*)acceptance);
    break;
  }
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
  if (connectCallback(this, address))
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
  if (role == Role::Listener)
  {
    return false;
  }
  bool isIdle;
  {
    std::unique_lock<std::mutex> lock(mutexGlobal);
    if ((isReading) || (isWriting))
    {
      isIdle = true;
    }
    else
    {
      isIdle = this->isIdle;
      if (!isIdle)
      {
        this->isIdle = true;
      }
    }
  }
  if (isIdle)
  {
    return false;
  }
  threads->enqueue(1, "Read", [this](bool* result){*result = true;this->read();return 0;});
  threads->enqueue(2, "Write", [this](bool* result){*result = true;this->write();return 0;});
  std::cout << "idle" << std::endl;
  if (role == Role::Client)
  {
    int result = 0;
    switch (protocol)
    {
    case Protocol::TCP:
      result = uv_read_start((uv_stream_t*)tcp,
                             [](uv_handle_t* handle, size_t length, uv_buf_t* buffer){*buffer = static_cast<MonoSock*>(handle->data)->allocate(length);},
                             [](uv_stream_t* stream, ssize_t length, const uv_buf_t* buffer){static_cast<MonoSock*>(stream->data)->receive(reinterpret_cast<const uint8_t*>(buffer->base), length);free(buffer->base);});
      break;
    default:
      result = -1;
      break;
    }
    if (result != 0)
    {
      return false;
    }
  }
  return true;
}

void fu::MonoSock::read()
{
  std::cout << "read" << std::endl;
  bool isReading = true;
  {
    std::unique_lock<std::mutex> lock(mutexGlobal);
    this->isReading = true;
  }
  while (isReading)
  {
    std::vector<uv_buf_t> buffers;
    {
      std::unique_lock<std::mutex> lock(mutexRead);
      if (getIsIdle())
      {
        if (receives.empty())
        {
          conditionRead.wait(lock);
          if (!getIsIdle())
          {
            isReading = false;
          }
          else
          {
            buffers = receives;
            receives.clear();
          }
        }
        else
        {
          buffers = receives;
          receives.clear();
        }
      }
      else
      {
        isReading = false;
      }
    }
    while (!buffers.empty())
    {
      if (isReading)
      {
        receiveCallback(this, reinterpret_cast<const uint8_t*>(buffers.front().base), buffers.front().len);
      }
      free(buffers.front().base);
      buffers.erase(buffers.begin());
    }
  }
  {
    std::unique_lock<std::mutex> lock(mutexGlobal);
    this->isReading = false;
  }
}

void fu::MonoSock::write()
{
  std::cout << "write" << std::endl;
  bool isWriting = true;
  {
    std::unique_lock<std::mutex> lock(mutexGlobal);
    this->isWriting = true;
  }
  while (isWriting)
  {
    std::vector<uv_buf_t> buffers;
    {
      std::unique_lock<std::mutex> lock(mutexWrite);
      if (getIsIdle())
      {
        if (sends.empty())
        {
          conditionWrite.wait(lock);
          if (!getIsIdle())
          {
            isWriting = false;
          }
          else
          {
            buffers = sends;
            sends.clear();
          }
        }
        else
        {
          buffers = sends;
          sends.clear();
        }
      }
      else
      {
        isWriting = false;
      }
    }
    while (!buffers.empty())
    {
      if (isWriting)
      {
        int result = -1;
        uv_write_t* writer = new uv_write_t();
        {
          std::unique_lock<std::mutex> lock(mutexWrite);
          writers.push_back(std::pair<uv_write_t*, uv_buf_t>(writer, buffers.front()));
        }
        switch (protocol)
        {
        case Protocol::TCP:
          if (tcp != nullptr)
          {
            result = uv_write(writer, (uv_stream_t*)tcp, &writers.back().second, 1, [](uv_write_t* writer, int status){static_cast<MonoSock*>(writer->handle->data)->wrote(writer, status);});
          }
          else
          {
            if ((parent != nullptr) && (index >= 0))
            {
              result = uv_write(writer, (uv_stream_t*)parent->tcpAccepts[index], &writers.back().second, 1, [](uv_write_t* writer, int status){static_cast<MonoSock*>(writer->handle->data)->wrote(writer, status);});
            }
          }
          break;
        }
        if (result != 0)
        {
          std::unique_lock<std::mutex> lock(mutexWrite);
          for (auto i = writers.begin(); i != writers.end(); i++)
          {
            if (i->first == writer)
            {
              delete i->first;
              free(i->second.base);
              writers.erase(i);
              break;
            }
          }
        }
      }
      buffers.erase(buffers.begin());
    }
  }
  {
    std::unique_lock<std::mutex> lock(mutexGlobal);
    this->isWriting = false;
  }
}

bool fu::MonoSock::receive(const uint8_t* bytes, ssize_t length)
{
  if (length <= 0)
  {
    if (owner != nullptr)
    {
      owner->handle([this](){this->disconnect();});
    }
    return false;
  }
  uv_buf_t buffer = allocate(length);
  memcpy(buffer.base, reinterpret_cast<const char*>(bytes), length);
  {
    std::unique_lock<std::mutex> lock(mutexRead);
    receives.push_back(buffer);
    conditionRead.notify_all();
  }
  return true;
}

bool fu::MonoSock::send(const uint8_t* bytes, size_t length)
{
  uv_buf_t buffer = allocate(length);
  memcpy(buffer.base, reinterpret_cast<const char*>(bytes), length);
  {
    std::unique_lock<std::mutex> lock(mutexWrite);
    sends.push_back(buffer);
    conditionWrite.notify_all();
  }
  return true;
}

void fu::MonoSock::kill()
{
  ThreadPool* pool = new ThreadPool(1);
  pool->enqueue(0, "KillMono", [=](bool* result){*result = false;delete this;return 0;});
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
