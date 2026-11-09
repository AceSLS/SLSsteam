{
  rev,
  lib,
  pkgs,
}:
pkgs.stdenv.mkDerivation {
  pname = "SLSsteam";
  version = "${rev}";
  src = ../.;

  nativeBuildInputs = with pkgs; [
    pkg-config
  ];

  env.GIT_COMMIT_HASH = rev;

  buildInputs = with pkgs; [
    curl
    dbus
    luajit
  ];

  postPatch = ''
    patchShebangs embed-version.sh embed-config.sh
  '';

  buildPhase = ''
    make clean
    make audit-libs
  '';

  installPhase = ''
    mkdir -p $out/
    cp bin/SLSsteam.so $out/
  '';

  postFixup = ''
    patchelf --remove-rpath $out/SLSsteam.so
  '';

  meta = {
    description = "Steamclient Modification for Linux";
    homepage = "https://github.com/AceSLS/SLSsteam";
    license = lib.licenses.agpl3Only;
    platforms = lib.platforms.linux;
  };
}
