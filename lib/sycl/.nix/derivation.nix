{ lib
, stdenv
, scons
, clang-tools
, forstio
, pname
, version
}:

stdenv.mkDerivation {
	pname = "${pname}-sycl";
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
		forstio.remote-sycl
	];

	doCheck = true;
	checkPhase = ''
		scons test
		./bin/tests
	'';

	preferLocalBuild = true;

	outputs = [ "out" "dev" ];
}
