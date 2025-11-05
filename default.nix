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
		sha256 = "sha256:1za6a9379s1i4wb1lpj6bxd6brs6ivk3zvs3pjys4wmhpsji6csj";
	}) + "/default.nix"){
		inherit stdenv;
		inherit clang-tools;
		inherit adaptive-cpp;
	}).forstio;

	pname = "kel-lbm";
	version = "0.0.3";
in rec {
	kel-lbm = {
		core = pkgs.callPackage ./lib/core/.nix/derivation.nix {
			inherit forstio stdenv clang-tools pname version;
		};
		sycl = pkgs.callPackage ./lib/sycl/.nix/derivation.nix {
			inherit forstio stdenv clang-tools pname version kel-lbm adaptive-cpp;
		};
	};

	examples = {
		cavity_2d_gpu = pkgs.callPackage ./examples/cavity_2d_gpu/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel-lbm;
		};
		
		cavity_2d = pkgs.callPackage ./examples/cavity_2d/.nix/derivation.nix {
			inherit pname version stdenv forstio;
			inherit kel-lbm;
		};

		poiseulle_particles_channel_2d = pkgs.callPackage ./examples/poiseulle_particles_channel_2d/.nix/derivation.nix {
			inherit pname version stdenv forstio;
			inherit kel-lbm;
		};

		meta_2d = pkgs.callPackage ./examples/meta_2d/.nix/derivation.nix {
			inherit pname version stdenv forstio;
			inherit kel-lbm;
		};

		planetary_3d = pkgs.callPackage ./examples/planetary_3d/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel-lbm;
		};

		poiseulle_2d = pkgs.callPackage ./examples/poiseulle_3d/.nix/derivation.nix {
			inherit pname version stdenv forstio;
			inherit kel-lbm;
		};

		poiseulle_3d = pkgs.callPackage ./examples/poiseulle_3d/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel-lbm;
		};
	};

	debug = {
		inherit adaptive-cpp;
	};

	release = {
		dev = {
			inherit kel-lbm;
		};

		examples = pkgs.symlinkJoin {
			name = "kel-lbm-sims-${version}";
			paths = [
				kel-lbm.core
				kel-lbm.sycl
				examples.cavity_2d_gpu
			];
		};
	};
}
