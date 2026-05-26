{ pkgs ? (import <nixpkgs> {})
, llvmPackages ? pkgs.llvmPackages_18
, lld ? pkgs.lld_18
, stdenv ? llvmPackages.stdenv
, clang-tools ? llvmPackages.clang-tools
}:

let
	glad_opengl_33_core = stdenv.mkDerivation {
		pname = "glad_opengl_33_core";
		version = "0.0.1";

		dontUnpack = true;

		nativeBuildInputs = [
			pkgs.python312Packages.glad2
		];

		buildPhase = ''
			mkdir -p glad_out
			glad \
			--api gl:core=3.3 \
			--out-path glad_out \
			--reproducible \
			c
			mkdir -p glad_out/lib
			cc -Iglad_out/include -fPIC -shared -o glad_out/lib/libgl.so glad_out/src/gl.c
		'';

		installPhase = ''
			mv glad_out $out
		'';
	};

	adaptive-cpp = pkgs.callPackage .nix/adaptive-cpp.nix {
		inherit stdenv llvmPackages lld;

		config = {
			rocm = false;
		};
	};

	forstio = let
		forstioSrc = stdenv.mkDerivation {
			name = "forstio-src";

		 src = builtins.fetchurl {
			url = "https://git.keldu.de/forstio-forstio/snapshot/master.tar.gz";
			sha256 = "sha256:1ra0z2z0hbhrc372kbs6bs7qh00xc9xj3b40pvs0cjfdpvcfb538";
		 };

		 phases = [ "unpackPhase" "installPhase" ];

		 unpackPhase = ''
			mkdir source
			tar -xzf "$src" -C source --strip-components=1
		 '';

		 installPhase = ''
			cp -r source $out
		 '';
	  };
	in
	(import "${forstioSrc}/default.nix" {
		inherit stdenv clang-tools adaptive-cpp;
	}).forstio;

	pname = "kel-lbm";
	version = "0.0.5";
in rec {
	kel = {
		lbm = {
			core = pkgs.callPackage ./lib/core/.nix/derivation.nix {
				inherit forstio stdenv clang-tools pname version;
			};
			sycl = pkgs.callPackage ./lib/sycl/.nix/derivation.nix {
				inherit forstio stdenv clang-tools pname version kel adaptive-cpp;
			};
		};
	};

	util = {
		lbm_ogl_renderer = pkgs.callPackage ./util/ogl_renderer/.nix/derivation.nix {
			inherit forstio stdenv clang-tools pname version kel;
			kel-glad = glad_opengl_33_core;
		};
	};

	examples = {
		cavity_2d_gpu = pkgs.callPackage ./examples/cavity_2d_gpu/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};
		
		poiseulle_2d_gpu = pkgs.callPackage ./examples/poiseulle_2d_gpu/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};
		
		poiseulle_3d_gpu = pkgs.callPackage ./examples/poiseulle_3d_gpu/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};
		
		cavity_2d = pkgs.callPackage ./examples/cavity_2d/.nix/derivation.nix {
			inherit pname version stdenv forstio;
			inherit kel;
		};

		meta = pkgs.callPackage ./examples/meta/.nix/derivation.nix {
			inherit pname version stdenv forstio;
			inherit kel;
		};

		planetary_3d = pkgs.callPackage ./examples/planetary_3d/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};

		poiseulle_2d = pkgs.callPackage ./examples/poiseulle_3d/.nix/derivation.nix {
			inherit pname version stdenv forstio;
			inherit kel;
		};
		
		poiseulle_particles_2d_psm_gpu = pkgs.callPackage ./examples/poiseulle_particles_2d_psm_gpu/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};
		
		poiseulle_particles_2d_hlbm_gpu = pkgs.callPackage ./examples/poiseulle_particles_2d_hlbm_gpu/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};
		
		poiseulle_particles_2d_bgk_gpu = pkgs.callPackage ./examples/poiseulle_particles_2d_bgk_gpu/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};
		
		poiseulle_particles_2d_ibm_gpu = pkgs.callPackage ./examples/poiseulle_particles_2d_ibm_gpu/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};
		
		poiseulle_particles_2d_fplbm_gpu = pkgs.callPackage ./examples/poiseulle_particles_2d_fplbm_gpu/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};

		poiseulle_3d = pkgs.callPackage ./examples/poiseulle_3d/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};

		particles_gpu = pkgs.callPackage ./examples/particles_gpu/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};
		
		settling_cubes_2d_ibm_gpu = pkgs.callPackage ./examples/settling_cubes_2d_ibm_gpu/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};

		heterogeneous_computing = pkgs.callPackage ./examples/heterogeneous_computing/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};
	};

	debug = {
		inherit adaptive-cpp;
	};

	release = {
		dev = {
			inherit kel;
		};

		examples = pkgs.symlinkJoin {
			name = "kel-lbm-sims-${version}";
			paths = [
				kel.lbm.core
				kel.lbm.sycl
				examples.meta
				examples.poiseulle_particles_2d_bgk_gpu
				examples.poiseulle_particles_2d_psm_gpu
				examples.poiseulle_particles_2d_hlbm_gpu
				examples.poiseulle_particles_2d_fplbm_gpu
				examples.poiseulle_3d_gpu
			];
		};
	};

	default = release.examples;
}
