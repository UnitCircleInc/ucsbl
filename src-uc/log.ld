SECTIONS {
  .logdata (INFO) :
  {
    _slogstr = .;
    KEEP(*(.logstr))
    . = ALIGN(4);
  } > LOGDATA
}
