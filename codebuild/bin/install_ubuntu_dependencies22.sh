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

# Shim code to get local docker/ec2 instances bootstraped like a CodeBuild instance.
# Not actually used by CodeBuild.

source codebuild/bin/s2n_setup_env.sh

set -e

github_apt(){
  curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg | gpg --dearmor -o /usr/share/keyrings/githubcli-archive-keyring.gpg
  echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" | tee /etc/apt/sources.list.d/github-cli.list
  apt update -y
  apt install -y gh
}
get_rust() {
  apt install -y clang-15 sudo
  #Make clang-15 the default clang
  update-alternatives --install /usr/bin/clang clang /usr/lib/llvm-15/bin/clang 1500 --slave /usr/bin/clang++ clang++ /usr/lib/llvm-15/bin/clang --slave /usr/bin/llvm-cov llvm-cov /usr/lib/llvm-15/bin/llvm-cov 
  curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
  source $HOME/.cargo/env
  rustup default nightly
}

# these are the packages necessary for a cross-compiling 32 bit build of s2n-tls.
get_32_bit_packages() {
    # tell the OS that we are interested in 32 bit packages
    dpkg --add-architecture i386
    # tell apt to update itself so 32 bit packages are "visible"
    apt update -y
    # download the 32 bit libcrypto
    apt install -y libssl-dev:i386
    # download the 32 bit glibc
    apt install -y gcc-multilib
}

base_packages() {
  echo "Installing repositories and base packages"
  apt update -y
  apt install -y software-properties-common
  add-apt-repository ppa:ubuntu-toolchain-r/test -y
  #add-apt-repository ppa:longsleep/golang-backports -y
  apt-get update -o Acquire::CompressionTypes::Order::=gz

  DEPENDENCIES="unzip make indent iproute2 kwstyle libssl-dev net-tools tcpdump valgrind lcov m4 nettle-dev nettle-bin pkg-config psmisc gcc-12 g++-12 gcc-9 zlib1g-dev python3-pip python3-testresources llvm curl shellcheck git tox cmake libtool ninja-build golang quilt jq apache2"
  if [[ -n "${GCC_VERSION:-}" ]] && [[ "${GCC_VERSION:-}" != "NONE" ]]; then
    DEPENDENCIES+=" gcc-$GCC_VERSION g++-$GCC_VERSION";
  fi
  if ! command -v python3.9 &> /dev/null; then
    add-apt-repository ppa:deadsnakes/ppa -y
    DEPENDENCIES+=" python3.9 python3.9-distutils";
  fi

  apt-get -y install --no-install-recommends ${DEPENDENCIES}
  # Make gcc-12 the default
  update-alternatives --install /usr/bin/gcc gcc /usr/bin/x86_64-linux-gnu-gcc-12 1200 --slave /usr/bin/g++ g++ /usr/bin/x86_64-linux-gnu-g++-12
}

if [[ `grep -c 'VERSION_ID="22' /etc/os-release` -ne 1 ]]; then 
  echo "Expected Ubuntu 22";
  exit 255
fi
base_packages
github_apt
get_rust
get_32_bit_packages

# If prlimit is not on our current PATH, download and compile prlimit manually. s2n needs prlimit to memlock pages
if ! type prlimit > /dev/null && [[ ! -d "$PRLIMIT_INSTALL_DIR" ]]; then
    mkdir -p "$PRLIMIT_INSTALL_DIR";
    codebuild/bin/install_prlimit.sh "$(mktemp -d)" "$PRLIMIT_INSTALL_DIR";
fi

if [[ "$TESTS" == "ctverif" || "$TESTS" == "ALL" ]] && [[ ! -d "$CTVERIF_INSTALL_DIR" ]]; then
    mkdir -p "$CTVERIF_INSTALL_DIR" && codebuild/bin/install_ctverif.sh "$CTVERIF_INSTALL_DIR" > /dev/null ; fi

if [[ "$TESTS" == "sidetrail" || "$TESTS" == "ALL" ]] ; then
    codebuild/bin/install_sidetrail_dependencies.sh ; fi

if [[ "$TESTS" == "sidetrail" || "$TESTS" == "ALL" ]] && [[ ! -d "$SIDETRAIL_INSTALL_DIR" ]]; then
    mkdir -p "$SIDETRAIL_INSTALL_DIR" && codebuild/bin/install_sidetrail.sh "$SIDETRAIL_INSTALL_DIR" > /dev/null ; fi
