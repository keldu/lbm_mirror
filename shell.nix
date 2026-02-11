{ pkgs ? import <nixpkgs> {}
, ...
}:

pkgs.mkShell {
	buildInputs = [
		pkgs.scons
		pkgs.clang
		pkgs.clang-tools
	];

	shellHook = ''
		if [ ! -f compile_commands.json ]; then
			printf "Generating compile_commands.json...\n"
			scons cdb
		fi
	'';
}
