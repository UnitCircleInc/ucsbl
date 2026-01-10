ASMFLAGS += \
  -mthumb -mcpu=cortex-m4 -mabi=aapcs \
  -mfloat-abi=hard -mfpu=fpv4-sp-d16 -mfp16-format=ieee \

CFLAGS  += \
  -mthumb -mcpu=cortex-m4 -mabi=aapcs -fshort-enums \
  -mfloat-abi=hard -mfpu=fpv4-sp-d16 -mfp16-format=ieee \

LDFLAGS += \
  -mfloat-abi=hard -mfpu=fpv4-sp-d16 -mfp16-format=ieee \

