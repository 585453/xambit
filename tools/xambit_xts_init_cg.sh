#! /xts/bin/sh

# XAmbit - Cross boundary data transfer library
# Copyright (C) 2016-2017 BAE Systems Electronic Systems, Inc.
#
# This file is part of XAmbit.
#
# XAmbit is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# XAmbit is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied
# warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with XAmbit.  If not, see <http://www.gnu.org/licenses/>.
#

# Channel Graph Parser - Takes a channel graph and creates a basic Stop 
# security policy based on it. The format of the channel graph config is as
# follows:
#
# One or more lines in the form:
#   DOMAIN=domain_id
# where domain_id is a unique identifer for that domain
#
# One or more lines in the form:
#   A->B;ch_id
# where A and B are domain_id's and ch_id is a unique name for that channel.
# The domain_id before the '->' string denotes the sending domain and the 
# domain_id after the '->' string denotes the receiving domain.
#
# Example:
#
# DOMAIN=sender
# DOMAIN=filter
# DOMAIN=receiver
# sender->filter;sndflt0
# filter->receiver;fltrec0

# Could alternatively use DOT, GML, or TGF... 

# WARNING: This script is intended to be used only on the Stop operating system

CFGDIR="/xts/cfg"
SECDIR="${CFGDIR}/security"
ROLEDIR="${SECDIR}/role"
BLBDIR="${SECDIR}/blb"
AUTHDIR="${SECDIR}/auth/users"
FIFODIR="/ch_fifos"
HOMEDIR="/home"
DEFAULT_BLB="syslo"
ADMIN_AUTH="admin"
DOMAINS=''

# Definition of new roles to be created
DOMAIN_READ_ROLE_DEF="sys.access,auth_get,flock,label_get,label_role_set,open,read,stat,search"
DOMAIN_WRITE_ROLE_DEF="sys.create,delete,link,label_dac_set,open,truncate,write"
DOMAIN_EXECUTE_ROLE_DEF="sys.execute"

CHANNEL_WRITE_ROLE_DEF="sys.access,auth_get,flock,label_get,label_role_set,open,read,stat,search,write"
CHANNEL_READ_ROLE_DEF="sys.access,auth_get,flock,label_get,label_role_set,open,read,stat,search"

NEW_ROLES=''
# WARNING: This role name must not already exist on the system!
ADM_COL_ROLE="xall"


create_domain() {
    DOM=$1
    if [ x${DOM} = "x" ]
    then
	echo "Missing Domains"
	usage
    fi

    # Create an auth with name ${DOM}
    echo "Adding user for domain ${DOM}"
    useradd ${DOM}

    # Init clearance
    echo ":${DEFAULT_BLB}|sys,all_rw:${DEFAULT_BLB}:${DOM}.${DOM}.0700|sys,all_rw:${DEFAULT_BLB}:${DOM}.${DOM}.0777" > ${AUTHDIR}/${DOM}/clearance

    # Set the password and login attributes for the user
    echo "Please enter the account password for domain user ${DOM}"
    passwd ${DOM}

}

get_clearance_min() {
    AUTH=$1
    echo "`cat ${AUTHDIR}/${AUTH}/clearance | awk -F '|' '{print $1}'`"
}

get_clearance_def() {
    AUTH=$1
    echo "`cat ${AUTHDIR}/${AUTH}/clearance | awk -F '|' '{print $2}'`"
}

get_clearance_max() {
    AUTH=$1
    echo "`cat ${AUTHDIR}/${AUTH}/clearance | awk -F '|' '{print $3}'`"
}

clearance_role_del_max() {
    AUTH=$1
    ROLE=$2

    MAX=$( get_clearance_max ${AUTH} )

    ROLES=`echo ${MAX} | awk -F ':' '{print $1}'`
    ROLES=`echo ${ROLES} | sed "s/\<${ROLE}\>//"`

    # Remove extraneous commas
    ROLES=`echo ${ROLES} | sed "s/^,//"`
    ROLES=`echo ${ROLES} | sed "s/,$//"`
    ROLES=`echo ${ROLES} | sed "s/,,/,/"`

    echo "||${ROLES}" > ${AUTHDIR}/${AUTH}/clearance
}

clearance_role_del_def() {
    AUTH=$1
    ROLE=$2

    DEF=$( get_clearance_def ${AUTH} )

    ROLES=`echo ${DEF} | awk -F ':' '{print $1}'`
    ROLES=`echo ${ROLES} | sed "s/\<${ROLE}\>//"`

    # Remove extraneous commas
    ROLES=`echo ${ROLES} | sed "s/^,//"`
    ROLES=`echo ${ROLES} | sed "s/,$//"`
    ROLES=`echo ${ROLES} | sed "s/,,/,/"`

    echo "|${ROLES}" > ${AUTHDIR}/${AUTH}/clearance
}

clearance_role_del_min() {
    AUTH=$1
    ROLE=$2

    MIN=$( get_clearance_max ${AUTH} )

    ROLES=`echo ${MIN} | awk -F ':' '{print $1}'`
    ROLES=`echo ${ROLES} | sed "s/\<${ROLE}\>//"`

    # Remove extraneous commas
    ROLES=`echo ${ROLES} | sed "s/^,//"`
    ROLES=`echo ${ROLES} | sed "s/,$//"`
    ROLES=`echo ${ROLES} | sed "s/,,/,/"`

    echo "${ROLES}" > ${AUTHDIR}/${AUTH}/clearance
}

clearance_role_add_max() {
    AUTH=$1
    ROLE=$2

    MAX=$( get_clearance_max ${AUTH} )

    ROLES=`echo ${MAX} | awk -F ':' '{print $1}'`

    echo "||${ROLES},${ROLE}" > ${AUTHDIR}/${AUTH}/clearance
}

clearance_role_add_def() {
    AUTH=$1
    ROLE=$2

    DEF=$( get_clearance_def ${AUTH} )

    ROLES=`echo ${DEF} | awk -F ':' '{print $1}'`

    echo "|${ROLES},${ROLE}|" > ${AUTHDIR}/${AUTH}/clearance
}

clearance_role_add_min() {
    AUTH=$1
    ROLE=$2

    MIN=$( get_clearance_min ${AUTH} )

    ROLES=`echo ${MIN} | awk -F ':' '{print $1}'`

    echo "${ROLES},${ROLE}||" > ${AUTHDIR}/${AUTH}/clearance
}

clearance_role_add() {
    AUTH=$1
    ROLE=$2

    echo "Adding ${ROLE} to ${AUTH}'s clearance"
    clearance_role_add_max ${AUTH} ${ROLE}
    clearance_role_add_def ${AUTH} ${ROLE}
    clearance_role_add_min ${AUTH} ${ROLE}
}

self_label_add_role() {
    ROLE=$1
    OLD_ROLES=`sec_label -p | awk -F ':' '{print $1}'`
    sec_label -pl "${OLD_ROLES},${ROLE}"
}

self_label_del_role() {
    ROLE=$1
    ROLES=`sec_label -p | awk -F ':' '{print $1}'`
    ROLES=`echo ${ROLES} | sed "s/\<${ROLE}\>//"`

    # Remove extraneous commas
    ROLES=`echo ${ROLES} | sed "s/^,//"`
    ROLES=`echo ${ROLES} | sed "s/,$//"`
    ROLES=`echo ${ROLES} | sed "s/,,/,/"`
    sec_label -pl "${ROLES}"
}

usage() {
    echo "Usage: xts_init_cg.sh [options] FILE"
    echo "Options:"
    echo "    -h, --help             Display this help message"
    exit 1
}

ARG=`getopt -o h --long help -n 'xambit_xts_init_cg.sh' -- "$@"` || usage
eval set -- "$ARG"

while true ; do
    case "$1" in
	-h|--help)
	    usage
	    ;;
	--)
	    shift;
	    break
	    ;;
	*)
	    usage
	    ;;
    esac
done

CONFIG=$1
if [ x"${CONFIG}" = x ]
then
    usage
fi

CUR_ADM_ROLES=$( get_clearance_max ${ADMIN_AUTH} )
CUR_ADM_ROLES=`echo ${CUR_ADM_ROLES} | awk -F ':' '{print $1}'`
if [ `echo -ne "${CUR_ADM_ROLES}" | wc | awk '{print $3}'` -gt `echo -ne "${ADM_COL_ROLE}" | wc | awk '{print $3}'` ]
then
    echo "${CUR_ADM_ROLES}." > ${ROLEDIR}/${ADM_COL_ROLE}
    echo "||${ADM_COL_ROLE}" > ${AUTHDIR}/${ADMIN_AUTH}/clearance
    sec_label -pl ${ADM_COL_ROLE}
fi


mkdir -p ${FIFODIR}

for DOM in `grep '^DOMAIN=' ${CONFIG} | awk -F '=' '{print $2}'`
do
    # Create the users
    create_domain ${DOM}

    # Create Distinct Read/Write/Execute roles for ${DOM}
    echo "Creating read/write/execute roles for domain ${DOM}"
    echo "${DOMAIN_READ_ROLE_DEF}" > ${ROLEDIR}/${DOM}_r
    echo "${DOMAIN_WRITE_ROLE_DEF}" > ${ROLEDIR}/${DOM}_w
    echo "${DOMAIN_EXECUTE_ROLE_DEF}" > ${ROLEDIR}/${DOM}_x
    NEW_ROLES="${NEW_ROLES} ${DOM}_r ${DOM}_w ${DOM}_x"
    echo "${DOM}_r,${DOM}_w,${DOM}_x." > ${ROLEDIR}/${DOM}_rwx

    clearance_role_add ${DOM} ${DOM}_rwx
    clearance_role_add_max ${ADMIN_AUTH} ${DOM}_rwx
    
    self_label_add_role ${DOM}_rwx
    echo "Setting label on ${DOM}'s home directory"
    sec_label -l ${DOM}_rwx ${HOMEDIR}/${DOM}

    #Find all channels that have ${DOM} as a sender
    SEND_CHANNEL_NAMES=`grep "^${DOM}\->" ${CONFIG} | awk -F ';' '{print $2}'`

    # Create FIFO Read/Write roles
    for CHANNEL in ${SEND_CHANNEL_NAMES}
    do
	CH_ROLE="${DOM}_ch_w"
	echo "Creating role ${CH_ROLE}"
        echo "${CHANNEL_WRITE_ROLE_DEF}" > ${ROLEDIR}/${CH_ROLE}
	NEW_ROLES="${NEW_ROLES} ${CH_ROLE}"
	clearance_role_add ${DOM} ${CH_ROLE}
	clearance_role_add_max ${ADMIN_AUTH} ${CH_ROLE}
	
	self_label_add_role ${CH_ROLE}

	if [ ! -p ${FIFODIR}/${CHANNEL} ]
	then
	    echo "Creating channel fifo ${FIFODIR}/${CHANNEL}"
	    mknod ${FIFODIR}/${CHANNEL} p
	    echo "Setting label ${CH_ROLE}:${DEFAULT_BLB}:${DOM}.${DOM}.0666 on ${FIFODIR}/${CHANNEL}"
	    sec_label -l "${CH_ROLE}:${DEFAULT_BLB}:${DOM}.${DOM}.0666" ${FIFODIR}/${CHANNEL}
	else
	    CUR_ROLES=`sec_label ${FIFODIR}/${CHANNEL} | awk -F ':' '{print $1}'`
	    echo "Setting label ${CUR_ROLES},${CH_ROLE} on ${FIFODIR}/${CHANNEL}"
	    sec_label -l "${CUR_ROLES},${CH_ROLE}" ${FIFODIR}/${CHANNEL}
	fi
	self_label_del_role ${CH_ROLE}
	clearance_role_del_max ${ADMIN_AUTH} ${CH_ROLE}

    done

    #Find all channels that have ${DOM} as a receiver
    REC_CHANNEL_NAMES=`grep "\->${DOM};" ${CONFIG} | awk -F ';' '{print $2}'`

    for CHANNEL in ${REC_CHANNEL_NAMES}
    do
	CH_ROLE="${DOM}_ch_r"
	echo "Creating role ${CH_ROLE}"
	echo "${CHANNEL_READ_ROLE_DEF}" > ${ROLEDIR}/${CH_ROLE}
	NEW_ROLES="${NEW_ROLES} ${CH_ROLE}"
	clearance_role_add ${DOM} ${CH_ROLE}
	clearance_role_add_max ${ADMIN_AUTH} ${CH_ROLE}

	self_label_add_role ${CH_ROLE}
	
	if [ ! -p ${FIFODIR}/${CHANNEL} ]
	then
	    echo "Creating channel fifo ${FIFODIR}/${CHANNEL}"
	    mknod ${FIFODIR}/${CHANNEL} p
	    echo "Setting label ${CH_ROLE}:${DEFAULT_BLB}:${DOM}.${DOM}.0666 on ${FIFODIR}/${CHANNEL}"
	    sec_label -l "${CH_ROLE}:${DEFAULT_BLB}:${DOM}.${DOM}.0666" ${FIFODIR}/${CHANNEL}
	else
	    CUR_ROLES=`sec_label ${FIFODIR}/${CHANNEL} | awk -F ':' '{print $1}'`
	    echo "Setting label ${CUR_ROLES},${CH_ROLE} on ${FIFODIR}/${CHANNEL}"
	    sec_label -l "${CUR_ROLES},${CH_ROLE}" ${FIFODIR}/${CHANNEL}
	fi
	self_label_del_role ${CH_ROLE}
	clearance_role_del_max ${ADMIN_AUTH} ${CH_ROLE}
    done

    self_label_del_role ${DOM}_rwx
    clearance_role_del_max ${ADMIN_AUTH} ${DOM}_rwx
done

echo "Removing sys from new roles..."
for ROLE in ${NEW_ROLES}
do
    echo "`cat ${ROLEDIR}/${ROLE} | sed "s/^sys\./null\./"`" > ${ROLEDIR}/${ROLE}
done

echo "Done"
