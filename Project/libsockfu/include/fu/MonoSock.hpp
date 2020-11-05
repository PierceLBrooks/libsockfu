
// Author: Pierce Brooks

#ifndef FU_MONO_SOCK_HPP
#define FU_MONO_SOCK_HPP

#include <fu/ThreadPool.hpp>
#include <uv.h>
#include <functional>
#include <cstdint>
#include <vector>

namespace fu
{
  class PolySock;
  class MonoSock
  {
    public:
      typedef std::function<void()> ConnectCallback;
      typedef std::function<void(const uint8_t*, size_t)> ReceiveCallback;
      enum Role
      {
        Server,
        Client,
      };
      enum Protocol
      {
        TCP,
        UDP,
      };
      MonoSock(Role role, Protocol protocol, int port);
      virtual ~MonoSock();
      bool disconnect();
      bool connect();
      bool listen();
      bool send(const uint8_t* bytes, size_t length);
      void setConnectCallback(ConnectCallback callback);
      void setReceiveCallback(ReceiveCallback callback);
    private:
      friend class PolySock;
      bool accept(int status);
      bool establish(int status);
      Role role;
      Protocol protocol;
      ConnectCallback connectCallback;
      ReceiveCallback receiveCallback;
      int tag;
      int port;
      bool isConnected;
      uv_tcp_t* tcp;
      uv_loop_t* loop;
      uv_connect_t* connection;
      std::vector<uv_tcp_t*> tcpAccepts;
      ThreadPool* threads;
  };
}

#endif
