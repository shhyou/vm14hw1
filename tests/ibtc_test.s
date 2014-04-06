global _start
[BITS 32]
_start:
  mov ecx, 20000000
  call _func
_func:
  pop eax
  sub esp, 4
  dec ecx
  jz  .exit
  jmp eax
.exit:
  xor eax,eax
  xor ebx,ebx
  inc eax
  int 0x80
