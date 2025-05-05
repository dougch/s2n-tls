# s2n-tls Memory Bank

This document serves as a comprehensive reference for the s2n-tls project, designed to help AI assistants provide accurate and helpful responses about the project without extensive additional prompting.

## 1. Project Overview

s2n-tls is a C99 implementation of the TLS/SSL protocols that is designed to be simple, small, fast, and with security as a priority. The name "s2n" is short for "signal to noise," referring to the act of encryption which transforms meaningful signals (data) into seemingly random noise.

### Key Features

- **Security-focused**: Prioritizes security over feature completeness
- **Simple API**: Designed to be easy to use correctly
- **Minimal dependencies**: Only requires libcrypto
- **Thorough testing**: Includes unit tests, integration tests, and fuzz testing

### Design Principles

1. **Maintain an excellent TLS/SSL implementation**
2. **Protect user data and keys**
3. **Stay simple**
4. **Write clear readable code with a light cognitive load**
5. **Defense in depth and systematically**
6. **Be easy to use and maintain sane defaults**
7. **Provide great performance and responsiveness**
8. **Stay paranoid**
9. **Make data-driven decisions**

### License and Maintenance

s2n-tls is released and licensed under the Apache License 2.0 and is maintained by Amazon Web Services (AWS).

## 2. Open Source Information

### Apache 2.0 License

s2n-tls is licensed under the Apache License 2.0, which allows users to:
- Use the software for any purpose
- Distribute the software
- Modify the software
- Distribute modified versions of the software

### Copyright Header Requirements

Every source code file in s2n-tls must include a copy of the Apache Software License 2.0 header, as well as a correct copyright notification. The year of copyright should be the year in which the file was first created.

Example of the required header:

```c
/*
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
```

### Contribution Guidelines

Contributions to s2n-tls follow a review-then-commit model for code changes. Code additions are made by pull requests, and no author may merge their own pull request on code. Changes to documentation, including code comments, may be made more freely.

Contributors should:
- Read all documentation in the "docs/" directory
- For significant contributions, discuss the change by creating an issue first
- Report security-critical bugs via AWS's security vulnerability reporting process
- Ensure all code passes tests and follows the project's coding style
- Use conventional commit messages for all commits

#### Conventional Commit Messages

s2n-tls follows the conventional commit message format to maintain a clear and structured commit history. This format makes it easier to generate changelogs and understand the purpose of changes at a glance.

The basic structure of a conventional commit message is:

```
<type>[optional scope]: <description>

[optional body]

[optional footer(s)]
```

Common types include:
- `feat`: A new feature
- `fix`: A bug fix
- `docs`: Documentation changes
- `style`: Changes that don't affect code functionality (formatting, etc.)
- `refactor`: Code changes that neither fix bugs nor add features
- `test`: Adding or modifying tests
- `chore`: Changes to the build process, tools, etc.

Example commit messages:
```
feat(crypto): add support for new cipher suite
fix(stuffer): prevent buffer overflow in s2n_stuffer_read
docs: update API documentation for s2n_connection_new
test(handshake): add test case for client auth
```

## 3. Architecture Overview

### High-Level Architecture

s2n-tls is structured around a few key components:

```
s2n-tls
├── Memory Management (blobs and stuffers)
├── TLS State Machine
├── Crypto Primitives (via libcrypto)
├── I/O Abstraction
└── Error Handling
```

### Key Components

1. **s2n_connection**: The core structure representing a TLS connection
2. **s2n_config**: Configuration object for TLS settings
3. **s2n_blob**: Basic memory management structure
4. **s2n_stuffer**: Stream-oriented buffer for protocol handling
5. **State Machine**: Handles TLS protocol states and transitions

### Data Flow

1. Application creates a connection and config
2. TLS handshake is performed via s2n_negotiate()
3. Data is encrypted/decrypted via s2n_send()/s2n_recv()
4. Connection is closed via s2n_shutdown()

## 4. Core Concepts

### Memory Handling: Blobs and Stuffers

#### s2n_blob

A simple structure for tracking memory regions:

```c
struct s2n_blob {
    uint8_t *data;
    uint32_t size;
};
```

#### s2n_stuffer

A streaming buffer built on top of blobs:

```c
struct s2n_stuffer {
    struct s2n_blob blob;
    uint32_t read_cursor;
    uint32_t write_cursor;
    /* Additional fields omitted */
};
```

Stuffers provide:
- Stream-oriented reading/writing
- Automatic resizing (if configured as growable)
- Built-in serialization/deserialization functions
- Memory wiping for security

### Connection Lifecycle

1. **Creation**: `s2n_connection_new()`
2. **Configuration**: Set callbacks, preferences, certificates
3. **Handshake**: `s2n_negotiate()`
4. **Data Transfer**: `s2n_send()` and `s2n_recv()`
5. **Shutdown**: `s2n_shutdown()`
6. **Cleanup**: `s2n_connection_free()` or `s2n_connection_wipe()`

### State Machine

s2n-tls implements the TLS protocol using a table-driven state machine. The state machine handles:
- Message sequencing
- Protocol version negotiation
- Cipher suite negotiation
- Key exchange
- Authentication

The state machine is implemented using function pointers to avoid deep nesting of conditional logic.

### Error Handling Approach

s2n-tls uses a consistent error handling pattern:
- Functions return `S2N_SUCCESS` (0) on success or `S2N_FAILURE` (-1) on failure
- A thread-local `s2n_errno` variable indicates the specific error
- The `GUARD` macro is used to propagate errors up the call stack
- Blinding is used to prevent timing side-channels

### Security Mechanisms

1. **Compartmentalized random number generation**: Separate RNGs for public and private data
2. **Timing blinding**: Adds random delays to prevent timing side-channels
3. **Memory protection**: Prevents sensitive data from being swapped to disk
4. **Minimal feature set**: Avoids rarely used options with security implications
5. **Systematic bounds checking**: Prevents buffer overflows
6. **Table-based state machines**: Prevents invalid state transitions

## 5. API Reference Summary

### Initialization and Cleanup

- `s2n_init()`: Initialize the s2n-tls library
- `s2n_cleanup()`: Clean up thread-local resources
- `s2n_cleanup_final()`: Complete deinitialization of the library

### Configuration

- `s2n_config_new()`: Create a new configuration object
- `s2n_config_free()`: Free a configuration object
- `s2n_config_set_cipher_preferences()`: Set security policy
- `s2n_config_add_cert_chain_and_key_to_store()`: Add a certificate and key

### Connection Management

- `s2n_connection_new()`: Create a new connection
- `s2n_connection_set_config()`: Associate a config with a connection
- `s2n_connection_set_fd()`: Set file descriptors for I/O
- `s2n_connection_free()`: Free a connection
- `s2n_connection_wipe()`: Reset a connection for reuse

### I/O Operations

- `s2n_negotiate()`: Perform the TLS handshake
- `s2n_send()`: Send encrypted data
- `s2n_recv()`: Receive and decrypt data
- `s2n_shutdown()`: Close the TLS connection

### Error Handling

- `s2n_error_get_type()`: Get the category of an error
- `s2n_strerror()`: Get a human-readable error message
- `s2n_strerror_debug()`: Get detailed debug information for an error

## 6. Code Organization

### Directory Structure

```
s2n-tls/
├── api/            # Public API headers
├── bin/            # Example applications
├── crypto/         # Cryptographic operations
├── docs/           # Documentation
├── error/          # Error handling
├── stuffer/        # Buffer management
├── tests/          # Test suite
├── tls/            # TLS protocol implementation
└── utils/          # Utility functions
```

### Key Files

- `api/s2n.h`: Main public API header
- `tls/s2n_handshake_io.c`: TLS state machine
- `tls/s2n_connection.c`: Connection management
- `crypto/s2n_hash.c`: Cryptographic hash functions
- `stuffer/s2n_stuffer.c`: Buffer management

### Component Interactions

- **API Layer**: Provides a simple interface for applications
- **TLS Layer**: Implements the TLS protocol
- **Crypto Layer**: Interfaces with libcrypto for cryptographic operations
- **Stuffer Layer**: Handles buffer management and serialization
- **Error Layer**: Provides consistent error reporting

## 7. Development Patterns

### Error Handling Conventions

s2n-tls uses macros to simplify error handling:

- `GUARD(x)`: Ensures `x` is not an error, otherwise returns `S2N_FAILURE`
- `GUARD_PTR(x)`: Similar to `GUARD` but returns `NULL` on failure
- `ENSURE(condition, error)`: Checks a condition and returns an error if false
- `BAIL(error)`: Sets the error code and returns `S2N_FAILURE`

Example:
```c
GUARD(s2n_stuffer_read_uint16(in, &length));
ENSURE(length <= max_allowed, S2N_ERR_TOO_LARGE);
```

### Memory Management Patterns

- **Explicit tracking**: All memory regions are tracked with `s2n_blob`
- **Zero on free**: Memory is zeroed before being freed
- **Minimize allocations**: Reuse buffers when possible
- **Bounds checking**: All memory operations check bounds
- **Cleanup on error**: Resources are freed on error paths

### Security Practices

- **Constant-time operations**: For cryptographic operations
- **Minimal branching**: Linear control flow to avoid timing side-channels
- **Defensive programming**: Validate all inputs
- **Explicit initialization**: All memory is explicitly initialized
- **Minimal dependencies**: Reduces attack surface

### License Compliance and Copyright Headers

When adding new files to the project:
1. Include the Apache 2.0 license header at the top of the file
2. Set the copyright year to the current year
3. Ensure the file follows the project's coding style
4. Add appropriate documentation

## 8. Common Tasks and Solutions

### Setting up a TLS Server

```c
struct s2n_config *config = s2n_config_new();
s2n_config_add_cert_chain_and_key_to_store(config, cert_chain_and_key);
s2n_config_set_cipher_preferences(config, "default");

struct s2n_connection *conn = s2n_connection_new(S2N_SERVER);
s2n_connection_set_config(conn, config);
s2n_connection_set_fd(conn, socket_fd);

s2n_blocked_status blocked;
if (s2n_negotiate(conn, &blocked) != S2N_SUCCESS) {
    /* Handle error */
}

/* Send/receive data */
s2n_send(conn, buffer, size, &blocked);
s2n_recv(conn, buffer, size, &blocked);

/* Clean up */
s2n_shutdown(conn, &blocked);
s2n_connection_free(conn);
s2n_config_free(config);
```

### Setting up a TLS Client

```c
struct s2n_config *config = s2n_config_new();
s2n_config_set_cipher_preferences(config, "default");

struct s2n_connection *conn = s2n_connection_new(S2N_CLIENT);
s2n_connection_set_config(conn, config);
s2n_connection_set_fd(conn, socket_fd);
s2n_set_server_name(conn, "example.com");

s2n_blocked_status blocked;
if (s2n_negotiate(conn, &blocked) != S2N_SUCCESS) {
    /* Handle error */
}

/* Send/receive data */
s2n_send(conn, buffer, size, &blocked);
s2n_recv(conn, buffer, size, &blocked);

/* Clean up */
s2n_shutdown(conn, &blocked);
s2n_connection_free(conn);
s2n_config_free(config);
```

### Handling Errors

```c
if (s2n_negotiate(conn, &blocked) != S2N_SUCCESS) {
    if (s2n_error_get_type(s2n_errno) == S2N_ERR_T_BLOCKED) {
        /* Connection is blocked, try again later */
    } else {
        /* Log the error */
        fprintf(stderr, "Error: %s\n", s2n_strerror(s2n_errno, "EN"));
        fprintf(stderr, "Debug: %s\n", s2n_strerror_debug(s2n_errno, "EN"));
    }
}
```

### Adding New Files with Proper Licensing

When adding a new file to the project:

1. Start with the Apache 2.0 license header:
```c
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
```

2. Add appropriate include guards for header files:
```c
#pragma once
```

3. Follow the project's coding style and conventions

## 9. Glossary of Terms

### TLS-Specific Terminology

- **TLS**: Transport Layer Security, a cryptographic protocol for secure communications
- **SSL**: Secure Sockets Layer, the predecessor to TLS
- **Cipher Suite**: A combination of algorithms for key exchange, encryption, and message authentication
- **Handshake**: The process of establishing a secure connection
- **Certificate**: A digital document that verifies the identity of a server or client
- **Session Resumption**: Reusing parameters from a previous session to speed up the handshake
- **Perfect Forward Secrecy (PFS)**: A property that ensures past communications cannot be decrypted if long-term keys are compromised

### s2n-tls Specific Terminology

- **s2n_blob**: Basic memory management structure
- **s2n_stuffer**: Stream-oriented buffer for protocol handling
- **s2n_connection**: Structure representing a TLS connection
- **s2n_config**: Configuration object for TLS settings
- **GUARD**: Macro for propagating errors
- **ENSURE**: Macro for checking conditions
- **Blinding**: Adding random delays to prevent timing side-channels
