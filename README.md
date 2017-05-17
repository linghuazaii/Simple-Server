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
   
   
