{ lib
, stdenv
, scons
, clang-tools
, libdatachannel
, pname
, version
, kel
}:

stdenv.mkDerivation {
	pname = "${pname}-web-swarm";
	inherit version;
	src = ./..;

	nativeBuildInputs = [
		scons
		clang-tools
	];

	# Ordinary native derivations, so unlike the webgl module these get their
	# include paths from nix the usual way.
	#
	# libdatachannel ships CMake files but no pkg-config, hence the plain
	# LIBS=['datachannel'] in the SConstruct rather than a pkg-config lookup.
	# Note there is no forstio here: libdatachannel owns the threads and the
	# event handling, and forstio has no WebSocket or WebRTC to offer.
	buildInputs = [
		libdatachannel
		kel.lbm.core
		kel.lbm.swarm
	];

	# No checkPhase, and that is a limitation rather than a choice. The nix build
	# sandbox runs in a network namespace holding only a loopback interface, and
	# ICE finds no usable candidate there -- libdatachannel reports "Failed to
	# gather local ICE candidates" and negotiation never completes. Verified: the
	# same two binaries negotiate and register fine outside the sandbox.
	#
	# The end to end check therefore ships as an executable instead, so it is one
	# command rather than a procedure:
	#
	#   nix-build -A poc.web_swarm.native
	#   ./result/bin/kel_lbm_web_swarm_check
	#
	# The protocol itself is covered natively and in-sandbox by kel.lbm.swarm.
	doCheck = false;

	preferLocalBuild = true;

	outputs = [ "out" ];
}
