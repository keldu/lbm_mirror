{ lib
, stdenv
, scons
, clang-tools
, pname
, version
, forstio
, kel
, adaptive-cpp
# Names the build so the ROCm and non-ROCm variants land in distinct outputs.
, variant ? ""
}:

stdenv.mkDerivation {
	pname = "${pname}-sycl${variant}";
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
		#forstio.remote-sycl
		kel.lbm.core
		adaptive-cpp
	];

	doCheck = true;
	checkPhase = ''
		# AdaptiveCpp caches JIT output under $HOME, which is /homeless-shelter
		# in a nix build. Point it somewhere writable or the tests die before
		# running a single case.
		export HOME="$TMPDIR"

		scons test
		./bin/tests
	'';

	preferLocalBuild = true;

	outputs = [ "out" "dev" ];
}
