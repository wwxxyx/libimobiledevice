/*
 * socket.c
 *
 * Copyright (C) 2012-2018 Nikias Bassen <nikias@gmx.li>
 * Copyright (C) 2012 Martin Szulecki <m.szulecki@libimobiledevice.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#ifdef _MSC_VER
#include <winsock2.h> 
#include <windows.h>
#else
#include <unistd.h>
#include <sys/time.h>
#endif

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
static int wsa_init = 0;
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#endif
#include "socket.h"

#define RECV_TIMEOUT 20000
#define CONNECT_TIMEOUT 5000

#ifndef ECONNRESET
#define ECONNRESET 108
#endif

static int verbose = 0;

void socket_set_verbose(int level)
{
	verbose = level;
}

#ifndef WIN32
int socket_create_unix(const char *filename)
{
	struct sockaddr_un name;
	int sock;
#ifdef SO_NOSIGPIPE
	int yes = 1;
#endif

	// remove if still present
	unlink(filename);

	/* Create the socket. */
	sock = socket(PF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("socket");
		return -1;
	}

#ifdef SO_NOSIGPIPE
	if (setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, (void*)&yes, sizeof(int)) == -1) {
		perror("setsockopt()");
		socket_close(sock);
		return -1;
	}
#endif

	/* Bind a name to the socket. */
	name.sun_family = AF_UNIX;
	strncpy(name.sun_path, filename, sizeof(name.sun_path));
	name.sun_path[sizeof(name.sun_path) - 1] = '\0';

	if (bind(sock, (struct sockaddr*)&name, sizeof(name)) < 0) {
		perror("bind");
		socket_close(sock);
		return -1;
	}

	if (listen(sock, 10) < 0) {
		perror("listen");
		socket_close(sock);
		return -1;
	}

	return sock;
}

int socket_connect_unix(const char *filename)
{
	struct sockaddr_un name;
	int sfd = -1;
	struct stat fst;
#ifdef SO_NOSIGPIPE
	int yes = 1;
#endif
	int bufsize = 0x20000;

	// check if socket file exists...
	if (stat(filename, &fst) != 0) {
		if (verbose >= 2)
			fprintf(stderr, "%s: stat '%s': %s\n", __func__, filename,
					strerror(errno));
		return -1;
	}
	// ... and if it is a unix domain socket
	if (!S_ISSOCK(fst.st_mode)) {
		if (verbose >= 2)
			fprintf(stderr, "%s: File '%s' is not a socket!\n", __func__,
					filename);
		return -1;
	}
	// make a new socket
	if ((sfd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
		if (verbose >= 2)
			fprintf(stderr, "%s: socket: %s\n", __func__, strerror(errno));
		return -1;
	}

	if (setsockopt(sfd, SOL_SOCKET, SO_SNDBUF, (void*)&bufsize, sizeof(int)) == -1) {
		perror("Could not set send buffer for socket");
	}

	if (setsockopt(sfd, SOL_SOCKET, SO_RCVBUF, (void*)&bufsize, sizeof(int)) == -1) {
		perror("Could not set receive buffer for socket");
	}

#ifdef SO_NOSIGPIPE
	if (setsockopt(sfd, SOL_SOCKET, SO_NOSIGPIPE, (void*)&yes, sizeof(int)) == -1) {
		perror("setsockopt()");
		socket_close(sfd);
		return -1;
	}
#endif
	// and connect to 'filename'
	name.sun_family = AF_UNIX;
	strncpy(name.sun_path, filename, sizeof(name.sun_path));
	name.sun_path[sizeof(name.sun_path) - 1] = 0;

	if (connect(sfd, (struct sockaddr*)&name, sizeof(name)) < 0) {
		socket_close(sfd);
		if (verbose >= 2)
			fprintf(stderr, "%s: connect: %s\n", __func__,
					strerror(errno));
		return -1;
	}

	return sfd;
}
#endif

int socket_create(uint16_t port)
{
	int sfd = -1;
	int yes = 1;
#ifdef WIN32
	WSADATA wsa_data;
	if (!wsa_init) {
		if (WSAStartup(MAKEWORD(2,2), &wsa_data) != ERROR_SUCCESS) {
			fprintf(stderr, "WSAStartup failed!\n");
			ExitProcess(-1);
		}
		wsa_init = 1;
	}
#endif
	struct sockaddr_in saddr;

	if (0 > (sfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP))) {
		perror("socket()");
		return -1;
	}

	if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (void*)&yes, sizeof(int)) == -1) {
		perror("setsockopt()");
		socket_close(sfd);
		return -1;
	}

#ifdef SO_NOSIGPIPE
	if (setsockopt(sfd, SOL_SOCKET, SO_NOSIGPIPE, (void*)&yes, sizeof(int)) == -1) {
		perror("setsockopt()");
		socket_close(sfd);
		return -1;
	}
#endif

	memset((void *) &saddr, 0, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	saddr.sin_port = htons(port);

	if (0 > bind(sfd, (struct sockaddr *) &saddr, sizeof(saddr))) {
		perror("bind()");
		socket_close(sfd);
		return -1;
	}

	if (listen(sfd, 1) == -1) {
		perror("listen()");
		socket_close(sfd);
		return -1;
	}

	return sfd;
}

int socket_connect(const char *addr, uint16_t port)
{
	int sfd = -1;
	int yes = 1;
	int bufsize = 0x20000;
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	char portstr[8];
	int res;
#ifdef WIN32
	u_long l_yes = 1;
	u_long l_no = 0;
	WSADATA wsa_data;
	if (!wsa_init) {
		if (WSAStartup(MAKEWORD(2,2), &wsa_data) != ERROR_SUCCESS) {
			fprintf(stderr, "WSAStartup failed!\n");
			ExitProcess(-1);
		}
		wsa_init = 1;
	}
#else
	int flags = 0;
#endif

	if (!addr) {
		errno = EINVAL;
		return -1;
	}

	memset(&hints, '\0', sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = IPPROTO_TCP;

	sprintf(portstr, "%d", port);

	res = getaddrinfo(addr, portstr, &hints, &result);
	if (res != 0) {
		fprintf(stderr, "%s: getaddrinfo: %s\n", __func__, gai_strerror(res));
		return -1;
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sfd == -1) {
			continue;
		}

		if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (void*)&yes, sizeof(int)) == -1) {
			perror("setsockopt()");
			socket_close(sfd);
			continue;
		}

#ifdef WIN32
		ioctlsocket(sfd, FIONBIO, &l_yes);
#else
		fcntl(sfd, F_SETFL, O_NONBLOCK);
#endif

		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) {
			break;
		}
#ifdef WIN32
		if (WSAGetLastError() == WSAEWOULDBLOCK)
#else
		if (errno == EINPROGRESS)
#endif
		{
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(sfd, &fds);

			struct timeval timeout;
			timeout.tv_sec = CONNECT_TIMEOUT / 1000;
			timeout.tv_usec = (CONNECT_TIMEOUT - (timeout.tv_sec * 1000)) * 1000;
			if (select(sfd + 1, NULL, &fds, NULL, &timeout) == 1) {
				int so_error;
				socklen_t len = sizeof(so_error);
				getsockopt(sfd, SOL_SOCKET, SO_ERROR, (void*)&so_error, &len);
				if (so_error == 0) {
					break;
				}
			}
		}
		socket_close(sfd);
	}

	freeaddrinfo(result);

	if (rp == NULL) {
		if (verbose >= 2)
			fprintf(stderr, "%s: Could not connect to %s:%d\n", __func__, addr, port);
		return -1;
	}

#ifdef WIN32
	ioctlsocket(sfd, FIONBIO, &l_no);
#else
	flags = fcntl(sfd, F_GETFL, 0);
	fcntl(sfd, F_SETFL, flags & (~O_NONBLOCK));
#endif

#ifdef SO_NOSIGPIPE
	if (setsockopt(sfd, SOL_SOCKET, SO_NOSIGPIPE, (void*)&yes, sizeof(int)) == -1) {
		perror("setsockopt()");
		socket_close(sfd);
		return -1;
	}
#endif

	if (setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, (void*)&yes, sizeof(int)) == -1) {
		perror("Could not set TCP_NODELAY on socket");
	}

	if (setsockopt(sfd, SOL_SOCKET, SO_SNDBUF, (void*)&bufsize, sizeof(int)) == -1) {
		perror("Could not set send buffer for socket");
	}

	if (setsockopt(sfd, SOL_SOCKET, SO_RCVBUF, (void*)&bufsize, sizeof(int)) == -1) {
		perror("Could not set receive buffer for socket");
	}

	return sfd;
}

int socket_check_fd(int fd, fd_mode fdm, unsigned int timeout)
{
	fd_set fds;
	int sret;
	int eagain;
	struct timeval to;
	struct timeval *pto;

	if (fd < 0) {
		if (verbose >= 2)
			fprintf(stderr, "ERROR: invalid fd in check_fd %d\n", fd);
		return -1;
	}

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	sret = -1;

	do {
		if (timeout > 0) {
			to.tv_sec = (time_t) (timeout / 1000);
			to.tv_usec = (time_t) ((timeout - (to.tv_sec * 1000)) * 1000);
			pto = &to;
		} else {
			pto = NULL;
		}
		eagain = 0;
		switch (fdm) {
		case FDM_READ:
			sret = select(fd + 1, &fds, NULL, NULL, pto);
			break;
		case FDM_WRITE:
			sret = select(fd + 1, NULL, &fds, NULL, pto);
			break;
		case FDM_EXCEPT:
			sret = select(fd + 1, NULL, NULL, &fds, pto);
			break;
		default:
			return -1;
		}

		if (sret < 0) {
			switch (errno) {
			case EINTR:
				// interrupt signal in select
				if (verbose >= 2)
					fprintf(stderr, "%s: EINTR\n", __func__);
				eagain = 1;
				break;
			case EAGAIN:
				if (verbose >= 2)
					fprintf(stderr, "%s: EAGAIN\n", __func__);
				break;
			default:
				if (verbose >= 2)
					fprintf(stderr, "%s: select failed: %s\n", __func__,
							strerror(errno));
				return -1;
			}
		} else if (sret == 0) {
			if (verbose >= 2)
				fprintf(stderr, "%s: timeout\n", __func__);
			return -ETIMEDOUT;
		}
	} while (eagain);

	return sret;
}

int socket_accept(int fd, uint16_t port)
{
#ifdef WIN32
	int addr_len;
#else
	socklen_t addr_len;
#endif
	int result;
	struct sockaddr_in addr;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(port);

	addr_len = sizeof(addr);
	result = accept(fd, (struct sockaddr*)&addr, &addr_len);

	return result;
}

int socket_shutdown(int fd, int how)
{
	return shutdown(fd, how);
}

int socket_close(int fd) {
#ifdef WIN32
	return closesocket(fd);
#else
	return close(fd);
#endif
}

int socket_receive(int fd, void *data, size_t length)
{
	return socket_receive_timeout(fd, data, length, 0, RECV_TIMEOUT);
}

int socket_peek(int fd, void *data, size_t length)
{
	return socket_receive_timeout(fd, data, length, MSG_PEEK, RECV_TIMEOUT);
}

int socket_receive_timeout(int fd, void *data, size_t length, int flags,
					 unsigned int timeout)
{
	int res;
	int result;

	// check if data is available
	res = socket_check_fd(fd, FDM_READ, timeout);
	if (res <= 0) {
		return res;
	}
	// if we get here, there _is_ data available
	result = recv(fd, data, length, flags);
	if (res > 0 && result == 0) {
		// but this is an error condition
		if (verbose >= 3)
			fprintf(stderr, "%s: fd=%d recv returned 0\n", __func__, fd);
		return -ECONNRESET;
	}
	if (result < 0) {
		return -errno;
	}
	return result;
}

int socket_send(int fd, void *data, size_t length)
{
	int flags = 0;
#ifdef MSG_NOSIGNAL
	flags |= MSG_NOSIGNAL;
#endif
	return send(fd, data, length, flags);
}
