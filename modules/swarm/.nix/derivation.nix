{ lib
, stdenv
, scons
, clang-tools
, forstio
, pname
, version
, kel
}:

stdenv.mkDerivation {
	pname = "${pname}-swarm";
	inherit version;
	src = ./..;

	nativeBuildInputs = [
		scons
		clang-tools
	];

	# The module itself is forstio free -- it is schemas and arithmetic. forstio
	# is here only for the test runner, exactly as in modules/core.
	buildInputs = [
		forstio.core
		kel.lbm.core
	];

	doCheck = true;

	checkPhase = ''
		scons test
		./bin/tests
	'';

	preferLocalBuild = true;

	outputs = [ "out" "dev" ];
}
