
// Author: Pierce Brooks

#ifndef FU_MONO_SOCK_HPP
#define FU_MONO_SOCK_HPP

#include <functional>
#include <cstdint>

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
      MonoSock(Role role);
      virtual ~MonoSock();
      bool send(const uint8_t* bytes, size_t length);
      void setConnectCallback(ConnectCallback callback);
      void setReceiveCallback(ReceiveCallback callback);
    private:
      friend class PolySock;
      Role role;
      ConnectCallback connectCallback;
      ReceiveCallback receiveCallback;
      int tag;
  };
}

#endif
