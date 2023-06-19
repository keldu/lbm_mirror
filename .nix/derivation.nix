{ lib
, stdenvNoCC
, scons
, clang
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
  ];

  buildInputs = [
    forstio.core
    forstio.async
    forstio.codec
    kel-unit
  ];

  outputs = [ "out" "dev" ];
}
