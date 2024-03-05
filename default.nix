{ pkgs ? import <nixpkgs> {}
, stdenv ? pkgs.stdenv
, clang-tools ? pkgs.clang-tools_16
}:

let
  forstio = (import ((builtins.fetchGit {
    url = "git@git.keldu.de:forstio/forstio";
    ref = "dev";
  }).outPath + "/default.nix"){
    inherit stdenv;
    inherit clang-tools;
  }).forstio;

  kel-unit = (import ((builtins.fetchGit {
    url = "git@git.keldu.de:libs/unit";
    ref = "dev";
  }).outPath + "/default.nix"){
    inherit stdenv;
    inherit clang-tools;
  });

in pkgs.callPackage ./.nix/derivation.nix {
  inherit forstio;
  inherit kel-unit;
  inherit stdenv;
  inherit clang-tools;
}
