
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
      bool getIsStarted() const;
      void run();
      void wait();
      bool start();
      bool stop();
      bool push(int tag, MonoSock* sock);
      MonoSock* pop(int tag);
      bool pop(MonoSock* sock);
      void pop();
      void kill();
    private:
      bool start(MonoSock* sock);
      bool stop(MonoSock* sock);
      std::map<int, MonoSock*> socks;
      std::condition_variable condition;
      mutable std::mutex mutex;
      bool isStarted;
      ThreadPool* threads;
    protected:
      virtual ~PolySock();
  };
}

#endif
