#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <fcntl.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <linux/if_tun.h>
#include <linux/sockios.h>

#define TUNDEV "/dev/net/tun"
#define HELP \
	"usage: %s { create | delete | bridge } ...\n" \
	"\n" \
	"   create $TAP $USER $GROUP (defaults to root:root)\n" \
	"   delete $TAP\n" \
	"   bridge $TAP $BRIDGE\n" \
	"\n" \
	"Version 0.0.1 <cubicool@gmail.com>"

/* Enumerations representing the difference ways in which this utility can fail. */
typedef enum _error_code {
	ERR_CLI_ARGS = 0,
	ERR_DEV_NET_TUN,
	ERR_BAD_IOCTL,
	ERR_USER_NAME,
	ERR_GROUP_NAME,
	ERR_BRIDGE_SOCKET,
	ERR_BRIDGE_IFINDEX,
	ERR_BRIDGE_IOCTL
} error_code;

static const char* error_code_strings[] = {
	HELP,
	"Couldn't open file: %s",
	"Bad ioctl using value: %s",
	"No user: %s",
	"No group: %s",
	"Bridge API socket creation failed",
	"Couldn't find tap interface: %s",
	"Couldn't bridge interface"
};

/* Enums used when setting the tap interface state. */
typedef enum _state {
	UP,
	DOWN
} state;

/* A function wrapping error printing and program exit; this sometimes complicates closing open file
 * descriptiors, but such is programming. */
static void error(error_code ec, ...) {
	va_list vl;

	va_start(vl, ec);

	vfprintf(stderr, error_code_strings[ec], vl);

	/* If there's a standard errno.h error, print it out as well. */
	if(errno) fprintf(stderr, " (%s)", strerror(errno));

	fprintf(stderr, "\n");

	va_end(vl);

	/* Again, because we exit here, you'll need to take care to close any open file descriptors
	 * prior to calling this function. */
	exit(ec + 1);
}

/* Converts a user name string to their UID. */
static int get_uid(const char* username) {
	struct passwd* pw = getpwnam(username);

	if(!pw) error(ERR_USER_NAME, username);

	return pw->pw_uid;
}

/* Converts a group name to its GID. */
static int get_gid(const char* groupname) {
	struct group* gr = getgrnam(groupname);

	if(!gr) error(ERR_GROUP_NAME);

	return gr->gr_gid;
}

/* We need to open/close the tun/tap control node between every ioctl. */
static int tap_open() {
	int fd = open(TUNDEV, O_RDWR);

	if(fd < 0) error(ERR_DEV_NET_TUN, TUNDEV);

	return fd;
}

#define tap_ioctl(request, data) \
	if(ioctl(fd, request, data)) { \
		close(fd); \
		error(ERR_BAD_IOCTL, #request); \
	}

/* Creates a tap interface using the specified user/group combo. */
static void tap_create(const char* tap, const char* username, const char* groupname) {
	struct ifreq ifr;

	uid_t uid = get_uid(username);
	gid_t gid = get_gid(groupname);

	int fd = tap_open();

	strncpy(ifr.ifr_name, tap, IFNAMSIZ - 1);

	ifr.ifr_flags = IFF_NO_PI | IFF_TAP;

	tap_ioctl(TUNSETIFF, &ifr);
	tap_ioctl(TUNSETOWNER, uid);
	tap_ioctl(TUNSETGROUP, gid);
	tap_ioctl(TUNSETPERSIST, 1);

	close(fd);
}

/* Deletes the specified tap interface. */
static void tap_delete(const char* tap) {
	struct ifreq ifr;

	int fd = tap_open();

	strncpy(ifr.ifr_name, tap, IFNAMSIZ - 1);

	ifr.ifr_flags = IFF_TAP;

	tap_ioctl(TUNSETIFF, &ifr);
	tap_ioctl(TUNSETPERSIST, 0);

	close(fd);
}

/* Bridges the tap interface using the specified bridge. This utility does NOT create bridge
 * interfaces, as that is generally setup for us during boot. */
static void tap_bridge(const char* tap, const char* bridge) {
	struct ifreq ifr;
	int br_socket = -1;
	int ifindex = -1;

	/* Open a local "kernel" socket to communicate bridge info. */
	br_socket = socket(AF_LOCAL, SOCK_STREAM, 0);

	if(br_socket < 0) error(ERR_BRIDGE_SOCKET);

	/* Set the tap interface to be bridged, and make sure it exists! */
	ifindex = if_nametoindex(tap);

	if(!ifindex) error(ERR_BRIDGE_IFINDEX, tap);

	ifr.ifr_ifindex = ifindex;

	/* Finally, call the ioctl to add the interface. */
	strncpy(ifr.ifr_name, bridge, IFNAMSIZ - 1);

	if(ioctl(br_socket, SIOCBRADDIF, &ifr)) error(ERR_BRIDGE_IOCTL, tap);
}

/* Sets an interface up or down. */
static void tap_up_down(const char* tap, state s) {
	struct ifreq ifr;

	int fd = socket(PF_INET, SOCK_DGRAM, 0);

	strncpy(ifr.ifr_name, tap, IFNAMSIZ - 1);

	/* Get the current interface flags and toggle IFF_UP. */
	tap_ioctl(SIOCGIFFLAGS, &ifr);

	if(s == UP) ifr.ifr_flags |= IFF_UP;

	else ifr.ifr_flags ^= IFF_UP;

	/* Set the interface flags after toggling IFF_UP. */
	tap_ioctl(SIOCSIFFLAGS, &ifr);

	close(fd);
}

int main(int argc, char** argv) {
	const char* action = NULL;
	const char* tap = NULL;

	if(argc < 3) error(ERR_CLI_ARGS, argv[0]);

	action = argv[1];
	tap = argv[2];

	/* create $TAP $USER $GROUP */
	if(!strncmp(action, "create", 6)) {
		const char* user = "root";
		const char* group = "root";

		if(argc >= 4) user = argv[3];
		if(argc >= 5) group = argv[4];

		tap_create(tap, user, group);
		tap_up_down(tap, UP);
	}

	/* delete $TAP */
	if(!strncmp(action, "delete", 6)) {
		tap_up_down(tap, DOWN);
		tap_delete(tap);
	}

	/* bridge $TAP $BRIDGE */
	if(!strncmp(action, "bridge", 6)) {
		if(argc < 4) error(ERR_CLI_ARGS, argv[0]);

		tap_bridge(tap, argv[3]);
	}

	return 0;
}

