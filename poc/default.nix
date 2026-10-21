{ pkgs ? import <nixpkgs> {}
}:

{
	web_swarm = import ./web_swarm/default.nix {};
}
