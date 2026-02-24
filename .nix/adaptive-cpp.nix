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
  }
, rocmPackages
, spirv-tools
, spirv-headers
, spirv-llvm-translator
}:

let
	version = "25.02.0";
in stdenv.mkDerivation {
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
	];

  cmakeFlags = [
    "-DCLANG_INCLUDE_PATH=${llvmPackages.libclang.dev}/include"
  ];

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
  '';

  postPatch = ''
    patchShebangs .
  '';
}
