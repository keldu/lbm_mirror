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
		packageSet = pkgs.callPackage ./default.nix {
			pkgs = pkgs;
		};
	in {
		packages.${system} = packageSet;
	};
}
