
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
      void run();
      void wait();
      bool start();
      bool stop();
      bool push(int tag, MonoSock* sock);
      MonoSock* pop(int tag);
      bool pop(MonoSock* sock);
      void pop();
    private:
      bool start(MonoSock* sock);
      bool stop(MonoSock* sock);
      std::map<int, MonoSock*> socks;
      std::condition_variable condition;
      std::mutex mutex;
      bool isStarted;
      ThreadPool* threads;
  };
}

#endif
