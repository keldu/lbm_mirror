{ pkgs ? import <nixpkgs> {}
}:

{
	ui = pkgs.callPackage ./ui/.nix/derivation.nix {};
}
