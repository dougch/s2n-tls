# s2n-tls API Reference

This document provides a comprehensive reference for the s2n-tls API, including detailed function descriptions, parameters, return values, and usage examples.

## Table of Contents

1. [Library Initialization and Cleanup](#library-initialization-and-cleanup)
2. [Error Handling](#error-handling)
3. [Configuration Management](#configuration-management)
4. [Connection Management](#connection-management)
5. [I/O Operations](#io-operations)
6. [Blobs and Memory Management](#blobs-and-memory-management)
7. [Stuffers](#stuffers)
8. [Crypto Operations](#crypto-operations)
9. [Advanced Features](#advanced-features)
10. [Complete Examples](#complete-examples)

## Library Initialization and Cleanup

### s2n_init

```c
int s2n_init(void);
```

**Description**: Initializes the s2n-tls library. This must be called before any other s2n-tls function.

**Parameters**: None

**Returns**:
- `S2N_SUCCESS` (0) on success
- `S2N_FAILURE` (-1) on failure

**Example**:
```c
if (s2n_init() != S2N_SUCCESS) {
    fprintf(stderr, "Error initializing s2n: %s\n", s2n_strerror(s2n_errno, "EN"));
    exit(1);
}
```

### s2n_cleanup

```c
int s2n_cleanup(void);
```

**Description**: Cleans up thread-local resources used by s2n-tls. This should be called when a thread is done using s2n-tls.

**Parameters**: None

**Returns**:
- `S2N_SUCCESS` (0) on success
- `S2N_FAILURE` (-1) on failure

**Example**:
```c
if (s2n_cleanup() != S2N_SUCCESS) {
    fprintf(stderr, "Error cleaning up s2n: %s\n", s2n_strerror(s2n_errno, "EN"));
}
```

### s2n_cleanup_final

```c
int s2n_cleanup_final(void);
```

**Description**: Performs final cleanup of the s2n-tls library. This should be called when the application is done using s2n-tls entirely.

**Parameters**: None

**Returns**:
- `S2N_SUCCESS` (0) on success
- `S2N_FAILURE` (-1) on failure

**Example**:
```c
if (s2n_cleanup_final() != S2N_SUCCESS) {
    fprintf(stderr, "Error performing final cleanup of s2n: %s\n", s2n_strerror(s2n_errno, "EN"));
}
```

## Error Handling

### s2n_error_get_type

```c
enum s2n_error_type s2n_error_get_type(int error);
```

**Description**: Gets the type/category of an error.

**Parameters**:
- `error`: The error code (typically `s2n_errno`)

**Returns**: An `s2n_error_type` enum value indicating the category of the error.

**Example**:
```c
if (s2n_negotiate(conn, &blocked) != S2N_SUCCESS) {
    if (s2n_error_get_type(s2n_errno) == S2N_ERR_T_BLOCKED) {
        // Connection is blocked, try again later
    } else {
        // Handle other errors
        fprintf(stderr, "Error: %s\n", s2n_strerror(s2n_errno, "EN"));
    }
}
```

### s2n_strerror

```c
const char *s2n_strerror(int error, const char *lang);
```

**Description**: Returns a human-readable error message for the given error code.

**Parameters**:
- `error`: The error code (typically `s2n_errno`)
- `lang`: The language code for the error message (e.g., "EN" for English)

**Returns**: A string containing the error message.

**Example**:
```c
fprintf(stderr, "Error: %s\n", s2n_strerror(s2n_errno, "EN"));
```

### s2n_strerror_debug

```c
const char *s2n_strerror_debug(int error, const char *lang);
```

**Description**: Returns detailed debug information for the given error code.

**Parameters**:
- `error`: The error code (typically `s2n_errno`)
- `lang`: The language code for the error message (e.g., "EN" for English)

**Returns**: A string containing detailed debug information.

**Example**:
```c
fprintf(stderr, "Debug: %s\n", s2n_strerror_debug(s2n_errno, "EN"));
```

## Configuration Management

### s2n_config_new

```c
struct s2n_config *s2n_config_new(void);
```

**Description**: Creates a new configuration object.

**Parameters**: None

**Returns**: A pointer to a new `s2n_config` structure, or NULL on failure.

**Example**:
```c
struct s2n_config *config = s2n_config_new();
if (config == NULL) {
    fprintf(stderr, "Error creating config: %s\n", s2n_strerror(s2n_errno, "EN"));
    exit(1);
}
```

### s2n_config_free

```c
int s2n_config_free(struct s2n_config *config);
```

**Description**: Frees a configuration object.

**Parameters**:
- `config`: The configuration object to free

**Returns**:
- `S2N_SUCCESS` (0) on success
- `S2N_FAILURE` (-1) on failure

**Example**:
```c
if (s2n_config_free(config) != S2N_SUCCESS) {
    fprintf(stderr, "Error freeing config: %s\n", s2n_strerror(s2n_errno, "EN"));
}
```

### s2n_config_set_cipher_preferences

```c
int s2n_config_set_cipher_preferences(struct s2n_config *config, const char *version);
```

**Description**: Sets the security policy (cipher preferences) for a configuration.

**Parameters**:
- `config`: The configuration object
- `version`: The name of the security policy (e.g., "default", "20170718", "20190801")

**Returns**:
- `S2N_SUCCESS` (0) on success
- `S2N_FAILURE` (-1) on failure

**Example**:
```c
if (s2n_config_set_cipher_preferences(config, "default") != S2N_SUCCESS) {
    fprintf(stderr, "Error setting cipher preferences: %s\n", s2n_strerror(s2n_errno, "EN"));
    exit(1);
}
```

### s2n_config_add_cert_chain_and_key_to_store

```c
int s2n_config_add_cert_chain_and_key_to_store(struct s2n_config *config, struct s2n_cert_chain_and_key *cert_key_pair);
```

**Description**: Adds a certificate and private key to a configuration's certificate store.

**Parameters**:
- `config`: The configuration object
- `cert_key_pair`: The certificate and key pair to add

**Returns**:
- `S2N_SUCCESS` (0) on success
- `S2N_FAILURE` (-1) on failure

**Example**:
```c
struct s2n_cert_chain_and_key *cert_chain_and_key = NULL;
if (s2n_cert_chain_and_key_new(&cert_chain_and_key) != S2N_SUCCESS) {
    // Handle error
}
if (s2n_cert_chain_and_key_load_pem(cert_chain_and_key, "cert.pem", "key.pem") != S2N_SUCCESS) {
    // Handle error
}
if (s2n_config_add_cert_chain_and_key_to_store(config, cert_chain_and_key) != S2N_SUCCESS) {
    fprintf(stderr, "Error adding certificate and key: %s\n", s2n_strerror(s2n_errno, "EN"));
    exit(1);
}
```

## Connection Management

### s2n_connection_new

```c
struct s2n_connection *s2n_connection_new(enum s2n_mode mode);
```

**Description**: Creates a new connection object.

**Parameters**:
- `mode`: The mode for the connection (`S2N_CLIENT` or `S2N_SERVER`)

**Returns**: A pointer to a new `s2n_connection` structure, or NULL on failure.

**Example**:
```c
struct s2n_connection *conn = s2n_connection_new(S2N_SERVER);
if (conn == NULL) {
    fprintf(stderr, "Error creating connection: %s\n", s2n_strerror(s2n_errno, "EN"));
    exit(1);
}
```

### s2n_connection_set_config

```c
int s2n_connection_set_config(struct s2n_connection *conn, struct s2n_config *config);
```

**Description**: Associates a configuration with a connection.

**Parameters**:
- `conn`: The connection object
- `config`: The configuration object

**Returns**:
- `S2N_SUCCESS` (0) on success
- `S2N_FAILURE` (-1) on failure

**Example**:
```c
if (s2n_connection_set_config(conn, config) != S2N_SUCCESS) {
    fprintf(stderr, "Error setting connection config: %s\n", s2n_strerror(s2n_errno, "EN"));
    exit(1);
}
```

### s2n_connection_set_fd

```c
int s2n_connection_set_fd(struct s2n_connection *conn, int fd);
```

**Description**: Sets the file descriptor for a connection's I/O.

**Parameters**:
- `conn`: The connection object
- `fd`: The file descriptor

**Returns**:
- `S2N_SUCCESS` (0) on success
- `S2N_FAILURE` (-1) on failure

**Example**:
```c
if (s2n_connection_set_fd(conn, socket_fd) != S2N_SUCCESS) {
    fprintf(stderr, "Error setting connection fd: %s\n", s2n_strerror(s2n_errno, "EN"));
    exit(1);
}
```

### s2n_connection_free

```c
int s2n_connection_free(struct s2n_connection *conn);
```

**Description**: Frees a connection object.

**Parameters**:
- `conn`: The connection object to free

**Returns**:
- `S2N_SUCCESS` (0) on success
- `S2N_FAILURE` (-1) on failure

**Example**:
```c
if (s2n_connection_free(conn) != S2N_SUCCESS) {
    fprintf(stderr, "Error freeing connection: %s\n", s2n_strerror(s2n_errno, "EN"));
}
```

### s2n_connection_wipe

```c
int s2n_connection_wipe(struct s2n_connection *conn);
```

**Description**: Wipes a connection object, resetting it for reuse.

**Parameters**:
- `conn`: The connection object to wipe

**Returns**:
- `S2N_SUCCESS` (0) on success
- `S2N_FAILURE` (-1) on failure

**Example**:
```c
if (s2n_connection_wipe(conn) != S2N_SUCCESS) {
    fprintf(stderr, "Error wiping connection: %s\n", s2n_strerror(s2n_errno, "EN"));
    exit(1);
}
```

## I/O Operations

### s2n_negotiate

```c
int s2n_negotiate(struct s2n_connection *conn, s2n_blocked_status *blocked);
```

**Description**: Performs the TLS handshake.

**Parameters**:
- `conn`: The connection object
- `blocked`: A pointer to an `s2n_blocked_status` variable that will be set to indicate if the operation is blocked

**Returns**:
- `S2N_SUCCESS` (0) on success
- `S2N_FAILURE` (-1) on failure

**Example**:
```c
s2n_blocked_status blocked;
if (s2n_negotiate(conn, &blocked) != S2N_SUCCESS) {
    if (s2n_error_get_type(s2n_errno) == S2N_ERR_T_BLOCKED) {
        // Connection is blocked, try again later
    } else {
        fprintf(stderr, "Error negotiating: %s\n", s2n_strerror(s2n_errno, "EN"));
        exit(1);
    }
}
```

### s2n_send

```c
ssize_t s2n_send(struct s2n_connection *conn, const void *buf, ssize_t size, s2n_blocked_status *blocked);
```

**Description**: Sends data over a TLS connection.

**Parameters**:
- `conn`: The connection object
- `buf`: The buffer containing the data to send
- `size`: The size of the data to send
- `blocked`: A pointer to an `s2n_blocked_status` variable that will be set to indicate if the operation is blocked

**Returns**: The number of bytes sent, or -1 on error.

**Example**:
```c
s2n_blocked_status blocked;
ssize_t bytes_sent = s2n_send(conn, buffer, buffer_size, &blocked);
if (bytes_sent < 0) {
    if (s2n_error_get_type(s2n_errno) == S2N_ERR_T_BLOCKED) {
        // Connection is blocked, try again later
    } else {
        fprintf(stderr, "Error sending data: %s\n", s2n_strerror(s2n_errno, "EN"));
        exit(1);
    }
}
```

### s2n_recv

```c
ssize_t s2n_recv(struct s2n_connection *conn, void *buf, ssize_t size, s2n_blocked_status *blocked);
```

**Description**: Receives data from a TLS connection.

**Parameters**:
- `conn`: The connection object
- `buf`: The buffer to store the received data
- `size`: The size of the buffer
- `blocked`: A pointer to an `s2n_blocked_status` variable that will be set to indicate if the operation is blocked

**Returns**: The number of bytes received, or -1 on error.

**Example**:
```c
s2n_blocked_status blocked;
ssize_t bytes_received = s2n_recv(conn, buffer, buffer_size, &blocked);
if (bytes_received < 0) {
    if (s2n_error_get_type(s2n_errno) == S2N_ERR_T_BLOCKED) {
        // Connection is blocked, try again later
    } else {
        fprintf(stderr, "Error receiving data: %s\n", s2n_strerror(s2n_errno, "EN"));
        exit(1);
    }
}
```

### s2n_shutdown

```c
int s2n_shutdown(struct s2n_connection *conn, s2n_blocked_status *blocked);
```

**Description**: Closes a TLS connection.

**Parameters**:
- `conn`: The connection object
- `blocked`: A pointer to an `s2n_blocked_status` variable that will be set to indicate if the operation is blocked

**Returns**:
- `S2N_SUCCESS` (0) on success
- `S2N_FAILURE` (-1) on failure

**Example**:
```c
s2n_blocked_status blocked;
if (s2n_shutdown(conn, &blocked) != S2N_SUCCESS) {
    if (s2n_error_get_type(s2n_errno) == S2N_ERR_T_BLOCKED) {
        // Connection is blocked, try again later
    } else {
        fprintf(stderr, "Error shutting down: %s\n", s2n_strerror(s2n_errno, "EN"));
    }
}
```

## Blobs and Memory Management

### s2n_blob_init

```c
int s2n_blob_init(struct s2n_blob *blob, uint8_t *data, uint32_t size);
```

**Description**: Initializes a blob structure with the given data and size.

**Parameters**:
- `blob`: The blob structure to initialize
- `data`: The data pointer
- `size`: The size of the data

**Returns**:
- `S2N_SUCCESS` (0) on success
- `S2N_FAILURE` (-1) on failure

**Example**:
```c
struct s2n_blob blob;
uint8_t data[1024];
if (s2n_blob_init(&blob, data, sizeof(data)) != S2N_SUCCESS) {
    fprintf(stderr, "Error initializing blob: %s\n", s2n_strerror(s2n_errno, "EN"));
    exit(1);
}
```

### s2n_blob_zero

```c
int s2n_blob_zero(struct s2n_blob *blob);
```

**Description**: Zeros out the memory in a blob.

**Parameters**:
- `blob`: The blob structure

**Returns**:
- `S2N_SUCCESS` (0) on success
- `S2N_FAILURE` (-1) on failure

**Example**:
```c
if (s2n_blob_zero(&blob) != S2N_SUCCESS) {
    fprintf(stderr, "Error zeroing blob: %s\n", s2n_strerror(s2n_errno, "EN"));
    exit(1);
}
```

## Stuffers

### s2n_stuffer_init

```c
int s2n_stuffer_init(struct s2n_stuffer *stuffer, struct s2n_blob *blob);
```

**Description**: Initializes a stuffer with the given blob.

**Parameters**:
- `stuffer`: The stuffer structure to initialize
- `blob`: The blob structure to use for the stuffer

**Returns**:
- `S2N_SUCCESS` (0) on success
- `S2N_FAILURE` (-1) on failure

**Example**:
```c
struct s2n_stuffer stuffer;
struct s2n_blob blob;
uint8_t data[1024];
if (s2n_blob_init(&blob, data, sizeof(data)) != S2N_SUCCESS) {
    // Handle error
}
if (s2n_stuffer_init(&stuffer, &blob) != S2N_SUCCESS) {
    fprintf(stderr, "Error initializing stuffer: %s\n", s2n_strerror(s2n_errno, "EN"));
    exit(1);
}
```

### s2n_stuffer_alloc

```c
int s2n_stuffer_alloc(struct s2n_stuffer *stuffer, uint32_t size);
```

**Description**: Allocates memory for a stuffer.

**Parameters**:
- `stuffer`: The stuffer structure
- `size`: The size to allocate

**Returns**:
- `S2N_SUCCESS` (0) on success
- `S2N_FAILURE` (-1) on failure

**Example**:
```c
struct s2n_stuffer stuffer;
if (s2n_stuffer_alloc(&stuffer, 1024) != S2N_SUCCESS) {
    fprintf(stderr, "Error allocating stuffer: %s\n", s2n_strerror(s2n_errno, "EN"));
    exit(1);
}
```

### s2n_stuffer_free

```c
int s2n_stuffer_free(struct s2n_stuffer *stuffer);
```

**Description**: Frees memory allocated for a stuffer.

**Parameters**:
- `stuffer`: The stuffer structure

**Returns**:
- `S2N_SUCCESS` (0) on success
- `S2N_FAILURE` (-1) on failure

**Example**:
```c
if (s2n_stuffer_free(&stuffer) != S2N_SUCCESS) {
    fprintf(stderr, "Error freeing stuffer: %s\n", s2n_strerror(s2n_errno, "EN"));
}
```

## Crypto Operations

### s2n_hash_new

```c
int s2n_hash_new(struct s2n_hash_state *state);
```

**Description**: Creates a new hash state.

**Parameters**:
- `state`: The hash state structure

**Returns**:
- `S2N_SUCCESS` (0) on success
- `S2N_FAILURE` (-1) on failure

**Example**:
```c
struct s2n_hash_state hash_state;
if (s2n_hash_new(&hash_state) != S2N_SUCCESS) {
    fprintf(stderr, "Error creating hash state: %s\n", s2n_strerror(s2n_errno, "EN"));
    exit(1);
}
```

### s2n_hash_free

```c
int s2n_hash_free(struct s2n_hash_state *state);
```

**Description**: Frees a hash state.

**Parameters**:
- `state`: The hash state structure

**Returns**:
- `S2N_SUCCESS` (0) on success
- `S2N_FAILURE` (-1) on failure

**Example**:
```c
if (s2n_hash_free(&hash_state) != S2N_SUCCESS) {
    fprintf(stderr, "Error freeing hash state: %s\n", s2n_strerror(s2n_errno, "EN"));
}
```

## Advanced Features

### s2n_connection_set_verify_host_callback

```c
int s2n_connection_set_verify_host_callback(struct s2n_connection *conn, s2n_verify_host_fn verify_host_fn, void *data);
```

**Description**: Sets a callback for hostname verification.

**Parameters**:
- `conn`: The connection object
- `verify_host_fn`: The callback function
- `data`: User data to pass to the callback

**Returns**:
- `S2N_SUCCESS` (0) on success
- `S2N_FAILURE` (-1) on failure

**Example**:
```c
int verify_host(const char *host_name, size_t host_name_len, void *data) {
    // Custom hostname verification logic
    return 0; // Return 0 for success, -1 for failure
}

if (s2n_connection_set_verify_host_callback(conn, verify_host, NULL) != S2N_SUCCESS) {
    fprintf(stderr, "Error setting verify host callback: %s\n", s2n_strerror(s2n_errno, "EN"));
    exit(1);
}
```

### s2n_connection_set_session

```c
int s2n_connection_set_session(struct s2n_connection *conn, const uint8_t *session, size_t length);
```

**Description**: Sets session data for session resumption.

**Parameters**:
- `conn`: The connection object
- `session`: The session data
- `length`: The length of the session data

**Returns**:
- `S2N_SUCCESS` (0) on success
- `S2N_FAILURE` (-1) on failure

**Example**:
```c
if (s2n_connection_set_session(conn, session_data, session_data_len) != S2N_SUCCESS) {
    fprintf(stderr, "Error setting session: %s\n", s2n_strerror(s2n_errno, "EN"));
    exit(1);
}
```

## Complete Examples

### TLS Server Example

```c
#include <errno.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <s2n.h>

int main(int argc, char **argv) {
    struct addrinfo hints, *result;
    int listen_fd, conn_fd, ret;
    struct s2n_config *config;
    struct s2n_connection *conn;
    struct s2n_cert_chain_and_key *chain_and_key;
    s2n_blocked_status blocked;
    
    /* Initialize s2n */
    if (s2n_init() != S2N_SUCCESS) {
        fprintf(stderr, "Error initializing s2n: %s\n", s2n_strerror(s2n_errno, "EN"));
        exit(1);
    }
    
    /* Create a socket */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    
    if (getaddrinfo(NULL, "8443", &hints, &result) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", strerror(errno));
        exit(1);
    }
    
    listen_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (listen_fd == -1) {
        fprintf(stderr, "socket error: %s\n", strerror(errno));
        exit(1);
    }
    
    int yes = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        fprintf(stderr, "setsockopt error: %s\n", strerror(errno));
        exit(1);
    }
    
    if (bind(listen_fd, result->ai_addr, result->ai_addrlen) == -1) {
        fprintf(stderr, "bind error: %s\n", strerror(errno));
        exit(1);
    }
    
    if (listen(listen_fd, 128) == -1) {
        fprintf(stderr, "listen error: %s\n", strerror(errno));
        exit(1);
    }
    
    freeaddrinfo(result);
    
    /* Create s2n config */
    config = s2n_config_new();
    if (config == NULL) {
        fprintf(stderr, "Error creating config: %s\n", s2n_strerror(s2n_errno, "EN"));
        exit(1);
    }
    
    /* Load certificate and key */
    if (s2n_cert_chain_and_key_new(&chain_and_key) != S2N_SUCCESS) {
        fprintf(stderr, "Error creating cert chain and key: %s\n", s2n_strerror(s2n_errno, "EN"));
        exit(1);
    }
    
    if (s2n_cert_chain_and_key_load_pem(chain_and_key, "cert.pem", "key.pem") != S2N_SUCCESS) {
        fprintf(stderr, "Error loading cert and key: %s\n", s2n_strerror(s2n_errno, "EN"));
        exit(1);
    }
    
    if (s2n_config_add_cert_chain_and_key_to_store(config, chain_and_key) != S2N_SUCCESS) {
        fprintf(stderr, "Error adding cert and key to config: %s\n", s2n_strerror(s2n_errno, "EN"));
        exit(1);
    }
    
    /* Set cipher preferences */
    if (s2n_config_set_cipher_preferences(config, "default") != S2N_SUCCESS) {
        fprintf(stderr, "Error setting cipher preferences: %s\n", s2n_strerror(s2n_errno, "EN"));
        exit(1);
    }
    
    /* Accept connections */
    while (1) {
        conn_fd = accept(listen_fd, NULL, NULL);
        if (conn_fd == -1) {
            fprintf(stderr, "accept error: %s\n", strerror(errno));
            continue;
        }
        
        /* Create s2n connection */
        conn = s2n_connection_new(S2N_SERVER);
        if (conn == NULL) {
            fprintf(stderr, "Error creating connection: %s\n", s2n_strerror(s2n_errno, "EN"));
            close(conn_fd);
            continue;
        }
        
        /* Set config */
        if (s2n_connection_set_config(conn, config) != S2N_SUCCESS) {
            fprintf(stderr, "Error setting config: %s\n", s2n_strerror(s2n_errno, "EN"));
            s2n_connection_free(conn);
            close(conn_fd);
            continue;
        }
        
        /* Set fd */
        if (s2n_connection_set_fd(conn, conn_fd) != S2N_SUCCESS) {
            fprintf(stderr, "Error setting fd: %s\n", s2n_strerror(s2n_errno, "EN"));
            s2n_connection_free(conn);
            close(conn_fd);
            continue;
        }
        
        /* Negotiate */
        do {
            ret = s2n_negotiate(conn, &blocked);
            if (ret != S2N_SUCCESS && s2n_error_get_type(s2n_errno) != S2N_ERR_T_BLOCKED) {
                fprintf(stderr, "Error negotiating: %s\n", s2n_strerror(s2n_errno, "EN"));
                s2n_connection_free(conn);
                close(conn_fd);
                continue;
            }
        } while (ret != S2N_SUCCESS);
        
        /* Echo data */
        uint8_t buffer[1024];
        while (1) {
            int bytes_read;
            do {
                bytes_read = s2n_recv(conn, buffer, sizeof(buffer), &blocked);
                if (bytes_read < 0 && s2n_error_get_type(s2n_errno) != S2N_ERR_T_BLOCKED) {
                    fprintf(stderr, "Error receiving: %s\n", s2n_strerror(s2n_errno, "EN"));
                    break;
                }
            } while (bytes_read < 0);
            
            if (bytes_read == 0) {
                break;
            }
            
            int bytes_written;
            do {
                bytes_written = s2n_send(conn, buffer, bytes_read, &blocked);
                if (bytes_written < 0 && s2n_error_get_type(s2n_errno) != S2N_ERR_T_BLOCKED) {
                    fprintf(stderr, "Error sending: %s\n", s2n_strerror(s2n_errno, "EN"));
                    break;
                }
            } while (bytes_written < 0);
        }
        
        /* Shutdown */
        do {
            ret = s2n_shutdown(conn, &blocked);
            if (ret != S2N_SUCCESS && s2n_error_get_type(s2n_errno) != S2N_ERR_T_BLOCKED
