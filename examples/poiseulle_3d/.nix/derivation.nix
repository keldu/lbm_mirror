{ lib
, stdenv
, scons
, clang-tools
, forstio
, pname
, version
, kel
, adaptive-cpp
}:

stdenv.mkDerivation {
	pname = pname + "-examples-" + "poiseulle_3d";
	inherit version;
	src = ./..;

	nativeBuildInputs = [
		scons
		clang-tools
	];

	buildInputs = [
		forstio.core
		forstio.remote-filesystem
		forstio.io
		forstio.async
		forstio.codec
		forstio.codec-unit
		forstio.codec-json
		forstio.remote
		adaptive-cpp
		kel.lbm.core
		kel.lbm.sycl
	];

	preferLocalBuild = true;

	outputs = [ "out" "dev" ];
}
