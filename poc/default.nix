{ pkgs ? import <nixpkgs> {}
, stdenv
, pname
, version
, kel
}:

{
	web_swarm = import ./web_swarm/default.nix {
		inherit pkgs stdenv pname version kel;
	};
}
