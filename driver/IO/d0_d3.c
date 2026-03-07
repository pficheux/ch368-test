// Send 0xff continuously to D0-D3 with MEM BAR (also works for IO BAR)
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>

int main (int ac, char **av)
{
  int fd, i = 0, j, delay = 0;
  unsigned char mask = 0xff;
  unsigned long cnt = 0;

  if (ac == 1) {
    fprintf (stderr, "Usage: %s <device-node> [delay-in-us]\n", av[0]);
    return 1;
  }
  
  fd = open (av[1], O_RDWR);
  if (fd < 0) {
    perror ("open");
    return 1;
  }

  if (ac > 2) {
    delay = atoi(av[2]);
    printf ("Using a %d us delay\n", delay);
  }
  else
    printf ("Using no delay !\n");

  while (1) {
    if (lseek (fd, 0, SEEK_SET) < 0) {
      perror ("lseek");
      return 1;
    }

    if (write (fd, &mask, 1) < 0) {
      perror ("write");
      return 1;
    }

    mask = ~mask;
    if (delay)
      usleep (delay);

    if (cnt && (cnt % 100000 == 0))
      printf ("cnt= %ld\n", cnt);
    cnt++;
  }

  close (fd);
  
  return 0;
}
