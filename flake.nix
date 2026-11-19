{
  description = "trux - TUI compositing framework";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ]; # eventfd => Linux only
      forAllSystems = f: nixpkgs.lib.genAttrs systems
        (system: f nixpkgs.legacyPackages.${system});

      mkTrux = pkgs: pkgs.stdenv.mkDerivation {
        pname = "trux";
        version = "0.1.0"; # keep in sync with project(VERSION) in CMakeLists.txt
        src = self;

        nativeBuildInputs = [ pkgs.cmake pkgs.ninja ];

        cmakeFlags = [ "-DTRUX_BUILD_DOCS=OFF" ];
        cmakeBuildType = "Release";

        doCheck = true; # runs ctest; set false if any test needs a tty

        meta = {
          description = "Immediate-mode TUI compositing framework (C++)";
          homepage = "https://github.com/Jlesster/trux";
          license = pkgs.lib.licenses.mit;
          platforms = pkgs.lib.platforms.linux;
        };
      };
    in
    {
      packages = forAllSystems (pkgs: rec {
        trux = mkTrux pkgs;
        default = trux;
      });
      overlays.default = final: _prev: { trux = mkTrux final; };
      checks = forAllSystems (pkgs: { default = self.packages.${pkgs.system}.trux; });

      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          inputsFrom = [ self.packages.${pkgs.system}.trux ];
          packages = [ pkgs.clang-tools pkgs.gdb pkgs.doxygen pkgs.graphviz ];
        };
      });
    };
}
