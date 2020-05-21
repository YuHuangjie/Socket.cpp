/*
 * socket.h -- definitions for the socket wrapper
 * 
 * Copyright (C) 2020 Yu Huangjie
 * 
 * License placeholder
 * 
 */

#ifndef SOCKET_H
#define SOCKET_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string>

/* 
 * error codes
 */
#define SOCKERR_OK              0
#define SOCKERR_FAULT          -1
#define SOCKERR_INVALID_ADDR   -2
#define SOCKERR_PEER_CLOSED    -3
#define SOCKERR_NODATA         -4

/*
 * I/O ability
 */
#define SOCK_READABLE           0x01
#define SOCK_WRITEABLE          0x02

class socket_in
{
public:
	socket_in();
	~socket_in();

	int bind(const std::string &ip, const uint16_t port);
	int connect(const std::string &ip, const uint16_t port);
	int listen(int max_queue);
	socket_in* accept();

    	ssize_t write(const void *buf, const size_t sz);
	/*
	 * Poll the socket for available data. If no data is available before
	 * time expires, return SOCKERR_NODATA. Otherwise, try receiving `sz`
	 * bytes of data.
	 */
    	ssize_t read(void *buf, const size_t sz, int seconds);
    	ssize_t read(void *buf, const size_t sz);

	int set_opt(int level, int optname, void* optval, int len);
	int get_opt(int level, int optname, void* optval, int &len);
	int set_blocking(bool blocking);
	int shutdown();
	void close();
	/* 
	 * get last error 
	 */
	std::string get_last_error();
	/*
	 * Monitor the socket, waiting until it becomes ready for reading or
	 * writing.
	 * 
	 * On success, select returns a combination of READ_ABILITY and 
	 * WRITE_ABILITY or 0 if no I/O is ready to perform. Otherwise, select
	 * return SOCKERR_FAULT on failure.
	 */
	int select(bool watch_read, bool watch_write, int seconds);

	int getnameinfo(std::string &ip, std::string &service);

private:
	int _sock;
	struct addrinfo _addrinfo;
	struct sockaddr _addr;
	std::string _last_err;
};

#endif /* SOCKET_H */
