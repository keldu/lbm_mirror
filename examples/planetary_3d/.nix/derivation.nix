{ lib
, stdenv
, scons
, clang-tools
, forstio
, pname
, version
, kel-lbm
}:

stdenv.mkDerivation {
	pname = pname + "-examples-" + "planetary_3d";
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
		forstio.remote-sycl
		forstio.remote-io
		kel-lbm
	];

	preferLocalBuild = true;

	outputs = [ "out" "dev" ];
}
