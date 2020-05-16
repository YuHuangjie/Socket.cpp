//
//  Socket.cpp
//  SocketServer
//
//  Created by Kay Makowsky on 15.06.16.
//  Copyright © 2016 Kay Makowsky. All rights reserved.
//
#undef UNICODE

#include <iostream>
#include <cstdlib>
#include <stdexcept>
#include <sstream>

#include "Socket.h"
#include <windows.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma warning( disable : 4267 )	// conversion from size_t to int

using namespace std;

static string GetErr(LPCTSTR lpszFunction)
{
	// Retrieve the system error message for the last-error code

	LPVOID lpMsgBuf;
	LPVOID lpDisplayBuf;
	DWORD dw = WSAGetLastError();
	ostringstream oss;

	FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)&lpMsgBuf,
		0, NULL);

	// Display the error message and exit the process

	lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
		(lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR));
	oss << lpszFunction << " failed with error " << dw << ": " << (LPSTR)lpMsgBuf;

	LocalFree(lpMsgBuf);
	LocalFree(lpDisplayBuf);
	return oss.str();
}

Socket::Socket()
{
	sock = INVALID_SOCKET;
	address[0] = '\0';
	port = 0;
	memset(&address_info, 0, sizeof address_info);
}

Socket::Socket(int domain, int type, int protocol)
{
    memset(&address_info, 0, sizeof address_info);
	address_info.ai_family = domain;
	address_info.ai_socktype = type;
	address_info.ai_protocol = protocol;

    sock = socket(domain, type , protocol);
    if (sock == INVALID_SOCKET) {
		throw runtime_error(GetErr(TEXT("socket")));
    }
    
	port = 0;
    address[0] = '\0';
}

int Socket::bind(const char *ip, const uint16_t port)
{
	sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (strcmp(ip, "any") || strcmp(ip, "ANY")) {
		addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	}
	else {
		int res = inet_pton(addr.sin_family, ip, &addr.sin_addr.S_un.S_addr);

		if (res == 0) {
			cerr << "inet_pton: invalid address " << ip << endl;
			return SOCKET_ERROR;
		}
		else if (res == -1) {
			cerr << GetErr(TEXT("bind")) << endl;
			return SOCKET_ERROR;
		}
	}

	if (::bind(sock, (SOCKADDR*)(&addr), sizeof(addr)) != 0) {
		cerr << GetErr(TEXT("bind")) << endl;
		return SOCKET_ERROR;
	}

	strcpy_s(address, sizeof(address), ip);
	this->port = port;
	address_info.ai_addrlen = sizeof(addr);
	address_info.ai_family = addr.sin_family;
	address_info.ai_addr = (SOCKADDR*)&addr;

    return 0;
}

int Socket::connect(const char *ip, const uint16_t port)
{
    struct addrinfo *res;

	strcpy_s(address, sizeof(address), ip);
	this->port = port;

	// Resolve the server address and port
	if (getaddrinfo(ip, to_string(port).c_str(), &address_info, &res) != 0) {
		cerr << GetErr(TEXT("getaddrinfo")) << endl;
		return SOCKET_ERROR;
	}

	// Attempt to connect to an address until one succeeds
	for (struct addrinfo *ptr = res; ptr != NULL; ptr = ptr->ai_next) {

		// Create a SOCKET for connecting to server
		sock = socket(ptr->ai_family, ptr->ai_socktype,	ptr->ai_protocol);
		if (sock == INVALID_SOCKET) {
			cerr << GetErr(TEXT("socket")) << endl;
			return SOCKET_ERROR;
		}

		// Connect to server.
		if (::connect(sock, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR) {
			closesocket(sock);
			sock = INVALID_SOCKET;
			continue;
		}
		break;
	}

	freeaddrinfo(res);

	if (sock == INVALID_SOCKET) {
		cerr << GetErr(TEXT("connect")) << endl;
		return SOCKET_ERROR;
	}

    return 0;
}

int Socket::listen(int max_queue){
	if (::listen(sock, max_queue) == SOCKET_ERROR) {
		cerr << GetErr(TEXT("listen")) << endl;
		return SOCKET_ERROR;
	}
    return 0;
}

Socket* Socket::accept()
{
    struct sockaddr_storage their_addr;
    socklen_t addr_size = sizeof their_addr;

	// Accept a client socket
    SOCKET newsock = ::accept(sock, (struct sockaddr *)&their_addr, &addr_size);
    if (newsock == INVALID_SOCKET) {
		cerr << GetErr(TEXT("accept")) << endl;
		return nullptr;
    }

	// Create a Socket object
    Socket *newSocket = new Socket(address_info.ai_family,
		address_info.ai_socktype,address_info.ai_protocol);
    newSocket->sock = newsock;
    
	// Resolve client address
    char host[NI_MAXHOST];
	char port[NI_MAXSERV];
	if (getnameinfo((struct sockaddr *)&their_addr, sizeof(their_addr), host,
		sizeof(host), port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
		cerr << GetErr(TEXT("getnameinfo")) << endl;
		return nullptr;
	}

	newSocket->port = std::stoi(port);
	strncpy_s(newSocket->address, sizeof(newSocket->address), host, sizeof(host));
    newSocket->address_info.ai_family = their_addr.ss_family;
    newSocket->address_info.ai_addr = (struct sockaddr *)&their_addr;
    return newSocket;
}

int Socket::write(const void *buf, const size_t sz)
{
	int sent = send(sock, static_cast<const char*>(buf), sz, 0);

	if (sent == SOCKET_ERROR) {
		cerr << GetErr(TEXT("send")) << endl;
		return SOCKET_ERROR;
	}
    return sent;
}

int Socket::safe_read(void *buf, const size_t sz, int seconds){
	vector<Socket> reads({ *this });

	if (Socket::select(&reads, NULL, NULL, seconds) < 1) {
		// No data available
		return -1;
	}

	// receive data
    int res = recv(sock, static_cast<char*>(buf), sz, 0);

	if (res == 0) {
		cout << "Connection closing..." << endl;
	}
	else if (res < 0) {
		cerr << GetErr(TEXT("recv")) << endl;
		closesocket(sock);
		return SOCKET_ERROR;
	}
	return res;
}

int Socket::read(void *buf, const size_t sz){
	// receive data
	int res = recv(sock, static_cast<char*>(buf), sz, 0);

	if (res == 0) {
		cout << "Connection closing..." << endl;
	}
	else if (res < 0) {
		cerr << GetErr(TEXT("recv")) << endl;
		closesocket(sock);
		return SOCKET_ERROR;
	}

    return res;
}

int Socket::write_to(const void *buf, const size_t sz, const char *ip, const uint16_t port)
{
	sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	int res = inet_pton(addr.sin_family, ip, &addr.sin_addr.S_un.S_addr);

	if (res == 0) {
		cerr << "inet_pton: invalid address " << ip << endl;
		return SOCKET_ERROR;
	}
	else if (res == -1) {
		cerr << GetErr(TEXT("bind")) << endl;
		return SOCKET_ERROR;
	}
    
	int sent = sendto(sock, static_cast<const char*>(buf), sz, 0, (SOCKADDR*)(&addr),
		sizeof(addr));

    if (sent == SOCKET_ERROR) {
		cerr << GetErr(TEXT("sendto")) << endl;
		return SOCKET_ERROR;
    }
    return sent;
}

int Socket::read_from(void *buf, const size_t sz)
{
	sockaddr_in senderAddr;
	int senderAddrSize = sizeof(senderAddr);

    int recved = recvfrom(sock, static_cast<char*>(buf), sz, 0, 
		(SOCKADDR *)& senderAddr, &senderAddrSize);

	if (recved == 0) {
		cout << "Connection closing..." << endl;
	}
	else if (recved < 0) {
		cerr << GetErr(TEXT("recvfrom")) << endl;
		closesocket(sock);
		return SOCKET_ERROR;
	}

    return recved;
}

int Socket::setopt(int level, int optname, const void* optval, int len)
{
	if (::setsockopt(sock, level, optname, static_cast<const char*>(optval), len) == SOCKET_ERROR) {
		cerr << GetErr(TEXT("setsockopt")) << endl;
		return SOCKET_ERROR;
	}
    return 0;
}

int Socket::getopt(int level, int optname, void* optval, int *len)
{
	if (::getsockopt(sock, level, optname, static_cast<char*>(optval), len) == SOCKET_ERROR) {
		cerr << GetErr(TEXT("getsockopt")) << endl;
		return SOCKET_ERROR;
	}
	return 0;
}

int Socket::set_blocking()
{
	u_long iMode = 0;
	if (ioctlsocket(sock, FIONBIO, &iMode) == SOCKET_ERROR) {
		cerr << GetErr(TEXT("ioctlsocket")) << endl;
		return SOCKET_ERROR;
	}
	return 0;
}

int Socket::set_non_blocking()
{
	u_long iMode = 1;
	if (ioctlsocket(sock, FIONBIO, &iMode) == SOCKET_ERROR) {
		cerr << GetErr(TEXT("ioctlsocket")) << endl;
		return SOCKET_ERROR;
	}
    return 0;
}

int Socket::shutdown(int how){
	if (::shutdown(sock, how) == SOCKET_ERROR) {
		cerr << GetErr(TEXT("shutdown")) << endl;
		return SOCKET_ERROR;
	}
    return 0;
}

void Socket::close(){
	if (::closesocket(sock) == SOCKET_ERROR) {
		cerr << GetErr(TEXT("closesocket")) << endl;
	}
}

bool Socket::initialize()
{
	// Initialize Winsock
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		cerr << GetErr(TEXT("WSAStartup")) << endl;
		return false;
	}
	return true;
}

void Socket::terminate()
{
	WSACleanup();
}

int Socket::select(vector<Socket> *reads, vector<Socket> *writes, vector<Socket> *exceptions, int seconds) {
	struct timeval tv;
	fd_set readfds;
	fd_set writefds;
	fd_set exceptfds;

	tv.tv_sec = seconds;
	tv.tv_usec = 0;

	FD_ZERO(&readfds);

	FD_ZERO(&writefds);

	FD_ZERO(&exceptfds);

	if (reads != NULL) {
		for (int i = 0; i < reads->size(); i++) {
			SOCKET sock = reads->at(i).sock;
			FD_SET(sock, &readfds);
		}
	}
	if (writes != NULL) {
		for (int i = 0; i < writes->size(); i++) {
			SOCKET sock = writes->at(i).sock;
			FD_SET(sock, &writefds);
		}
	}
	if (exceptions != NULL) {
		for (int i = 0; i < exceptions->size(); i++) {
			SOCKET sock = exceptions->at(i).sock;
			FD_SET(sock, &exceptfds);
		}
	}

	// check readibility, writability and any error conditions
	int sn = ::select(0, &readfds, &writefds, &exceptfds, &tv);

	if (sn == SOCKET_ERROR) {
		cerr << GetErr(TEXT("select")) << endl;
		return SOCKET_ERROR;
    }

    if (reads != NULL) {
		for (auto it = reads->begin(); it != reads->end(); ) {
			if (!FD_ISSET(it->sock, &readfds)) {
				it = reads->erase(it);
			} else {
				++it;
			}
		}
    }
    if (writes != NULL) {
		for (auto it = writes->begin(); it != writes->end(); ) {
			if (!FD_ISSET(it->sock, &writefds)) {
				it = writes->erase(it);
			} else {
				++it;
			}
		}
    }
    if (exceptions != NULL) {
		for (auto it = exceptions->begin(); it != exceptions->end(); ) {
			if (!FD_ISSET(it->sock, &exceptfds)) {
				it = exceptions->erase(it);
			} else {
				++it;
			}
		}
    }
    return sn;
}