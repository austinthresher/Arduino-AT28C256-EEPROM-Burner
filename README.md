# Arduino-AT28C256-EEPROM-Burner
This is a simple command line EEPROM burner / reader using an atmega238 for the AT28C256 and similar EEPROMs.

It builds and runs on Debian. I have not tested it on other platforms.

# Usage

Writing:

    ./eeprom_burner -W /dev/tty_your_arduino_here file_to_burn.bin
    
Reading:

    ./eeprom_burner -R /dev/tty_your_arduino_here file_to_dump_to.bin
