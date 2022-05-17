#!/bin/sh

/bin/cat /proc/self/environ | tr '\0' '\n'

container_mdev=/storage/trails/current/$LXC_NAME/mdev.json

if ! [ -f $container_mdev ]; then
	exit 0
fi

tmpf=`mktemp -t mdev.conf.XXXXXXX`

echo "CONTAINER_DEV: $container_mdev"

cat $container_mdev \
	| /lib/pv/JSON.sh -l \
	| grep '\["rules",' \
	| sed 's/[^[:space:]]*[[:space:]]"//;s/"$//' \
	> $tmpf

FOLLOW_X_PID=$LXC_PID MDEV_CONF=$tmpf mdev ${MDEV_VERBOSE:+-$MDEV_VERBOSE} -S -d >/dev/null 2>&1 </dev/null

