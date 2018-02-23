#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#define CHIP_SIZE 0x8000
#define BUFFER_SIZE 64
#define FILENAME "file.bin"

enum Command {
   READ,
   WRITE,
   ERASE,
   UNLOCK
};

enum Command cmd;

// TODO: Write functions for sending and receiving ascii buffers as binary

void exit_usage(char* arg0) {
   printf("USAGE: %s [-W | -R | -E | -U] [serial port] [filename]\n", arg0);
   exit(1);
}

uint8_t nibble_to_ascii(uint8_t val) {
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

uint8_t ascii_to_nibble(char digit) {
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
void fill_ascii_buffer(uint8_t *source, char *dest, int bin_count) {
   for (int i = 0; i < bin_count; ++i) {
      dest[i * 2]     = nibble_to_ascii(source[i] >> 4); // hi
      dest[i * 2 + 1] = nibble_to_ascii(source[i] & 0xF); // lo
   }
}

// Convert an ASCII buffer to binary
void fill_bin_buffer(char *source, uint8_t *dest, int ascii_count) {
   for (int i = 0; i < ascii_count; ++i) {
      if (i % 2 == 0) { // hi
         dest[i / 2] = ascii_to_nibble(source[i]) << 4;
      } else { // lo
         dest[i / 2] += ascii_to_nibble(source[i]);
      }
   }
}

int send(int file, void *data, int count) {
   int sent = write(file, data, count);
   if (sent != count) {
      perror("send");
      fprintf(stderr, "Error: Tried to send %d, but only sent %d.\n",
            count, sent);
      close(file);
      exit(1);
   }
   return sent;
}

int main(int argc, char** argv) {

   // Parse and verify arguments
   if (argc < 3) {
      exit_usage(argv[0]);
   }

   if (argv[1][0] != '-') {
      exit_usage(argv[0]);
   }

   switch (argv[1][1]) {
      case 'R':
         cmd = READ;
         break;
      case 'W':
         cmd = WRITE;
         break;
      case 'E':
         cmd = ERASE;
         break;
      case 'U':
         cmd = UNLOCK;
         break;
      default:
         exit_usage(argv[0]);
   }

   char *filename = NULL;
   if (argc == 4) {
      filename = argv[3];
   }
   else {
      printf("Using default filename %s\n", FILENAME);
   }

   // Open serial communication
   int com = open(argv[2], O_RDWR | O_NOCTTY /*| O_NDELAY*/);
   if (com < 0) {
      perror(argv[2]);
      exit(1);
   }
   fcntl(com, F_SETFL, 0); // We want reads to be blocking 

   struct termios options;
   tcgetattr(com, &options);
   cfsetispeed(&options, B9600);
   cfsetospeed(&options, B9600);
   options.c_cflag |= CLOCAL | CREAD;
   options.c_cflag &= ~PARENB; // No parity
   options.c_cflag &= ~CSTOPB; // One stop bit
   options.c_cflag &= ~CSIZE; // 8 bit characters
   options.c_cflag |= CS8;
   options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // Raw input
   options.c_iflag &= ~(IXON | IXOFF | IXANY); // No flow control
   options.c_oflag &= ~OPOST; // Raw output
   options.c_cc[VMIN] = BUFFER_SIZE * 2;
   options.c_cc[VTIME] = 50;

   tcsetattr(com, TCSANOW, &options);
   printf("Opened %s\n", argv[2]);
   sleep(2);

   uint8_t buf[BUFFER_SIZE];
   char ascii_buf[BUFFER_SIZE * 2];
   int addr  = 0;
   int recvd = 0;
   int file;
   switch (cmd) {
      case ERASE:
         printf("Not implemented yet.\n");
         break;
      case UNLOCK:
         printf("Sending UNLOCK command\n");
         send(com, "U", 1);
         break;
      case READ:
         file = open(filename == NULL ? FILENAME : filename,
               O_RDWR | O_CREAT);
         if (file < 0) {
            perror("read");
            fprintf(stderr, "Error: couldn't create output file.\n");
            close(com);
            exit(1);
         }
         printf("Sending READ command\n");
         send(com, "R", 1);
         sleep(1);
         printf("Receiving data\n");
         while (addr < CHIP_SIZE) {
            printf("%04X ", addr);
            recvd = read(com, ascii_buf, BUFFER_SIZE*2);
            if (recvd % 2 == 1) {
               fprintf(stderr, "\nError: Odd number of bytes\n");
               close(file);
               close(com);
               exit(1);
            }
            fill_bin_buffer(ascii_buf, buf, recvd);
            write(file, buf, recvd / 2);
            addr += recvd / 2;
            if (addr % 0x200 == 0) {
               printf("\n");
            }
            fflush(stdout);
         }
         close(file);
         printf("\nWrote %d bytes to %s\n", addr,
               filename == NULL ? FILENAME : filename);
         break;

      case WRITE:
         if (filename == NULL) {
            fprintf(stderr, "Error: No input file.\n");
            close(com);
            exit_usage(argv[0]);
         }
         file = open(filename, O_RDONLY);
         if (file < 0) {
            perror("write");
            fprintf(stderr, "Error: couldn't open input file.\n");
            close(com);
            exit(1);
         }
         printf("Sending WRITE command\n");
         send(com, "W", 1);
         sleep(1);
         printf("Sending data\n");
         while (addr < CHIP_SIZE) {
            uint16_t recvd_addr;
            read(com, &recvd_addr, 2);
//            if (addr != recvd_addr) {
//               fprintf(stderr, "Error: Address mismatch (%04X != %04X)\n",
//                     recvd_addr, addr);
//               close(file);
//               close(com);
//               exit(1);
//            }
            addr = recvd_addr;
            printf("%04X ", addr);
            lseek(file, addr, SEEK_SET);
            read(file, buf, BUFFER_SIZE); // TODO: Check for EOF
            fill_ascii_buffer(buf, ascii_buf, BUFFER_SIZE);
            send(com, ascii_buf, BUFFER_SIZE * 2);
            //addr += BUFFER_SIZE;
            if (addr % 0x200 == 0) {
               printf("\n");
            }
            fflush(stdout);
         }
         close(file);
         printf("\nSent %d bytes to %s\n", addr, argv[2]);
         break;
   }
   close(com);
   return 0;
}
