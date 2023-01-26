{
  description = "A flake for s2n-tls";

  inputs.nixpkgs.url = github:NixOS/nixpkgs/nixos-22.05;

  outputs = { self, nix, nixpkgs, flake-utils }: 
     flake-utils.lib.eachDefaultSystem (system:
      let pkgs = nixpkgs.legacyPackages.${system};
   in rec {
      packages.s2n-tls = pkgs.stdenv.mkDerivation {
        src = self;
        name = "s2n-tls";
        inherit system; 

        buildInputs = [ pkgs.cmake
                        pkgs.openssl ]; # s2n-config has find_dependency(LibCrypto

        cmakeFlags = [
            "-DBUILD_SHARED_LIBS=ON"
            "-DUNSAFE_TREAT_WARNINGS_AS_ERRORS=OFF" # disable -Werror
            "-DCMAKE_BUILD_TYPE=RelWithDebInfo"
            "-DS2N_NO_PQ=1"
        ];

      };
      defaultPackage = packages.s2n-tls;
      
   });
 }
