/**
 * @file libimobiledevice/libimobiledevice.h
 * @brief Device/Connection handling and communication
 * \internal
 *
 * Copyright (c) 2010-2019 Nikias Bassen All Rights Reserved.
 * Copyright (c) 2010-2014 Martin Szulecki All Rights Reserved.
 * Copyright (c) 2014 Christophe Fergeau All Rights Reserved.
 * Copyright (c) 2008 Jonathan Beck All Rights Reserved.
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

#ifndef IMOBILEDEVICE_H
#define IMOBILEDEVICE_H

// Visual C++ requires the declaration of the methods to be exactly the same in
// the public headers and the source files; otherwise it throws C2375.
// Other compilers don't care.
// In the .c files, we use the LIBIMOBILEDEVICE_API definition which depends on 
// other definitions from config.h. We can't reference config.h from public headers.
// See https://gcc.gnu.org/wiki/Visibility for some details on this syntax
#ifdef _MSC_VER
#define LIBIMOBILEDEVICE_API_MSC __declspec( dllexport )
#else 
#define LIBIMOBILEDEVICE_API_MSC __attribute__ ((visibility ("default"))) 
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <plist/plist.h>

/** Error Codes */
typedef enum {
	IDEVICE_E_SUCCESS         =  0,
	IDEVICE_E_INVALID_ARG     = -1,
	IDEVICE_E_UNKNOWN_ERROR   = -2,
	IDEVICE_E_NO_DEVICE       = -3,
	IDEVICE_E_NOT_ENOUGH_DATA = -4,
	IDEVICE_E_SSL_ERROR       = -6,
	IDEVICE_E_TIMEOUT         = -7
} idevice_error_t;

typedef struct idevice_private idevice_private;
typedef idevice_private *idevice_t; /**< The device handle. */

typedef struct idevice_connection_private idevice_connection_private;
typedef idevice_connection_private *idevice_connection_t; /**< The connection handle. */

/** Options for idevice_new_with_options() */
enum idevice_options {
	IDEVICE_LOOKUP_USBMUX = 1 << 1,  /**< include USBMUX devices during lookup */
	IDEVICE_LOOKUP_NETWORK = 1 << 2, /**< include network devices during lookup */
	IDEVICE_LOOKUP_PREFER_NETWORK = 1 << 3 /**< prefer network connection if device is available via USBMUX *and* network */
};

/** Type of connection a device is available on */
enum idevice_connection_type {
	CONNECTION_USBMUXD = 1,
	CONNECTION_NETWORK
};

struct idevice_info {
	char *udid;
	enum idevice_connection_type conn_type;
	void* conn_data;
};
typedef struct idevice_info* idevice_info_t;

/* discovery (events/asynchronous) */
/** The event type for device add or removal */
enum idevice_event_type {
	IDEVICE_DEVICE_ADD = 1,
	IDEVICE_DEVICE_REMOVE,
	IDEVICE_DEVICE_PAIRED
};

/**
* specifies how libusbmuxd should connect to usbmuxd
*/
enum idevice_socket_type {
	// Use UNIX sockets. The default on Linux and macOS.
	IDEVICE_SOCKET_TYPE_UNIX = 1,

	// Use TCP sockets. The default and only option on Windows.
	IDEVICE_SOCKET_TYPE_TCP = 2
};

/* event data structure */
/** Provides information about the occurred event. */
typedef struct {
	enum idevice_event_type event; /**< The event type. */
	const char *udid; /**< The device unique id. */
	enum idevice_connection_type conn_type; /**< The connection type. */
} idevice_event_t;

/* event callback function prototype */
/** Callback to notifiy if a device was added or removed. */
typedef void (*idevice_event_cb_t) (const idevice_event_t *event, void *user_data);

/** Enables calling applications to capture debug messages from libimobiledevice */
typedef void(*idevice_debug_cb_t) (char *message);

/* functions */

/**
  * Sets the callback to invoke when writing out debug messages. If this callback is set, messages
  * will be written to this callback instead of the standard output.
  *
  * @param callback The callback which will receive the debug messages. Set to NULL to redirect to stdout.
  */
LIBIMOBILEDEVICE_API_MSC void idevice_set_debug_callback(idevice_debug_cb_t callback);

/**
 * Set the level of debugging.
 *
 * @param level Set to 0 for no debug output, 1 to enable basic debug output and 2 to enable full debug output.
 * When set to 2, the values of buffers being sent across the wire are printed out as well, this results in very
 * verbose output.
 */
LIBIMOBILEDEVICE_API_MSC void idevice_set_debug_level(int level);

/**
 * Register a callback function that will be called when device add/remove
 * events occur.
 *
 * @param callback Callback function to call.
 * @param user_data Application-specific data passed as parameter
 *   to the registered callback function.
 *
 * @return IDEVICE_E_SUCCESS on success or an error value when an error occurred.
 */
LIBIMOBILEDEVICE_API_MSC idevice_error_t idevice_event_subscribe(idevice_event_cb_t callback, void *user_data);

/**
 * Release the event callback function that has been registered with
 *  idevice_event_subscribe().
 *
 * @return IDEVICE_E_SUCCESS on success or an error value when an error occurred.
 */
LIBIMOBILEDEVICE_API_MSC idevice_error_t idevice_event_unsubscribe(void);

/* discovery (synchronous) */

/**
 * Get a list of UDIDs of currently available devices (USBMUX devices only).
 *
 * @param devices List of UDIDs of devices that are currently available.
 *   This list is terminated by a NULL pointer.
 * @param count Number of devices found.
 *
 * @return IDEVICE_E_SUCCESS on success or an error value when an error occurred.
 *
 * @note This function only returns the UDIDs of USBMUX devices. To also include
 *   network devices in the list, use idevice_get_device_list_extended().
 * @see idevice_get_device_list_extended
 */
LIBIMOBILEDEVICE_API_MSC idevice_error_t idevice_get_device_list(char ***devices, int *count);

/**
 * Free a list of device UDIDs.
 *
 * @param devices List of UDIDs to free.
 *
 * @return Always returnes IDEVICE_E_SUCCESS.
 */
LIBIMOBILEDEVICE_API_MSC idevice_error_t idevice_device_list_free(char **devices);

/**
 * Get a list of currently available devices
 *
 * @param devices List of idevice_info_t records with device information.
 *   This list is terminated by a NULL pointer.
 * @param count Number of devices included in the list.
 *
 * @return IDEVICE_E_SUCCESS on success or an error value when an error occurred.
 */
LIBIMOBILEDEVICE_API_MSC idevice_error_t idevice_get_device_list_extended(idevice_info_t **devices, int *count);

/**
 * Free an extended device list retrieved through idevice_get_device_list_extended().
 *
 * @param devices Device list to free.
 *
 * @return IDEVICE_E_SUCCESS on success or an error value when an error occurred.
 */
LIBIMOBILEDEVICE_API_MSC idevice_error_t idevice_device_list_extended_free(idevice_info_t *devices);

/* device structure creation and destruction */

/**
 * Creates an idevice_t structure for the device specified by UDID,
 *  if the device is available (USBMUX devices only).
 *
 * @note The resulting idevice_t structure has to be freed with
 * idevice_free() if it is no longer used.
 * If you need to connect to a device available via network, use
 * idevice_new_with_options() and include IDEVICE_LOOKUP_NETWORK in options.
 *
 * @see idevice_new_with_options
 *
 * @param device Upon calling this function, a pointer to a location of type
 *  idevice_t. On successful return, this location will be populated.
 * @param udid The UDID to match.
 *
 * @return IDEVICE_E_SUCCESS if ok, otherwise an error code.
 */
LIBIMOBILEDEVICE_API_MSC idevice_error_t idevice_new(idevice_t *device, const char *udid);

/**
 * Creates an idevice_t structure for the device specified by UDID,
 *  if the device is available, with the given lookup options.
 *
 * @note The resulting idevice_t structure has to be freed with
 * idevice_free() if it is no longer used.
 *
 * @param device Upon calling this function, a pointer to a location of type
 *   idevice_t. On successful return, this location will be populated.
 * @param udid The UDID to match.
 * @param options Specifies what connection types should be considered
 *   when looking up devices. Accepts bitwise or'ed values of idevice_options.
 *   If 0 (no option) is specified it will default to IDEVICE_LOOKUP_USBMUX.
 *   To lookup both USB and network-connected devices, pass
 *   IDEVICE_LOOKUP_USBMUX | IDEVICE_LOOKUP_NETWORK. If a device is available
 *   both via USBMUX *and* network, it will select the USB connection.
 *   This behavior can be changed by adding IDEVICE_LOOKUP_PREFER_NETWORK
 *   to the options in which case it will select the network connection.
 *
 * @return IDEVICE_E_SUCCESS if ok, otherwise an error code.
 */
LIBIMOBILEDEVICE_API_MSC idevice_error_t idevice_new_with_options(idevice_t *device, const char *udid, enum idevice_options options);

/**
 * Cleans up an idevice structure, then frees the structure itself.
 *
 * @param device idevice_t to free.
 */
LIBIMOBILEDEVICE_API_MSC idevice_error_t idevice_free(idevice_t device);

/* connection/disconnection */

/**
 * Set up a connection to the given device.
 *
 * @param device The device to connect to.
 * @param port The destination port to connect to.
 * @param connection Pointer to an idevice_connection_t that will be filled
 *   with the necessary data of the connection.
 *
 * @return IDEVICE_E_SUCCESS if ok, otherwise an error code.
 */
LIBIMOBILEDEVICE_API_MSC idevice_error_t idevice_connect(idevice_t device, uint16_t port, idevice_connection_t *connection);

/**
 * Disconnect from the device and clean up the connection structure.
 *
 * @param connection The connection to close.
 *
 * @return IDEVICE_E_SUCCESS if ok, otherwise an error code.
 */
LIBIMOBILEDEVICE_API_MSC idevice_error_t idevice_disconnect(idevice_connection_t connection);

/* communication */

/**
 * Send data to a device via the given connection.
 *
 * @param connection The connection to send data over.
 * @param data Buffer with data to send.
 * @param len Size of the buffer to send.
 * @param sent_bytes Pointer to an uint32_t that will be filled
 *   with the number of bytes actually sent.
 *
 * @return IDEVICE_E_SUCCESS if ok, otherwise an error code.
 */
LIBIMOBILEDEVICE_API_MSC idevice_error_t idevice_connection_send(idevice_connection_t connection, const char *data, uint32_t len, uint32_t *sent_bytes);

/**
 * Receive data from a device via the given connection.
 * This function will return after the given timeout even if no data has been
 * received.
 *
 * @param connection The connection to receive data from.
 * @param data Buffer that will be filled with the received data.
 *   This buffer has to be large enough to hold len bytes.
 * @param len Buffer size or number of bytes to receive.
 * @param recv_bytes Number of bytes actually received.
 * @param timeout Timeout in milliseconds after which this function should
 *   return even if no data has been received.
 *
 * @return IDEVICE_E_SUCCESS if ok, otherwise an error code.
 */
LIBIMOBILEDEVICE_API_MSC idevice_error_t idevice_connection_receive_timeout(idevice_connection_t connection, char *data, uint32_t len, uint32_t *recv_bytes, unsigned int timeout);

/**
 * Receive data from a device via the given connection.
 * This function is like idevice_connection_receive_timeout, but with a
 * predefined reasonable timeout.
 *
 * @param connection The connection to receive data from.
 * @param data Buffer that will be filled with the received data.
 *   This buffer has to be large enough to hold len bytes.
 * @param len Buffer size or number of bytes to receive.
 * @param recv_bytes Number of bytes actually received.
 *
 * @return IDEVICE_E_SUCCESS if ok, otherwise an error code.
 */
LIBIMOBILEDEVICE_API_MSC idevice_error_t idevice_connection_receive(idevice_connection_t connection, char *data, uint32_t len, uint32_t *recv_bytes);

/**
 * Enables SSL for the given connection.
 *
 * @param connection The connection to enable SSL for.
 *
 * @return IDEVICE_E_SUCCESS on success, IDEVICE_E_INVALID_ARG when connection
 *     is NULL or connection->ssl_data is non-NULL, or IDEVICE_E_SSL_ERROR when
 *     SSL initialization, setup, or handshake fails.
 */
LIBIMOBILEDEVICE_API_MSC idevice_error_t idevice_connection_enable_ssl(idevice_connection_t connection);

/**
 * Disable SSL for the given connection.
 *
 * @param connection The connection to disable SSL for.
 *
 * @return IDEVICE_E_SUCCESS on success, IDEVICE_E_INVALID_ARG when connection
 *     is NULL. This function also returns IDEVICE_E_SUCCESS when SSL is not
 *     enabled and does no further error checking on cleanup.
 */
LIBIMOBILEDEVICE_API_MSC idevice_error_t idevice_connection_disable_ssl(idevice_connection_t connection);

/**
 * Get the underlying file descriptor for a connection
 *
 * @param connection The connection to get fd of
 * @param fd Pointer to an int where the fd is stored
 *
 * @return IDEVICE_E_SUCCESS if ok, otherwise an error code.
 */
LIBIMOBILEDEVICE_API_MSC idevice_error_t idevice_connection_get_fd(idevice_connection_t connection, int *fd);

/* misc */

/**
 * Gets the handle or (usbmux device id) of the device.
 */
LIBIMOBILEDEVICE_API_MSC idevice_error_t idevice_get_handle(idevice_t device, uint32_t *handle);

/**
 * Gets the unique id for the device.
 */
LIBIMOBILEDEVICE_API_MSC idevice_error_t idevice_get_udid(idevice_t device, char **udid);

/**
* Sets the socket type (Unix socket or TCP socket) libimobiledevice should use when connecting
* to usbmuxd.
*
* @param value IDEVICE_SOCKET_TYPE_UNIX or IDEVICE_SOCKET_TYPE_TCP
*
* @return 0 on success or negative on error
*/
LIBIMOBILEDEVICE_API_MSC idevice_error_t idevice_set_socket_type(enum idevice_socket_type value);

/**
* Gets the socket type (Unix socket or TCP socket) libimobiledevice should use when connecting
* to usbmuxd.
*
* @param value A pointer to an integer which will reveive the current socket type
*
* @return 0 on success or negative on error
*/
LIBIMOBILEDEVICE_API_MSC idevice_error_t idevice_get_socket_type(enum idevice_socket_type* value);

/**
* Sets the TCP endpoint to which libimobiledevice will connect if the socket type is set to
* SOCKET_TYPE_TCP
*
* @param host The hostname or IP address to which to connect
* @param port The port to which to connect.
*
* @return 0 on success or negative on error
*/
LIBIMOBILEDEVICE_API_MSC idevice_error_t idevice_set_tcp_endpoint(const char* host, uint16_t port);

/**
* Gets the TCP endpoint to which libimobiledevice will connect if the socket type is set to
* SOCKET_TYPE_TCP
*
* @param host A pointer which will be set to the hostname or IP address to which to connect.
*             The caller must free this string.
* @param port The port to which to connect
*
* @return 0 on success or negative on error
*/
LIBIMOBILEDEVICE_API_MSC idevice_error_t idevice_get_tcp_endpoint(char** host, uint16_t* port);

#ifdef __cplusplus
}
#endif

#endif

