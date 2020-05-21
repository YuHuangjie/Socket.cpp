/*
 * socket.h -- definitions for the socket wrapper
 * 
 * Copyright (C) 2020 Yu Huangjie
 * 
 * License placeholder
 * 
 */

#include <stdexcept>
#include <cstring>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include "socket_in.h"

using namespace std;

static string get_err(int errnum)
{
	const size_t size = 256;
	char buf[size] = "Unknown error";
	char *e;

	e = strerror_r(errnum, buf, size);
	return e;
}

/*
 * Ask for a new TCP/IPv4 socket.
 */
socket_in::socket_in()
{
	int domain = AF_INET;
	int type = SOCK_STREAM;
	int protocol = 0;

	_sock = socket(domain, type, protocol);
	if (_sock < 0) {
		_last_err = get_err(errno);
		throw runtime_error(get_err(errno));
	}

	memset(&_addrinfo, 0, sizeof(_addrinfo));
	memset(&_addr, 0, sizeof(_addr));
	_addrinfo.ai_family = domain;
	_addrinfo.ai_socktype = type;
	_addrinfo.ai_protocol = protocol;
}

socket_in::~socket_in()
{
	close();
}

int socket_in::bind(const std::string &ip, const uint16_t port)
{
	sockaddr_in addr;
	int res;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	if (ip == "any" || ip == "ANY") {
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
	} else {
		res = inet_pton(addr.sin_family, ip.c_str(), &addr.sin_addr.s_addr);
		if (res == 0) {
			_last_err = "binding invalid IP address";
			return SOCKERR_INVALID_ADDR;
		} else if (res < 0) {
			_last_err = get_err(errno);
			return SOCKERR_INVALID_ADDR;
		}
	}

	if (::bind(_sock, (sockaddr*)&addr, sizeof(addr)) != 0) {
		_last_err = get_err(errno);
		return SOCKERR_FAULT;
	}

	_addr = *((sockaddr *)&addr);
	_addrinfo.ai_addrlen = sizeof(_addr);
	_addrinfo.ai_addr = &_addr;

   	return SOCKERR_OK;
}

int socket_in::connect(const std::string &ip, const uint16_t port)
{	
	int status;
	struct addrinfo *res;
	struct addrinfo *p;

	status = getaddrinfo(ip.c_str(), to_string(port).c_str(), &_addrinfo, 
			&res);
	if (status != 0) {
		_last_err = get_err(errno);
		return SOCKERR_INVALID_ADDR;
	}

	for (p = res; p; p = p->ai_next) {
		if (::connect(_sock, p->ai_addr, p->ai_addrlen) == 0)
			break;
		_last_err = get_err(errno);
	}
	if (!p) {
		return SOCKERR_FAULT;
	}
	_addr = *p->ai_addr;
	_addrinfo.ai_addrlen = p->ai_addrlen;
	_addrinfo.ai_addr = &_addr;
	freeaddrinfo(res);
	return SOCKERR_OK;
}

int socket_in::listen(int max_queue)
{
	int status;

	status = ::listen(_sock, max_queue);
	if (status < 0) {
		_last_err = get_err(errno);
		return SOCKERR_FAULT;
	}
	return SOCKERR_OK;
}

socket_in *socket_in::accept()
{
	struct sockaddr peer;
	socklen_t size = sizeof(peer);
	int clsock;	/* client socket */
	socket_in *clsocket;
	char host[NI_MAXHOST];
	int status;

	clsock = ::accept(_sock, &peer, &size);
	if (clsock < 0) {
		_last_err = get_err(errno);
		return nullptr;
	}

	clsocket = new socket_in();
	clsocket->_sock = clsock;
	clsocket->_addr = peer;
	clsocket->_addrinfo.ai_family = peer.sa_family;
	clsocket->_addrinfo.ai_socktype = SOCK_STREAM;
	clsocket->_addrinfo.ai_addrlen = size;
	clsocket->_addrinfo.ai_addr = &(clsocket->_addr);
	return clsocket;
}

ssize_t socket_in::write(const void *buf, const size_t sz)
{
	ssize_t sent;
	
	sent = ::send(_sock, buf, sz, 0);
	if (sent < 0) {
		_last_err = get_err(errno);
		return SOCKERR_FAULT;
	}
	return sent;
}

ssize_t socket_in::read(void *buf, const size_t sz, int seconds)
{
	ssize_t res;

	if (sz == 0) 
		return 0;
	if (!(select(true, false, seconds) & SOCK_READABLE)) {
		/* No data available */
		_last_err = "no data available for reading";
		return SOCKERR_NODATA;
	}
	return read(buf, sz);
}

ssize_t socket_in::read(void *buf, const size_t sz)
{
	ssize_t res;

	if (sz == 0) 
		return 0;
	res = recv(_sock, buf, sz, 0);
	if (res < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
		_last_err = "no data available for reading";
		return SOCKERR_NODATA;
	} else if (res < 0) {
		_last_err = get_err(errno);
		return SOCKERR_FAULT;
	} else if (res == 0) {
		_last_err = "peer closed";
		return SOCKERR_PEER_CLOSED;
	}
	return res;
}

int socket_in::set_opt(int level, int optname, void* optval, int len)
{
	socklen_t l = len;

	if (::setsockopt(_sock, level, optname, optval, l) < 0) {
		_last_err = get_err(errno);
		return SOCKERR_FAULT;
	}
	return SOCKERR_OK;
}

int socket_in::get_opt(int level, int optname, void* optval, int &len)
{
	socklen_t l;

	if (::getsockopt(_sock, level, optname, optval, &l) < 0) {
		_last_err = get_err(errno);
		return SOCKERR_FAULT;
	}
	len = l;
	return SOCKERR_OK;
}

int socket_in::set_blocking(bool blocking)
{
	int flag;
	
	flag = fcntl(_sock, F_GETFL, NULL);
	if (flag < 0) 
		goto err;
	if ((flag & O_NONBLOCK) && !blocking)
		return SOCKERR_OK;
	if (!(flag & O_NONBLOCK) && blocking)
		return SOCKERR_OK;
	flag = blocking ? flag ^ O_NONBLOCK : flag | O_NONBLOCK;
	if (fcntl(_sock, F_SETFL, flag) < 0) 
		goto err;
	return SOCKERR_OK;

err:
	_last_err = get_err(errno);
	return SOCKERR_FAULT;
}

int socket_in::shutdown()
{
	int status = ::shutdown(_sock, SHUT_RDWR);
	if (status < 0) {
		_last_err = get_err(errno);
		return SOCKERR_FAULT;
	}
	return SOCKERR_OK;
}

void socket_in::close()
{
	::close(_sock);
	_sock = -1;
}

int socket_in::select(bool watch_read, bool watch_write, int seconds)
{
	struct timeval tv;
	fd_set readfds;
	fd_set writefds;
	int nfds, res;
	
	tv.tv_sec = seconds;
	tv.tv_usec = 0;
	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	
	if (watch_read)
		FD_SET(_sock, &readfds);
	if (watch_write)
		FD_SET(_sock, &writefds);
	res = ::select(_sock + 1, watch_read ? &readfds : nullptr, 
		watch_write ? &writefds : nullptr, nullptr, &tv);
	
	if (res < 0) {
		_last_err = get_err(errno);
		return SOCKERR_FAULT;
	}
	res = FD_ISSET(_sock, &readfds) ? SOCK_READABLE : 0;
	res |= FD_ISSET(_sock, &writefds) ? SOCK_WRITEABLE : 0;
	return res;
}

string socket_in::get_last_error()
{
	string e = _last_err;
	_last_err = "";
	return string("socket_in: ") + e;
}

int socket_in::getnameinfo(string &ip, string &service)
{
	char host[NI_MAXHOST];
	char serv[NI_MAXSERV];

	if (::getnameinfo(&_addr, sizeof(_addr), host, sizeof(host), serv,
			 sizeof(serv), NI_NUMERICSERV | NI_NUMERICHOST) < 0) {
		_last_err = get_err(errno);
		return SOCKERR_FAULT;
	}
	ip = host;
	service = serv;
	return SOCKERR_OK;
}
