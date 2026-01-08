{
	description = "Flake wrapper for project";

	inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.11";

	outputs = { self, nixpkgs }:
		let
			pkgs = nixpkgs.legacyPackages.x86_64-linux;
			packagesSet = import ./default.nix { inherit pkgs; };
		in {
			packages.x86_64-linux = packagesSet;
		};
}
