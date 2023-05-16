package main

import (
	"debug/elf"
	"flag"
	"fmt"
	"os"
)

func main() {
	flag.Parse()

	e, err := elf.Open(flag.Arg(0))
	if err != nil {
		fmt.Fprintf(os.Stderr, "Reading %s: %v\n", flag.Arg(0), err)
		os.Exit(1)
	}
	defer e.Close()

	
	for _, prog := range e.Progs {
		if prog.ProgHeader.Type == elf.PT_LOAD && prog.ProgHeader.Vaddr >= 0x400  {
			for i := 0; i < int(prog.ProgHeader.Filesz); i++ {
				var buff [1]byte
				if _, err := prog.ReadAt(buff[:], int64(i)); err != nil {
					panic(err)
				}
				fmt.Printf("0x%02x, ", buff[0])
				if i+1 == int(prog.ProgHeader.Filesz) {
					fmt.Println()
				}
			}
		}
	}
}