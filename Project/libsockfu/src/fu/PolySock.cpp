
// Author: Pierce Brooks

#include <fu/PolySock.hpp>

fu::PolySock::PolySock()
{
  
}

fu::PolySock::~PolySock()
{
  
}

bool fu::PolySock::start()
{
  return true;
}

bool fu::PolySock::stop()
{
  return true;
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

bool fu::PolySock::pull(MonoSock* sock)
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

fu::MonoSock* fu::PolySock::pull(int tag)
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

