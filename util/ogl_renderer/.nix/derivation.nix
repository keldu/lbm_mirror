{ lib
, stdenv
, scons
, clang-tools
, pkg-config
, pname
, version
, forstio
, kel
, libGL
, kel-glad
, sdl3
}:

stdenv.mkDerivation {
	pname = pname + "-util-ogl-renderer";
	inherit version;
	src = ./..;

	nativeBuildInputs = [
		scons
		clang-tools
		pkg-config
	];

	buildInputs = [
		forstio.core
		forstio.codec
		forstio.codec-json
		forstio.codec-unit
		kel.lbm.core
		libGL
		kel-glad
		sdl3
	];

	preferLocalBuild = true;
}
