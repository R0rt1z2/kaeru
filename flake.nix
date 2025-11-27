{
  description = "Kaeru build environment";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.05";
  inputs.frostix = {
    url = "github:shomykohai/frostix";
    inputs.nixpkgs.follows = "nixpkgs";
  };

  outputs = {
    self,
    nixpkgs,
    ...
  } @ inputs: {
    devShells.x86_64-linux.default = let
      pkgs = import nixpkgs {system = "x86_64-linux";};
      frostix = inputs.frostix.packages.${pkgs.system};
    in
      pkgs.mkShell {
        nativeBuildInputs = [
          pkgs.git
          pkgs.python3
          pkgs.gnumake
          pkgs.android-tools
          pkgs.python3Packages.capstone

          frostix.lkpatcher
          frostix.gcc-arm-linux-gnueabihf
        ];

        shellHook = ''
          export CROSS_COMPILE=arm-none-linux-gnueabihf-
          export CC=${frostix.gcc-arm-linux-gnueabihf}/bin/''${CROSS_COMPILE}gcc
          export AS=${frostix.gcc-arm-linux-gnueabihf}/bin/''${CROSS_COMPILE}as
          export AR=${frostix.gcc-arm-linux-gnueabihf}/bin/''${CROSS_COMPILE}ar
          export LD=${frostix.gcc-arm-linux-gnueabihf}/bin/''${CROSS_COMPILE}ld
          export OBJCOPY=${frostix.gcc-arm-linux-gnueabihf}/bin/''${CROSS_COMPILE}objcopy
          export OBJDUMP=${frostix.gcc-arm-linux-gnueabihf}/bin/''${CROSS_COMPILE}objdump
          export NM=${frostix.gcc-arm-linux-gnueabihf}/bin/''${CROSS_COMPILE}nm
          export RANLIB=${frostix.gcc-arm-linux-gnueabihf}/bin/''${CROSS_COMPILE}ranlib

          export MAKEFLAGS="-e --no-print-directory"
        '';
      };
  };
}
