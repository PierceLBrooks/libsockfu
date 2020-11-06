
// Author: Pierce Brooks

#include <fu/PolySock.hpp>
#include <iostream>

fu::PolySock::PolySock() :
  isStarted(false)
{
  threads = new ThreadPool(1);
}

fu::PolySock::~PolySock()
{
  stop();
  for (auto i = socks.begin(); i != socks.end(); i++)
  {
    delete i->second;
  }
  socks.clear();
  delete threads;
}

bool fu::PolySock::start(MonoSock* sock)
{
  if (sock == nullptr)
  {
    return false;
  }
  bool success = false;
  switch (sock->role)
  {
  case MonoSock::Role::Listener:
    success = sock->listen();
    break;
  case MonoSock::Role::Client:
    success = sock->connect();
    break;
  case MonoSock::Role::Server:
    success = sock->idle();
    break;
  }
  return success;
}

bool fu::PolySock::stop(MonoSock* sock)
{
  if (sock == nullptr)
  {
    return false;
  }
  return sock->disconnect();
}

bool fu::PolySock::start()
{
  if (isStarted)
  {
    return false;
  }
  isStarted = true;
  threads->enqueue(0, "Run", [](PolySock* that){that->run();}, this);
  for (auto i = socks.begin(); i != socks.end(); i++)
  {
    if (i->second->role == MonoSock::Role::Server)
    {
      continue;
    }
    if (!start(i->second))
    {
      std::cout << i->second->tag << std::endl;
      return false;
    }
  }
  return true;
}

bool fu::PolySock::stop()
{
  condition.notify_all();
  if (!isStarted)
  {
    return false;
  }
  isStarted = false;
  for (auto i = socks.begin(); i != socks.end(); i++)
  {
    stop(i->second);
  }
  return true;
}

void fu::PolySock::wait()
{
  while (isStarted)
  {
    //std::cout << "wait" << std::endl;
    std::chrono::milliseconds timespan(100);
    std::unique_lock<std::mutex> lock(mutex);
    condition.wait(lock, [this]{return !this->isStarted;});
    if (!isStarted)
    {
      break;
    }
    //std::this_thread::sleep_for(timespan);
  }
}

void fu::PolySock::run()
{
  while (isStarted)
  {
    //std::cout << "run" << std::endl;
    std::chrono::milliseconds timespan(100);
    if (socks.empty())
    {
      stop();
      break;
    }
    //std::this_thread::sleep_for(timespan);
  }
}

bool fu::PolySock::push(int tag, MonoSock* sock)
{
  std::cout << "push" << std::endl;
  if (sock == nullptr)
  {
    return false;
  }
  auto i = socks.find(tag);
  if (i != socks.end())
  {
    return false;
  }
  sock->tag = tag;
  sock->owner = this;
  if (isStarted)
  {
    if (!start(sock))
    {
      sock->owner = nullptr;
      return false;
    }
  }
  socks[tag] = sock;
  return true;
}

bool fu::PolySock::pop(MonoSock* sock)
{
  if (sock == nullptr)
  {
    return false;
  }
  auto i = socks.find(sock->tag);
  if (i == socks.end())
  {
    return false;
  }
  i->second->owner = nullptr;
  socks.erase(i);
  return true;
}

fu::MonoSock* fu::PolySock::pop(int tag)
{
  auto i = socks.find(tag);
  if (i == socks.end())
  {
    return nullptr;
  }
  MonoSock* sock = i->second;
  sock->owner = nullptr;
  socks.erase(i);
  return sock;
}

void fu::PolySock::pop()
{
  std::cout << "pop" << std::endl;
  condition.notify_all();
  while (!socks.empty())
  {
    MonoSock* sock = pop(socks.begin()->first);
    if (sock != nullptr)
    {
      stop(sock);
      delete sock;
    }
  }
}
