
// Author: Pierce Brooks

#include <fu/PolySock.hpp>
#include <fu/MonoSock.hpp>
#include <iostream>
#include <string>
#include <cstdlib>
#include <cstdio>
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

int main()
{
#ifdef _WIN32
  WSADATA wsaData;
  int iResult;
  iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
  if (iResult != 0) {
    printf("WSAStartup failed: %d\n", iResult);
    return 1;
  }
#endif
  fu::PolySock* poly = new fu::PolySock();
  fu::MonoSock* monoCli = new fu::MonoSock(fu::MonoSock::Role::Client, fu::MonoSock::Protocol::TCP, 9009);
  fu::MonoSock* monoSer = new fu::MonoSock(fu::MonoSock::Role::Server, fu::MonoSock::Protocol::TCP, 9009);
  monoCli->setConnectCallback([&](){std::string str = "Hello, world!";monoCli->send(reinterpret_cast<const uint8_t*>(str.c_str()), str.length());poly->pop();});
  monoSer->setReceiveCallback([&](const uint8_t* bytes, size_t length){std::cout << std::string(reinterpret_cast<const char*>(bytes), length) << std::endl;poly->pop();});
  poly->push(1, monoCli);
  poly->push(2, monoSer);
  poly->start();
  std::cout << "Running..." << std::endl;
  poly->wait();
  //poly->pop(monoCli);
  //poly->pop(monoSer);
  //delete monoCli;
  //delete monoSer;
  delete poly;
  return 0;
}
