{ lib
, stdenv
, fetchFromGitHub
}:

stdenv.mkDerivation {
	pname = "BadChimp";
	version = "0.0.0";

	src = fetchFromGitHub {
	};
}
