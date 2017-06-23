/*
 * XAmbit - Cross boundary data transfer library
 * Copyright (C) 2016-2017 BAE Systems.
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
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <xambit.h>
#include <zlib.h>

#define CSUM_8_ADD(x, total)						    \
    do {								    \
	uint8_t _s;							    \
	for (_s=0; _s<sizeof(*x); _s++) {				    \
	    *total += *((uint8_t *)x + _s);				    \
	}								    \
    } while (0);

/* TODO: libFFI support */
static xambit_type_validator_t *lookup_type_validator(xambit_channel_t *ch,
				uint32_t tid);
static void add_type_validator(xambit_channel_t *ch,
				xambit_type_validator_t *tv);
static void set_hdr_csum(xambit_parcel_hdr_t *phdr);
static int validate_hdr_csum(xambit_parcel_hdr_t *phdr);
static int verify_parcel(xambit_channel_t *ch, xambit_parcel_hdr_t *p);
static int prepare_parcel(xambit_channel_t *ch, xambit_parcel_hdr_t *p);
static int channel_send_buf(xambit_channel_t *ch,
			    xambit_parcel_hdr_t *hdr,
			    void *buf);
static int channel_receive_buf(xambit_channel_t *ch,
			      xambit_parcel_hdr_t **phdr,
			      void **buf);
static void xambit_clear_type_map(xambit_channel_t *ch);


/*  Function Name:	channel_fifo_open
 *
 *  Scope:		Module
 *
 *  Purpose:		To open a FIFO object and create a channel obect to the
 *			caller.
 *
 *  Assumptions:	.
 *
 *  Notes:		Depending on the flags passed, this may create the FIFO
 *			object given in the path. The resulting channel is
 *			uni-directional.
 *
 *  Return Value:	Pointer to a xambit_channel_t on success, or NULL on
 *			failure. On failure, errno is set indicating the error
 *			condition.
 */
xambit_channel_t *channel_fifo_open(const char *path, int flags, int write)
{
    int			err;
    size_t		len;
    xambit_channel_t	*ch;
    struct stat		st;
    int			o_flags;

    o_flags = write ? O_WRONLY : O_RDONLY;

    ch = malloc(sizeof(xambit_channel_t));
    if (ch == NULL)
    {
	errno = ENOMEM;
	return NULL;
    }

    ch->tvm = malloc(sizeof(xambit_tv_map_t));
    if (ch->tvm == NULL)
    {
	errno = ENOMEM;
	goto out2;
    }

    memset(ch->tvm, 0, sizeof(xambit_tv_map_t));

    ch->type = XAMBIT_CH_FIFO;
    ch->flags = flags;
    ch->direction = write ? XAMBIT_CHOUT : XAMBIT_CHIN;
    ch->num_types = 0;

    len = strlen(path);
    if (len < PATH_MAX)
    {
	strncpy(ch->path, path, PATH_MAX - 1);
	ch->path[len] = '\0';
    }
    else
    {
	errno = ENAMETOOLONG;
	goto out;
    }

    if (stat(ch->path, &st) < 0)
	goto out;

    if ((!S_ISFIFO(st.st_mode)) && (!S_ISCHR(st.st_mode)))
    {
	errno = EINVAL;
	goto out;
    }

    err = access(ch->path, write ? W_OK : R_OK);
    if (err < 0)
	    goto out;

    ch->fd = open(ch->path, o_flags);
    if (ch->fd < 0)
	goto out;

    return ch;

out:
    free(ch->tvm);
out2:
    free(ch);
    return NULL;
}

/*  Function Name:	channel_close
 *
 *  Scope:		Module
 *
 *  Purpose:		To close a channel and free the associated structure.
 *
 *  Assumptions:	.
 *
 *  Notes:
 *
 *  Return Value:	0 on success, -1 on error and errno is set
 *			appropriately. If there is an error, the channel
 *			structure is NOT freed.
 */
int channel_close(xambit_channel_t *ch)
{
    int err;

    err = close(ch->fd);
    if (err < 0)
	goto out;

    xambit_clear_type_map(ch);
    free(ch);
out:
    return err;
}

static void set_hdr_csum(xambit_parcel_hdr_t *phdr)
{
    uint32_t crc = crc32(0, Z_NULL, 0);
    crc = crc32(crc, (void *)phdr, sizeof(*phdr) - sizeof(phdr->hdr_checksum));
    phdr->hdr_checksum = crc;
}

static int validate_hdr_csum(xambit_parcel_hdr_t *phdr)
{
    uint32_t crc = crc32(0, Z_NULL, 0);
    crc = crc32(crc, (void *)phdr, sizeof(*phdr) - sizeof(phdr->hdr_checksum));
    return (crc == phdr->hdr_checksum) ? 0 : XAMBIT_ERR_CHKSUM;
}

static int verify_parcel(xambit_channel_t *ch, xambit_parcel_hdr_t *p)
{
    int err = 0;

    /* TODO: if need to switch from network byte order, this is where it should
     * be done. Packet header only; user is responsible for converting parcel
     * data. */

    err = validate_hdr_csum(p);

    return err;
}

static int prepare_parcel(xambit_channel_t *ch, xambit_parcel_hdr_t *p)
{
    set_hdr_csum(p);
    return 0;

    /* TODO: if need to switch to network byte order, this is where it should be
     * done. Packet header only; user is responsible for converting parcel data.
     * */
}

/*  Function Name:	channel_send_buf
 *
 *  Scope:		Local
 *
 *  Purpose:		To send a chunk of data
 *
 *  Assumptions:	.
 *
 *  Notes:		.
 *
 *  Return Value:	On error a negetive value will be returned and errno
 *			will be set appropriately. Negetive values other than -1
 *			represent XAmbit specific error conditions. On success a
 *			non-negetive value is returned.
 */
static int channel_send_buf(xambit_channel_t *ch,
			    xambit_parcel_hdr_t *hdr,
			    void *buf)
{
    xambit_type_validator_t *tv;
    ssize_t	(*ch_write)(int, const void *, size_t) = NULL;
    int		count = 0;
    int		rem = 0;
    int		err;

    err = prepare_parcel(ch, hdr);
    if (err < 0)
	goto out;

    tv = lookup_type_validator(ch, hdr->type);
    if (tv == NULL)
	return XAMBIT_ERR_BAD_TYPE;

    err = tv->validate(hdr, buf);
    if (err < 0)
    {
	err = XAMBIT_ERR_VALIDATE;
	goto out;
    }

    switch (ch->type)
    {
	case XAMBIT_CH_FIFO:
	    ch_write = write;
	    break;
#ifdef NOT_YET
	case XAMBIT_CH_SOCK:
	    break;
#endif
	default:
	    break;
    }

    if (ch_write == NULL)
    {
	err = XAMBIT_ERR_STD;
	errno = EINVAL;
	goto out;
    }

    /* Write header first, then data */
    err = ch_write(ch->fd, hdr, sizeof(xambit_parcel_hdr_t));
    if (err != sizeof(xambit_parcel_hdr_t))
    {
	err = XAMBIT_ERR_STD;
	errno = EIO;
	goto out;
    }

    rem = hdr->length;;
    while (rem > 0)
    {
	err = ch_write(ch->fd, buf + count, rem);
	if (err < 0)
	    goto out;
	rem -= err;
	count += err;
    }

    err = 0;
out:
    return err;
}

/* Caller must free allocated memory p */
static int channel_receive_buf(xambit_channel_t *ch,
			      xambit_parcel_hdr_t **phdr,
			      void **buf)
{
    uint64_t		size = 0;
    uint64_t		count = 0;
    uint64_t		rem = 0;
    xambit_type_validator_t *tv;
    xambit_parcel_hdr_t	*hdr;
    void		*data;
    ssize_t		(*ch_read)(int, void *, size_t) = NULL;
    int			err = 0;

    if (phdr == NULL)
    {
	err = XAMBIT_ERR_STD;
	errno = EINVAL;
	goto out;
    }

    switch (ch->type)
    {
	case XAMBIT_CH_FIFO:
	    ch_read = read;
	    break;
#ifdef NOT_YET
	case XAMBIT_CH_SOCK:
	    break;
#endif
	default:
	    break;
    }

    if (ch_read == NULL)
    {
	err = XAMBIT_ERR_STD;
	errno = EINVAL;
	goto out;
    }

    hdr = malloc(sizeof(xambit_parcel_hdr_t));
    if (hdr == NULL)
    {
	err = XAMBIT_ERR_STD;
	errno = ENOMEM;
	goto error2;
    }

    err = ch_read(ch->fd, hdr, sizeof(*hdr));
    if (err < 0) /* Warning: send/receive sync error possib */
	goto error2;

    /* For now, mandate that an entire parcel header must be read at once */
    if (err < sizeof(*hdr))
    { /* Warning: send/receive sync error possible */
	err = XAMBIT_ERR_STD;
	errno = EINVAL;
	goto error2;
    }

    if (hdr->version != XAMBIT_HDR_VERSION)
    { /* Warning: send/receive sync error possible */
	err = XAMBIT_ERR_HDR_VER;
	errno = EINVAL;
	goto error2;
    }

    rem = hdr->length;
    data = malloc(rem);
    if (data == NULL)
    { /* Warning: send/receive sync error possible */
	err = XAMBIT_ERR_STD;
	errno = ENOMEM;
	goto error2;
    }

    while (rem > 0)
    {
	size = ch_read(ch->fd, data + count, rem);
	if (size < 0)
	{ /* Warning: send/receive sync error possible */
	    err = XAMBIT_ERR_STD;
	    goto error1;
	}
	count += size;
	rem -= size;
    }

    err = verify_parcel(ch, hdr);
    if (err < 0)
	goto error1;

    tv = lookup_type_validator(ch, hdr->type);
    if (tv == NULL)
    {
	err = XAMBIT_ERR_BAD_TYPE;
	goto error1;
    }

    err = tv->validate(hdr, data);
    if (err < 0)
    {
	err = XAMBIT_ERR_VALIDATE;
	goto error1;
    }

    *phdr = hdr;
    *buf = data;
out:
    return err;

error1:
    free(data);
error2:
    free(hdr);
    return err;
}

int channel_send_file(xambit_channel_t *ch, const char *path, uint32_t tid)
{
    int			err;
    int			fd;
    void		*data;
    xambit_parcel_hdr_t	hdr;
    struct stat		file;
    size_t		size;
    size_t		index;
    ssize_t		count;

    err = stat(path, &file);
    if (err < 0)
	return err;

    size = file.st_size;

    hdr.length = size;
    hdr.type = tid;
    hdr.flags = XAMBIT_BLOCK;
    hdr.version = XAMBIT_HDR_VERSION;

    err = open(path, O_RDONLY);
    if (err < 0)
	goto out;

    fd = err;

    data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED)
    {
	err = -1;
	goto error;
    }

    err = channel_send_buf(ch, &hdr, data);

    munmap(data, size);
error:
    close(fd);
out:
    return err;
}

int channel_send(xambit_channel_t *ch, void *buf, size_t size, uint32_t tid)
{
    xambit_parcel_hdr_t	hdr;
    int			err;

    hdr.length = size;
    hdr.type = tid;
    hdr.flags = XAMBIT_BLOCK;
    hdr.version = XAMBIT_HDR_VERSION;

    err = channel_send_buf(ch, &hdr, buf);

    return err;
}

int channel_receive_to_file(xambit_channel_t *ch, const char *path,
			      int oflags, mode_t omode)
{
    int			    err;
    int			    fd;
    xambit_parcel_hdr_t	    *hdr = NULL;
    void		    *buf = NULL;

    err = channel_receive_buf(ch, &hdr, &buf);
    if (err < 0)
	goto out;

    fd = open(path, oflags, omode);
    if (fd < 0)
    {
	err = fd;
	goto out;
    }

    /* TODO: worry about not writing all the data? */
    err = write(fd, buf, hdr->length);

    close(fd);

out:
    if (hdr != NULL) free(hdr);
    if (buf != NULL) free(buf);
    return err;
}

int channel_receive(xambit_channel_t *ch, void **buffer,
		    xambit_parcel_hdr_t **header)
{
    int			err;
    xambit_parcel_hdr_t	*hdr = NULL;
    void		*buf = NULL;

    err = channel_receive_buf(ch, &hdr, &buf);
    if (err < 0)
	goto error;

    *buffer = buf;
    *header = hdr;
    return err;

error:
    if (hdr != NULL) free(hdr);
    if (buf != NULL) free(buf);
    return err;
}

static void xambit_clear_type_map(xambit_channel_t *ch)
{
    int i;

    for (i=0; i < XAMBIT_VT_LEN; i++)
    {
	xambit_type_validator_t *t = ch->tvm->map[i];
	while (t != NULL)
	{
	    xambit_type_validator_t *d = t;
	    t = t->next;
	    free(d);
	}
    }
}

static xambit_type_validator_t *lookup_type_validator(xambit_channel_t *ch,
				uint32_t tid)
{
    int index;
    xambit_type_validator_t *tv;

    index = tid % XAMBIT_VT_LEN;
    tv = ch->tvm->map[index];

    while ((tv != NULL) && (tv->type_id != tid))
	tv = tv->next;

    return tv;
}

static void add_type_validator(xambit_channel_t *ch, xambit_type_validator_t *tv)
{
    int index;
    xambit_type_validator_t *last = NULL;

    index = tv->type_id % XAMBIT_VT_LEN;

    if (ch->tvm->map[index] == NULL)
    {
	ch->tvm->map[index] = tv;
    }
    else
    {
	last = ch->tvm->map[index];
	while (last->next != NULL)
	{
	    last = last->next;
	}
	last->next = tv;
    }
}

int channel_register_type(xambit_channel_t *ch,
			  uint32_t type_id,
			  int (*validate)(xambit_parcel_hdr_t *hdr, void *data))
{
    int				err;
    xambit_type_validator_t	*tv;

    if (ch == NULL || validate == NULL)
    {
	errno = EINVAL;
	return -1;
    }

    tv = lookup_type_validator(ch, type_id);
    if (tv != NULL)
    {
	errno = EEXIST;
	return -1;
    }

    tv = malloc(sizeof(xambit_type_validator_t));
    if (tv == NULL)
    {
	errno = ENOMEM;
	return -1;
    }

    tv->type_id = type_id;
    tv->validate = validate;
    tv->prev = NULL;
    tv->next = NULL;

    add_type_validator(ch, tv);
    ch->num_types++;
    return 0;
}

/* TODO: unregister_type? */

int null_validator(xambit_parcel_hdr_t *p, void *data)
{
    return 0;
}

int default_validator(xambit_parcel_hdr_t *p, void *data)
{
    return -1;
}
