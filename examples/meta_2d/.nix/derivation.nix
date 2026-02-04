{ lib
, stdenv
, scons
, clang-tools
, forstio
, pname
, version
, kel
}:

stdenv.mkDerivation {
	pname = pname + "-examples-" + "meta_2d";
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
		kel.lbm.core
  ];

	preferLocalBuild = true;

  outputs = [ "out" "dev" ];
}
