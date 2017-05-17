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

   
   
