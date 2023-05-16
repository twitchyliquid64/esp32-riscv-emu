# esp32-riscv-emu

1. Enter dev shell: `nix-shell`
2. Compile example rust program: `cd test_prog && cargo build`
3. Dump raw program bytes: `cd test_prog && go run ../readelf.go target/riscv32i-unknown-none-elf/debug/test_prog`
4. Put the raw bytes at the bottom of the `static_ram` array in `riscv_test.ino`
5. Compile & flash with:

```shell
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc,DFUOnBoot=default,PSRAM=opi,PartitionScheme=huge_app,CPUFreq=240,FlashMode=qio,FlashSize=16M,EraseFlash=none,UploadSpeed=921600,DebugLevel=none,EventsCore=1,JTAGAdapter=builtin,LoopCore=1,MSCOnBoot=default,USBMode=hwcdc ./ --warnings more

arduino-cli upload --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc,DFUOnBoot=default,PSRAM=opi,PartitionScheme=huge_app,CPUFreq=240,FlashMode=qio,FlashSize=16M,EraseFlash=none,UploadSpeed=921600,DebugLevel=none,EventsCore=1,JTAGAdapter=builtin,LoopCore=1,MSCOnBoot=default,USBMode=hwcdc ./ --port /dev/ttyACM0 && tail -f /dev/ttyACM0
```

(Assumes lilygo T-display S3 as test device)