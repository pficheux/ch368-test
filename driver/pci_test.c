#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

char buf[256];

int main (int ac, char **av)
{
	int fd;

	fd = open ("/dev/mydriver_pcidemo0", O_RDONLY);
	if (fd < 0) {
		perror ("open");
		exit (1);
	}


	if (lseek (fd, 8, SEEK_SET) < 0)
		perror ("lseek");


	if (read (fd, buf, 8) < 0) 
		perror ("read");
	else
		printf ("0x%02x 0x%02x\n", buf[0] & 0xff, buf[1] & 0xff);

	close (fd);
}
