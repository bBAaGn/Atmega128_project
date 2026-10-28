MCU     = atmega128
F_CPU   = 16000000UL
TARGET  = main
SRC     = src/main.c
BUILD   = build

CC      = avr-gcc
OBJCOPY = avr-objcopy
CFLAGS  = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -Wall -std=gnu99

all: $(BUILD)/$(TARGET).hex

$(BUILD)/$(TARGET).elf: $(SRC) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD):
	mkdir "$(BUILD)"

$(BUILD)/$(TARGET).hex: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@


clean:
	rm -rf $(BUILD)
