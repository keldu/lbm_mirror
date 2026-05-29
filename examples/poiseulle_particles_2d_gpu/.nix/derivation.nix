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
, slip ? false
, particle_coupling ? "fplbm"
}:

let
	slip_txt = if slip then "slip" else "noslip";
in stdenv.mkDerivation {
	pname = "${pname}-examples-poiseulle_2d_gpu_${particle_coupling}_${slip_txt}";
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
