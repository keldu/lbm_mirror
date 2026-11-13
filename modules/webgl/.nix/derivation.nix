{ lib
, stdenv
, scons
, clang-tools
, emscripten
, python3
, nodejs
, pname
, version
, kel
}:

stdenv.mkDerivation {
	pname = "${pname}-webgl";
	inherit version;
	src = ./..;

	nativeBuildInputs = [
		scons
		clang-tools
		emscripten
		python3
		nodejs
	];

	# Deliberately empty. em++ is not a nix cc-wrapper, so it never reads
	# NIX_CFLAGS_COMPILE and cannot consume buildInputs the way core and sycl do.
	# Include roots are handed to scons explicitly below, which also has the
	# virtue of making the dependency visible rather than ambient.
	buildInputs = [ ];

	sconsFlags = [
		"kel_include=${kel.lbm.core.dev}/include:${kel.lbm.swarm.dev}/include"
	];

	# Same class of problem as the `export HOME="$TMPDIR"` AdaptiveCpp hack in
	# modules/sycl/.nix/derivation.nix: a toolchain that insists on a writable
	# cache inside a read-only build.
	#
	# emscripten's wrappers source locate_cache.sh, which points EM_CACHE at a
	# /tmp path. In the sandbox that is writable but empty, so emcc would
	# rebuild the entire sysroot for every target. Seed it from the cache the
	# emscripten derivation already precompiled (~81MB, with sysroot and
	# sysroot_install.stamp) and make it writable.
	preBuild = ''
		export HOME="$TMPDIR"
		export EM_CACHE="$TMPDIR/em_cache"
		cp -r ${emscripten}/share/emscripten/cache "$EM_CACHE"
		chmod -R u+w "$EM_CACHE"
	'';

	# Nothing here can run without a browser; the probe only has to link.
	doCheck = false;

	preferLocalBuild = true;

	outputs = [ "out" "dev" ];
}
