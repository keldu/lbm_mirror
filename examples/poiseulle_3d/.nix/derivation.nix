{ lib
, stdenv
, scons
, clang-tools
, forstio
, pname
, version
, kel-lbm
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
		forstio.async
		forstio.codec
		forstio.codec-unit
		forstio.codec-json
		forstio.remote
		forstio.remote-sycl
		adaptive-cpp
		kel-lbm
	];

	preferLocalBuild = true;

	outputs = [ "out" "dev" ];
}
