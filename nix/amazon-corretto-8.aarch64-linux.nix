{ pkgs }:
pkgs.stdenv.mkDerivation rec {
  pname = "amazon-corretto";
  version = "8";

  src = pkgs.fetchzip {
    url =
      "https://corretto.aws/downloads/latest/amazon-corretto-8-aarch64-linux-jdk.tar.gz";
    sha256 = "sha256-7a6b1fc6762bb4c1defc32d361059e52927e91b2e87791948ba4525ec18f0fbe";
  };

  buildInputs = with pkgs; [
    cpio
    file
    which
    zip
    perl
    zlib
    cups
    freetype
    harfbuzz
    libjpeg
    giflib
    libpng
    zlib
    lcms2
    fontconfig
    glib
    xorg.libX11
    xorg.libXrender
    xorg.libXext
    xorg.libXtst
    xorg.libXt
    xorg.libXtst
    xorg.libXi
    xorg.libXinerama
    xorg.libXcursor
    xorg.libXrandr
    gtk2-x11
    gdk-pixbuf
    xorg.libXxf86vm
  ];

  buildPhase = ''
    echo "Corretto is already built"
  '';

  installPhase = ''
    mkdir $out
    cp -av ./* $out/
    echo $out after install
    ls $out/
  '';
}
