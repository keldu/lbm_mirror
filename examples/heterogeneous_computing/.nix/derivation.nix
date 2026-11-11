{ lib
, stdenv
, scons
, clang-tools
, forstio
, pname
, version
, adaptive-cpp
, kel
}:

stdenv.mkDerivation {
	pname = pname + "-examples-" + "heterogeneous_computing";
	inherit version;
	src = ./..;

	nativeBuildInputs = [
		scons
		clang-tools
	];

	buildInputs = [
		forstio.core
		forstio.async
		forstio.io
		forstio.io_codec
		forstio.codec
		forstio.codec-unit
		forstio.codec-json
		forstio.remote
		forstio.remote-filesystem
		forstio.remote-io
		forstio.remote-sycl
		kel.lbm.core
		adaptive-cpp
	];

	preferLocalBuild = true;

	outputs = [ "out" "dev" ];
}
