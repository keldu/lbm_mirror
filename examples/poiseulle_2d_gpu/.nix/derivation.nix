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
	pname = pname + "-examples-" + "poiseulle_2d_gpu";
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
		forstio.remote
		forstio.codec-json
		forstio.io
		adaptive-cpp
		kel.lbm.core
#		kel-lbm.sycl
	];

	preferLocalBuild = true;

	outputs = [ "out" "dev" ];
}
