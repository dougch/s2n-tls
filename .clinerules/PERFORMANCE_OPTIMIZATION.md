# s2n-tls Performance Optimization Guide

This document provides guidelines and best practices for optimizing the performance of applications using s2n-tls. It covers techniques for improving handshake speed, throughput, and resource usage.

## Table of Contents

1. [Understanding Performance Factors](#understanding-performance-factors)
2. [Handshake Optimization](#handshake-optimization)
3. [Data Transfer Optimization](#data-transfer-optimization)
4. [Memory Usage Optimization](#memory-usage-optimization)
5. [CPU Usage Optimization](#cpu-usage-optimization)
6. [Benchmarking and Profiling](#benchmarking-and-profiling)
7. [Platform-Specific Optimizations](#platform-specific-optimizations)

## Understanding Performance Factors

Several factors affect the performance of TLS connections:

1. **Handshake Overhead**: The TLS handshake requires multiple round trips and cryptographic operations.
2. **Cipher Suite Selection**: Different cipher suites have different performance characteristics.
3. **Certificate Size**: Larger certificates increase handshake size and processing time.
4. **Session Resumption**: Resuming sessions can significantly reduce handshake overhead.
5. **I/O Model**: The I/O model (blocking vs. non-blocking) affects throughput and latency.
6. **Buffer Sizes**: Appropriate buffer sizes can improve throughput.
7. **Hardware Acceleration**: Cryptographic hardware acceleration can improve performance.

## Handshake Optimization

### 1. Enable Session Resumption

Session resumption allows clients to reconnect to servers without performing a full handshake, significantly reducing latency and computational overhead.

```c
/* Enable session resumption */
if (s2n_config_set_session_cache_onoff(config, 1) != S2N_SUCCESS) {
    /* Handle error */
}
```

### 2. Use TLS 1.3

TLS 1.3 reduces handshake latency by requiring fewer round trips compared to TLS 1.2.

```c
/* Set cipher preferences to use TLS 1.3 */
if (s2n_config_set_cipher_preferences(config, "default_tls13") != S2N_SUCCESS) {
    /* Handle error */
}
```

### 3. Use Smaller Certificate Chains

Smaller certificate chains reduce the amount of data that needs to be transmitted and processed during the handshake.

- Use certificates with smaller key sizes (e.g., ECDSA instead of RSA)
- Minimize the number of certificates in the chain
- Use certificate compression if supported

### 4. Enable 0-RTT (TLS 1.3)

TLS 1.3 supports 0-RTT (Zero Round Trip Time) mode, which allows clients to send data to the server in the first message, reducing latency for resumed sessions.

```c
/* Enable 0-RTT (Early Data) */
if (s2n_config_set_early_data_cb(config, early_data_cb, NULL) != S2N_SUCCESS) {
    /* Handle error */
}
```

Note: 0-RTT has security implications and should be used carefully.

### 5. Use OCSP Stapling

OCSP stapling allows the server to include the OCSP response in the handshake, eliminating the need for clients to make separate OCSP requests.

```c
/* Enable OCSP stapling */
if (s2n_config_set_status_request_type(config, S2N_STATUS_REQUEST_OCSP) != S2N_SUCCESS) {
    /* Handle error */
}
```

## Data Transfer Optimization

### 1. Use Efficient Cipher Suites

Choose cipher suites that provide a good balance between security and performance.

```c
/* Use modern, efficient cipher suites */
if (s2n_config_set_cipher_preferences(config, "20190801") != S2N_SUCCESS) {
    /* Handle error */
}
```

AES-GCM is generally faster than AES-CBC, especially on hardware with AES-NI support. ChaCha20-Poly1305 can be faster on platforms without AES hardware acceleration.

### 2. Use Appropriate Buffer Sizes

Choose buffer sizes that match your application's needs:

- Too small: Increased overhead due to frequent reads/writes
- Too large: Wasted memory and potentially increased latency

```c
/* Example: Receive data with an appropriate buffer size */
uint8_t buffer[16384]; /* 16 KB buffer */
s2n_blocked_status blocked;
ssize_t bytes_read = s2n_recv(conn, buffer, sizeof(buffer), &blocked);
```

### 3. Batch Data

Batch small writes into larger chunks to reduce the overhead of TLS record processing.

```c
/* Instead of multiple small writes */
s2n_send(conn, small_data1, small_size1, &blocked);
s2n_send(conn, small_data2, small_size2, &blocked);

/* Batch data into a larger buffer */
memcpy(large_buffer, small_data1, small_size1);
memcpy(large_buffer + small_size1, small_data2, small_size2);
s2n_send(conn, large_buffer, small_size1 + small_size2, &blocked);
```

### 4. Use Corking/Nagling Appropriately

TCP corking/nagling can improve throughput by reducing the number of small packets, but it can also increase latency.

```c
/* Disable Nagle's algorithm for latency-sensitive applications */
int value = 1;
setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, &value, sizeof(int));

/* Enable TCP_CORK for throughput-sensitive applications */
value = 1;
setsockopt(socket_fd, IPPROTO_TCP, TCP_CORK, &value, sizeof(int));
/* ... send multiple pieces of data ... */
value = 0;
setsockopt(socket_fd, IPPROTO_TCP, TCP_CORK, &value, sizeof(int));
```

### 5. Use Non-Blocking I/O

Non-blocking I/O allows your application to perform other work while waiting for I/O operations to complete.

```c
/* Set the socket to non-blocking mode */
int flags = fcntl(socket_fd, F_GETFL, 0);
fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);

/* Use s2n-tls with non-blocking I/O */
s2n_blocked_status blocked;
int ret;
do {
    ret = s2n_negotiate(conn, &blocked);
    if (ret != S2N_SUCCESS && s2n_error_get_type(s2n_errno) != S2N_ERR_T_BLOCKED) {
        /* Handle error */
        break;
    }
    
    /* If blocked, use select/poll/epoll to wait for the socket to be ready */
    if (blocked == S2N_BLOCKED_ON_READ) {
        /* Wait for the socket to be readable */
    } else if (blocked == S2N_BLOCKED_ON_WRITE) {
        /* Wait for the socket to be writable */
    }
} while (ret != S2N_SUCCESS);
```

## Memory Usage Optimization

### 1. Reuse Connections

Instead of creating and destroying connections for each client request, reuse connections when possible.

```c
/* Wipe a connection for reuse */
if (s2n_connection_wipe(conn) != S2N_SUCCESS) {
    /* Handle error */
}
```

### 2. Use Connection Pools

For server applications, maintain a pool of connections that can be reused for multiple clients.

```c
/* Example connection pool implementation */
struct connection_pool {
    struct s2n_connection **connections;
    size_t size;
    size_t used;
};

/* Initialize a connection pool */
struct connection_pool *pool_init(size_t size) {
    struct connection_pool *pool = malloc(sizeof(struct connection_pool));
    if (pool == NULL) {
        return NULL;
    }
    
    pool->connections = malloc(size * sizeof(struct s2n_connection *));
    if (pool->connections == NULL) {
        free(pool);
        return NULL;
    }
    
    pool->size = size;
    pool->used = 0;
    
    for (size_t i = 0; i < size; i++) {
        pool->connections[i] = s2n_connection_new(S2N_SERVER);
        if (pool->connections[i] == NULL) {
            /* Handle error */
            for (size_t j = 0; j < i; j++) {
                s2n_connection_free(pool->connections[j]);
            }
            free(pool->connections);
            free(pool);
            return NULL;
        }
    }
    
    return pool;
}

/* Get a connection from the pool */
struct s2n_connection *pool_get(struct connection_pool *pool) {
    if (pool->used >= pool->size) {
        return NULL;
    }
    
    return pool->connections[pool->used++];
}

/* Return a connection to the pool */
void pool_return(struct connection_pool *pool, struct s2n_connection *conn) {
    if (pool->used == 0) {
        return;
    }
    
    if (s2n_connection_wipe(conn) != S2N_SUCCESS) {
        /* Handle error */
        return;
    }
    
    pool->connections[--pool->used] = conn;
}
```

### 3. Optimize Stuffer Usage

Stuffers are used extensively in s2n-tls for buffer management. Optimize their usage to reduce memory allocations.

```c
/* Reuse stuffers instead of creating new ones */
struct s2n_stuffer stuffer;
if (s2n_stuffer_alloc(&stuffer, initial_size) != S2N_SUCCESS) {
    /* Handle error */
}

/* Use the stuffer multiple times */
for (int i = 0; i < iterations; i++) {
    /* Reset the stuffer for reuse */
    if (s2n_stuffer_rewrite(&stuffer) != S2N_SUCCESS) {
        /* Handle error */
    }
    
    /* Use the stuffer */
    /* ... */
}

/* Free the stuffer when done */
if (s2n_stuffer_free(&stuffer) != S2N_SUCCESS) {
    /* Handle error */
}
```

### 4. Use Static Buffers When Appropriate

For fixed-size data, use static buffers instead of dynamic allocation.

```c
/* Use a static buffer for a fixed-size operation */
uint8_t static_buffer[1024];
struct s2n_blob blob;
if (s2n_blob_init(&blob, static_buffer, sizeof(static_buffer)) != S2N_SUCCESS) {
    /* Handle error */
}

struct s2n_stuffer stuffer;
if (s2n_stuffer_init(&stuffer, &blob) != S2N_SUCCESS) {
    /* Handle error */
}
```

## CPU Usage Optimization

### 1. Use Hardware Acceleration

s2n-tls automatically uses hardware acceleration when available, but you can ensure it's being used effectively:

- Ensure your platform has the necessary hardware (e.g., AES-NI, CLMUL)
- Use cipher suites that can be hardware-accelerated
- Check that your OpenSSL/LibreSSL build includes hardware acceleration support

### 2. Choose Appropriate Cipher Suites

Different cipher suites have different CPU usage characteristics:

- AES-GCM is faster than AES-CBC on hardware with AES-NI
- ChaCha20-Poly1305 can be faster on platforms without AES hardware acceleration
- ECDHE key exchange is generally faster than DHE

```c
/* Use cipher suites optimized for your platform */
if (s2n_config_set_cipher_preferences(config, "20190801") != S2N_SUCCESS) {
    /* Handle error */
}
```

### 3. Minimize Handshakes

Handshakes are CPU-intensive due to the cryptographic operations involved. Minimize them by:

- Using session resumption
- Keeping connections alive with keep-alive mechanisms
- Using connection pooling

### 4. Use Asynchronous Operations

For CPU-intensive operations, consider using asynchronous processing to avoid blocking the main thread.

```c
/* Set an asynchronous callback for private key operations */
if (s2n_config_set_async_pkey_callback(config, async_pkey_callback) != S2N_SUCCESS) {
    /* Handle error */
}
```

### 5. Optimize Thread Usage

For multi-threaded applications:

- Use one connection per thread to avoid locking overhead
- Consider using a thread pool to handle multiple connections
- Avoid excessive thread creation and destruction

## Benchmarking and Profiling

### 1. Benchmark Your Application

Regularly benchmark your application to identify performance bottlenecks:

```bash
# Example: Use OpenSSL's s_time to benchmark a server
openssl s_time -connect localhost:443 -new -cipher AES128-GCM-SHA256
```

### 2. Profile CPU Usage

Use profiling tools to identify CPU hotspots:

```bash
# Example: Use perf to profile a Linux application
perf record -g ./your_application
perf report
```

### 3. Monitor Memory Usage

Monitor memory usage to identify leaks and excessive allocations:

```bash
# Example: Use Valgrind to check memory usage
valgrind --tool=massif ./your_application
ms_print massif.out.*
```

### 4. Use s2n-tls's Built-in Benchmarks

s2n-tls includes benchmarks that can help you understand the performance characteristics of different operations:

```bash
# Build and run s2n-tls benchmarks
cd tests/benchmark
make
./benchmark
```

## Platform-Specific Optimizations

### Linux

1. **Use TCP_FASTOPEN**: Reduces handshake latency by sending data in the initial SYN packet.

```c
int qlen = 5;
setsockopt(socket_fd, IPPROTO_TCP, TCP_FASTOPEN, &qlen, sizeof(qlen));
```

2. **Use SO_REUSEPORT**: Allows multiple threads to accept connections on the same port, improving load balancing.

```c
int reuse = 1;
setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
```

3. **Use epoll for I/O Multiplexing**: More efficient than select/poll for large numbers of connections.

```c
int epoll_fd = epoll_create1(0);
struct epoll_event event;
event.events = EPOLLIN | EPOLLOUT | EPOLLET;
event.data.fd = socket_fd;
epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &event);
```

### macOS/BSD

1. **Use kqueue for I/O Multiplexing**: More efficient than select/poll for large numbers of connections.

```c
int kq = kqueue();
struct kevent event;
EV_SET(&event, socket_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
kevent(kq, &event, 1, NULL, 0, NULL);
```

### Windows

1. **Use IOCP (I/O Completion Ports)**: Efficient for handling many concurrent connections.

```c
HANDLE iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
CreateIoCompletionPort((HANDLE)socket, iocp, (ULONG_PTR)connection_data, 0);
```

2. **Use AcceptEx**: More efficient than accept() for accepting connections.

```c
LPFN_ACCEPTEX AcceptEx = NULL;
GUID guid = WSAID_ACCEPTEX;
DWORD bytes = 0;
WSAIoctl(listen_socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
         &AcceptEx, sizeof(AcceptEx), &bytes, NULL, NULL);
```

### AWS

1. **Use Elastic Network Adapter (ENA)**: Provides enhanced networking performance on AWS instances.

2. **Use c5/m5 Instance Types**: These instance types offer improved networking performance.

3. **Use Placement Groups**: Cluster placement groups provide lower latency between instances.
