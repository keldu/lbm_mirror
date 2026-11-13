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
	pname = pname + "-examples-" + "cavity_2d_gpu";
	inherit version;
	src = ./..;

	nativeBuildInputs = [
		scons
		clang-tools
		python3
	];

	buildInputs = [
		forstio.core
		forstio.io
		forstio.async
		forstio.codec
		forstio.codec-unit
		forstio.remote
		forstio.remote-filesystem
		forstio.codec-json
		adaptive-cpp
		kel.lbm.core
		kel.lbm.sycl
#		kel-lbm.sycl
	];

	preferLocalBuild = true;
}
