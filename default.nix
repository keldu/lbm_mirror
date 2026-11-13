{ pkgs ? (import <nixpkgs> {})
, llvmPackages ? pkgs.llvmPackages_20
, lld ? pkgs.lld_20
, stdenv ? llvmPackages.stdenv
, clang-tools ? llvmPackages.clang-tools
# Build AdaptiveCpp with the ROCm backend. Off by default so the usual
# host/OpenMP path stays cheap to build; turn on with
#   nix-build -A <attr> --arg rocm true
, rocm ? false
# Same for the CUDA backend:
#   nix-build -A <attr> --arg cuda true
# Note that nixpkgs ships the CUDA toolkit as unfree, so this additionally
# needs allowUnfree, e.g.
#   nix-build -A kel.lbm.sycl-cuda --arg cuda true \\
#     --arg pkgs '(import <nixpkgs> { config.allowUnfree = true; })'
, cuda ? false
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

	# One builder, three instances, so the GPU toolchains differ from the base
	# one in exactly one input and nothing else.
	mkAdaptiveCpp = backend: pkgs.callPackage .nix/adaptive-cpp.nix {
		inherit stdenv llvmPackages lld;

		config = {
			rocm = backend == "rocm";
			cuda = backend == "cuda";
		};
	};

	adaptive-cpp-base = mkAdaptiveCpp "host";
	adaptive-cpp-rocm = mkAdaptiveCpp "rocm";
	adaptive-cpp-cuda = mkAdaptiveCpp "cuda";

	# What the examples and the default build use. Flip with --arg rocm/cuda true.
	adaptive-cpp =
		if rocm then adaptive-cpp-rocm
		else if cuda then adaptive-cpp-cuda
		else adaptive-cpp-base;

	# forstio master as of 2026-11-09. Bump together with the sha256 below.
	forstioRev = "97ae1a8f23074fea3784a008921c5d9fc3c1e8fb";

	forstio = let
		forstioSrc = stdenv.mkDerivation {
			name = "forstio-src";

		 # Pinned to a commit, not to master. A branch snapshot is a moving
		 # target: every push to forstio regenerates the tarball and the sha256
		 # below stops matching, so the build breaks with a hash mismatch for
		 # reasons that have nothing to do with this repo.
		 #
		 # To bump: set forstioRev, then run
		 #   nix-prefetch-url --type sha256 \
		 #     https://git.keldu.de/forstio-forstio/snapshot/<rev>.tar.gz
		 src = builtins.fetchurl {
			url = "https://git.keldu.de/forstio-forstio/snapshot/${forstioRev}.tar.gz";
			sha256 = "sha256:0rmss3d83ryb24s8nirjrnxz32irws3hzwqkkxhk4xcf3rn8p8ls";
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
			core = pkgs.callPackage ./modules/core/.nix/derivation.nix {
				inherit forstio stdenv clang-tools pname version;
			};
			# Both toolchains, same sources, same everything else -- so a failure
			# in one and not the other points at the backend rather than the code.
			sycl = pkgs.callPackage ./modules/sycl/.nix/derivation.nix {
				inherit forstio stdenv clang-tools pname version kel;
				adaptive-cpp = adaptive-cpp-base;
			};
			sycl-rocm = pkgs.callPackage ./modules/sycl/.nix/derivation.nix {
				inherit forstio stdenv clang-tools pname version kel;
				adaptive-cpp = adaptive-cpp-rocm;
				variant = "-rocm";
			};
			sycl-cuda = pkgs.callPackage ./modules/sycl/.nix/derivation.nix {
				inherit forstio stdenv clang-tools pname version kel;
				adaptive-cpp = adaptive-cpp-cuda;
				variant = "-cuda";
			};

			# Control protocol shared by the orchestrator and the workers.
			# Schemas and arithmetic only -- no transport, no GL -- so it stays
			# testable natively on both sides of the browser boundary.
			swarm = pkgs.callPackage ./modules/swarm/.nix/derivation.nix {
				inherit forstio stdenv clang-tools pname version kel;
			};

			# Browser backend. Built by em++ rather than the nix cc-wrapper, which
			# is why it takes its include roots through sconsFlags instead of
			# buildInputs.
			webgl = pkgs.callPackage ./modules/webgl/.nix/derivation.nix {
				inherit stdenv clang-tools pname version kel;
			};
		};
	};

	# Proof of concept work. Reachable from the top level so it actually gets
	# built; it was previously orphaned and had drifted into not evaluating.
	poc = import ./poc/default.nix {
		inherit pkgs stdenv pname version kel;
	};

	util = {
		lbm_ogl_renderer = pkgs.callPackage ./util/ogl_renderer/.nix/derivation.nix {
			inherit forstio stdenv clang-tools pname version kel;
			kel-glad = glad_opengl_33_core;
		};
	};

	ex = {
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

		# Same case on a 32x16x16 domain, so it runs end to end in seconds.
		poiseulle_3d_gpu_minimal = pkgs.callPackage ./examples/poiseulle_3d_gpu_minimal/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};

		# 32^3 channel carrying one HLBM-coupled sphere. The first 3D case with
		# a particle at all, and the stepping stone to the fixed-axis rotor.
		poiseulle_particle_3d_hlbm_gpu = pkgs.callPackage ./examples/poiseulle_particle_3d_hlbm_gpu/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};

		# Fixed-position rotor, free to spin about one arbitrary fixed axis and
		# driven by the fluid. Angular velocity is an output.
		vawt_3d_hlbm_gpu = pkgs.callPackage ./examples/vawt_3d_hlbm_gpu/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};
		
		moving_poiseulle_particles_2d_hlbm_gpu = pkgs.callPackage ./examples/moving_poiseulle_particles_2d_hlbm_gpu/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};

		moving_poiseulle_particles_2d_fplbm_gpu = pkgs.callPackage ./examples/moving_poiseulle_particles_2d_fplbm_gpu/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};
		
		moving_poiseulle_particles_2d_psm_gpu = pkgs.callPackage ./examples/moving_poiseulle_particles_2d_psm_gpu/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
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

		poiseulle_particles_2d_all_gpu = pkgs.symlinkJoin {
			name = "poiseulle_particles_2d_all_gpu";
			paths = [
				ex.poiseulle_particles_2d_bgk_gpu
				ex.poiseulle_particles_2d_psm_gpu
				ex.poiseulle_particles_2d_hlbm_gpu
				ex.poiseulle_particles_2d_fplbm_gpu
				ex.poiseulle_particles_2d_ibm_gpu
			];
		};
		
		poiseulle_particles_2d_gpu = pkgs.callPackage ./examples/poiseulle_particles_2d_gpu/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};

		stokes_drag_particle_2d_bgk_gpu = pkgs.callPackage ./examples/stokes_drag_particle_2d_bgk_gpu/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};

		stokes_drag_particle_2d_psm_gpu = pkgs.callPackage ./examples/stokes_drag_particle_2d_psm_gpu/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};

		stokes_drag_particle_2d_fplbm_gpu = pkgs.callPackage ./examples/stokes_drag_particle_2d_fplbm_gpu/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};

		stokes_drag_particle_2d_hlbm_gpu = pkgs.callPackage ./examples/stokes_drag_particle_2d_hlbm_gpu/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};

		poiseulle_3d = pkgs.callPackage ./examples/poiseulle_3d/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};

		settling_cubes_2d_ibm_gpu = pkgs.callPackage ./examples/settling_cubes_2d_ibm_gpu/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};
		
		settling_cubes_2d_hlbm_gpu = pkgs.callPackage ./examples/settling_spheres_2d_hlbm_gpu/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};

		stdkr_2d = pkgs.callPackage ./examples/schaefer_turek_durst_krause_rannbacher/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};
		
		stdkr_2d_bgk = pkgs.callPackage ./examples/stdkr_2d_bgk/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};

		ops_2d = pkgs.callPackage ./examples/one_particle_sedimentation_2d/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};
		
		throat_2d = pkgs.callPackage ./examples/throat_clog_2d/.nix/derivation.nix {
			inherit pname version stdenv forstio adaptive-cpp;
			inherit kel;
		};

		settling_particles = pkgs.symlinkJoin {
			name = "kel-lbm-settling_particles-${version}";
			paths = [
				ex.settling_cubes_2d_ibm_gpu
			];
		};
	};

	debug = {
		inherit adaptive-cpp;
	};

	release = {
		dev = {
			inherit kel;
		};

		ex = pkgs.symlinkJoin {
			name = "kel-lbm-sims-${version}";
			paths = [
				kel.lbm.core
				kel.lbm.sycl
				ex.poiseulle_particles_2d_bgk_gpu
				ex.poiseulle_particles_2d_psm_gpu
				ex.poiseulle_particles_2d_hlbm_gpu
				ex.poiseulle_particles_2d_fplbm_gpu
				ex.poiseulle_particles_2d_ibm_gpu
			];
		};

		poiseulle_particles_2d = pkgs.symlinkJoin {
			name = "kel-lbm-sims-poiseulle_particles_2d-${version}";
			paths = [
				ex.poiseulle_particles_2d_bgk_gpu
				ex.poiseulle_particles_2d_psm_gpu
				ex.poiseulle_particles_2d_hlbm_gpu
				ex.poiseulle_particles_2d_fplbm_gpu
			];
		};
		stokes_drag_2d = pkgs.symlinkJoin {
			name = "kel-lbm-sims-stokes_drag_2d-${version}";
			paths = [
				ex.stokes_drag_particle_2d_bgk_gpu
				ex.stokes_drag_particle_2d_psm_gpu
				ex.stokes_drag_particle_2d_hlbm_gpu
				ex.stokes_drag_particle_2d_fplbm_gpu
			];
		};

		ecmor = pkgs.symlinkJoin {
			name = "kel_ecmor";
			paths = [
				release.stokes_drag_2d
				release.poiseulle_particles_2d
			];
		};
	};

	default = release.ex;
}
