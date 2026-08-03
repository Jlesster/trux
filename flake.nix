{
  description = "Base C++ dev shell (no project-specific libs — CMake/Ninja/clang only)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        devShells.default = pkgs.mkShell {
          nativeBuildInputs = with pkgs; [
            cmake
            ninja
            pkg-config
            clang-tools # clangd, clang-format, clang-tidy
            gdb
          ];

          buildInputs = with pkgs; [
            clang
            # uncomment as needed:
            # catch2_3     # unit testing
            # gtest        # unit testing (Google Test)
            # fmt          # formatting library
            # boost        # boost libs
          ];

          shellHook = ''
            export CC=clang
            export CXX=clang++
            export CMAKE_GENERATOR=Ninja
            export CMAKE_EXPORT_COMPILE_COMMANDS=ON
            echo "base C++ shell ready ($CXX $(clang --version | head -n1 | grep -oP '\d+\.\d+\.\d+'))"
          '';
        };
      }
    );
}
