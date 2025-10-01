{ pkgs ? import <nixpkgs> {}
, stdenv ? pkgs.llvmPackages_19.stdenv
, clang-tools ? pkgs.clang-tools_19
}:

let
  forstio = (import ((builtins.fetchGit {
    url = "git@git.keldu.de:forstio/forstio";
    ref = "dev";
  }).outPath + "/default.nix"){
    inherit stdenv;
    inherit clang-tools;
	}).forstio;

	adaptive-cpp = pkgs.callPackage ./.nix/adaptive-cpp.nix {
			inherit stdenv;
			llvmPackages = pkgs.llvmPackages_19;
			lld = pkgs.lld_19;
	};

	pname = "kel-lbm";
	version = "0.0.2";
in rec {
	lbm = pkgs.callPackage ./.nix/derivation.nix {
		inherit forstio;
		inherit stdenv;
		inherit clang-tools;
	};

	examples = {
		cavity_2d_gpu = pkgs.callPackage ./examples/cavity_2d_gpu/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			kel-lbm = lbm;
		};

		poiseulle_particles_channel_2d = pkgs.callPackage ./examples/poiseulle_particles_channel_2d/.nix/derivation.nix {
			inherit pname version stdenv forstio;
			kel-lbm = lbm;
		};
	};

	debug = {
		inherit adaptive-cpp;
	};
}
