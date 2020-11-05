
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
  case MonoSock::Role::Server:
    success = sock->listen();
    break;
  case MonoSock::Role::Client:
    success = sock->connect();
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
  threads->enqueue(0, [](PolySock* that){that->run();}, this);
  for (auto i = socks.begin(); i != socks.end(); i++)
  {
    if (!start(i->second))
    {
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
    std::chrono::milliseconds timespan(100);
    std::unique_lock<std::mutex> lock(mutex);
    condition.wait_for(lock, timespan, [this]{return !this->isStarted;});
    if (!isStarted)
    {
      break;
    }
    std::this_thread::sleep_for(timespan);
  }
}

void fu::PolySock::run()
{
  while (isStarted)
  {
    std::chrono::milliseconds timespan(100);
    if (socks.empty())
    {
      stop();
      break;
    }
    std::this_thread::sleep_for(timespan);
  }
}

bool fu::PolySock::push(int tag, MonoSock* sock)
{
  auto i = socks.find(tag);
  if (i != socks.end())
  {
    return false;
  }
  sock->tag = tag;
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
  socks.erase(i);
  return sock;
}

void fu::PolySock::pop()
{
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
