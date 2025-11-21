# s2n-tls Common Patterns

This document describes common code patterns, idioms, and conventions used throughout the s2n-tls codebase. Understanding these patterns will help you read, understand, and contribute to the project more effectively.

## Table of Contents

1. [Error Handling Patterns](#error-handling-patterns)
2. [Memory Management Patterns](#memory-management-patterns)
3. [State Machine Patterns](#state-machine-patterns)
4. [I/O Patterns](#io-patterns)
5. [Crypto Operation Patterns](#crypto-operation-patterns)
6. [Testing Patterns](#testing-patterns)
7. [Defensive Programming Patterns](#defensive-programming-patterns)

## Error Handling Patterns

s2n-tls uses a consistent error handling approach throughout the codebase. Understanding these patterns is crucial for working with the library effectively.

### Return Values

Most functions in s2n-tls return an integer status code:
- `S2N_SUCCESS` (0) indicates success
- `S2N_FAILURE` (-1) indicates failure

When a function fails, it sets the thread-local `s2n_errno` variable to indicate the specific error.

```c
if (s2n_function() != S2N_SUCCESS) {
    /* Handle error */
    fprintf(stderr, "Error: %s\n", s2n_strerror(s2n_errno, "EN"));
}
```

### GUARD Macro

The `GUARD` macro is used extensively to propagate errors up the call stack. It checks if a function returns `S2N_FAILURE` and, if so, immediately returns `S2N_FAILURE` from the current function.

```c
/* Definition of GUARD macro */
#define GUARD(x) do { \
    if ((x) < 0) { \
        return S2N_FAILURE; \
    } \
} while (0)

/* Example usage */
GUARD(s2n_stuffer_read_uint16(in, &length));
```

### GUARD_PTR Macro

Similar to `GUARD`, but returns `NULL` instead of `S2N_FAILURE`.

```c
/* Definition of GUARD_PTR macro */
#define GUARD_PTR(x) do { \
    if ((x) < 0) { \
        return NULL; \
    } \
} while (0)

/* Example usage */
GUARD_PTR(s2n_alloc(&blob, size));
```

### ENSURE Macro

The `ENSURE` macro checks a condition and returns an error if the condition is false.

```c
/* Definition of ENSURE macro */
#define ENSURE(condition, error) do { \
    if (!(condition)) { \
        S2N_ERROR(error); \
    } \
} while (0)

/* Example usage */
ENSURE(length <= max_allowed, S2N_ERR_TOO_LARGE);
```

### BAIL Macro

The `BAIL` macro sets the error code and returns `S2N_FAILURE`.

```c
/* Definition of BAIL macro */
#define BAIL(error) do { \
    S2N_ERROR(error); \
} while (0)

/* Example usage */
if (invalid_condition) {
    BAIL(S2N_ERR_INVALID_ARGUMENT);
}
```

### Notnull Macro

The `notnull` macro checks if a pointer is not NULL.

```c
/* Definition of notnull macro */
#define notnull_check(ptr) do { \
    if ((ptr) == NULL) { \
        BAIL(S2N_ERR_NULL); \
    } \
} while (0)

/* Example usage */
notnull_check(conn);
```

### Error Types

s2n-tls categorizes errors into different types:

```c
enum s2n_error_type {
    S2N_ERR_T_OK = 0,
    S2N_ERR_T_IO,
    S2N_ERR_T_CLOSED,
    S2N_ERR_T_BLOCKED,
    S2N_ERR_T_ALERT,
    S2N_ERR_T_PROTO,
    S2N_ERR_T_INTERNAL,
    S2N_ERR_T_USAGE
};
```

You can check the type of an error using `s2n_error_get_type()`:

```c
if (s2n_negotiate(conn, &blocked) != S2N_SUCCESS) {
    if (s2n_error_get_type(s2n_errno) == S2N_ERR_T_BLOCKED) {
        /* Connection is blocked, try again later */
    } else {
        /* Handle other errors */
    }
}
```

## Memory Management Patterns

s2n-tls uses a structured approach to memory management to prevent leaks and ensure security.

### Blobs

The `s2n_blob` structure is the basic unit of memory management in s2n-tls:

```c
struct s2n_blob {
    uint8_t *data;
    uint32_t size;
};
```

Blobs are used to track memory regions and ensure proper bounds checking.

```c
/* Initialize a blob with existing data */
struct s2n_blob blob;
GUARD(s2n_blob_init(&blob, data, size));

/* Allocate a new blob */
struct s2n_blob blob;
GUARD(s2n_alloc(&blob, size));

/* Zero a blob */
GUARD(s2n_blob_zero(&blob));

/* Free a blob */
GUARD(s2n_free(&blob));
```

### Stuffers

Stuffers are built on top of blobs and provide stream-oriented buffer operations:

```c
struct s2n_stuffer {
    struct s2n_blob blob;
    uint32_t read_cursor;
    uint32_t write_cursor;
    /* Additional fields omitted */
};
```

Stuffers are used extensively for parsing and generating TLS protocol messages.

```c
/* Initialize a stuffer with existing data */
struct s2n_stuffer stuffer;
GUARD(s2n_stuffer_init(&stuffer, &blob));

/* Allocate a new stuffer */
struct s2n_stuffer stuffer;
GUARD(s2n_stuffer_alloc(&stuffer, size));

/* Read from a stuffer */
uint8_t value;
GUARD(s2n_stuffer_read_uint8(&stuffer, &value));

/* Write to a stuffer */
GUARD(s2n_stuffer_write_uint8(&stuffer, value));

/* Free a stuffer */
GUARD(s2n_stuffer_free(&stuffer));
```

### Memory Cleanup

s2n-tls ensures that sensitive data is zeroed before memory is freed:

```c
/* Zero and free a blob */
GUARD(s2n_blob_zero(&blob));
GUARD(s2n_free(&blob));

/* Wipe a connection for reuse */
GUARD(s2n_connection_wipe(conn));

/* Free a connection */
GUARD(s2n_connection_free(conn));
```

## State Machine Patterns

s2n-tls implements the TLS protocol using a table-driven state machine to prevent invalid state transitions.

### Handshake State Machine

The handshake state machine is defined using a table of state transitions:

```c
static struct s2n_handshake_action state_machine[] = {
    /* Initial state */
    [CLIENT_HELLO] = {
        .record_type = TLS_HANDSHAKE,
        .message_type = TLS_CLIENT_HELLO,
        .handler[S2N_SERVER] = s2n_server_handle_client_hello,
        .next_state[S2N_SERVER] = SERVER_HELLO,
    },
    /* Additional states omitted */
};
```

The state machine is executed by the `s2n_advance_message` function, which ensures that only valid state transitions occur.

### Connection States

The `s2n_connection` structure includes fields to track the current state of the connection:

```c
struct s2n_connection {
    /* Current state of the handshake */
    s2n_handshake_status handshake_status;
    
    /* Current handshake state */
    enum handshake_state handshake_state;
    
    /* Additional fields omitted */
};
```

## I/O Patterns

s2n-tls provides a simple I/O abstraction for sending and receiving data over a TLS connection.

### Blocked I/O Handling

s2n-tls uses non-blocking I/O and provides a mechanism to handle blocked operations:

```c
s2n_blocked_status blocked;
int ret;

do {
    ret = s2n_negotiate(conn, &blocked);
    if (ret != S2N_SUCCESS && s2n_error_get_type(s2n_errno) != S2N_ERR_T_BLOCKED) {
        /* Handle error */
        break;
    }
} while (ret != S2N_SUCCESS);
```

### Send and Receive

The `s2n_send` and `s2n_recv` functions are used to send and receive data over a TLS connection:

```c
/* Send data */
s2n_blocked_status blocked;
ssize_t bytes_sent = s2n_send(conn, buffer, buffer_size, &blocked);
if (bytes_sent < 0) {
    /* Handle error */
}

/* Receive data */
s2n_blocked_status blocked;
ssize_t bytes_received = s2n_recv(conn, buffer, buffer_size, &blocked);
if (bytes_received < 0) {
    /* Handle error */
}
```

## Crypto Operation Patterns

s2n-tls provides a consistent interface for cryptographic operations.

### Hash Operations

```c
/* Initialize a hash state */
struct s2n_hash_state hash_state;
GUARD(s2n_hash_new(&hash_state));
GUARD(s2n_hash_init(&hash_state, S2N_HASH_SHA256));

/* Update the hash */
GUARD(s2n_hash_update(&hash_state, data, size));

/* Finalize the hash */
uint8_t digest[32];
GUARD(s2n_hash_digest(&hash_state, digest, sizeof(digest)));

/* Free the hash state */
GUARD(s2n_hash_free(&hash_state));
```

### HMAC Operations

```c
/* Initialize an HMAC state */
struct s2n_hmac_state hmac_state;
GUARD(s2n_hmac_new(&hmac_state));
GUARD(s2n_hmac_init(&hmac_state, S2N_HMAC_SHA256, key, key_size));

/* Update the HMAC */
GUARD(s2n_hmac_update(&hmac_state, data, size));

/* Finalize the HMAC */
uint8_t digest[32];
GUARD(s2n_hmac_digest(&hmac_state, digest, sizeof(digest)));

/* Free the HMAC state */
GUARD(s2n_hmac_free(&hmac_state));
```

## Testing Patterns

s2n-tls includes a comprehensive test suite that follows consistent patterns.

### Unit Tests

Unit tests are written using a simple test framework:

```c
int main(int argc, char **argv) {
    BEGIN_TEST();
    
    /* Test code */
    EXPECT_SUCCESS(s2n_function_to_test());
    EXPECT_FAILURE(s2n_function_that_should_fail());
    EXPECT_EQUAL(value1, value2);
    
    END_TEST();
}
```

### Mocks

s2n-tls uses function pointers to enable mocking for testing:

```c
/* Define a mock function */
int mock_function(void *data) {
    /* Mock implementation */
    return S2N_SUCCESS;
}

/* Set the mock */
original_function = s2n_actual_function;
s2n_actual_function = mock_function;

/* Test with the mock */
EXPECT_SUCCESS(function_that_uses_s2n_actual_function());

/* Restore the original function */
s2n_actual_function = original_function;
```

## Defensive Programming Patterns

s2n-tls employs defensive programming techniques to ensure security and reliability.

### Constant-Time Operations

Security-critical operations are implemented to run in constant time to prevent timing side-channel attacks:

```c
/* Constant-time comparison */
int s2n_constant_time_equals(const uint8_t *a, const uint8_t *b, uint32_t len) {
    uint8_t xor = 0;
    for (uint32_t i = 0; i < len; i++) {
        xor |= a[i] ^ b[i];
    }
    return !xor;
}

/* Constant-time conditional copy */
int s2n_constant_time_copy_or_dont(uint8_t *dest, const uint8_t *src, uint32_t len, uint8_t dont) {
    uint8_t mask = ~(dont - 1);
    for (uint32_t i = 0; i < len; i++) {
        dest[i] = (src[i] & mask) | (dest[i] & ~mask);
    }
    return S2N_SUCCESS;
}
```

### Bounds Checking

All array accesses include bounds checking to prevent buffer overflows:

```c
/* Ensure the stuffer has enough data */
ENSURE(s2n_stuffer_data_available(in) >= length, S2N_ERR_BAD_MESSAGE);

/* Read data from the stuffer */
GUARD(s2n_stuffer_read_bytes(in, data, length));
```

### Input Validation

All inputs are validated before processing:

```c
/* Validate a certificate */
ENSURE(cert != NULL, S2N_ERR_NULL);
ENSURE(cert->raw.size > 0, S2N_ERR_INVALID_ARGUMENT);

/* Validate a cipher suite */
ENSURE(cipher_suite->available, S2N_ERR_CIPHER_NOT_SUPPORTED);
```

### Explicit Initialization

All data structures are explicitly initialized to prevent undefined behavior:

```c
/* Initialize a connection */
struct s2n_connection *conn = s2n_connection_new(S2N_SERVER);
if (conn == NULL) {
    /* Handle error */
}

/* Initialize a config */
struct s2n_config *config = s2n_config_new();
if (config == NULL) {
    /* Handle error */
}
```

### Resource Cleanup

Resources are always cleaned up, even on error paths:

```c
struct s2n_connection *conn = s2n_connection_new(S2N_SERVER);
if (conn == NULL) {
    /* Handle error */
}

struct s2n_config *config = s2n_config_new();
if (config == NULL) {
    s2n_connection_free(conn);
    /* Handle error */
}

if (s2n_connection_set_config(conn, config) != S2N_SUCCESS) {
    s2n_connection_free(conn);
    s2n_config_free(config);
    /* Handle error */
}

/* Use the connection and config */

/* Clean up */
s2n_connection_free(conn);
s2n_config_free(config);
