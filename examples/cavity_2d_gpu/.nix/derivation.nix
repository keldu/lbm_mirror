{ lib
, stdenv
, scons
, clang-tools
, forstio
, adaptive-cpp
, pname
, version
, kel-lbm
}:

stdenv.mkDerivation {
	pname = pname + "-examples-" + "cavity_2d_gpu";
	inherit version;
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
		adaptive-cpp
		kel-lbm
  ];

  doCheck = true;
  checkPhase = ''
    scons test
    ./bin/tests
	'';

	preferLocalBuild = true;

  outputs = [ "out" "dev" ];
}
