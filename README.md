# Simple-Server
I am reading the source code of `Libevent`, so this simple server is extrated from the wisdom of `Libevent` `epoll` design. And none `Libevent` API is used, so this simple server is built from scratch.

### Note
Things need to think about in this simple server:
 - For listening socket, this options needs to think about: `SOCK_NONBLOCK`, `SOCK_CLOEXEC` (No need, since I don't use `fork`, so there is no leak), `SO_KEEPALIVE`(funny, I'll do some research), `SO_REUSEADDR`(do more deep research on this, even though I know it is for not hanging on listening socket when restart), `SO_REUSEPORT`(do more deep research, this one is funny in linux), `TCP_DEFER_ACCEPT`(this one is useful, but I just forget ...)
 - everything is automated, everything is event based, things happen, callback functions are called. Event based design is natural, if I want to write a scalable server, the first thing is to abstract all things to events and hide the deep implementation of polling loop.
 - `epoll_create` is a legacy? Should we always use `epoll_create1`?
 - `timerfd_create` can be used to create a timer event, funny, I think it won't be used.
 - `accept4` and `accept`

### Detail
Details after research:
 - `SO_KEEPALIVE` in listening socket: 
   + [TCP Keepalive HOWTO](http://tldp.org/HOWTO/html_single/TCP-Keepalive-HOWTO/)
   + according to man-page of `man 2 accept`, it seems that the descriptor flags inheritence is not reliable. so I won't use `SO_KEEPALIVE` in the `bind` socket. and this option should be configurable by user. so this simple server example won't use `SO_KEEPALIVE` option.
 - `SO_REUSEADDR` and `SO_REUSEPORT`:
   + [Socket options SO_REUSEADDR and SO_REUSEPORT, how do they differ? Do they mean the same across all major operating systems?](http://stackoverflow.com/questions/14388706/socket-options-so-reuseaddr-and-so-reuseport-how-do-they-differ-do-they-mean-t)
   + in linux man page
     - `SO_REUSEADDR`: Indicates that the rules used in validating addresses supplied in a bind(2) call should allow reuse of local addresses. For AF_INET sockets this means that a socket may bind, except when there is an active listening socket bound to the address.   When the listening socket is bound to INADDR_ANY with a specific port then it is not possible to bind to this port for any local address.  Argument is an integer boolean flag. 
     - `SO_REUSEPORT`:  Permits multiple AF_INET or AF_INET6 sockets to be bound to an identical socket address. This option must be set on each socket (including the first socket) prior to  calling bind(2) on the socket. To prevent port hijacking, all of the processes binding to the same address must have the same effective UID. This option can be employed with both TCP and UDP sockets. For TCP sockets, this option allows accept(2) load distribution in a multi-threaded server to be improved by using a distinct listener socket for each  thread.  This provides improved load distribution as compared to traditional techniques such using a single accept(2)ing thread that distributes connections, or having multiple threads that compete to accept(2) from the same socket. For UDP sockets, the use of this option can provide better distribution of incoming datagrams to multiple processes (or threads) as compared to the  traditional  tech‐nique of having multiple processes compete to receive datagrams on the same socket.
     - so I won't use `SO_REUSEPORT`, I think there is no bottle-neck in `accpet`. for each physical connection, even if you have multiple threads accept on differenct sockets which use `SO_REUSEPORT`, if packet is distributed to every socket with duplicates, then it is bad design, you need make sure each packet is processed once. or if packet is add to one socket with no duplicates, then it is no need to use `SO_REUSEPORT`, since the ready queue for each physical connection shared by all sockets in this connection. 
 - at this point I am going to talk about `TIME_WAIT`, I didn't understand it rightly before:
   + the TCP State Machine
   + <img src="http://tcpipguide.com/free/diagrams/tcpfsm.png" />
   + so `TIME_WAIT` status is for the end which do `close` first, and to make sure the peer end receive the last ack. if the last ack is lost, `close` end will close connection after `linger` time. and `FIN` from peer end will retransmit after timeout happened. since `close` end has already close the connection. so a `RST` is sent back, connection is closed for peer end.
   + but I have a question? if let server close connection first, there must be too many connection in `TIME_WAIT` status, if let client close connection first, there might be too many active connections in server side if client doesn't close connection. I think I should always let client `close` connection first, and there is a timeout in server side, HEHE, digging back to `Libevent` source code. `Libevent`'s implementation is checking elapsed time for all events everytime.
 - `TCP_CORK`: gather packet frame, send by one time like scatter/gather io. can be used with `TCP_NODELAY`, but no need to use this option in this little example.
 - `TCP_DEFER_ACCEPT`: it is good to set this option, because in real world we don't want to be waken up if tcp connection has just established. use `TCP_DEFER_ACCEPT` means that wake me up when data comes.
 - `TCP_NODELAY`: set this option to disable Nagel Algorithm:
   + [Nagel Algorithm](https://en.wikipedia.org/wiki/Nagle%27s_algorithm)
   + I think Nagel Algorithm should be disabled in my simple server, because it is multithreading, I can't ensure the order is always `read-write-read-write`, it is normal that a `write-write-read` happens. 
 - `TCP_QUICKACK`: since we have set `TCP_NODELAY`, there is no need to set `TCP_QUICKACK`, just use default delayed ack to improve network performance.
 - `epoll_create` is just legacy, no much difference with `epoll_create1`
 - no much difference between `accept` and `accept4`, `accept4` is convenient for set `O_NONBLOCKING` for connected socket.
 - for `EPOLLONESHOT`, there is no need for use this option if we `read` until `EAGAIN` every time.
 
### Enough Details, Going To Implementation
 - compile server with `g++ server.cpp threadpool.c -o server -lpthread`  
 - compile client with `g++ client.cpp threadpool.c -o client -lpthread`
