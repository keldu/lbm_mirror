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
		kel-lbm
  ];

	preferLocalBuild = true;

  outputs = [ "out" "dev" ];
}
