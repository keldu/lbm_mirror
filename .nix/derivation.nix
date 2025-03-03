{ lib
, stdenv
, scons
, clang-tools
, forstio
}:

stdenv.mkDerivation {
  pname = "kel-lbm";
  version = "0.0.0";
  src = ./..;

  nativeBuildInputs = [
    scons
    clang-tools
  ];

  buildInputs = [
    forstio.core
    forstio.async
    forstio.codec
    forstio.codec-unit
  ];

  outputs = [ "out" "dev" ];
}
