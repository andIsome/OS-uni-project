# OS-uni-project: message broker
# 
# specifications for the project were translated:
# 
# _Message Broker_

The second exercise of the project aims to build a simple system for publishing and subscribing messages, which are stored in the FS file system.
The system will have a standalone server process, to which different client processes can connect, to publish or receive messages in a given message storage box.

## Starting point

To solve the second exercise, groups should use as a basis their solution from the 1st exercise or access the new code base, which extends the original version of FS in the following ways:

- FS's main operations are synchronized using a single global latch (_mutex_).
Although less parallel than the intended solution for the first exercise, this synchronization solution is sufficient to implement the new requirements;
- The `tfs_unlink` operation, which allows removing files, is implemented.

Additionally, the code base includes skeletons for:

1. The _mbroker_ server program (in the `mbroker` directory);
2. The client implementation for publishing (in the `publisher` directory);
3. The client implementation for subscribing (in the `subscriber` directory);
4. The implementation of the management client (on the `manager` board).

Instead of the new code base, groups that have a robust solution in the 1st exercise are encouraged to build the solution based on their version, which will be more concurrency-optimized up front.

## 1. System architecture

The system consists of the server (_mbroker_) and several publishers (_publishers_), subscribers (_subscribers_) and managers (_manager_).

### Message Boxes

A fundamental concept of the system is message boxes.
Each box can have one publisher and multiple subscribers.
The _publisher_ places messages in the box, and the multiple _subscribers_ read the messages from the box.
Each box is supported on the server by a file in TFS.
For this reason, the life cycle of a box is distinct from the life cycle of the _publisher_ who posts messages there.
Moreover, it is possible that a box may have several _publishers_ during its lifetime, although only one at a time.

Box creation and removal operations are managed by the _manager_.
Additionally, the _manager_ allows you to list existing boxes in the _mbroker_.

### 1.1. server

The server incorporates FS and is a standalone process, initialized as follows:

```sh
$ mbroker <register_pipe_name> <max_sessions>
```

The server creates a _named pipe_ whose name is as given in the above argument.
It is through this _named pipe_, created by the server, that client processes will be able to connect to register.

Any client process can connect to the server's _named pipe_ and send it a message requesting that a session be started.
A **session** consists of having a _named pipe_ of the client, where the client sends messages (if it is a publisher) or where the client receives messages (if it is a subscriber).
There is also the _manager_.
A given client only takes on one role, i.e. it is either exclusively a publisher, a subscriber, or a manager.

The _named pipe_ of the session must be created in advance by the client.
In the registration message, the client sends the name of the _named pipe_ to be used during the session.

The session _named pipe_ name must be chosen manually when a client is launched and to ensure that there are no conflicts with other competing clients.

A session remains open until one of the following happens:

1. A client closes its _named pipe_, implicitly signaling the end of the session;
2. The box is removed by the manager.

The _named pipe_ must be removed from the file system by the respective client after the session ends.

The server accepts a maximum number of concurrent sessions, defined by the value of the `max_sessions` argument.

In the following subsections we describe the client-server protocol in more detail, i.e. the contents of the request and response messages exchanged between clients and server.

#### 1.1.1. Server architecture

The server should use the _main thread_ to manage the _named registration pipe_ and launch `max_sessions` _threads_ to process sessions.
When a new registration request arrives, it should be routed to an _thread_ that is available, which will process it for as long as necessary.
To manage these requests, preventing _threads_ from being on active hold, the _main thread_ and _worker threads_ cooperate using a **producer-consumer thread**, according to the interface provided in the `producer-consumer.h` file.
In this way, when a new registration request arrives, it is placed in the queue and as soon as a _thread_ becomes available, it will consume and process that request.

The server architecture is summarized in the following figure:

![](img/architecture_proj2.png)

- The _mbroker_ uses TFS to store the messages from the boxes;
- The _main thread_ receives requests through the _register pipe_ and places them in a producer-consumer queue;
- The _worker threads_ execute client requests, dedicating themselves to serving one client at a time;
- They cooperate with the _main thread_ through a producer-consumer queue, which avoids active waiting.

### 1.2. _Publisher_

A publisher is a process launched as follows:

```sh
pub <register_pipe_name> <pipe_name> <box_name>
```

As soon as it is launched, _publisher_, asks to log into the _mbroker_ server, indicating the message box to which you want to write messages.
If the connection is accepted (it can be rejected if there is already a _publisher_ connected to the box, for example) it receives messages from `stdin` and then publishes them.
A **message** corresponds to one line of `stdin`, truncated to a given maximum value and delimited by a ``0`, like a C _string_.
The message must not include a final ``n`.

If the _publisher_ receives an EOF (_End Of File_, for example, with a Ctrl-D), it must terminate the session by closing the _named pipe_.

### 1.3. _Subscriber_

A subscriber is a process launched as follows:

```sh
sub <register_pipe_name> <pipe_name> <box_name>
```

Once it is launched, the _subscriber_:

1. connects to _mbroker_, indicating which message box it wants to subscribe to;
2. Collects the messages already stored there and prints them one by one in `stdout`, delimited by `\n`;
3. It listens for new messages;
4. Prints new messages when they are written to the _named pipe_ for which it has an open session.

To terminate the _subscriber_, it must properly process `SIGINT` (i.e., Ctrl-C), closing the session and printing in `stdout` the number of messages received during the session.

### 1.4. _Manager_

A manager is a process launched in one of the following ways:

```sh
manager <register_pipe_name> <pipe_name> create <box_name>
manager <register_pipe_name> <pipe_name> remove <box_name>
manager <register_pipe_name> <pipe_name> list
```

As soon as it is launched, the _manager_:

 1. sends the request to _mbroker_;
 2. Receives the response in the _named pipe_ created by the _manager_ itself;
 3. Prints the response and finishes.

### 1.5. Examples of execution

A first **example** considers the **sequential** operation of the clients:

 1. A _manager_ creates the `bla` box;
 2. A _publisher_ connects to the same box, writes 3 messages, and disconnects;
 3. A _subscriber_ connects to the same box and starts receiving messages;
 4. Receives all three, one at a time, and then waits for more messages.

In a second **example**, more interesting, there will be **competition** between clients:

 1. A _publisher_ connects;
 2. Meanwhile, a _subscriber_ for the same box, connects too;
 3. The _publisher_ places messages in the box and they are immediately delivered to the _subscriber_, and are still registered in the file;
 4. Another _subscriber_ connects to the same box, and starts receiving all the messages from the beginning;
 5. Now, when the _publisher_ writes a new message, both _subscriber_ receive the message directly.

## 2. protocol

To moderate the interaction between the server and the clients, a protocol is established, which defines how messages are serialized, i.e. how they are arranged in a _buffer_ of _bytes_.
This kind of protocol is sometimes referred to as a _wire protocol_, in an allusion to the data that actually circulate in the transmission medium, which in this case will be the _named pipes_.

The content of each message must follow the following format, where:

- The `|` symbol denotes the concatenation of elements in a message;
- All request messages are started by a code identifying the requested operation (`OP_CODE`);
- The _strings_ carrying, for example, the names of _named pipes_ are of fixed length, indicated in the message.
In case of text with a smaller size, the additional characters should be filled in with ``0`.

### 2.1. Registering

The _named pipe_ of the server, which only receives registrations from new clients, should receive messages of the following type:

_publisher_ registration request:

```
[ code = 1 (uint8_t) ] | [ client_named_pipe_path (char[256]) ] | [ box_name (char[32]) ]
```

Request to register _subscriber_:

```
[ code = 2 (uint8_t) ] | [ client_named_pipe_path (char[256]) ] | [ box_name (char[32]) ]
```

Request for box creation:

```
[ code = 3 (uint8_t) ] | [ client_named_pipe_path (char[256]) ] | [ box_name (char[32]) ]
```

Response to box creation request:

```
[ code = 4 (uint8_t) ] | [ return_code (int32_t) ] | [ error_message (char[1024]) ]
```

The return code should be `0` if the box was created successfully, and `-1` in case of an error.
On error the error message is sent (otherwise it is simply initialized with ``0`).

Request to remove box:

```
[ code = 5 (uint8_t) ] | [ client_named_pipe_path (char[256]) ] | [ box_name (char[32]) ]
```

Response to box removal request:

```
[ code = 6 (uint8_t) ] | [ return_code (int32_t) ] | [ error_message (char[1024]) ]
```

Request for box listing:

```
[ code = 7 (uint8_t) ] | [ client_named_pipe_path (char[256]) ]
```

The response to the box listing comes in several messages, of the following type:

```
[ code = 8 (uint8_t) ] | [ last (uint8_t) ] | [ box_name (char[32]) ] | [ box_size (uint64_t) ] | [ n_publishers (uint64_t) ] | [ n_subscribers (uint64_t) ]
```

The `last` byte is `1` if this is the last box in the listing and `0` otherwise.
`box_size` is the size (in _bytes_) of the box, `n_publisher` (`0` or `1`) indicates whether there is a _publisher_ attached to the box at that time and `n_subscriber` indicates the number of subscribers to the box at that time.

If there are no boxes, the response is a message with `last` to `1` and `box_name` all filled in with ``0`.

### 2.2 _Publisher_

The publisher sends messages to the server of type:

```
[ code = 9 (uint8_t) ] | [ message (char[1024]) ]
```

### 2.3 _Subscriber_

The server sends messages to the subscriber of type:

```
[ code = 10 (uint8_t) ] | [ message (char[1024]) ]
```

## 3. Implementation requirements

### 3.1. Client handling

When the server starts, it launches a set of `S` tasks (_thread pool_), which are waiting for registration requests to handle, which it will receive through the producer-consumer queue.
The _main thread_ manages the _named pipe_ of registration, and places the registration requests in the producer-consumer queue.
When a _thread_ ends a session, it is left waiting for a new session to handle.

### 3.2 Storage bins

Messages received by the server must be placed in a box.
In practice, a box corresponds to a file in FS.
The file must be created when the box is created by the _manager_, and deleted when the box is removed.
All incoming messages are written to the end of the file, separated by ``0`.

In short, messages are accumulated in the boxes.
When a subscriber connects to a box, the corresponding file is opened and the messages start being read from the beginning (even if the same subscriber or another has received them before).
Further messages generated by the _publisher_ of a box should also be delivered to the _subscribers_ of the box.
This functionality should be implemented using **condition variables** in order to avoid active waits.

### 3.3 Message formatting

To standardize the _output_ of the various commands (for `stdout`), the format with which these should be printed is provided.

### 3.4 Producer-Consumer Queue

The producer-consumer queue is the most complex synchronization structure in the project.
Therefore, this component will be evaluated in isolation (i.e., there will be tests using only the interface described in `producer-consumer.h`) to ensure its correctness.
As such, the interface of `producer-consumer.h` should not be changed.

Otherwise, groups are free to change the code base as they see fit.

#### Subscriber Messages

```c
fprintf(stdout, "%s\n", message);
```

#### Results of Manager Operations

If the create or remove box command is successful, it should print:

```c
fprintf(stdout, "OK\n");
```

If an error occurred:

```c
fprintf(stdout, "ERROR %s\n", error_message);
```

#### Box Listing

Each line of the box listing should print as follows:

```c
fprintf(stdout, "%s %zu %zu %zun", box_name, box_size, n_publishers, n_subscribers);
```

The boxes must be sorted alphabetically, and the server is not guaranteed to send them in that order (i.e., the client must sort the boxes before printing them).

If there are no boxes, the following should be printed:

```c
fprintf(stdout, "NO BOXES FOUND");
```

### 3.5 Active Waiting

In the design, **never** active wait mechanisms should be used.

## 4. Suggested Implementation

It is suggested that you implement the project through the following steps:

1. implement the command line interfaces (CLI) of the clients;
2. Implement the serialization of the communication protocol;
3. Implement a basic version of `mbroker`, where there is only one _thread_ that, in cycle, a) receives a registration request; b) handles the corresponding session; and c) goes back to waiting for the registration request;
4. Implement the producer-consumer queue;
5. Use the producer-consumer queue to manage and route registration requests to the _worker threads_.
