 
PRG            = strommessung
OBJ            = main.o uart/uart.o
MCU_TARGET     = atmega48
OPTIMIZE       = -Os

DEFS           = -Iuart -std=c99
LIBS           = -lm

CC             = avr-gcc

# Override is only needed by avr-lib build system.

override CFLAGS        = -g -Wall $(OPTIMIZE) -mmcu=$(MCU_TARGET) $(DEFS)
override LDFLAGS       = -Wl,-Map,$(PRG).map

OBJCOPY        = avr-objcopy
OBJDUMP        = avr-objdump

all: $(PRG).elf lst hex

$(PRG).elf: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

# dependency:
main.o: main.c

clean:
	rm -rf $(OBJ) $(PRG).elf *.eps *.png *.pdf *.bak $(PRG).lst $(PRG).bin $(PRG).hex $(PRG).map $(PRG).srec

lst:  $(PRG).lst

%.lst: %.elf
	$(OBJDUMP) -h -S $< > $@

hex:  $(PRG).hex

%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -O ihex $< $@

flash: hex
	avrdude -P usb -c avrispmkII -p m48 -U flash:w:$(PRG).hex

fuse:
	avrdude -P usb -c avrispmkII -p m48 -U hfuse:w:0xdc:m -U lfuse:w:0xd7:m
