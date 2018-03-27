// Firmware for an atmega328 based EEPROM programmer.
// Written and tested with an Atmel 28c256, but should
// work with other pin-compatible chips.


// ----------------
// Macros and types
// ----------------

#define FORCE_INLINE __attribute__((always_inline))

#define PIN_A0 14
#define PIN_A1 15
#define PIN_A2 16
#define PIN_A3 17
#define PIN_A4 18
#define PIN_A5 19

// Shift registers
#define SERIAL_PIN PIN_A0
#define LATCH_PIN  PIN_A2
#define SHIFT_PIN  PIN_A3

// EEPROM
#define WRITE_PIN  PIN_A4
#define READ_PIN   PIN_A5
#define CHIP_PIN   PIN_A1

// Status LEDs
#define WRITE_LED 10
#define READ_LED  11

#define ushort uint16_t

#define BUFFER_SIZE 64

enum State {
   REST,
   READ,
   WRITE,
   ERR
};


// --------------------
// Variable definitions
// --------------------

enum State state = REST;
int blink_timer  = 0;
ushort addr      = 0x0000;
bool wait        = true;
byte page_buffer[64];
byte ascii_buffer[128];

// -------------------
// Function prototypes
// -------------------

inline void shiftByte(byte data) FORCE_INLINE;
inline void setAddrBus(ushort addr) FORCE_INLINE;
inline void setDataBus(byte val) FORCE_INLINE;
inline void setWriteMode() FORCE_INLINE;
inline void setReadMode() FORCE_INLINE;
inline void writePage(ushort address, byte *buffer, int num) FORCE_INLINE;
inline void writeToAddr(ushort address, byte value) FORCE_INLINE;
inline byte readFromAddr(ushort address) FORCE_INLINE;
inline byte readDataBus() FORCE_INLINE;
inline char nibbleToASCII(byte val) FORCE_INLINE;
inline byte ASCIIToNibble(char digit) FORCE_INLINE;
void fillASCIIBuffer(byte *source, byte *dest, int bin_count);
void fillBinBuffer(byte *source, byte *dest, int ascii_count);


// ----------------
// ASCII conversion
// ----------------

char nibbleToASCII(byte val) {
   switch(val) {
      case 0x0: return '0';
      case 0x1: return '1';
      case 0x2: return '2';
      case 0x3: return '3';
      case 0x4: return '4';
      case 0x5: return '5';
      case 0x6: return '6';
      case 0x7: return '7';
      case 0x8: return '8';
      case 0x9: return '9';
      case 0xA: return 'A';
      case 0xB: return 'B';
      case 0xC: return 'C';
      case 0xD: return 'D';
      case 0xE: return 'E';
      case 0xF: return 'F';
   }
   return '0';
}

byte ASCIIToNibble(char digit) {
   switch(digit) {
      case '0': return 0;
      case '1': return 1;
      case '2': return 2;
      case '3': return 3;
      case '4': return 4;
      case '5': return 5;
      case '6': return 6;
      case '7': return 7;
      case '8': return 8;
      case '9': return 9;
      case 'A': return 10;
      case 'B': return 11;
      case 'C': return 12;
      case 'D': return 13;
      case 'E': return 14;
      case 'F': return 15;
   }
   return 0;
}

// Convert a binary buffer to ASCII
void fillASCIIBuffer(byte *source, byte *dest, int bin_count) {
   for (int i = 0; i < bin_count; ++i) {
      dest[i * 2]     = nibbleToASCII(source[i] >> 4); // hi
      dest[i * 2 + 1] = nibbleToASCII(source[i] & 0xF); // lo
   }
}

// Convert an ASCII buffer to binary
void fillBinBuffer(byte *source, byte *dest, int ascii_count) {
   for (int i = 0; i < ascii_count; ++i) {
      if (i % 2 == 0) { // hi
         dest[i / 2] = ASCIIToNibble(source[i]) << 4;
      } else { // lo
         dest[i / 2] += ASCIIToNibble(source[i]);
      }
   }
}


// --------------------
// Bus I/O 
// --------------------


// These Bus I/O methods are modified versions of the ones
// found in MEEPROMMERfirmware.ino from the MEEPROMMER source:
//    https://github.com/mkeller0815/MEEPROMMER


// Writes a single byte to the shift registers
void shiftByte(byte val) {
   for (int i = 0; i < 8; ++i) {
      digitalWrite(SHIFT_PIN, LOW);
      digitalWrite(SERIAL_PIN, val & (0x80 >> i));
      digitalWrite(SHIFT_PIN, HIGH);
   }
}


// Places a 16 bit address on the shift registers.
// Writing hi or lo first may need to be swapped
// depending on wiring. The 28c256 only uses the
// lower 15 bits.
void setAddrBus(ushort addr) {
   byte hi = addr >> 8;
   byte lo = addr & 0xFF;
   digitalWrite(LATCH_PIN, LOW);
   shiftByte(lo);
   shiftByte(hi);
   digitalWrite(LATCH_PIN, HIGH);
}


// Places the byte on the data bus.
void setDataBus(byte val) {
   digitalWrite(2, val & 1);
   digitalWrite(3, (val >> 1) & 1);
   digitalWrite(4, (val >> 2) & 1);
   digitalWrite(5, (val >> 3) & 1);
   digitalWrite(6, (val >> 4) & 1);
   digitalWrite(7, (val >> 5) & 1);
   digitalWrite(8, (val >> 6) & 1);
   digitalWrite(9, (val >> 7) & 1);

   // MEEPROMMER doesn't mask PIND, this fixes some bad writes
//   PORTD = (PIND & 3) | (val << 2); 
}

byte readDataBus() {
   return digitalRead(2)
        |(digitalRead(3) << 1)
        |(digitalRead(4) << 2)
        |(digitalRead(5) << 3)
        |(digitalRead(6) << 4)
        |(digitalRead(7) << 5)
        |(digitalRead(8) << 6)
        |(digitalRead(9) << 7);
}


// Set the data bus pins to output data
void setWriteMode() {
   for (int i = 0; i < 8; ++i) {
      pinMode(2 + i, OUTPUT);
   }
}


// Set the data bus pins to read data
void setReadMode() {
   for (int i = 0; i < 8; ++i) {
      pinMode(2 + i, INPUT);
   }
}


// -------------
// EEPROM Access
// -------------

void writePage(ushort address, byte *buffer, int num) {
   digitalWrite(CHIP_PIN, HIGH);
   digitalWrite(WRITE_PIN, HIGH);
   digitalWrite(READ_PIN, HIGH);

   for (int i = 0; i < num; ++i) {
      digitalWrite(CHIP_PIN, LOW);
      setAddrBus(address + i);
      digitalWrite(WRITE_PIN, LOW);  
      setDataBus(buffer[i]);
//      delayMicroseconds(1);
      digitalWrite(WRITE_PIN, HIGH);
      digitalWrite(CHIP_PIN, HIGH);
   }
}

// Writes a byte using BUFFER_SIZE write timing.
// Data pins must be in write mode before calling this.
void writeToAddr(ushort address, byte value) {
   digitalWrite(CHIP_PIN, HIGH);
   digitalWrite(WRITE_PIN, HIGH);
   digitalWrite(READ_PIN, HIGH);
   digitalWrite(CHIP_PIN, LOW);
   setAddrBus(address);
   digitalWrite(WRITE_PIN, LOW);  
   setDataBus(value);
//   delayMicroseconds(1);
   digitalWrite(WRITE_PIN, HIGH);
   digitalWrite(CHIP_PIN, HIGH);
}


// Reads a byte from the given address.
// Data pins must be in read mode before calling this.
byte readFromAddr(ushort address) {
   setAddrBus(address);
   digitalWrite(CHIP_PIN, HIGH);
   digitalWrite(WRITE_PIN, HIGH);
   digitalWrite(READ_PIN, HIGH);
   digitalWrite(CHIP_PIN, LOW);
   digitalWrite(READ_PIN, LOW);
   delayMicroseconds(1);
   byte value = readDataBus();
   digitalWrite(READ_PIN, HIGH);
   digitalWrite(CHIP_PIN, HIGH);
   return value;
}


// This disables the write protect mode on the Atmel 28c256.
// Write mode must be enabled before calling this.
void disableProtectMode() {
   writeToAddr(0x5555, 0xAA);
   writeToAddr(0x2AAA, 0x55);
   writeToAddr(0x5555, 0x80);
   writeToAddr(0x5555, 0xAA);
   writeToAddr(0x2AAA, 0x55);
   writeToAddr(0x5555, 0x20);
}


// --------------------------
// Arduino built-in functions
// --------------------------


void setup() {
   pinMode(SERIAL_PIN, OUTPUT);
   pinMode(LATCH_PIN,  OUTPUT);
   pinMode(SHIFT_PIN,  OUTPUT);
   pinMode(WRITE_PIN,  OUTPUT);
   pinMode(READ_PIN,   OUTPUT);
   pinMode(CHIP_PIN,   OUTPUT);
   pinMode(WRITE_LED,  OUTPUT);
   pinMode(READ_LED,   OUTPUT);
   digitalWrite(READ_PIN,  HIGH);
   digitalWrite(CHIP_PIN,  HIGH);
   digitalWrite(WRITE_PIN, HIGH);
   digitalWrite(SHIFT_PIN, HIGH);

   Serial.begin(9600);
}


void loop() {

   // Wait for a command over serial
   if (wait) {

      // These pins are all active when low
      digitalWrite(CHIP_PIN,   HIGH);
      digitalWrite(WRITE_PIN,  HIGH);
      digitalWrite(READ_PIN,   HIGH);
      setAddrBus(0);
      setDataBus(0);

      switch (state) {
         case ERR:
            for (int i = 0; i < 3; ++i) {
               digitalWrite(READ_LED, HIGH);
               digitalWrite(WRITE_LED, HIGH);
               delay(250);
               digitalWrite(READ_LED, LOW);
               digitalWrite(WRITE_LED, LOW);
               delay(250);
            }
            state = REST;
            break;
            
         default:
            digitalWrite(READ_LED, HIGH);
            digitalWrite(WRITE_LED, HIGH);
            delay(10);
            break;
      }
   }
   else {
      switch(state) {
         case WRITE:
            while (state == WRITE) {
               digitalWrite(CHIP_PIN,   HIGH);
               digitalWrite(WRITE_PIN,  HIGH);
               digitalWrite(READ_PIN,   HIGH);
               int timeout = 0;
               int blinker = 0;
               // Send the current address to ensure we're synced
               Serial.write((byte*)&addr, 2);
//               if (addr > 0x8000) {
//                 state = REST;
//                 wait = true;
//                 break;
//               }
               while (!Serial.available()) {
                  delay(1);
                  blinker++;
                  if (blinker > 200) {
                     blinker = 0;
                     if (addr != 0) { // Only timeout after receiving data
                        timeout++;
                        if (timeout > 15) { // 3 second timeout
                           state = ERR;
                           wait = true;
                           return;
                        }
                     }
                  }
                  digitalWrite(WRITE_LED, blinker < 100 ? HIGH : LOW);
               }
               
               digitalWrite(WRITE_LED, LOW);
               int ascii_bytes =
                  Serial.readBytes((char*)ascii_buffer, BUFFER_SIZE * 2); 
               fillBinBuffer(ascii_buffer, page_buffer, ascii_bytes);
               int bytes_to_write = ascii_bytes / 2;
               digitalWrite(WRITE_LED, HIGH);
               setWriteMode();
               for (int i = 0; i < bytes_to_write; ++i) {
                  writeToAddr(addr, page_buffer[i]);
                  addr++;
               }
               setReadMode();
               bool done = false;
               while (!done) {
                  int check = readFromAddr(addr);
                  // When bit 6 stops flipping, the write is done
                  done = true;
                  if ((check & 0x40) != (readFromAddr(addr) & 0x40)) {
                     digitalWrite(READ_LED, HIGH);
                     done = false;
                  }
               }  
               digitalWrite(READ_LED, LOW);
            }
           break;
        case READ:
           while (state == READ) {
             for (int i = 0; i < BUFFER_SIZE; i++) {
                page_buffer[i] = readFromAddr(addr++);
             }
             fillASCIIBuffer(page_buffer, ascii_buffer, BUFFER_SIZE);
             Serial.write(ascii_buffer, BUFFER_SIZE * 2);
             if (addr >= 0x8000) {
                state = REST;
                wait = true;
             }
           }
           break;
        default:
           wait = true;
      }
   }
}


void serialEvent() {
  while (Serial.available() && state == REST) {
      char command = Serial.read();
      switch (command) {
      case 'R': // Read EEPROM contents
         state = READ;
         addr  = 0x0000;
         wait  = false;
         setReadMode();
         digitalWrite(WRITE_LED, LOW);
         digitalWrite(READ_LED, HIGH);
         delay(1500);
         return;
      case 'E': // Erase EEPROM
         // TODO
         break;
      case 'U': // Unlock write protected EEPROM 
         setWriteMode();
         digitalWrite(WRITE_LED, HIGH);
         digitalWrite(READ_LED, LOW);
         disableProtectMode();
         state = REST;
         wait = true;
         return;
      case 'W': // Write data to EEPROM 
         // Don't accidentally write anything sent so far to the chip
         while (Serial.available() > 0) {
            Serial.read();
         }
         state = WRITE;
         addr  = 0x0000;
         wait  = false;
         digitalWrite(WRITE_LED, HIGH);
         digitalWrite(READ_LED, LOW);
         setWriteMode();
         return;
      }
      state = ERR; 
   }
}
