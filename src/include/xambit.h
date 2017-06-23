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
 *
 */


#ifndef XAMBIT_H
#define XAMBIT_H

/* FIXME: properly determine if using gcc */
#define PACKED __attribute__((packed))

#if defined(XTS)
#include <xts/limits.h>
#else
#include <linux/limits.h>
#endif

#include <inttypes.h>

#define XAMBIT_CH_FIFO		0x00
#ifdef NOT_YET
#define XAMBIT_CH_SOCK		0x01
#endif

/* Parcel Flags */
#define XAMBIT_BLOCK		0x00	    /* 0 = Block data where data is
					       complete within a single parcel */
#if 0
#define XAMBIT_STREAM		0x01	    /* 1 = Stream data where parcel data
					       may be segmented into
					       MAX_STREAM_SIZE byte chunks. */
#endif

/* Channel Direction */
#define XAMBIT_CHIN		0x00	    /* Reader */
#define XAMBIT_CHOUT		0x01	    /* Writer */

/* Channel Flags */
#if 0
#define XAMBIT_CREATE_ON_OPEN	0x0001
#define XAMBIT_DEL_ON_CLOSE	0x0002
#endif

/* XAmbit Error Conditions */
#define XAMBIT_ERR_STD		-1	    /* Standard system error, use errno */
#define XAMBIT_ERR_CHKSUM	-2	    /* Header checksum error */
#define XAMBIT_ERR_VALIDATE	-3	    /* The user-defined validate
					       function returned a negative
					       value */
#define XAMBIT_ERR_BAD_TYPE	-4	    /* Invalid data type */
#define XAMBIT_ERR_HDR_VER	-5	    /* Received an incompatible parcel
					       header */

/* Constants */
#define XAMBIT_VT_LEN		64	    /* Size of validator table map */
#define MAX_STREAM_SIZE		(0x1 << 14) /* 16K */

#define XAMBIT_HDR_VERSION	1

/* ******************  Parcel structure ******************* */
typedef struct xambit_parcel_hdr_s {/* Version 1 */
    uint32_t	version;
    uint32_t	type;		    /* User-defined type; determines which
				       validate callback routine will be used */
    uint32_t	flags;
    uint64_t	length;		    /* Size in bytes of data buffer - don't
				       want a size_t here because sender and
				       receiver can be different systems with
				       different deffinitions of a size_t*/
    uint32_t	hdr_checksum;	    /* Header only - must be last item in header */
} PACKED xambit_parcel_hdr_t;


/* ***************** Type Validator Table ***************** */
typedef struct xambit_type_validator_s {
    uint32_t	type_id;
    int		(*validate)(xambit_parcel_hdr_t *hdr, void *data);
    /* TODO: Locking */
    struct xambit_type_validator_s *prev;
    struct xambit_type_validator_s *next;
} xambit_type_validator_t;

typedef struct xambit_tv_map_s {
    xambit_type_validator_t * map[XAMBIT_VT_LEN];
} xambit_tv_map_t;

/* ******************* Channel Structures ******************* */
typedef struct xambit_channel_s {
    uint32_t	fd;
    uint32_t	flags;
    uint32_t	num_types;	    /* Number of registered type validators */
    xambit_tv_map_t *tvm;
    uint8_t	type;		    /* FIFO or Socket */
    uint8_t	direction;	    /* Reader or Writer */
    union {
	/* FIFO channel data */
	char	    path[PATH_MAX];

	/* Socket channel data */

    };
} xambit_channel_t;

typedef struct channel_type_ops_s {
    xambit_channel_t	*ch;
    int			type_id;
    int (*val_in)(xambit_parcel_hdr_t *p, void *d);
    int (*val_out)(xambit_parcel_hdr_t *p, void *d);
} channel_type_ops_t;

/* ****************** Methods ******************** */
xambit_channel_t * channel_fifo_open(const char *path, int flags, int write);
int channel_close(xambit_channel_t *ch);

int channel_send_file(xambit_channel_t *ch, const char *path, uint32_t tid);
int channel_send(xambit_channel_t *ch, void *buf, size_t size, uint32_t tid);

int channel_receive_to_file(xambit_channel_t *ch, const char *path,
	int oflags, mode_t omode);
int channel_receive(xambit_channel_t *ch, void **buf,
	xambit_parcel_hdr_t **header);

int channel_register_type(xambit_channel_t *,
	uint32_t type_id,
	int (*validate)(xambit_parcel_hdr_t *hdr, void *data));

int null_validator(xambit_parcel_hdr_t *p, void *data);
int default_validator(xambit_parcel_hdr_t *p, void *data);

#endif
