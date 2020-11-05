
// Author: Pierce Brooks

#ifndef FU_POLY_SOCK_HPP
#define FU_POLY_SOCK_HPP

#include <fu/MonoSock.hpp>
#include <map>

namespace fu
{
  class MonoSock;
  class PolySock
  {
    public:
      PolySock();
      virtual ~PolySock();
      bool start();
      bool stop();
      bool push(int tag, MonoSock* sock);
      MonoSock* pull(int tag);
      bool pull(MonoSock* sock);
    private:
      std::map<int, MonoSock*> socks;
  };
}

#endif
