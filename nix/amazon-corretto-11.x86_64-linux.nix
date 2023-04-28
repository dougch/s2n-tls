{ pkgs }:
pkgs.stdenv.mkDerivation rec {
  pname = "amazon-corretto";
  version = "11";

  # https://docs.aws.amazon.com/corretto/latest/corretto-11-ug/downloads-list.html
  src = pkgs.fetchzip {
    url =
      "https://corretto.aws/downloads/latest/amazon-corretto-11-x64-linux-jdk.tar.gz";
    sha256 = "sha256-d3b7de2a0916da0d3826d980e9718a64932a160c33e8dfa6dbff2a91fef56976";
  };

  nativeBuildInputs = [ pkgs.autoPatchelfHook ];

  buildInputs = with pkgs; [
    cpio
    file
    which
    zip
    zlib
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
