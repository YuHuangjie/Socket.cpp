Easy C++ Socket Class
=====================

**Simple Socket Echo Server**
Can also be combined with lists and threads to run multiple socket connections at the same time.
In this example the server accepts only one client.
But if you store the sockets and iterate through them (or split them with threads in multiple processes) you can accept multiple clients.
```cpp
socket_in server;
socket_in *peer;
int reuse = 1;
string ip, serv;
const size_t sz = 13;
char buf[sz];
int res;

/*
 * Establish connection
 */
if (server.set_opt(SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
        goto server_err;
if (server.bind("127.0.0.1", 23333) < 0)
        goto server_err;
if (server.getnameinfo(ip, serv) < 0)
        goto server_err;
cout << "server: " << ip << ": " << serv << endl;
if (server.listen(1) < 0) 
        goto server_err;
peer = server.accept();
if (!peer)
        goto server_err;
if (peer->getnameinfo(ip, serv) < 0)
        goto peer_err;
cout << "accept peer: " << ip << ": " << serv << endl;

/* 
 * Exchange data
 */
if (peer->read(buf, sz) < 0)
        goto peer_err;
cout << "read without timer: " << buf << endl;
buf[0] = '\0';

while (true) {
        res = peer->read(buf, sz, 2);
        if (res == SOCKERR_NODATA) {
                cout << "no data available in 2 seconds" << endl;
        }
        else if (res == SOCKERR_PEER_CLOSED || res < 0) {
                goto peer_err;
        }
        else {
                cout << "read with timer: " << buf << endl;
                break;
        }
}

peer->set_blocking(false);
while (true) {
        res = peer->read(buf, sz);
        if (res == SOCKERR_NODATA)
                cout << "no data available in nonblocking mode" << endl;
        else if (res == SOCKERR_PEER_CLOSED || res < 0) 
                goto peer_err;
        else 
                cout << "read without timer: " << buf << endl;
}

/* 
 * Release socket descriptors
 */
peer->close();
server.close();
return 0;

peer_err:
        cerr << peer->get_last_error() << endl;
        peer->close();
        server.close();
        return 1;
server_err:
        cerr << server.get_last_error() << endl;
        server.close();
        if (peer)
                peer->close();
        return 1;
```

**Simple Socket Client**
```cpp
socket_in client;
string ip, service;
const size_t sz = 13;
char msg[sz] = "Hello, there";

/*
 * Establish connection
 */
if (client.connect("127.0.0.1", 23333) < 0) 
        goto client_err;
if (client.getnameinfo(ip, service) < 0)
        goto client_err;
cout << "server: " << ip << ": " << service << endl;

/* 
 * Exchange data
 */
client.write(msg, sz);

this_thread::sleep_for(chrono::seconds(5));
client.write(msg, sz);

this_thread::sleep_for(chrono::seconds(2));
client.write(msg, sz);

/* 
 * Release socket descriptors
 */
client.close();

return 0;

client_err:
        cerr << client.get_last_error() << endl;
        return 1;
```
