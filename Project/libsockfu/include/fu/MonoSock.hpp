
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
      typedef std::function<void(MonoSock* sock)> DisconnectCallback;
      typedef std::function<bool(MonoSock* sock, const std::string& address)> ConnectCallback;
      typedef std::function<void(MonoSock* sock, const uint8_t* bytes, size_t length)> ReceiveCallback;
      enum Role
      {
        Server,
        Client,
        Listener,
      };
      enum Protocol
      {
        TCP,
        UDP,
      };
      MonoSock(Role role, Protocol protocol, int port);
      bool getIsConnected() const;
      bool getIsIdle() const;
      bool disconnect();
      bool connect();
      bool listen();
      bool receive(const uint8_t* bytes, ssize_t length);
      bool send(const uint8_t* bytes, size_t length);
      void kill();
      void setDisconnectCallback(DisconnectCallback callback);
      void setConnectCallback(ConnectCallback callback);
      void setReceiveCallback(ReceiveCallback callback);
    private:
      friend class PolySock;
      void run();
      void walk(uv_handle_t* handle);
      void close(uv_handle_t* handle);
      bool wrote(uv_write_t* writer, int status);
      bool accept(int status);
      bool establish(int status);
      bool establish(const std::string& address, int status);
      bool establish(const struct sockaddr_storage* address, int status);
      bool idle();
      void read();
      void write();
      Role role;
      Protocol protocol;
      int index;
      int tag;
      int port;
      bool isRunning;
      bool isReading;
      bool isWriting;
      bool isConnected;
      bool isIdle;
      uv_tcp_t* tcp;
      uv_loop_t* loop;
      uv_connect_t* connection;
      std::condition_variable conditionRead;
      std::condition_variable conditionWrite;
      mutable std::mutex mutexGlobal;
      mutable std::mutex mutexWrite;
      mutable std::mutex mutexRead;
    protected:
      virtual ~MonoSock();
      uv_buf_t allocate(size_t length);
      std::string peer;
      std::vector<MonoSock*> children;
      std::vector<uv_tcp_t*> tcpAccepts;
      std::vector<std::pair<uv_write_t*, uv_buf_t>> writers;
      std::vector<uv_buf_t> receives;
      std::vector<uv_buf_t> sends;
      MonoSock* parent;
      PolySock* owner;
      ThreadPool* threads;
      DisconnectCallback disconnectCallback;
      ConnectCallback connectCallback;
      ReceiveCallback receiveCallback;
  };
}

#endif
