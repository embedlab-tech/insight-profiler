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
        # General nixpkgs — Python, Node, host tools
        pkgs = import nixpkgs { inherit system; };

        # Separate nixpkgs instance for ESP-IDF — uses nixpkgs-esp-dev's own
        # pinned nixpkgs which has python310 required by the ESP-IDF derivation.
        esp-pkgs = import nixpkgs-esp-dev.inputs.nixpkgs {
          inherit system;
          overlays = [ nixpkgs-esp-dev.overlays.default ];
          config = {
            # ecdsa is a dependency of esptool, flagged for CVE-2024-23342
            # (timing side-channel). Acceptable for local dev tooling.
            permittedInsecurePackages = [ "python3.13-ecdsa-0.19.1" ];
          };
        };

        # Shared host tools used across multiple shells
        commonTools = with pkgs; [ git ];

        # ESP-IDF shell inputs
        firmwareInputs = [
          esp-pkgs.esp-idf-full  # ESP-IDF v5.x, xtensa-esp32s3-elf toolchain
          pkgs.cmake
          pkgs.ninja
        ] ++ commonTools;

        # Python SDK shell inputs
        pythonInputs = with pkgs; [
          python312   # matches requires-python = ">=3.12" in pyproject.toml
          uv          # fast package manager — replaces pip + venv
          ruff        # linter + formatter (used in pyproject.toml)
        ] ++ commonTools;

        # JS SDK shell inputs
        jsInputs = with pkgs; [
          nodejs_22   # LTS, satisfies engines.node >= 20
        ] ++ commonTools;

      in
      {
        devShells = {
          # ESP32-S3 firmware — nix develop .#firmware
          firmware = pkgs.mkShell {
            name = "insight-profiler-firmware";
            buildInputs = firmwareInputs;
            shellHook = ''
              echo "Insight Profiler — firmware (ESP32-S3)"
              echo "IDF: $IDF_PATH"
              echo "Build:   cd firmware && idf.py build"
              echo "Flash:   cd firmware && idf.py -p /dev/cu.usbmodem* flash monitor"
            '';
          };

          # Python SDK — nix develop .#python-sdk
          python-sdk = pkgs.mkShell {
            name = "insight-profiler-python-sdk";
            buildInputs = pythonInputs;
            shellHook = ''
              echo "Insight Profiler — Python SDK"
              echo "Python: $(python --version)"
              echo "Setup:  cd sdk-python && uv sync"
              echo "Test:   cd sdk-python && uv run pytest"
              echo "Lint:   cd sdk-python && uv run ruff check ."
            '';
          };

          # JS / TypeScript SDK — nix develop .#js-sdk
          js-sdk = pkgs.mkShell {
            name = "insight-profiler-js-sdk";
            buildInputs = jsInputs;
            shellHook = ''
              echo "Insight Profiler — JS/TypeScript SDK"
              echo "Node: $(node --version)"
              echo "Setup: cd sdk-js && npm install"
              echo "Build: cd sdk-js && npm run build"
              echo "Test:  cd sdk-js && npm test"
            '';
          };

          # All-in-one — nix develop (default)
          default = pkgs.mkShell {
            name = "insight-profiler";
            buildInputs = firmwareInputs ++ pythonInputs ++ jsInputs;
            shellHook = ''
              echo "Insight Profiler — full dev environment"
              echo ""
              echo "  firmware   → cd firmware   && idf.py build"
              echo "  python-sdk → cd sdk-python && uv sync && uv run pytest"
              echo "  js-sdk     → cd sdk-js     && npm install && npm run build"
              echo ""
              echo "Or use focused shells: nix develop .#firmware / .#python-sdk / .#js-sdk"
            '';
          };
        };
      }
    );
}
