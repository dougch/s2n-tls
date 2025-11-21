# s2n-tls Security Guidelines

This document provides comprehensive security guidelines and best practices for using s2n-tls, as well as explanations of the security mechanisms implemented within the library.

## Table of Contents

1. [Security Philosophy](#security-philosophy)
2. [Security Mechanisms](#security-mechanisms)
3. [Best Practices](#best-practices)
4. [Common Vulnerabilities to Avoid](#common-vulnerabilities-to-avoid)
5. [Security Testing](#security-testing)
6. [Reporting Security Issues](#reporting-security-issues)

## Security Philosophy

s2n-tls was built with the following security principles in mind:

### 1. Security Over Features

s2n-tls prioritizes security over feature completeness. The library implements the most commonly used features of TLS/SSL while avoiding rarely used options that have historically caused security vulnerabilities.

### 2. Simplicity

The codebase is designed to be small, simple, and easy to understand. This approach reduces the attack surface and makes security audits more effective.

### 3. Defense in Depth

s2n-tls employs multiple layers of security controls to protect against various types of attacks. If one defense mechanism fails, others are in place to prevent exploitation.

### 4. Paranoia

The library assumes that any input could be malicious and validates all data before processing it.

### 5. Data-Driven Decisions

Security decisions in s2n-tls are based on empirical evidence and careful analysis rather than assumptions.

## Security Mechanisms

### Memory Safety

s2n-tls implements several mechanisms to ensure memory safety:

1. **Explicit Memory Management**: All memory regions are tracked using `s2n_blob` structures.
2. **Zero on Free**: Memory is zeroed before being freed to prevent sensitive data leakage.
3. **Bounds Checking**: All memory operations include bounds checking to prevent buffer overflows.
4. **Memory Locking**: Sensitive data can be locked in memory to prevent it from being swapped to disk.

Example of proper memory handling:

```c
struct s2n_blob blob;
GUARD(s2n_blob_init(&blob, data, size));
/* Use the blob */
GUARD(s2n_blob_zero(&blob)); /* Zero the memory before freeing */
```

### Protection Against Side-Channel Attacks

s2n-tls includes protections against various side-channel attacks:

1. **Constant-Time Operations**: Cryptographic operations are implemented to run in constant time, regardless of the input values.
2. **Blinding**: Random delays are added to operations to prevent timing analysis.
3. **Separate RNGs**: Separate random number generators are used for public and private operations.

Example of blinding implementation:

```c
/* Add a random delay to prevent timing analysis */
GUARD(s2n_public_random(delay_value, sizeof(delay_value)));
sleep_time_in_nanoseconds(delay_value);
```

### State Machine Safety

The TLS protocol is implemented using a table-driven state machine to prevent invalid state transitions:

1. **Explicit States**: All protocol states are explicitly defined.
2. **Validated Transitions**: Only valid state transitions are allowed.
3. **Error Handling**: Errors result in immediate termination of the handshake.

### Input Validation

All inputs are validated before processing:

1. **Length Checking**: Message lengths are checked against expected values.
2. **Format Validation**: Message formats are validated against the TLS specification.
3. **Bounds Checking**: All array accesses include bounds checking.

Example of input validation:

```c
/* Ensure the message length is within bounds */
ENSURE(message_length <= S2N_MAXIMUM_FRAGMENT_LENGTH, S2N_ERR_BAD_MESSAGE);

/* Validate the message format */
ENSURE(s2n_stuffer_data_available(in) >= message_length, S2N_ERR_BAD_MESSAGE);
```

### Cryptographic Protections

s2n-tls implements various cryptographic protections:

1. **Modern Cipher Suites**: Support for modern, secure cipher suites.
2. **Perfect Forward Secrecy**: Support for key exchange methods that provide forward secrecy.
3. **Secure Defaults**: Secure cipher suites are enabled by default.

## Best Practices

### Configuration

1. **Use Secure Cipher Preferences**:
   ```c
   /* Use a secure cipher preference */
   GUARD(s2n_config_set_cipher_preferences(config, "20190801")); /* Modern, secure ciphers */
   ```

2. **Enable Certificate Verification**:
   ```c
   /* Enable certificate verification */
   GUARD(s2n_config_set_check_stapled_ocsp_response(config, 1));
   GUARD(s2n_config_disable_x509_verification(config, 0)); /* Ensure verification is enabled */
   ```

3. **Set a Proper Security Policy**:
   ```c
   /* Set a security policy that aligns with your requirements */
   GUARD(s2n_config_set_cipher_preferences(config, "default_tls13"));
   ```

### Connection Handling

1. **Validate Peer Certificates**:
   ```c
   /* Set a custom certificate verification callback if needed */
   GUARD(s2n_config_set_verify_host_callback(config, verify_host_fn, NULL));
   ```

2. **Check for Negotiation Success**:
   ```c
   /* Ensure the handshake completed successfully */
   s2n_blocked_status blocked;
   if (s2n_negotiate(conn, &blocked) != S2N_SUCCESS) {
       /* Handle error */
   }
   ```

3. **Properly Handle Blocked I/O**:
   ```c
   /* Handle blocked I/O correctly */
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

### Error Handling

1. **Check Return Values**:
   ```c
   /* Always check return values */
   if (s2n_some_function() != S2N_SUCCESS) {
       /* Handle error */
   }
   ```

2. **Use the GUARD Macro**:
   ```c
   /* Use the GUARD macro for error propagation */
   GUARD(s2n_some_function());
   ```

3. **Distinguish Between Error Types**:
   ```c
   /* Distinguish between different types of errors */
   if (s2n_negotiate(conn, &blocked) != S2N_SUCCESS) {
       if (s2n_error_get_type(s2n_errno) == S2N_ERR_T_BLOCKED) {
           /* Handle blocked I/O */
       } else if (s2n_error_get_type(s2n_errno) == S2N_ERR_T_ALERT) {
           /* Handle TLS alert */
       } else {
           /* Handle other errors */
       }
   }
   ```

### Resource Management

1. **Free Resources**:
   ```c
   /* Always free resources */
   GUARD(s2n_connection_free(conn));
   GUARD(s2n_config_free(config));
   ```

2. **Use Cleanup Functions**:
   ```c
   /* Use cleanup functions when done with s2n */
   GUARD(s2n_cleanup());
   ```

## Common Vulnerabilities to Avoid

### 1. Certificate Validation Bypass

**Vulnerability**: Disabling certificate validation or not properly verifying certificates.

**Prevention**:
- Always enable certificate validation.
- Use proper hostname verification.
- Check certificate revocation status when possible.

```c
/* Do NOT do this in production code */
s2n_config_disable_x509_verification(config, 1); /* INSECURE */

/* Instead, ensure verification is enabled */
s2n_config_disable_x509_verification(config, 0);
```

### 2. Weak Cipher Suites

**Vulnerability**: Using weak or deprecated cipher suites.

**Prevention**:
- Use modern cipher preferences.
- Regularly update your security policy as new vulnerabilities are discovered.

```c
/* Avoid using outdated cipher preferences */
s2n_config_set_cipher_preferences(config, "default"); /* May include older ciphers */

/* Instead, use a modern cipher preference */
s2n_config_set_cipher_preferences(config, "20190801"); /* Modern ciphers only */
```

### 3. Improper Error Handling

**Vulnerability**: Not checking return values or improperly handling errors.

**Prevention**:
- Always check return values.
- Use the GUARD macro for error propagation.
- Handle different types of errors appropriately.

```c
/* Avoid ignoring return values */
s2n_negotiate(conn, &blocked); /* INCORRECT - return value not checked */

/* Instead, check return values and handle errors */
if (s2n_negotiate(conn, &blocked) != S2N_SUCCESS) {
    /* Handle error */
}
```

### 4. Memory Leaks

**Vulnerability**: Not freeing resources properly.

**Prevention**:
- Always free resources when done with them.
- Use cleanup functions when done with s2n.

```c
/* Avoid memory leaks */
conn = s2n_connection_new(S2N_CLIENT);
/* ... use connection ... */
/* INCORRECT - connection not freed */

/* Instead, free resources when done */
conn = s2n_connection_new(S2N_CLIENT);
/* ... use connection ... */
s2n_connection_free(conn); /* Correct */
```

### 5. Insufficient Randomness

**Vulnerability**: Using weak random number generators or not properly seeding them.

**Prevention**:
- Let s2n-tls handle randomness for you.
- If you need randomness outside of s2n-tls, use a cryptographically secure random number generator.

## Security Testing

s2n-tls undergoes extensive security testing:

### 1. Unit Tests

The library includes comprehensive unit tests that verify the correctness of individual components.

### 2. Integration Tests

Integration tests verify that the components work together correctly and that the library interoperates with other TLS implementations.

### 3. Fuzz Testing

s2n-tls is regularly fuzz tested to find potential vulnerabilities.

### 4. Formal Verification

Parts of s2n-tls have been formally verified using tools like CBMC and SAW.

### 5. Third-Party Audits

The library undergoes regular security audits by third-party security experts.

## Reporting Security Issues

If you discover a security issue in s2n-tls, please report it according to the following process:

1. **Do Not Disclose Publicly**: Do not disclose the issue publicly until it has been addressed.
2. **Report to AWS Security**: Report the issue to AWS Security via the vulnerability reporting page: https://aws.amazon.com/security/vulnerability-reporting/
3. **Provide Details**: Include as much detail as possible, including steps to reproduce the issue.
4. **Wait for Response**: AWS Security will acknowledge your report and provide updates on the investigation.

## Additional Resources

- [s2n-tls GitHub Repository](https://github.com/aws/s2n-tls)
- [AWS Security Bulletins](https://aws.amazon.com/security/security-bulletins/)
- [TLS Best Practices (IETF)](https://datatracker.ietf.org/doc/html/rfc7525)
