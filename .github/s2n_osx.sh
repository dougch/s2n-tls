#!/bin/bash
#!/bin/bash
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License").
# You may not use this file except in compliance with the License.
# A copy of the License is located at
#
#  http://aws.amazon.com/apache2.0
#
# or in the "license" file accompanying this file. This file is distributed
# on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
# express or implied. See the License for the specific language governing
# permissions and limitations under the License.
#
set -eu

export CTEST_OUTPUT_ON_FAILURE=1
BREWINSTLLPATH=$(brew --prefix openssl)
OPENSSL_INSTALL_DIR="${BREWINSTLLPATH:-"/opt/homebrew/Cellar/openssl"}"

echo "Using OpenSSL at $OPENSSL_INSTALL_DIR"
# Build with debug symbols
cmake . -Bbuild -GNinja \
-DCMAKE_BUILD_TYPE=Debug \
-DCMAKE_PREFIX_PATH=${OPENSSL_INSTALL_DIR} ..

cmake --build ./build -j $(nproc)
time CTEST_PARALLEL_LEVEL=$(nproc) ninja -C build test

# Build shared library
cmake . -Bbuild -GNinja \
-DCMAKE_BUILD_TYPE=Debug \
-DCMAKE_PREFIX_PATH=${OPENSSL_INSTALL_DIR} .. \
-DBUILD_SHARED_LIBS=ON

cmake --build ./build -j $(nproc)
time CTEST_PARALLEL_LEVEL=$(nproc) ninja -C build test
