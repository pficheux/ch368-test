#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>


int main (int ac, char **av)
{
  int fd, i = 0, j;
  char c = 0x55;

  if (ac == 1) {
    fprintf (stderr, "Usage: %s <device-node>\n", av[0]);
    return 1;
  }
  
  fd = open (av[1], O_RDWR);
  if (fd < 0) {
    perror ("open");
    return 1;
  }

  for (i = 0 ; i < 5 ; i++) {
    if (write (fd, &c, 1) < 0) {
      perror ("write");
      return 1;
    }

    c++;
  }

  /*
  if (read (fd, &c, 1) < 0) {
      perror ("read");
      return 1;
  }

  printf ("c= %02x\n", c & 255);
  */

  close (fd);
  
  return 0;
}
