{ lib
, stdenv
, fetchFromGitHub
, cmake
, makeWrapper
, boost
, llvmPackages
, lld
, libffi
, libxml2
, python3
, config ? {
    rocm = false;
    cuda = false;
  }
, rocmPackages
, cudaPackages
, spirv-tools
, spirv-headers
, spirv-llvm-translator
}:

let
	version = "25.02.0";
in stdenv.mkDerivation ({
	pname = "adaptive-cpp-kel-custom";
	inherit version;

	src = fetchFromGitHub {
		owner = "AdaptiveCpp";
		repo = "AdaptiveCpp";
		tag = "v${version}";
		sha256 = "sha256-vXfw8+xn3/DYxUKp3QGdQ8sEbDwyk+8jDCyuvQOXigc=";
	};

	nativeBuildInputs = [
    cmake
    makeWrapper
	];

	buildInputs = [
		boost
		llvmPackages.openmp
		llvmPackages.libclang
		llvmPackages.llvm
		python3
	] ++ lib.optionals config.rocm [
		rocmPackages.clr
		rocmPackages.rocm-runtime
		libffi
		libxml2
		spirv-tools
		spirv-headers
		spirv-llvm-translator
	] ++ lib.optionals config.cuda [
		cudaPackages.cuda_cudart
		cudaPackages.cuda_nvcc
	];

  cmakeFlags = [
    "-DCLANG_INCLUDE_PATH=${llvmPackages.libclang.dev}/include"
  ] ++ lib.optionals config.rocm [
    "-DWITH_ROCM_BACKEND=ON"
  ] ++ lib.optionals config.cuda [
    "-DWITH_CUDA_BACKEND=ON"

    # nixpkgs splits the toolkit across outputs, so point cmake at the joined
    # root rather than letting it search /usr/local/cuda.
    "-DCUDA_TOOLKIT_ROOT_DIR=${cudaPackages.cudatoolkit}"
  ] ++ lib.optionals (config.rocm || config.cuda) [
    # Keep OpenCL and Level Zero off. src/CMakeLists.txt turns WITH_LLVM_TO_SPIRV
    # on for either of them, and that target FetchContent-clones
    # SPIRV-LLVM-Translator at configure time, which a nix build cannot do --
    # it has neither git nor network. Neither ROCm (amdgcn) nor CUDA (PTX) goes
    # through SPIR-V, so turning them off costs nothing here.
    #
    # This only bites with a GPU backend on: rocmPackages.clr and the CUDA
    # toolkit both bring OpenCL in, so the backend autodetects on, where in the
    # base build it stays off.
    "-DWITH_OPENCL_BACKEND=OFF"
    "-DWITH_LEVEL_ZERO_BACKEND=OFF"
  ];

  # nixpkgs 26.05 turns the zerocallusedregs hardening flag on by default, and
  # clang rejects -fzero-call-used-regs for the amdgcn-amd-amdhsa target, so
  # every device bitcode object under src/libkernel/sscp/amdgpu fails to build.
  # Not an AdaptiveCpp problem -- the same source built fine on 25.11.

  postInstall = ''
    if [ -d "$out/include/AdaptiveCpp" ]; then
      mv $out/include/AdaptiveCpp .hidden-fake
      mv .hidden-fake/* $out/include/
      rmdir .hidden-fake
    fi
  '';

  postFixup = ''
    wrapProgram $out/bin/syclcc-clang \
      --prefix PATH : ${lib.makeBinPath [ lld python3 ]} \
      --add-flags "-L${llvmPackages.openmp}/lib" \
      --add-flags "-I${llvmPackages.openmp.dev}/include" \
  '' + lib.optionalString config.rocm ''
    --add-flags "--rocm-device-lib-path=${rocmPackages.rocm-device-libs}/amdgcn/bitcode"
  '' + lib.optionalString config.cuda ''
    --add-flags "--cuda-path=${cudaPackages.cudatoolkit}"
  '';

  postPatch = ''
    patchShebangs .
  '';
} // lib.optionalAttrs (config.rocm || config.cuda) {
  # nixpkgs 26.05 turns the zerocallusedregs hardening flag on by default, and
  # clang rejects -fzero-call-used-regs for the amdgcn-amd-amdhsa target, so
  # every device bitcode object under src/libkernel/sscp/amdgpu fails to build.
  # Not an AdaptiveCpp problem -- the same source built fine on 25.11.
  #
  # nvptx64 rejects the same flag, so the CUDA backend needs it off as well.
  #
  # Set via optionalAttrs rather than lib.optionals so the attribute is absent
  # (not just empty) on the base build, which keeps its derivation hash stable.
  hardeningDisable = [ "zerocallusedregs" ];
})
