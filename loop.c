#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>

#include <linux/loop.h>

#include "init.h"
#include "loop.h"

int get_free_loop(char *devname)
{
	int lctlfd, dev;

	lctlfd = open("/dev/loop-control", O_RDWR);
	if (lctlfd < 0)
		return -1;

	dev = ioctl(lctlfd, LOOP_CTL_GET_FREE);
	if (dev < 0)
               return -1;

	sprintf(devname, "/dev/loop%d", dev);

	return 0;
}

int bind_loop_dev(char *devname, char *file, int *loop_fd, int *file_fd)
{
	int loopfd = *loop_fd;
	int filefd = *file_fd;

	loopfd = open(devname, O_RDWR);
	if (loopfd < 0)
		return -1;

	filefd = open(file, O_RDWR);
	if (filefd < 0)
		return -1;

	if (ioctl(loopfd, LOOP_SET_FD, filefd) < 0)
		return -1;

	*loop_fd = loopfd;
	*file_fd = filefd;

	return 0;	
}

int mount_loop(char *src, char *dest, char *fstype, int *loop_fd, int *file_fd)
{
	int ret;
	char devname[128];

	if (get_free_loop(devname) < 0)
		return -1;

	if (bind_loop_dev(devname, src, loop_fd, file_fd) < 0)
		return -1;

	// Make dest if it doesn't exist
	if (mkdir_p(dest, 0644) < 0)
		return -1;	


	ret = mount(devname, dest, fstype, 0, NULL);
	if (ret < 0) {
		printf("Could not mount \"%s\" at \"%s\" (type=\"%s\")\n", src, dest, fstype);
		return ret;
	}

	return 0;
}

int unmount_loop(char *dest, int loop_fd, int file_fd)
{
	int ret;

	ret = umount(dest);
	if (ret < 0)
		goto out;

	ret = ioctl(loop_fd, LOOP_CLR_FD, 0);
	if (ret < 0)
		goto out;

	ret = close(loop_fd);
	if (ret < 0)
		goto out;

	ret = close(file_fd);
	if (ret < 0)
		goto out;

	printf("SYSTEMC: Umounted '%s' volume\n", dest);

out:
	return ret;
}
