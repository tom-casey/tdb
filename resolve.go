package main
import (
    "os"
    "debug/elf"
)

func variable_to_address(filename string, variable_name string, scope uintptr){
    r,err := os.Open(filename)
    if err !=nil {
        panic(err)
    }
    elf_file,err := elf.NewFile(r)
    if err!=nil{
        panic(err)
    }
    dwarf_data, err = elf_file.DWARF()
    if err!=nil{
        panic(err)
    }
    
}
