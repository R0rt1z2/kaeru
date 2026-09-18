{
  description = "Kaeru build environment";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
  inputs.frostix = {
    url = "github:shomykohai/frostix";
  };

  outputs = {
    self,
    nixpkgs,
    ...
  } @ inputs: let
    system = "x86_64-linux";
    pkgs = import nixpkgs { inherit system; };
    frostix = inputs.frostix.legacyPackages.${system};
    gccToolchain = frostix.gcc-toolchain.gcc-arm-linux-gnueabihf-12;
  in {
    devShells.${system}.default = pkgs.mkShell {
      nativeBuildInputs = [
        pkgs.git
        pkgs.python3
        pkgs.gnumake
        pkgs.android-tools
        pkgs.python3Packages.capstone

        frostix.lkpatcher
        gccToolchain
      ];

      shellHook = ''
        export CROSS_COMPILE=arm-none-linux-gnueabihf-
        export CC=${gccToolchain}/bin/''${CROSS_COMPILE}gcc
        export AS=${gccToolchain}/bin/''${CROSS_COMPILE}as
        export AR=${gccToolchain}/bin/''${CROSS_COMPILE}ar
        export LD=${gccToolchain}/bin/''${CROSS_COMPILE}ld
        export OBJCOPY=${gccToolchain}/bin/''${CROSS_COMPILE}objcopy
        export OBJDUMP=${gccToolchain}/bin/''${CROSS_COMPILE}objdump
        export NM=${gccToolchain}/bin/''${CROSS_COMPILE}nm
        export RANLIB=${gccToolchain}/bin/''${CROSS_COMPILE}ranlib

        export MAKEFLAGS="-e --no-print-directory"
      '';
    };
  };
}
