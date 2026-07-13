{ lib
, stdenv
, scons
, clang-tools
, forstio
, python3
, pname
, version
, adaptive-cpp
, kel
}:

stdenv.mkDerivation {
	pname = pname + "-poc-" + "triangle_stream";
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
		forstio.io
    forstio.remote
    forstio.remote-filesystem
		forstio.codec-json
		adaptive-cpp
		kel.lbm.core
		kel.lbm.sycl
	];

	preferLocalBuild = true;

	outputs = [ "out" "dev" ];
}
