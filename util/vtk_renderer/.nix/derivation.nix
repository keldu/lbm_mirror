{ lib
, stdenv
, gnumake
, pkg-config
, vtk
}:

stdenv.mkDerivation {
	pname = "kel-vtk-renderer";
	version = "0.0.1";

	nativeBuildInputs = [
		gnumake
		pkg-config
	];

	buildInputs = [
		vtk
	];

	installPhase = ''
		mkdir -p $out/bin
		cp kel_vtk_renderer $out/bin
	'';
}
