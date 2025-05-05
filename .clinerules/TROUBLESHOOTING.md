# s2n-tls Troubleshooting Guide

This document provides guidance on common issues encountered when using s2n-tls, along with their solutions. It covers error codes, debugging techniques, and common pitfalls.

## Table of Contents

1. [Understanding Error Codes](#understanding-error-codes)
2. [Common Issues and Solutions](#common-issues-and-solutions)
3. [Debugging Techniques](#debugging-techniques)
4. [Performance Issues](#performance-issues)
5. [Compatibility Issues](#compatibility-issues)
6. [Build Issues](#build-issues)

## Understanding Error Codes

s2n-tls uses a consistent error reporting mechanism. When a function returns `S2N_FAILURE` (-1), it sets the thread-local `s2n_errno` variable to indicate the specific error.

### Error Types

Errors in s2n-tls are categorized into different types:

| Error Type | Description |
|------------|-------------|
| `S2N_ERR_T_OK` | No error |
| `S2N_ERR_T_IO` | I/O error |
| `S2N_ERR_T_CLOSED` | Connection closed |
| `S2N_ERR_T_BLOCKED` | Operation would block |
| `S2N_ERR_T_ALERT` | TLS alert received |
| `S2N_ERR_T_PROTO` | Protocol error |
| `S2N_ERR_T_INTERNAL` | Internal error |
| `S2N_ERR_T_USAGE` | API usage error |

You can check the type of an error using `s2n_error_get_type()`:

```c
if (s2n_negotiate(conn, &blocked) != S2N_SUCCESS) {
    if (s2n_error_get_type(s2n_errno) == S2N_ERR_T_BLOCKED) {
        /* Connection is blocked, try again later */
    } else {
        /* Handle other errors */
        fprintf(stderr, "Error: %s\n", s2n_strerror(s2n_errno, "EN"));
        fprintf(stderr, "Debug: %s\n", s2n_strerror_debug(s2n_errno, "EN"));
    }
}
```

### Common Error Codes

| Error Code | Description | Possible Causes |
|------------|-------------|----------------|
| `S2N_ERR_NULL` | Null pointer | A required pointer parameter was NULL |
| `S2N_ERR_CLOSED` | Connection closed | The connection was closed by the peer |
| `S2N_ERR_BLOCKED` | Operation would block | Non-blocking I/O would block |
| `S2N_ERR_ALERT` | TLS alert received | The peer sent a TLS alert |
| `S2N_ERR_ENCRYPT` | Encryption failure | Failed to encrypt data |
| `S2N_ERR_DECRYPT` | Decryption failure | Failed to decrypt data |
| `S2N_ERR_BAD_MESSAGE` | Bad message | Received an invalid TLS message |
| `S2N_ERR_KEY_INIT` | Key initialization error | Failed to initialize a key |
| `S2N_ERR_DH_PARAMS_CREATE` | DH params create error | Failed to create DH parameters |
| `S2N_ERR_SIGN` | Sign error | Failed to sign data |
| `S2N_ERR_VERIFY_SIGNATURE` | Verify signature error | Failed to verify a signature |
| `S2N_ERR_ALLOC` | Allocation error | Failed to allocate memory |
| `S2N_ERR_MLOCK` | Mlock error | Failed to lock memory |
| `S2N_ERR_MUNLOCK` | Munlock error | Failed to unlock memory |
| `S2N_ERR_MMAP` | Mmap error | Failed to map memory |
| `S2N_ERR_MUNMAP` | Munmap error | Failed to unmap memory |
| `S2N_ERR_INVALID_ARGUMENT` | Invalid argument | An argument to a function was invalid |
| `S2N_ERR_PRECONDITION_VIOLATION` | Precondition violation | A precondition for a function was not met |

## Common Issues and Solutions

### 1. Handshake Failures

#### Issue: Handshake fails with `S2N_ERR_BAD_MESSAGE`

**Possible Causes**:
- Protocol version mismatch
- Cipher suite mismatch
- Malformed message

**Solutions**:
- Check that both client and server support compatible TLS versions
- Ensure that the client and server have at least one cipher suite in common
- Verify that the messages are not being corrupted in transit

```c
/* Set compatible cipher preferences */
if (s2n_config_set_cipher_preferences(config, "default_tls13") != S2N_SUCCESS) {
    /* Handle error */
}
```

#### Issue: Handshake fails with `S2N_ERR_CERT_UNTRUSTED`

**Possible Causes**:
- The server's certificate is not trusted by the client
- The certificate has expired
- The certificate has been revoked

**Solutions**:
- Add the server's CA certificate to the client's trust store
- Update the server's certificate if it has expired
- Check the certificate's revocation status

```c
/* Add a CA certificate to the trust store */
if (s2n_config_add_pem_to_trust_store(config, ca_pem) != S2N_SUCCESS) {
    /* Handle error */
}
```

### 2. I/O Issues

#### Issue: `s2n_negotiate` returns `S2N_ERR_T_BLOCKED`

**Possible Causes**:
- Non-blocking I/O would block

**Solutions**:
- Retry the operation when the socket is ready for I/O

```c
s2n_blocked_status blocked;
int ret;

do {
    ret = s2n_negotiate(conn, &blocked);
    if (ret != S2N_SUCCESS && s2n_error_get_type(s2n_errno) != S2N_ERR_T_BLOCKED) {
        /* Handle error */
        break;
    }
    
    /* If blocked, wait for the socket to be ready */
    if (blocked == S2N_BLOCKED_ON_READ) {
        /* Wait for the socket to be readable */
    } else if (blocked == S2N_BLOCKED_ON_WRITE) {
        /* Wait for the socket to be writable */
    }
} while (ret != S2N_SUCCESS);
```

#### Issue: `s2n_send` or `s2n_recv` returns -1

**Possible Causes**:
- Connection closed
- I/O error
- Non-blocking I/O would block

**Solutions**:
- Check the error type using `s2n_error_get_type()`
- Handle closed connections and I/O errors appropriately
- Retry blocked operations when the socket is ready

```c
s2n_blocked_status blocked;
ssize_t bytes_received = s2n_recv(conn, buffer, buffer_size, &blocked);
if (bytes_received < 0) {
    if (s2n_error_get_type(s2n_errno) == S2N_ERR_T_BLOCKED) {
        /* Retry when the socket is ready */
    } else if (s2n_error_get_type(s2n_errno) == S2N_ERR_T_CLOSED) {
        /* Connection closed */
    } else {
        /* Handle other errors */
    }
}
```

### 3. Certificate Issues

#### Issue: Certificate validation fails

**Possible Causes**:
- The certificate is not trusted
- The certificate has expired
- The certificate's hostname does not match the expected hostname

**Solutions**:
- Add the CA certificate to the trust store
- Update the certificate if it has expired
- Ensure the certificate's hostname matches the expected hostname

```c
/* Set the verification hostname */
if (s2n_set_server_name(conn, "example.com") != S2N_SUCCESS) {
    /* Handle error */
}
```

#### Issue: Unable to load certificate and key

**Possible Causes**:
- File not found
- Invalid format
- Incorrect password for encrypted key

**Solutions**:
- Check that the file paths are correct
- Ensure the files are in PEM format
- Provide the correct password for encrypted keys

```c
/* Load certificate and key */
struct s2n_cert_chain_and_key *chain_and_key = NULL;
if (s2n_cert_chain_and_key_new(&chain_and_key) != S2N_SUCCESS) {
    /* Handle error */
}

if (s2n_cert_chain_and_key_load_pem(chain_and_key, "cert.pem", "key.pem") != S2N_SUCCESS) {
    /* Handle error */
    fprintf(stderr, "Error loading cert and key: %s\n", s2n_strerror(s2n_errno, "EN"));
}
```

### 4. Memory Issues

#### Issue: Memory leaks

**Possible Causes**:
- Not freeing resources
- Early returns without cleanup

**Solutions**:
- Always free resources when done with them
- Use cleanup functions in all exit paths

```c
struct s2n_connection *conn = s2n_connection_new(S2N_SERVER);
if (conn == NULL) {
    /* Handle error */
}

/* ... use connection ... */

/* Always free the connection when done */
if (s2n_connection_free(conn) != S2N_SUCCESS) {
    /* Handle error */
}
```

#### Issue: `S2N_ERR_ALLOC` error

**Possible Causes**:
- Out of memory
- Memory allocation failure

**Solutions**:
- Check system memory usage
- Reduce memory usage in the application

```c
/* Handle allocation errors */
if (s2n_alloc(&blob, size) != S2N_SUCCESS) {
    if (s2n_errno == S2N_ERR_ALLOC) {
        /* Handle out of memory */
    } else {
        /* Handle other errors */
    }
}
```

## Debugging Techniques

### 1. Enable Debug Output

s2n-tls provides detailed error messages that can help diagnose issues:

```c
if (s2n_negotiate(conn, &blocked) != S2N_SUCCESS) {
    fprintf(stderr, "Error: %s\n", s2n_strerror(s2n_errno, "EN"));
    fprintf(stderr, "Debug: %s\n", s2n_strerror_debug(s2n_errno, "EN"));
}
```

### 2. Use Wireshark to Analyze TLS Traffic

Wireshark can capture and analyze TLS traffic, which can help diagnose protocol-level issues:

1. Capture traffic using Wireshark
2. Filter for TLS traffic using the filter `tls`
3. Analyze the TLS handshake and data exchange

Note: Wireshark cannot decrypt TLS traffic unless you provide the session keys.

### 3. Use s2n-tls's Built-in Tracing

s2n-tls includes tracing capabilities that can be enabled at compile time:

```bash
# Build with tracing enabled
make CFLAGS="-DS2N_TRACE=1"
```

This will output detailed information about the TLS handshake and data exchange.

### 4. Check for Memory Leaks

Use tools like Valgrind to check for memory leaks:

```bash
valgrind --leak-check=full --show-leak-kinds=all ./your_program
```

### 5. Use GDB for Debugging

GDB can be used to debug s2n-tls applications:

```bash
gdb ./your_program
```

Common GDB commands:
- `break s2n_function_name` - Set a breakpoint at a function
- `run` - Start the program
- `bt` - Show the backtrace
- `print variable` - Print the value of a variable
- `continue` - Continue execution

## Performance Issues

### 1. Slow Handshakes

#### Possible Causes:
- Large certificate chains
- Expensive cipher suites
- Network latency

#### Solutions:
- Use smaller certificate chains
- Use more efficient cipher suites
- Enable session resumption

```c
/* Enable session resumption */
if (s2n_config_set_session_cache_onoff(config, 1) != S2N_SUCCESS) {
    /* Handle error */
}
```

### 2. Slow Data Transfer

#### Possible Causes:
- Inefficient cipher suite
- Small buffer sizes
- Network congestion

#### Solutions:
- Use more efficient cipher suites
- Increase buffer sizes
- Optimize network usage

```c
/* Use a more efficient cipher suite */
if (s2n_config_set_cipher_preferences(config, "20190801") != S2N_SUCCESS) {
    /* Handle error */
}
```

## Compatibility Issues

### 1. Incompatible TLS Versions

#### Possible Causes:
- Client and server support different TLS versions

#### Solutions:
- Configure both client and server to use compatible TLS versions

```c
/* Set security policy with TLS 1.2 and TLS 1.3 support */
if (s2n_config_set_cipher_preferences(config, "default_tls13") != S2N_SUCCESS) {
    /* Handle error */
}
```

### 2. Incompatible Cipher Suites

#### Possible Causes:
- Client and server have no cipher suites in common

#### Solutions:
- Configure both client and server to use compatible cipher suites

```c
/* Set a widely compatible cipher preference */
if (s2n_config_set_cipher_preferences(config, "default") != S2N_SUCCESS) {
    /* Handle error */
}
```

### 3. Incompatible Extensions

#### Possible Causes:
- Client and server handle TLS extensions differently

#### Solutions:
- Configure extensions to be compatible

```c
/* Disable OCSP stapling if causing compatibility issues */
if (s2n_config_set_status_request_type(config, S2N_STATUS_REQUEST_NONE) != S2N_SUCCESS) {
    /* Handle error */
}
```

## Build Issues

### 1. Missing Dependencies

#### Possible Causes:
- Required libraries not installed

#### Solutions:
- Install required dependencies

```bash
# On Ubuntu/Debian
sudo apt-get install libssl-dev

# On CentOS/RHEL
sudo yum install openssl-devel

# On macOS
brew install openssl
```

### 2. Compilation Errors

#### Possible Causes:
- Incompatible compiler version
- Missing header files
- Incorrect build flags

#### Solutions:
- Use a compatible compiler version
- Install missing header files
- Set correct build flags

```bash
# Set compiler and flags
export CC=gcc
export CFLAGS="-I/path/to/include -O2"
export LDFLAGS="-L/path/to/lib"

# Build s2n-tls
make
```

### 3. Test Failures

#### Possible Causes:
- Environment issues
- Code changes breaking tests

#### Solutions:
- Fix environment issues
- Fix code changes to pass tests

```bash
# Run tests
make test
```

If specific tests are failing, you can run them individually:

```bash
# Run a specific test
./tests/unit/s2n_test_name
