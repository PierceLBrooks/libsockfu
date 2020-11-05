
// Author: Pierce Brooks

#include <fu/PolySock.hpp>
#include <fu/MonoSock.hpp>
#include <iostream>
#include <string>

int main()
{
  fu::PolySock* poly = new fu::PolySock();
  fu::MonoSock* monoCli = new fu::MonoSock(fu::MonoSock::Role::Client);
  fu::MonoSock* monoSer = new fu::MonoSock(fu::MonoSock::Role::Server);
  monoCli->setConnectCallback([&](){std::string str = "Hello, world!";monoCli->send(reinterpret_cast<const uint8_t*>(str.c_str()), str.length());});
  monoSer->setReceiveCallback([&](const uint8_t* bytes, size_t length){std::cout << std::string(reinterpret_cast<const char*>(bytes), length) << std::endl;});
  poly->push(1, monoCli);
  poly->push(2, monoSer);
  poly->start();
  poly->stop();
  poly->pull(monoCli);
  poly->pull(monoSer);
  delete monoCli;
  delete monoSer;
  delete poly;
  return 0;
}
