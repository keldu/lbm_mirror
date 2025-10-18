{ lib
, stdenv
, scons
, clang-tools
, forstio
}:

stdenv.mkDerivation {
  pname = "kel-lbm";
  version = "0.0.2";
  src = ../lib;

  nativeBuildInputs = [
    scons
    clang-tools
  ];

  buildInputs = [
    forstio.core
    forstio.async
    forstio.codec
    forstio.codec-unit
    forstio.codec-json
  ];

  doCheck = true;
  checkPhase = ''
    scons test
    ./bin/tests
	'';

	preferLocalBuild = true;

  outputs = [ "out" "dev" ];
}
