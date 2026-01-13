{ lib
, stdenv
, scons
, clang-tools
, forstio
, python3
, pname
, version
, kel
, adaptive-cpp
}:

stdenv.mkDerivation {
	pname = pname + "-examples-" + "particles";
	inherit version;
	src = ./..;

	nativeBuildInputs = [
		scons
		clang-tools
		python3
	];

	buildInputs = [
		forstio.core
		forstio.async
		forstio.codec
		forstio.codec-unit
		forstio.codec-json
		kel.lbm.core
		adaptive-cpp
	];

	preferLocalBuild = true;

	outputs = [ "out" "dev" ];
}
