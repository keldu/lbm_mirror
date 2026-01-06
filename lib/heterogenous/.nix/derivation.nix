{ lib
, stdenv
, scons
, clang-tools
, pname
, version
, forstio
, kel-lbm
, adaptive-cpp
}:

stdenv.mkDerivation {
	pname = "${pname}-heterogenous";
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
		forstio.remote
		kel-lbm.core
	];

	doCheck = true;
	checkPhase = ''
		scons test
		./bin/tests
	'';

	preferLocalBuild = true;

	outputs = [ "out" "dev" ];
}
