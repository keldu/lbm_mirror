{ lib
, stdenv
, scons
, clang-tools
, forstio
, pname
, version
}:

stdenv.mkDerivation {
	inherit pname version;
	src = ./..;

  nativeBuildInputs = [
    scons
    clang-tools
  ];

  # The library itself is forstio free; only the test suite still uses
  # forstio/test/suite.hpp, which needs core for its common.hpp and the
  # forstio-test runner.
  buildInputs = [
    forstio.core
  ];

  doCheck = true;
  checkPhase = ''
    scons test
    ./bin/tests
	'';

	preferLocalBuild = true;

  outputs = [ "out" "dev" ];
}
