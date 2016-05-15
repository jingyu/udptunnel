# udptunnel

A lightweight TCP over UDP tunneling implementation.

### Compiling
------

##### Linux

In source code directory, run:

```
    make
```

##### Windows

Open Visual Studio ¡°Native Tools Command Prompt¡±, go to source code directory,
run:

```
    nmake /f Makefile.msvc
```

### Running
------

The udptunnel program can run in server mode or in client mode. The command line
syntax is:

```
usage: udptunnel -s [host:]port [-a acl] ...
   or: udptunnel -c [host:]port -p host:port -t host:port

  Server options:
  -s    server mode. server host and port
  -a    access control list
        acl: [s=<src ip>,][d=<dst ip>,][dp=<dst port>,][a=allow|deny]

  Client options:
  -c    client mode. local TCP server host and port
  -t    tunnel server host and port
  -r    remote host and port

  Common options:
  -v    verbose level, 0-3, default is 1
        0 - Error, 1 - Warning, 2 - Info, 3 - Debug
  -h    show this help and exit
```

Server example:

```
    udptunnel -s 192.168.1.6:6688
```

Client example:

```
    udptunnel -c 0.0.0.0:1922 -t 192.168.1.6:6688 -r 127.0.0.1:22
```

### License

[Apache license](http://www.apache.org/licenses/LICENSE-2.0).

