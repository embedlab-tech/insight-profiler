{
  description = "Insight Profiler — firmware & SDK development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    # Do NOT follow our nixpkgs — nixpkgs-esp-dev needs its own pinned nixpkgs
    # that still has python310 (removed from nixpkgs unstable in early 2026).
    nixpkgs-esp-dev.url = "github:mirrexagon/nixpkgs-esp-dev";
  };

  outputs = { self, nixpkgs, flake-utils, nixpkgs-esp-dev }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        # General nixpkgs for host tools
        pkgs = import nixpkgs { inherit system; };

        # Separate nixpkgs instance for ESP-IDF — uses nixpkgs-esp-dev's own
        # pinned nixpkgs which has python310 required by the ESP-IDF derivation.
        esp-pkgs = import nixpkgs-esp-dev.inputs.nixpkgs {
          inherit system;
          overlays = [ nixpkgs-esp-dev.overlays.default ];
          config = {
            # ecdsa is a dependency of esptool and is flagged for CVE-2024-23342
            # (timing side-channel in ECDSA). Acceptable for local dev tooling.
            permittedInsecurePackages = [ "python3.13-ecdsa-0.19.1" ];
          };
        };
      in
      {
        devShells = {
          firmware = pkgs.mkShell {
            name = "insight-profiler-firmware";

            buildInputs = [
              esp-pkgs.esp-idf-full   # ESP-IDF v5.x, all targets incl. ESP32-S3
              pkgs.git
              pkgs.cmake
              pkgs.ninja
            ];

            shellHook = ''
              echo "Insight Profiler — firmware dev environment"
              echo "Target : ESP32-S3"
              echo "IDF    : $IDF_PATH"
              echo ""
              echo "Build  : cd firmware && idf.py build"
              echo "Flash  : cd firmware && idf.py -p /dev/cu.usbmodem* flash"
              echo "Monitor: cd firmware && idf.py -p /dev/cu.usbmodem* monitor"
            '';
          };

          default = self.devShells.${system}.firmware;
        };
      }
    );
}
