{ lib
, stdenv
, scons
, clang-tools
, forstio
, pname
, version
}:

stdenv.mkDerivation {
	inherit pname version;
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
