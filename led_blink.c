#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>

// SCS, SCL control
static char data[] = { 0xfd, 0xfe, 0xff };

int main (int ac, char **av)
{
  int fd, i = 0;
  char c;
  
  fd = open ("/dev/pcidemo", O_RDWR);
  if (fd < 0) {
    perror ("open");
    exit (1);
  }

  while (1) {
    if (write (fd, &data[i], 1) < 0) {
      perror ("write");
      exit (1);
    }

    if (read (fd, &c, 1) < 0) {
      perror ("read");
      exit (1);
    }
    else
      printf ("value= %02x\n", c & 0xff);


    sleep (1);

    i++;
    if (i == 3)
      i = 0;
  }
  
  return 0;
}
