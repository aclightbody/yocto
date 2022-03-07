#! /bin/sh
### BEGIN INIT INFO
# Provides:          pi-blaster
# Required-Start:    $remote_fs $syslog
# Required-Stop:     $remote_fs $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Daemon for PWM control of the Raspberry Pi GPIO pins
# Description:       This tool will turn the given GPIO pins into outputs and 
#                    lets the application control the value on the pin by writing
#                    commands into /dev/pi-blaster, such as "0=0.3" to set the
#                    first pin to 30% PWM.
### END INIT INFO

# Author: Thomas Sarlandie <thomas@sarlandie.net>

PATH=/sbin:/usr/sbin:/bin:/usr/bin
DESC="Daemon for PWM control of the Raspberry Pi GPIO pins"
NAME=pi-blaster
DAEMON=/usr/sbin/$NAME
DAEMON_ARGS=""
SCRIPTNAME=/etc/init.d/$NAME

# Exit if the package is not installed
[ -x "$DAEMON" ] || exit 0

# Read configuration variable file if it is present
[ -r /etc/default/$NAME ] && . /etc/default/$NAME

# Load the VERBOSE setting and other rcS variables
#. /lib/init/vars.sh

# Define LSB log_* functions.
# Depend on lsb-base (>= 3.2-14) to ensure that this file is present
# and status_of_proc is working.
#. /lib/lsb/init-functions

#
# Function that starts the daemon/service
#
do_start()
{
	# Return
	#   0 if daemon has been started
	#   1 if daemon was already running
	#   2 if daemon could not be started
	start-stop-daemon --start --quiet --exec $DAEMON --test > /dev/null \
		|| return 1
	start-stop-daemon --start --quiet --exec $DAEMON -- \
		$DAEMON_ARGS \
		|| return 2
}

#
# Function that stops the daemon/service
#
do_stop()
{
	# Return
	#   0 if daemon has been stopped
	#   1 if daemon was already stopped
	#   2 if daemon could not be stopped
	#   other if a failure occurred
	start-stop-daemon --stop --quiet --retry=TERM/30/KILL/5 --name $NAME
	RETVAL="$?"
	[ "$RETVAL" = 2 ] && return 2
	# Wait for children to finish too if this is a daemon that forks
	# and if the daemon is only ever run from this initscript.
	# If the above conditions are not satisfied then add some other code
	# that waits for the process to drop all resources that could be
	# needed by services started subsequently.  A last resort is to
	# sleep for some time.
	start-stop-daemon --stop --quiet --oknodo --retry=0/30/KILL/5 --exec $DAEMON
	[ "$?" = 2 ] && return 2
	return "$RETVAL"
}

case "$1" in
  start)
	[ "$VERBOSE" != no ] && echo "Starting $DESC" "$NAME"
	do_start
	;;
  stop)
	[ "$VERBOSE" != no ] && echo "Stopping $DESC" "$NAME"
	do_stop
	;;
  restart|force-reload)
	#
	# If the "reload" option is implemented then remove the
	# 'force-reload' alias
	#
	echo "Restarting $DESC" "$NAME"
	do_stop
    do_start
	;;
  *)
	echo "Usage: $SCRIPTNAME {start|stop|restart|force-reload}" >&2
	exit 3
	;;
esac

:
