# s2n-tls Testing Guide

This document provides a comprehensive guide to testing s2n-tls applications and understanding the testing framework used in the s2n-tls project. It covers unit testing, integration testing, fuzz testing, and formal verification.

## Table of Contents

1. [Testing Philosophy](#testing-philosophy)
2. [Testing Framework](#testing-framework)
3. [Unit Testing](#unit-testing)
4. [Integration Testing](#integration-testing)
5. [Fuzz Testing](#fuzz-testing)
6. [Formal Verification](#formal-verification)
7. [Performance Testing](#performance-testing)
8. [Test Coverage](#test-coverage)
9. [Testing Your s2n-tls Applications](#testing-your-s2n-tls-applications)
10. [Continuous Integration](#continuous-integration)

## Testing Philosophy

s2n-tls follows a rigorous testing approach to ensure security and reliability. The project's testing philosophy includes:

1. **Comprehensive Testing**: Every feature and code path should be tested.
2. **Multiple Testing Methods**: Different testing methods (unit tests, integration tests, fuzz tests, formal verification) are used to catch different types of issues.
3. **Test-Driven Development**: Tests are often written before the implementation.
4. **Continuous Testing**: Tests are run automatically on every code change.
5. **Security Focus**: Special attention is given to security-critical components.

## Testing Framework

s2n-tls uses a custom testing framework that provides macros for common testing operations. The framework is defined in `tests/s2n_test.h`.

### Key Macros

```c
/* Begin and end a test */
BEGIN_TEST();
END_TEST();

/* Assert that a condition is true */
EXPECT_TRUE(condition);

/* Assert that a condition is false */
EXPECT_FALSE(condition);

/* Assert that two values are equal */
EXPECT_EQUAL(a, b);

/* Assert that two values are not equal */
EXPECT_NOT_EQUAL(a, b);

/* Assert that a function succeeds */
EXPECT_SUCCESS(function_call);

/* Assert that a function fails */
EXPECT_FAILURE(function_call);

/* Assert that a function fails with a specific error */
EXPECT_FAILURE_WITH_ERRNO(function_call, error);

/* Skip a test */
SKIP_TEST();
```

### Example Test

```c
#include "s2n_test.h"

int main(int argc, char **argv) {
    BEGIN_TEST();
    
    /* Test s2n_stuffer_init */
    {
        struct s2n_stuffer stuffer;
        struct s2n_blob blob;
        uint8_t data[100];
        
        EXPECT_SUCCESS(s2n_blob_init(&blob, data, sizeof(data)));
        EXPECT_SUCCESS(s2n_stuffer_init(&stuffer, &blob));
        EXPECT_EQUAL(stuffer.blob.data, data);
        EXPECT_EQUAL(stuffer.blob.size, sizeof(data));
        EXPECT_EQUAL(stuffer.read_cursor, 0);
        EXPECT_EQUAL(stuffer.write_cursor, 0);
    }
    
    END_TEST();
}
```

## Unit Testing

Unit tests focus on testing individual components in isolation. s2n-tls has extensive unit tests for all its components.

### Running Unit Tests

```bash
# Run all unit tests
make -C tests/unit

# Run a specific unit test
make -C tests/unit s2n_stuffer_test
```

### Writing Unit Tests

1. Create a new file in `tests/unit/` named after the component you're testing (e.g., `s2n_stuffer_test.c`).
2. Include the test framework header: `#include "s2n_test.h"`.
3. Write test cases using the testing macros.
4. Add the test to the Makefile in `tests/unit/`.

Example unit test:

```c
#include "s2n_test.h"
#include "stuffer/s2n_stuffer.h"

int main(int argc, char **argv) {
    BEGIN_TEST();
    
    /* Test s2n_stuffer_alloc */
    {
        struct s2n_stuffer stuffer;
        EXPECT_SUCCESS(s2n_stuffer_alloc(&stuffer, 100));
        EXPECT_EQUAL(stuffer.blob.size, 100);
        EXPECT_SUCCESS(s2n_stuffer_free(&stuffer));
    }
    
    END_TEST();
}
```

### Mocking

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

## Integration Testing

Integration tests verify that components work together correctly and that s2n-tls interoperates with other TLS implementations.

### Running Integration Tests

```bash
# Run all integration tests
make -C tests/integrationv2

# Run a specific integration test
make -C tests/integrationv2 test_client_hello
```

### Writing Integration Tests

s2n-tls uses a Python framework for integration testing. To write an integration test:

1. Create a new file in `tests/integrationv2/` (e.g., `test_example.py`).
2. Use the provided test fixtures and utilities.
3. Write test cases using pytest.

Example integration test:

```python
import pytest
from s2n_test_constants import S2N_TLS12, S2N_TLS13
from configuration import available_ports, ALL_CERTS
from common import ProviderOptions, Protocols, Ciphers

@pytest.mark.uncollect_if(func=invalid_test_parameters)
@pytest.mark.parametrize("cipher", [Ciphers.AES128_GCM_SHA256, Ciphers.AES256_GCM_SHA384])
@pytest.mark.parametrize("provider", ["openssl", "s2n"])
@pytest.mark.parametrize("protocol", [S2N_TLS12, S2N_TLS13])
def test_example(managed_process, cipher, provider, protocol):
    port = next(available_ports)
    
    client_options = ProviderOptions(
        mode="client",
        host="localhost",
        port=port,
        cipher=cipher,
        insecure=True,
        protocol=protocol
    )
    
    server_options = ProviderOptions(
        mode="server",
        host="localhost",
        port=port,
        cipher=cipher,
        key=ALL_CERTS["rsa_2048"]["key"],
        cert=ALL_CERTS["rsa_2048"]["cert"],
        protocol=protocol
    )
    
    server = managed_process(provider, server_options, timeout=5)
    client = managed_process(provider, client_options, timeout=5)
    
    for results in client.get_results():
        assert results.exception is None
        assert results.exit_code == 0
    
    for results in server.get_results():
        assert results.exception is None
        assert results.exit_code == 0
```

## Fuzz Testing

Fuzz testing involves providing random or malformed inputs to functions to find bugs and vulnerabilities.

### Running Fuzz Tests

```bash
# Build fuzz tests
make -C tests/fuzz

# Run a specific fuzz test
./tests/fuzz/s2n_stuffer_pem_fuzz_test
```

### Writing Fuzz Tests

1. Create a new file in `tests/fuzz/` (e.g., `s2n_example_fuzz_test.c`).
2. Implement the `LLVMFuzzerTestOneInput` function.
3. Add the test to the Makefile in `tests/fuzz/`.

Example fuzz test:

```c
#include <stdint.h>
#include <stdlib.h>
#include "stuffer/s2n_stuffer.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    struct s2n_stuffer stuffer;
    struct s2n_blob blob;
    
    /* Don't fuzz with very large inputs */
    if (size > 1024) {
        return 0;
    }
    
    /* Initialize a stuffer with the fuzz input */
    GUARD(s2n_blob_init(&blob, (uint8_t *)data, size));
    GUARD(s2n_stuffer_init(&stuffer, &blob));
    
    /* Call the function being tested */
    uint8_t u8;
    s2n_stuffer_read_uint8(&stuffer, &u8);
    
    return 0;
}
```

## Formal Verification

s2n-tls uses formal verification tools to mathematically prove the correctness of critical components.

### CBMC (C Bounded Model Checker)

CBMC is used to verify memory safety and functional correctness of C code.

```bash
# Run CBMC tests
make -C tests/cbmc
```

### SAW (Software Analysis Workbench)

SAW is used to verify the equivalence of cryptographic implementations.

```bash
# Run SAW tests
make -C tests/saw
```

### Writing Formal Verification Tests

Writing formal verification tests requires specialized knowledge. Refer to the documentation in `tests/cbmc/` and `tests/saw/` for examples and guidance.

## Performance Testing

s2n-tls includes benchmarks to measure the performance of various operations.

### Running Performance Tests

```bash
# Build and run benchmarks
make -C tests/benchmark
./tests/benchmark/benchmark
```

### Writing Performance Tests

1. Create a new file in `tests/benchmark/` (e.g., `s2n_example_benchmark.c`).
2. Use the benchmark framework to measure performance.
3. Add the benchmark to the Makefile in `tests/benchmark/`.

Example benchmark:

```c
#include "benchmark.h"
#include "stuffer/s2n_stuffer.h"

int main(int argc, char **argv) {
    BEGIN_BENCHMARK();
    
    /* Benchmark s2n_stuffer_alloc and s2n_stuffer_free */
    {
        struct s2n_stuffer stuffer;
        
        BENCHMARK(s2n_stuffer_alloc(&stuffer, 100), 1000);
        BENCHMARK(s2n_stuffer_free(&stuffer), 1000);
    }
    
    END_BENCHMARK();
}
```

## Test Coverage

s2n-tls uses code coverage tools to ensure that tests exercise all code paths.

### Generating Coverage Reports

```bash
# Generate coverage report
make -C coverage
```

### Analyzing Coverage Reports

Coverage reports are generated in HTML format and can be viewed in a web browser. Look for areas with low coverage and add tests to improve coverage.

## Testing Your s2n-tls Applications

When building applications that use s2n-tls, it's important to test them thoroughly.

### Unit Testing Your Application

1. Test your application's use of s2n-tls functions.
2. Verify that your application handles errors correctly.
3. Test edge cases and error conditions.

Example:

```c
#include "s2n_test.h"
#include "your_application.h"

int main(int argc, char **argv) {
    BEGIN_TEST();
    
    /* Test your application's initialization */
    EXPECT_SUCCESS(your_app_init());
    
    /* Test your application's TLS connection handling */
    EXPECT_SUCCESS(your_app_connect("example.com", 443));
    
    /* Test your application's error handling */
    EXPECT_FAILURE(your_app_connect("invalid-host", 443));
    
    END_TEST();
}
```

### Integration Testing Your Application

1. Test your application's interaction with other systems.
2. Verify that your application works with different TLS implementations.
3. Test with different network conditions.

Example using Python and pytest:

```python
import pytest
import subprocess
import time

def test_your_application():
    # Start your application
    app_process = subprocess.Popen(["./your_application"])
    time.sleep(1)  # Wait for it to start
    
    # Connect to your application using OpenSSL
    openssl_process = subprocess.Popen(
        ["openssl", "s_client", "-connect", "localhost:443"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )
    
    # Send data
    openssl_process.stdin.write(b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")
    openssl_process.stdin.flush()
    
    # Read response
    stdout, stderr = openssl_process.communicate(timeout=5)
    
    # Verify response
    assert b"HTTP/1.1 200 OK" in stdout
    
    # Clean up
    app_process.terminate()
    app_process.wait()
```

### Fuzz Testing Your Application

1. Identify inputs that your application processes.
2. Write fuzz tests that provide random or malformed inputs.
3. Monitor for crashes, memory leaks, and other issues.

Example using libFuzzer:

```c
#include <stdint.h>
#include <stdlib.h>
#include "your_application.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* Initialize your application */
    your_app_init();
    
    /* Process the fuzz input */
    your_app_process_data(data, size);
    
    /* Clean up */
    your_app_cleanup();
    
    return 0;
}
```

## Continuous Integration

s2n-tls uses continuous integration (CI) to automatically run tests on every code change.

### Setting Up CI for Your Application

1. Configure your CI system to build your application with s2n-tls.
2. Run unit tests, integration tests, and fuzz tests.
3. Generate and analyze code coverage reports.
4. Set up alerts for test failures.

Example GitHub Actions workflow:

```yaml
name: CI

on:
  push:
    branches: [ main ]
  pull_request:
    branches: [ main ]

jobs:
  build-and-test:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v2
    
    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y build-essential libssl-dev
    
    - name: Clone s2n-tls
      run: |
        git clone https://github.com/aws/s2n-tls.git
        cd s2n-tls
        git checkout v1.0.0
    
    - name: Build s2n-tls
      run: |
        cd s2n-tls
        make
    
    - name: Build your application
      run: |
        make
    
    - name: Run unit tests
      run: |
        make test
    
    - name: Run integration tests
      run: |
        make integration-test
