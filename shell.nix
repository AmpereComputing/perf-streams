{ pkgs ? import (fetchTarball {
    # nix version: 25.11
    url = "https://github.com/NixOS/nixpkgs/archive/0590cd39f728e129122770c029970378a79d076a.tar.gz";
  }) {} }:
pkgs.mkShell.override{stdenv = pkgs.gccStdenv; } {
  packages = with pkgs; [
    boost
    capstone
    clang-tools
    cmake
    fmt
    gcovr
    gdb
    google-benchmark
    gtest
    hatch
    ninja
    pkg-config
    protobuf
    protobufc
    ruff
    (pkgs.python3.withPackages(python-pkgs: [
      python-pkgs.build
      python-pkgs.capstone
      python-pkgs.coverage
      python-pkgs.protobuf
      python-pkgs.scikit-build
    ]))
  ];
}
