
// Author: Pierce Brooks

#include <fu/MonoSock.hpp>

fu::MonoSock::MonoSock(Role role) :
  role(role)
{
  
} 

fu::MonoSock::~MonoSock()
{
  
} 

bool fu::MonoSock::send(const uint8_t* bytes, size_t length)
{
  return true;
}  

void fu::MonoSock::setConnectCallback(ConnectCallback callback)
{
  connectCallback = callback;
}

void fu::MonoSock::setReceiveCallback(ReceiveCallback callback)
{
  receiveCallback = callback;
}
