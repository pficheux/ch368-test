#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>

// SDX, SCS, SCL control
static char data[] = { 0xff, 0x7f, 0xfd, 0xfe, 0xff };
//static char data[] = { 0x7f, 0xff };
// L1 -> L4 on/off
static char L1_L4_data[] = { 0x0f, 0x0e, 0x0d, 0x0b, 0x07, 0x0f }; // for L1 -> L4
// L2/L4 on, then L1/L3
//static char L1_L4_data[] = { 0x0f, 0x05, 0x0a, 0x0f }; 
//static char data[] = { 0xfd, 0xff }; 
//static char data[] = { 0xfe, 0xff };
//static char data[] = { 0xfb, 0xff };

int main (int ac, char **av)
{
  int fd, i = 0, j;
  char c;

  if (ac == 1) {
    fprintf (stderr, "Usage: %s <device-node>\n", av[0]);
    return 1;
  }
  
  fd = open (av[1], O_RDWR);
  if (fd < 0) {
    perror ("open");
    return 1;
  }


  while (1) {
    /* SDX, SCS, SCL -> offset 0xe8 */
    if (lseek (fd, 0xe8, SEEK_SET) < 0) {
      perror ("lseek");
      return 1;
    }

    if (write (fd, &data[i], 1) < 0) {
      perror ("write");
      return 1;
    }
    else
      printf ("w_value[%d] = %02x\n", i, data[i] & 0xff);

    /* SDX, SCS, SCL -> offset 0xe8 */
    if (lseek (fd, 0xe8, SEEK_SET) < 0) {
      perror ("lseek");
      return 1;
    }

    if (read (fd, &c, 1) < 0) {
      perror ("read");
      return 1;
    }
    else
      printf ("r_value[%d] = %02x\n", i, c & 0xff);

    usleep (500000);

    i++;
    if (i == sizeof(data)) {
      // L1 -> L4 -> send 0x00 or 0x0f at offset 0
      if (lseek (fd, 0, SEEK_SET) < 0) {
	perror ("lseek");
	return 1;
      }

      for (j = 0 ; j < sizeof(L1_L4_data) ; j++) {
	if (write (fd, &L1_L4_data[j], 1) < 0) {
	  perror ("write");
	  return 1;
	}
	else
	  printf ("w_value[%d] = %02x\n", i, L1_L4_data[j] & 0xff);

	usleep (500000);
      }

      i = 0;
    }
  }
  
  return 0;
}
