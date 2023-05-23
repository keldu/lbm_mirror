{ pkgs ? import <nixpkgs> {}
}:

let
  forstio = (import ((builtins.fetchGit {
    url = "git@git.keldu.de:forstio/forstio";
    ref = "dev";
  }).outPath + "/default.nix"){}).forstio;

  kel-unit = (import ((builtins.fetchGit {
    url = "git@git.keldu.de:libs/unit";
    ref = "dev";
  }).outPath + "/default.nix"){});

in pkgs.callPackage ./.nix/derivation.nix {
  inherit forstio;
  inherit kel-unit;
}
