{
  description = "A flake for s2n-tls";

  inputs =  {
    nixpkgs.url = github:NixOS/nixpkgs/nixos-22.05;
    flake-utils.url = "github:numtide/flake-utils";
    s2n-tls.url = "github:dougch/s2n-tls?ref=nix";
  };

  outputs = { self, nix, nixpkgs, flake-utils }: 
     flake-utils.lib.eachDefaultSystem (system:
      let pkgs = nixpkgs.legacyPackages.${system};
   in rec {
      packages.s2n-tls = pkgs.stdenv.mkDerivation {
        src = self;
        name = "s2n-tls";
        inherit system; 

        buildInputs = [ pkgs.cmake
                        pkgs.ninja
                        pkgs.openssl_3 ]; # s2n-config has find_dependency LibCrypto

        cmakeFlags = [
            "-DBUILD_SHARED_LIBS=ON"
            "-DUNSAFE_TREAT_WARNINGS_AS_ERRORS=OFF" # disable -Werror
            "-DCMAKE_BUILD_TYPE=RelWithDebInfo"
            "-DS2N_NO_PQ=1" # TODO: set when system like aarch64
        ];

      };
      packages.s2n-tls-openssl11 = pkgs.s2n-tls.overrideAttrs (finalAttrs: previousAttrs: {
        doCheck = true;
        buildInputs = [ pkgs.openssl_1_1 ]; # s2n-config has find_dependency LibCrypto
      });
      packages.s2n-tls-libressl = pkgs.s2n-tls.overrideAttrs (finalAttrs: previousAttrs: {
        doCheck = true;
        buildInputs = [ pkgs.libressl ]; # s2n-config has find_dependency LibCrypto
      });
      # TODO: boringssl shared lib no being found by cmake
      packages.s2n-tls-boringssl = pkgs.s2n-tls.overrideAttrs (finalAttrs: previousAttrs: {
        doCheck = true;
        buildInputs = [ pkgs.boringssl ]; # s2n-config has find_dependency LibCrypto
      });
      defaultPackage = packages.s2n-tls;
      
   });
 }
