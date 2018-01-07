package main
import (
    "C"
    "os"
    "debug/elf"
    "debug/dwarf"
)
//export address_to_function
func address_to_function(filename string, addr uint64) string{
    dwarf_data := get_dwarf_data(string)
    reader := dwarf_data.Reader()
    entry, err := reader.SeekPC(addr)
    if err != nil {
        return ""
    }
    
}
//export variable_to_address
func variable_to_address(filename string, varname string) uint64 {
    dwarf_data := get_dwarf_data(string)
    reader := dwarf_data.Reader()
}
//export line_to_address
func line_to_address(string filename, string sourcefile, line_num int) uint64{
    dwarf_data := get_dwarf_data(string)
    reader := dwarf_data.Reader()
}
//export address_to_line
func line_to_address(string filename, addr uint64) string {
    dwarf_data := get_dwarf_data(string)
    reader := dwarf_data.Reader()
}

func get_dwarf_data(filename string) DWARF.data {
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
    return dwarf_data
}
func main(){}
