{ lib
, stdenv
, scons
, clang-tools
, forstio
, keldu
, pname
, version
, adaptive-cpp-dev
, kel-lbm
}:

stdenv.mkDerivation {
	pname = pname + "-examples-" + "cavity_2d_gpu";
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
		forstio.remote
		forstio.codec-json
		adaptive-cpp-dev
		kel-lbm
  ];

	preferLocalBuild = true;

  outputs = [ "out" "dev" ];
}
