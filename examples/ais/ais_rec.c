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
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <xambit.h>

#include "../include/ex_types.h"

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

int validate_aivdm(xambit_parcel_hdr_t *hdr, void *data)
{
    return 0;
}

int validate_aivdo(xambit_parcel_hdr_t *hdr, void *data)
{
    return 0;
}

int main(int argc, char **argv)
{
    char		*fifo_path;
    void		*outbuf;
    int			err = 0;
    xambit_channel_t	*ch = NULL;
    struct sigaction	sig_close;
    struct stat		st;

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

    printf("The AIS receiver demo application is running.\n");
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

    /* Initialize the xambit channel */
    printf("Opening channel... ");
    fflush(stdout);
    ch = channel_fifo_open(fifo_path, 0, XAMBIT_CHIN);
    if (ch == NULL)
    {
	fprintf(stderr, "Failed to open the fifo channel: errno: %d\n", errno);
	goto out;
    }
    printf("done\n");

    err = channel_register_type(ch, XT_AIVDM, validate_aivdm);
    if (err < 0)
    {
	fprintf(stderr, "Could not register type %d\n", XT_FILE);
	goto out;
    }

    err = channel_register_type(ch, XT_AIVDO, validate_aivdo);
    if (err < 0)
    {
	fprintf(stderr, "Could not register type %d\n", XT_FILE);
	goto out;
    }

    while (1)
    {
	int		    err;
	xambit_parcel_hdr_t *phdr = NULL;

	if (do_close)
	    goto out;

	outbuf = NULL;
	phdr = NULL;
	err = channel_receive(ch, &outbuf, &phdr);
	if (err >= 0)
	{
	    fprintf(stdout, "%*.*s\n", (int)phdr->length, (int)phdr->length, (char *)outbuf);
	    free(outbuf);
	    free(phdr);
	}
    }

out:
    if (ch && channel_close(ch) < 0)
	fprintf(stderr, "Failed to close the fifo channel\n");

    return 0;
}



