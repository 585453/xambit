/*
 * XAmbit - Cross boundary data transfer library
 * Copyright (C) 2016-2017 BAE Systems Electronic Systems, Inc.
 *
 * This file is part of XAmbit.
 *
 * XAmbit is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * XAmbit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with XAmbit.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <errno.h>
#if defined(XTS)
#include <xts/limits.h>
#else
#include <linux/limits.h>
#endif
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <xambit.h>

#include "../include/ex_types.h"

#define DB_DIR	"outgoing"

#define WATCH_FLAGS (IN_CLOSE_WRITE | IN_MOVED_TO)

static int do_close;

static void handle_signal(int signo, siginfo_t *siginfo, void *context)
{
    switch (signo)
    {
    case SIGQUIT:
	printf("Received signal %d - initiating close\n", signo);
	fflush(stdout);
	do_close=1;
	break;
    default:
	do_close=0;
	break;
    }
}

int validate_file(xambit_parcel_hdr_t *hdr, void *data)
{
    printf("Validating a outgoing parcle of type %d\n", hdr->type);
    return 0;
}

int main(int argc, char **argv)
{
    char		path[PATH_MAX];
    char		*fifo_path;
    int			err = 0;
    xambit_channel_t	*ch = NULL;
    struct sigaction	sig_close;
    struct stat		st;
    int			ntfy_fd = 0;
    int			ntfy_wd = 0;

    if (argc < 2)
    {
	fprintf(stderr, "Must provide path to FIFO\n");
	goto out;
    }
    if (stat(argv[1], &st) < 0)
    {
	fprintf(stderr, "Could not find %s\n", argv[1]);
	goto out;
    }
    if (!S_ISFIFO(st.st_mode))
    {
	fprintf(stderr, "%s is not a FIFO\n", argv[1]);
	goto out;
    }
    fifo_path = argv[1];

    printf("The dropbox_send demo application is running.\n");
    printf("FIFO_PATH: %s\n", fifo_path);
    printf("Dropbox directory: %s\n", DB_DIR);
    printf("WARNING: any file placed in %s will be deleted after being sent\n",
	   DB_DIR);
    printf("Press <ctrl+\\> to close\n\n");

    /* Initialize Signal Handler */
    if (sigemptyset(&sig_close.sa_mask) < 0)
    {
	fprintf(stderr, "sigemptyset failed\n");
	goto out;
    }
    if (sigaddset(&sig_close.sa_mask, SIGQUIT) < 0)
    {
	fprintf(stderr, "sigaddset failed\n");
	goto out;
    }
    sig_close.sa_sigaction = handle_signal;
    sig_close.sa_flags = SA_SIGINFO;
    if (sigaction(SIGQUIT, &sig_close, NULL) < 0)
    {
	fprintf(stderr, "sigaction failed\n");
	goto out;
    }

    /* Initialize the dropbox directory */
    if (stat(DB_DIR, &st) < 0)
    {
	fprintf(stderr, "stat of %s failed\n", DB_DIR);
	goto out;
    }
    if (!S_ISDIR(st.st_mode))
    {
	fprintf(stderr, "%s is not a directory\n", DB_DIR);
	goto out;
    }

    /* Initialize inotify */
    if ((ntfy_fd = inotify_init1(IN_CLOEXEC)) < 0)
    {
	fprintf(stderr, "inotify_init1 failed\n");
	goto out;
    }
    if ((ntfy_wd = inotify_add_watch(ntfy_fd, DB_DIR, WATCH_FLAGS)) < 0)
    {
	fprintf(stderr, "inotify_add_watch failed\n");
	goto out;
    }


    /* Initialize the xambit channel */
    printf("Opening channel... ");
    fflush(stdout);
    ch = channel_fifo_open(fifo_path, 0, XAMBIT_CHOUT);
    if (ch == NULL)
    {
	fprintf(stderr, "Failed to open the fifo channel: errno: %d\n", errno);
	goto out;
    }
    printf("done\n");

    err = channel_register_type(ch, XT_FILE, validate_file);
    if (err < 0)
    {
	fprintf(stderr, "Could not register type %d\n", XT_FILE);
	goto out;
    }

    while (1)
    {
	char			    buf[16 * 1024];
	struct	    inotify_event   *event;
	int			    nbytes;

	if(do_close)
	    break;

	nbytes = read(ntfy_fd, buf, sizeof(buf));
	if (nbytes < 0)
	{
	    fprintf(stderr, "read of inotify fd failed\n");
	    goto out;
	}

	event = (struct inotify_event *)buf;
	do
	{
	    if (event->len)
	    {
		if (snprintf(path, sizeof(path), "%s/%s", DB_DIR, event->name) < 0)
		{
		    fprintf(stderr, "failed to construct pathname\n");
		    goto out;
		}
		printf("Sending file: <%s>\n", path);

		if (err = channel_send_file(ch, path, XT_FILE) < 0)
		{
		    fprintf(stderr, "channel_send_file failed - ret: %d\n", err);
		    goto out;
		}

		if (unlink(path) < 0)
		    fprintf(stderr, "Couldn't delete file\n");
	    }

	    nbytes -= sizeof(*event) + event->len;
	    event = (struct inotify_event *)((char *)event +
		    sizeof(*event) +
		    event->len);

	} while (nbytes > 0);
    }

out:
    if (ntfy_wd)
	inotify_rm_watch(ntfy_fd, ntfy_wd);

    if (ntfy_fd)
	close(ntfy_fd);

    if (ch && channel_close(ch) < 0)
	fprintf(stderr, "Failed to close the fifo channel\n");

    return 0;
}

