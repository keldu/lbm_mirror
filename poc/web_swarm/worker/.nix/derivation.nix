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
	pname = "${pname}-web-swarm-worker";
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
	# NIX_CFLAGS_COMPILE and cannot consume buildInputs the way the native
	# builds do. Include roots are handed to scons explicitly below, which also
	# has the virtue of making the dependency visible rather than ambient.
	buildInputs = [ ];

	sconsFlags = [
		("kel_include=" +
			"${kel.lbm.core.dev}/include:" +
			"${kel.lbm.swarm.dev}/include:" +
			"${kel.lbm.webgl.dev}/include")
	];

	# Same class of problem as the AdaptiveCpp HOME hack in
	# modules/sycl/.nix/derivation.nix: a toolchain that insists on a writable
	# cache inside a read-only build. Seed it from the cache the emscripten
	# derivation already precompiled, or every build rebuilds the sysroot.
	preBuild = ''
		export HOME="$TMPDIR"
		export EM_CACHE="$TMPDIR/em_cache"
		cp -r ${emscripten}/share/emscripten/cache "$EM_CACHE"
		chmod -R u+w "$EM_CACHE"
	'';

	# The real worker needs a browser. Its protocol half does not, so that part
	# is built for node and run here; only the GL probe is left uncovered.
	doCheck = true;

	checkPhase = ''
		runHook preCheck

		node ./bin/kel_lbm_worker_selftest.js

		runHook postCheck
	'';

	postInstall = ''
		cat > $out/share/kel-lbm/worker/README <<'EOT'
		Serve this directory over HTTP and open index.html.

		  python3 -m http.server 8000

		file:// will not work: wasm is fetched and file:// is a null origin.

		The orchestrator is a separate process on its own port -- a WebSocket
		server is not an HTTP server, so it does not serve this page. Start it
		with kel_lbm_orchestrator and point the page at it, either in the form
		on the page or with ?orchestrator=ws://host:port/
		EOT
	'';

	preferLocalBuild = true;

	outputs = [ "out" ];
}
