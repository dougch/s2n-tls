{
  description = "A flake for s2n-tls";

  inputs =  {
    nixpkgs.url = github:NixOS/nixpkgs/nixos-22.11;
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self,  nixpkgs, flake-utils }: 
     flake-utils.lib.eachDefaultSystem (system:
      let pkgs = nixpkgs.legacyPackages.${system};
      llvm = pkgs.pkgsLLVM.llvmPackages_latest;
      lib = nixpkgs.lib;
   in rec {
      packages.s2n-tls = pkgs.stdenv.mkDerivation {
        src = self;
        name = "s2n-tls";
        inherit system; 
        # From https://discourse.nixos.org/t/how-to-override-stdenv-for-all-packages-in-mkshell/10368/15
        nativeBuildInputs = [
          pkgs.cmake
          pkgs.pkg-config
          pkgs.ninja
          llvm.bintools
          pkgs.clang-tools_14
          llvm.lld
        ];
        buildInputs = with pkgs; [ 
                        clang
                        cmake
                        ninja
                        openssl_3 ]; # s2n-config has find_dependency LibCrypto

        cmakeFlags = [
            "-DBUILD_SHARED_LIBS=ON"
            "-DUNSAFE_TREAT_WARNINGS_AS_ERRORS=OFF" # disable -Werror
            "-DCMAKE_BUILD_TYPE=RelWithDebInfo"
        ];

      };
      packages.default = packages.s2n-tls;

      packages.s2n-tls-openssl3 = pkgs.s2n-tls.overrideAttrs (finalAttrs: previousAttrs: {
        doCheck = true;
      });
      packages.s2n-tls-openssl11 = pkgs.s2n-tls.overrideAttrs (finalAttrs: previousAttrs: {
        doCheck = true;
        buildInputs = [ pkgs.openssl_1_1 ]; # s2n-config has find_dependency LibCrypto
      });
      # TODO: crl test fails with libressl-3.6.1
      packages.s2n-tls-libressl = pkgs.s2n-tls.overrideAttrs (finalAttrs: previousAttrs: {
        doCheck = true;
        buildInputs = [ pkgs.libressl ]; # s2n-config has find_dependency LibCrypto
      });
      # TODO: shared lib not being found by cmake
      packages.s2n-tls-boringssl = pkgs.s2n-tls.overrideAttrs (finalAttrs: previousAttrs: {
        doCheck = true;
        buildInputs = [ pkgs.boringssl ]; # s2n-config has find_dependency LibCrypto
      });
      # TODO: shared lib not being found by cmake
      packages.s2n-tls-gnutls = pkgs.s2n-tls.overrideAttrs (finalAttrs: previousAttrs: {
        doCheck = true;
        buildInputs = [ pkgs.gnutls ]; # s2n-config has find_dependency LibCrypto
      });
      # TODO: shared lib not being found by cmake
      packages.s2n-tls-wolfssl = pkgs.s2n-tls.overrideAttrs (finalAttrs: previousAttrs: {
        doCheck = true;
        buildInputs = [ pkgs.wolfssl ]; # s2n-config has find_dependency LibCrypto
      });
      # TODO: shared lib not being found by cmake
      packages.s2n-tls-bearssl = pkgs.s2n-tls.overrideAttrs (finalAttrs: previousAttrs: {
        doCheck = true;
        buildInputs = [  ]; # s2n-config has find_dependency LibCrypto
      });
      
   });
 }
