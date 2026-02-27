#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "ch36x_lib.h"

static const char *device = "/dev/ch36xpci0";

#define BYTE_TO_BINARY(byte)  \
  ((byte) & 0x80 ? '1' : '0'), \
  ((byte) & 0x40 ? '1' : '0'), \
  ((byte) & 0x20 ? '1' : '0'), \
  ((byte) & 0x10 ? '1' : '0'), \
  ((byte) & 0x08 ? '1' : '0'), \
  ((byte) & 0x04 ? '1' : '0'), \
  ((byte) & 0x02 ? '1' : '0'), \
  ((byte) & 0x01 ? '1' : '0')

int main(int argc, char *argv[])
{
  int fd, i;
  int ret;
  char c;
  enum CHIP_TYPE chiptype;
  unsigned long iobase;
  unsigned long membase;
  int irq;
  uint8_t mask;

  fd = ch36x_open(device);
  if (fd < 0) {
    printf("ch36x_open error.\n");
    goto exit;
  }

  ret = ch36x_get_chiptype(fd, &chiptype);
  if (ret != 0) {
    printf("ch36x_get_chiptype error.\n");
    goto exit;
  }
  switch (chiptype) {
  case CHIP_CH365:
    printf("current chip model: CH365.\n");
    break;
  case CHIP_CH367:
    printf("current chip model: CH367.\n");
    break;
  case CHIP_CH368:
    printf("current chip model: CH368.\n");
    break;
  }

  ret = ch36x_get_irq(fd, &irq);
  if (ret != 0) {
    printf("ch36x_get_irq error.\n");
    goto exit;
  }
  printf("irq number:%d\n", irq);

  ret = ch36x_get_ioaddr(fd, &iobase);
  if (ret != 0) {
    printf("ch36x_get_ioaddr error.\n");
    goto exit;
  }
  printf("iobase:%lx\n", iobase);

  if (chiptype == CHIP_CH368) {
    ret = ch36x_get_memaddr(fd, &membase);
    if (ret != 0) {
      printf("ch36x_get_memaddr error.\n");
      goto exit;
    }
    printf("membase:%lx\n", membase);
  }

  // SDX
  #define V 0xfd
  
  mask = V;
  
  while (1) {
    //    printf ("writing %02x  %c%c%c%c%c%c%c%c\n", mask, BYTE_TO_BINARY(mask));
    ret = ch36x_write_io_byte(fd, (uint8_t)0xe8, mask);
    if (mask == V)
      mask = 0xff;
    else
      mask =V;

    usleep (500000);
  }

  ret = ch36x_close(fd);
  if (ret != 0) 
    printf("ch36x_close error.\n");

exit:
  return ret;
}

