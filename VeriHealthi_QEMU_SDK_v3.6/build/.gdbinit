set mem inaccessible-by-default off
set remotetimeout 250
set arch riscv:rv32
target remote localhost:1234
load
break main
continue
