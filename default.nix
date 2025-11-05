{ pkgs ? import <nixpkgs> {}
, stdenv ? pkgs.llvmPackages_19.stdenv
, clang-tools ? pkgs.clang-tools_19
}:

let

	adaptive-cpp = pkgs.callPackage .nix/adaptive-cpp.nix {
		inherit stdenv;
		llvmPackages = pkgs.llvmPackages_19;
		lld = pkgs.lld_19;
	};

	forstio = (import ((builtins.fetchTarball {
		url = "https://git.keldu.de/forstio-forstio/snapshot/dev.tar.gz";
		sha256 = "sha256:1mbak3ksppkij0325p6gz8mibbss0gfj4w85fs4f1jkz8wpdbkgq";
	}) + "/default.nix"){
		inherit stdenv;
		inherit clang-tools;
		inherit adaptive-cpp;
	}).forstio;

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
		
		cavity_2d = pkgs.callPackage ./examples/cavity_2d/.nix/derivation.nix {
			inherit pname version stdenv forstio;
			kel-lbm = lbm;
		};

		poiseulle_particles_channel_2d = pkgs.callPackage ./examples/poiseulle_particles_channel_2d/.nix/derivation.nix {
			inherit pname version stdenv forstio;
			kel-lbm = lbm;
		};

		meta_2d = pkgs.callPackage ./examples/meta_2d/.nix/derivation.nix {
			inherit pname version stdenv forstio;
			kel-lbm = lbm;
		};

		planetary_3d = pkgs.callPackage ./examples/planetary_3d/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			kel-lbm = lbm;
		};

		poiseulle_2d = pkgs.callPackage ./examples/poiseulle_3d/.nix/derivation.nix {
			inherit pname version stdenv forstio;
			kel-lbm = lbm;
		};

		poiseulle_3d = pkgs.callPackage ./examples/poiseulle_3d/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			kel-lbm = lbm;
		};
	};

	debug = {
		inherit adaptive-cpp;
	};

	release = {
		dev = {
			inherit lbm;
		};

		examples = pkgs.symlinkJoin {
			name = "kel-lbm-sims-${version}";
			paths = [
				lbm
				examples.cavity_2d_gpu
			];
		};
	};
}
