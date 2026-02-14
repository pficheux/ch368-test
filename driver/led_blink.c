#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>

// SDX, SCS, SCL control
static char data[] = { 0xff, 0x7f, 0xfd, 0xfe, 0xff };
//static char data[] = { 0x7f, 0xff };
static char L1_L4_data[] = { 0x00, 0xff }; // for L1 -> L4
//static char data[] = { 0xfd, 0xff }; 
//static char data[] = { 0xfe, 0xff };
//static char data[] = { 0xfb, 0xff };

int main (int ac, char **av)
{
  int fd, i = 0, j;
  char c;
  
  fd = open ("/dev/ch368_io", O_RDWR);
  if (fd < 0) {
    perror ("open");
    exit (1);
  }


  while (1) {
    /* SDX, SCS, SCL -> offset 0xe8 */
    if (lseek (fd, 0xe8, SEEK_SET) < 0) {
      perror ("lseek");
      exit (1);
    }

    if (write (fd, &data[i], 1) < 0) {
      perror ("write");
      exit (1);
    }
    else
      printf ("w_value[%d] = %02x\n", i, data[i] & 0xff);

    /* SDX, SCS, SCL -> offset 0xe8 */
    if (lseek (fd, 0xe8, SEEK_SET) < 0) {
      perror ("lseek");
      exit (1);
    }

    if (read (fd, &c, 1) < 0) {
      perror ("read");
      exit (1);
    }
    else
      printf ("r_value[%d] = %02x\n", i, c & 0xff);

    sleep (1);

    i++;
    if (i == sizeof(data)) {
      // L1 -> L4 -> send 0x00 or 0xff at offset 0
      if (lseek (fd, 0, SEEK_SET) < 0) {
	perror ("lseek");
	exit (1);
      }

      for (j = 0 ; j < 2 ; j++) {
	if (write (fd, &L1_L4_data[j], 1) < 0) {
	perror ("write");
	exit (1);
	}
	else
	  printf ("w_value[%d] = %02x\n", i, L1_L4_data[j] & 0xff);

	usleep (500000);
	//sleep (1);
      }

      i = 0;
    }
  }
  
  return 0;
}
