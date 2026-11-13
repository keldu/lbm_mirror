{ pkgs ? import <nixpkgs> {}
, stdenv
, pname
, version
, kel
}:

{
	# Orchestrator and native worker share a SConstruct and are checked against
	# each other, so they are one derivation.
	native = pkgs.callPackage ./.nix/derivation.nix {
		inherit stdenv pname version kel;
	};

	# The browser worker is separate: different toolchain, and it cannot be
	# tested in a check phase because a check phase has no browser.
	worker = pkgs.callPackage ./worker/.nix/derivation.nix {
		inherit stdenv pname version kel;
	};
}
