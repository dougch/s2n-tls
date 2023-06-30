#!/usr/bin/env python3

import os
from socket import SOL_TCP,socket

def lsmod(module_name:str):
    with open("/proc/modules", "r") as fh:
        modules = fh.readlines()
        for line in modules:
            if module_name in line:
                return True
        return False

def main():
    s = socket()
    s.bind(('localhost', 0))
    s.connect(s.getsockname())
    TCP_ULP = 31
    s.setsockopt(SOL_TCP, TCP_ULP, b'tls')
    if lsmod("tls") is True:
        print("tls kernel module is present.")
    else:
        raise SystemError("kernel tls module not loaded present.")


if __name__ == "__main__":
    main()
