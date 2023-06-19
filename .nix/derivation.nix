{ lib
, stdenvNoCC
, scons
, clang
, clang-tools
, forstio
, kel-unit
}:

stdenvNoCC.mkDerivation {
  pname = "kel-lbm";
  version = "0.0.0";
  src = ./..;

  nativeBuildInputs = [
    scons
    clang
    clang-tools
  ];

  buildInputs = [
    forstio.core
    forstio.async
    forstio.codec
    kel-unit
  ];

  outputs = [ "out" "dev" ];
}
