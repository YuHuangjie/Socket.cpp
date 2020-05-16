//
//  Socket.h
//  SocketServer
//
//  Created by Kay Makowsky on 14.06.16.
//  Copyright © 2016 Kay Makowsky. All rights reserved.
//

#ifndef SOCKET_H
#define SOCKET_H

#include <vector>
#include <WinSock2.h>
#include <ws2tcpip.h>

class __declspec(dllexport) Socket
{
public:
	SOCKET sock;
    char address[NI_MAXHOST];
	uint16_t port;
    struct addrinfo address_info;

    Socket();
    Socket(int domain,int type,int protocol);
    int bind(const char *ip, const uint16_t port);
    int connect(const char *ip, const uint16_t port);
    int listen(int max_queue);
    Socket* accept();

	// TCP read/write
    int write(const void *buf, const size_t sz);
    int read(void *buf, const size_t sz);
    int safe_read(void *buf, const size_t sz, int seconds);

	// UDP read/write
    int write_to(const void *buf, const size_t sz, const char *ip, const uint16_t port);
    int read_from(void *buf, const size_t sz);

    int setopt(int level, int optname, const void* optval, int len);
    int getopt(int level, int optname, void* optval, int *len);
    int set_blocking();
    int set_non_blocking();
    int shutdown(int how);
    void close();

	static bool initialize();
	static void terminate();
    static int select(std::vector<Socket> *reads, std::vector<Socket> *writes, 
		std::vector<Socket> *exceptions, int seconds);
};

#endif /* SOCKET_H */