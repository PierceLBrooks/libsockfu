
// Author: Pierce Brooks

#include <fu/PolySock.hpp>
#include <iostream>

fu::PolySock::PolySock() :
  isStarted(false)
{
  threads = new ThreadPool(4);
}

fu::PolySock::~PolySock()
{
  stop();
  {
    std::unique_lock<std::mutex> lock(mutex);
    for (auto i = socks.begin(); i != socks.end(); i++)
    {
      i->second->kill();
    }
    socks.clear();
  }
  if (threads != nullptr)
  {
    threads->kill();
    delete threads;
    threads = nullptr;
  }
}

bool fu::PolySock::getIsStarted() const
{
  bool isStarted;
  {
    std::unique_lock<std::mutex> lock(mutex);
    isStarted = this->isStarted;
  }
  return isStarted;
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
    std::cout << "listener" << std::endl;
    success = sock->listen();
    break;
  case MonoSock::Role::Client:
    std::cout << "client" << std::endl;
    success = sock->connect();
    break;
  case MonoSock::Role::Server:
    std::cout << "server" << std::endl;
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
  if (getIsStarted())
  {
    return false;
  }
  isStarted = true;
  threads->enqueue(0, "Run", [](bool* result, PolySock* that){*result = true;that->run();return 0;}, this);
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
  bool isStarted;
  {
    std::unique_lock<std::mutex> lock(mutex);
    isStarted = this->isStarted;
    if (isStarted)
    {
      condition.notify_all();
      this->isStarted = false;
    }
  }
  if (!isStarted)
  {
    return false;
  }
  for (auto i = socks.begin(); i != socks.end(); i++)
  {
    stop(i->second);
  }
  return true;
}

void fu::PolySock::wait()
{
  bool isWaiting = true;
  while (isWaiting)
  {
    std::unique_lock<std::mutex> lock(mutex);
    if (isStarted)
    {
      condition.wait(lock);
      if (!isStarted)
      {
        isWaiting = false;
      }
    }
    else
    {
      isWaiting = false;
    }
  }
}

void fu::PolySock::run()
{
  bool isRunning = true;
  while (isRunning)
  {
    bool isEmpty = false;
    {
      std::unique_lock<std::mutex> lock(mutex);
      if (socks.empty())
      {
        isEmpty = true;
      }
    }
    if (isEmpty)
    {
      isRunning = false;
    }
    else
    {
      std::chrono::milliseconds timespan(100);
      std::this_thread::sleep_for(timespan);
    }
  }
  stop();
}

bool fu::PolySock::push(int tag, MonoSock* sock)
{
  std::cout << "push " << tag << std::endl;
  if (sock == nullptr)
  {
    return false;
  }
  std::unique_lock<std::mutex> lock(mutex);
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
  std::unique_lock<std::mutex> lock(mutex);
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
  std::unique_lock<std::mutex> lock(mutex);
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
  std::unique_lock<std::mutex> lock(mutex);
  while (!socks.empty())
  {
    MonoSock* sock = socks.begin()->second;
    if (sock != nullptr)
    {
      sock->owner = nullptr;
      stop(sock);
      sock->kill();
    }
    socks.erase(socks.begin());
  }
  condition.notify_all();
}

void fu::PolySock::kill()
{
  ThreadPool* pool = new ThreadPool(1);
  pool->enqueue(0, "Kill", [=](bool* result){*result = false;delete this;return 0;});
}

void fu::PolySock::handle(Handler handler)
{
  threads->enqueue(1, "Handle", [=](bool* result){*result = true;handler();return 0;});
}
