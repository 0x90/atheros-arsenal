#!/bin/bash

 mips-linux-gnu-objdump -b binary -mmips:isa32 --adjust-vma=0x80010200 -EB -D ar5523.bin > ar5523_dump
