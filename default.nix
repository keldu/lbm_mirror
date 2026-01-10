{ pkgs ? import <nixpkgs> {}
, llvmPackages ? pkgs.llvmPackages_19
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
		inherit stdenv;
		llvmPackages = pkgs.llvmPackages_19;
		lld = pkgs.lld_19;
	};

	forstio = (import ((builtins.fetchTarball {
		url = "https://git.keldu.de/forstio-forstio/snapshot/master.tar.gz";
		sha256 = "sha256:19050mb1j4qq9j9y24y0ffhlj997bzn4my2wiprsgml58azf600m";
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

  util = {
    lbm_ogl_renderer = pkgs.callPackage ./util/ogl_renderer/.nix/derivation.nix {
      inherit forstio stdenv clang-tools pname version kel-lbm;
      kel-glad = glad_opengl_33_core;
    };
  };

	examples = {
		cavity_2d_gpu = pkgs.callPackage ./examples/cavity_2d_gpu/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel-lbm;
		};
		
		poiseulle_2d_gpu = pkgs.callPackage ./examples/poiseulle_2d_gpu/.nix/derivation.nix {
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

		particles_gpu = pkgs.callPackage ./examples/particles_gpu/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel-lbm;
		};

		heterogeneous_computing = pkgs.callPackage ./examples/heterogeneous_computing/.nix/derivation.nix {
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
				examples.cavity_2d_gpu
			];
		};
	};

	default = release.examples;
}
