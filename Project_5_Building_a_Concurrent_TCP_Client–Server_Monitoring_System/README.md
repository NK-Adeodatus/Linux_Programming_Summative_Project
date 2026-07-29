# Lab Equipment Reservation System

A concurrent TCP client-server application in C for a university lab's
shared equipment booking system. Multiple students/researchers can
connect simultaneously, authenticate, view available equipment, and
reserve an item — with the server safely preventing two users from
reserving the same equipment at once.

Built with POSIX sockets and pthreads on Linux.

## Features

- TCP server handling 5+ simultaneous clients, one thread per connection
- User authentication against a registered-user list
- Live equipment listing and single-item reservation per session
- Mutex-protected shared state — no double-booking, even under real
  concurrent load
- Graceful handling of clean logouts *and* abrupt client disconnects
  (server never crashes or hangs on a dropped connection)
- Server-side logging of connections, auth attempts, reservations, and
  live equipment/user status

## Requirements

- Linux (uses POSIX sockets, pthreads, `<arpa/inet.h>`)
- `gcc` and `make`
- No external libraries beyond the standard C library and `libpthread`

## Project Structure

```
.
├── protocol.h        Shared message-type constants (wire protocol)
├── net_utils.h/.c     Reliable framed send/recv helpers over TCP
├── shared.h/.c        Server-side shared state: users, equipment, mutexes
├── server.c           Multi-threaded TCP server
├── client.c            Interactive TCP client
├── Makefile            Build rules
├── DOCUMENTATION.md   Protocol, auth, concurrency & sync design writeup
└── demo_output.txt      Captured sample run (all required scenarios)
```

## Building

```bash
make
```

This produces two executables in the project directory: `server` and
`client`. `make clean` removes all build artifacts.

## Running

**1. Start the server** (defaults to port 8080 if no argument given):

```bash
./server [port]
```

The server prints connection, authentication, and reservation events
to stdout as they happen, along with a live status snapshot
(connected users + equipment reservation state) after every state
change.

**2. Start one or more clients**, each in its own terminal (or on a
separate machine, pointing at the server's IP):

```bash
./client [server_ip] [port]
```

Defaults: `server_ip = 127.0.0.1`, `port = 8080`.

**3. Follow the prompts:**

```
Enter your user ID: alice123
Authentication successful: Welcome
Available equipment:
  [1] Oscilloscope
  [2] SolderStation
  [3] LogicAnalyzer
Enter the ID of the equipment to reserve: 1
Reservation successful: Oscilloscope reserved successfully
Session closed. Goodbye, alice123
```

Registered user IDs are defined in `shared.c` (`registered_users[]`):
`alice123`, `bob456`, `carol789`, `dave000`. Any other ID will be
rejected during authentication.

## Trying Concurrency Yourself

Open several terminals and run `./client` in each at roughly the same
time, having two of them request the same equipment ID. You'll see
exactly one succeed and the other receive a
`Reservation failed: ... already reserved by ...` response — the
server log will show both requests being processed without either
corrupting the other's result.

## Documentation

See [`DOCUMENTATION.md`](DOCUMENTATION.md) for a full writeup of the
communication protocol, authentication process, concurrency model,
shared-resource synchronization strategy, session management, and
error/disconnection handling — including *why* each design choice was
made.

See [`demo_output.txt`](demo_output.txt) for real captured logs
covering successful/failed authentication, equipment listing,
successful and conflicting reservations, multiple concurrent clients,
and an abrupt-disconnect recovery test.