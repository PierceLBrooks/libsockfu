
// Author: Pierce Brooks

#include <fu/PolySock.hpp>
#include <fu/MonoSock.hpp>
#include <iostream>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <csignal>
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

static fu::PolySock* poly = nullptr;

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
    for (int i = 0; i != 1; i++) {
        std::cout << i << std::endl;
        poly = new fu::PolySock();
        fu::MonoSock* monoCli = new fu::MonoSock(fu::MonoSock::Role::Client, fu::MonoSock::Protocol::TCP, 9009);
        fu::MonoSock* monoSer = new fu::MonoSock(fu::MonoSock::Role::Listener, fu::MonoSock::Protocol::TCP, 9009);
        monoSer->setDisconnectCallback([](fu::MonoSock* sock){std::cout << "disconnect" << std::endl;});
        monoCli->setDisconnectCallback([](fu::MonoSock* sock){std::cout << "disconnect" << std::endl;});
        monoCli->setConnectCallback([](fu::MonoSock* sock, const std::string& address){std::string str = "Hello, world!";sock->send(reinterpret_cast<const uint8_t*>(str.c_str()), str.length());return true;});
        monoCli->setReceiveCallback([](fu::MonoSock* sock, const uint8_t* bytes, size_t length){std::cout << std::string(reinterpret_cast<const char*>(bytes), length) << std::endl;poly->pop();});
        monoSer->setReceiveCallback([](fu::MonoSock* sock, const uint8_t* bytes, size_t length){std::cout << std::string(reinterpret_cast<const char*>(bytes), length) << std::endl;sock->send(bytes, length);});
        poly->push(1, monoSer);
        poly->push(2, monoCli);
        monoCli = nullptr;
        monoSer = nullptr;
        signal(SIGINT, [](int signum){poly->pop();});
        signal(SIGTERM, [](int signum){poly->pop();});
        if (!poly->start())
        {
            return 2;
        }
        std::cout << "Running..." << std::endl;
        poly->wait();
        //poly->pop(monoCli);
        //poly->pop(monoSer);
        //delete monoCli;
        //delete monoSer;
        poly->kill();
        poly = nullptr;
        std::chrono::milliseconds timespan(2000);
        std::this_thread::sleep_for(timespan);
    }
    return 0;
}
