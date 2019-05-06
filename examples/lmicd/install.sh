#!/bin/sh

mkdir -p /var/log/lmicd
cp lmicd.sh /etc/init.d
chmod a+x /etc/init.d/lmicd.sh
cp lmicd /usr/local/bin
chmod a+x /usr/local/bin/lmicd
update-rc.d lmicd.sh defaults
cp send-ttn /usr/local/bin
chmod a+x /usr/local/bin/send-ttn