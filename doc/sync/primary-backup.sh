#!/bin/sh
# 
# (C) 2008 by Pablo Neira Ayuso <pablo@netfilter.org>
#
# This software may be used and distributed according to the terms
# of the GNU General Public License, incorporated herein by reference.
#
# Description:
#
# This is the script for primary-backup setups for keepalived
# (http://www.keepalived.org). You may adapt it to make it work with other
# high-availability managers.
#
# Do not forget to include the required modifications to your keepalived.conf
# file to invoke this script during keepalived's state transitions.
#
# Contributions to improve this script are welcome :).
#

CONNTRACKD_BIN=/usr/sbin/conntrackd
CONNTRACKD_LOCK=/var/lock/conntrack.lock
CONNTRACKD_CONFIG=/etc/conntrackd/conntrackd.conf

case "$1" in
  primary)
    #
    # commit the external cache into the kernel table
    #
    $CONNTRACKD_BIN -C $CONNTRACKD_CONFIG -c
    if [ $? -eq 1 ]
        logger "ERROR: failed to invoke conntrackd -c"

    #
    # flush the internal and the external caches
    #
    $CONNTRACKD_BIN -C $CONNTRACK_CONFIG -f
    if [ $? -eq 1 ]
    	logger "ERROR: failed to invoke conntrackd -f"

    #
    # resynchronize my internal cache to the kernel table
    #
    $CONNTRACKD_BIN -C $CONNTRACKD_CONFIG -R
    if [ $? -eq 1 ]
    	logger "ERROR: failed to invoke conntrackd -R"
    ;;
  backup)
    #
    # is conntrackd running? request some statistics to check it
    #
    $CONNTRACKD_BIN -C $CONNTRACKD_CONFIG -s
    if [ $? -eq 1 ]
    then
        #
	# something's wrong, do we have a lock file?
	#
    	if [ -f $CONNTRACKD_LOCK ]
	then
	    logger "WARNING: conntrackd was not cleanly stopped."
	    logger "If you suspect that it has crashed:"
	    logger "1) Enable coredumps"
	    logger "2) Try to reproduce the problem"
	    logger "3) Post the coredump to netfilter-devel@vger.kernel.org"
	    rm -f $CONNTRACKD_LOCK
	fi
	$CONNTRACKD_BIN -C $CONNTRACKD_CONFIG -d
	if [ $? -eq 1 ]
	then
	    logger "ERROR: cannot launch conntrackd"
	    exit 1
	fi
    fi
    #
    # shorten kernel conntrack timers to remove the zombie entries.
    #
    $CONNTRACKD_BIN -C $CONNTRACKD_CONFIG -t
    if [ $? -eq 1 ]
    	logger "ERROR: failed to invoke conntrackd -t"

    #
    # request resynchronization with master firewall replica (if any)
    # Note: this does nothing in the alarm approach.
    #
    $CONNTRACKD_BIN -C $CONNTRACKD_CONFIG -n
    if [ $? -eq 1 ]
    	logger "ERROR: failed to invoke conntrackd -n"
    ;;
  *)
    logger "ERROR: unknown state transition"
    echo "Usage: primary-backup.sh {primary|backup}"
    exit 1
    ;;
esac

exit 0
