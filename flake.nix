{
	description = "Flake wrapper for project";

	inputs = {
		nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.11";
	};

	outputs = { self, nixpkgs }:
	let
		system = "x86_64-linux";
		pkgs = import nixpkgs {
			inherit system;
		};
	in {
		packages.${system} = pkgs.callPackage ./lib/core/.nix/derivation.nix {
			inherit pkgs;
		};
	};
}
