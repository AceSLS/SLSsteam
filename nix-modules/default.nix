{
  rev,
  lib,
  pkgs,
}:
pkgs.pkgsi686Linux.stdenv.mkDerivation {
  pname = "SLSsteam";
  version = "${rev}";
  src = ../.;

  nativeBuildInputs = with pkgs; [
    pkg-config
    makeWrapper
  ];

  buildInputs = with pkgs.pkgsi686Linux; [
    openssl
    curl
    luajit
  ];

  postPatch = ''
    substituteInPlace src/log.cpp \
      --replace-fail "notify-send" "${lib.getExe pkgs.libnotify}"
  '';

  buildPhase = ''
    make audit-libs
  '';

  installPhase = ''
    mkdir -p $out/
    cp bin/SLSsteam.so $out/
    cp bin/library-inject.so $out/
  '';

  meta = {
    description = "Steamclient Modification for Linux";
    homepage = "https://github.com/AceSLS/SLSsteam";
    license = lib.licenses.agpl3Only;
    platforms = lib.platforms.linux;
  };
}
