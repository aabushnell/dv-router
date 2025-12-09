{
  description = "Distance-Vector Routing Dev Environment";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-25.11";
  };

  outputs = { self, nixpkgs, ... }: let
    system = "aarch64-darwin";
  in {
    devShells."${system}".default = let
      pkgs = import nixpkgs { inherit system; };
    in pkgs.mkShell {

      packages = with pkgs; [
        clang-tools
        clang
        gcc
      ];

      shellHook = ''
        export CPLUS_INCLUDE_PATH="${pkgs.llvmPackages.libcxx.dev}/include/c++/v1:$CPLUS_INCLUDE_PATH"
      '';
    };
  };
}
