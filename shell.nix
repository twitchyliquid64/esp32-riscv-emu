# arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc,DFUOnBoot=default,PSRAM=opi,PartitionScheme=huge_app,CPUFreq=240,FlashMode=qio,FlashSize=16M,EraseFlash=none,UploadSpeed=921600,DebugLevel=none,EventsCore=1,JTAGAdapter=builtin,LoopCore=1,MSCOnBoot=default,USBMode=hwcdc ./ --warnings more

# arduino-cli upload --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc,DFUOnBoot=default,PSRAM=opi,PartitionScheme=huge_app,CPUFreq=240,FlashMode=qio,FlashSize=16M,EraseFlash=none,UploadSpeed=921600,DebugLevel=none,EventsCore=1,JTAGAdapter=builtin,LoopCore=1,MSCOnBoot=default,USBMode=hwcdc ./ --port /dev/ttyACM0

let
  here = toString ./.;
  moz_overlay = import (builtins.fetchTarball
    "https://github.com/mozilla/nixpkgs-mozilla/archive/master.tar.gz");
  pkgs = import <nixpkgs> { overlays = [ moz_overlay ]; };
  rust = (pkgs.rustChannelOf {
    channel = "stable";
  }).rust.override {
    extensions = [ "rust-src" "rust-analysis" ];
    targets = [ "riscv32i-unknown-none-elf" ]; # maybe riscv32imc-unknown-none-elf in future?
  };
  rustPlatform = pkgs.makeRustPlatform {
    rustc = rust;
    cargo = rust;
  };

  my-python-packages = ps: with ps; [
    pyserial
  ];
  my-python = pkgs.python3.withPackages my-python-packages;  

in pkgs.mkShell {
  buildInputs = [
    rust
    pkgs.gcc-arm-embedded
    pkgs.arduino-cli
    pkgs.go_1_19

    my-python
  ];
  LIBCLANG_PATH = "${pkgs.llvmPackages.libclang}/lib";
  LD_LIBRARY_PATH = "${pkgs.stdenv.cc.cc.lib}/lib64:$LD_LIBRARY_PATH";
  CARGO_INCREMENTAL = 1;
}
