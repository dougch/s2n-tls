### Unit testing in a nix devshell

See the building.md file for instructions on building s2n-tls. To run unit tests, be sure you're running with the same libcrypto set as when the build was created. If in an unknown state, clean and rebuild.

- [ ] `unit`

Note: the `unit` bash function without any arguments will run all unit tests.  If you'd like to test a specific 
unit test, simply add a substring of the test name as an argument, e.g.: `unit ocsp`.

### Unit testing on ubuntu 

If you've setup your environment by hand, without using nix, you can still build and test s2n-tls, but be aware
 that the libcrypto selection process and the selection of the correct shared object at runtime can be problematic.

 Example build steps:
 - [ ] `rm -rf ./build` 
 - [ ] for libcrypto==awslc `cmake . -Bbuild -GNinja -DCMAKE_PREFIX_PATH=/usr/local/awslc`
 - [ ] `export S2N_LIBCRYPTO=awslc-fips-2024; cmake . -Bbuild -GNinja -DCMAKE_PREFIX_PATH=/usr/local/$S2N_LIBCRYPTO`
 - [ ] `export S2N_LIBCRYPTO=awslc-fips-main; cmake . -Bbuild -GNinja -DCMAKE_PREFIX_PATH=/usr/local/$S2N_LIBCRYPTO`
 - [ ] `cmake --build ./build -j $(nproc)`
 - [ ] `CTEST_PARALLEL_LEVEL=$(nproc) ninja -C ./build test`

These steps:
  - cleanup: if any previous attmpts exist, clean them up
  - tell cmake where to find the libcrypto we're inter 
  - build and run the tests with ninja