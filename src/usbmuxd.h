#ifndef USBMUXD_H
#define USBMUXD_H
#include "stdint.h"
#ifdef __cplusplus
extern "C" {
#endif

/** Device lookup options for usbmuxd_get_device. */
enum usbmux_lookup_options {
	DEVICE_LOOKUP_USBMUX = 1 << 1, /**< include USBMUX devices during lookup */
	DEVICE_LOOKUP_NETWORK = 1 << 2, /**< include network devices during lookup */
	DEVICE_LOOKUP_PREFER_NETWORK = 1 << 3 /**< prefer network connection if device is available via USBMUX *and* network */
};

/** Type of connection a device is available on */
enum usbmux_connection_type {
	CONNECTION_TYPE_USB = 1,
	CONNECTION_TYPE_NETWORK
};

/**
 * Device information structure holding data to identify the device.
 * The relevant 'handle' should be passed to 'usbmuxd_connect()', to
 * start a proxy connection.  The value 'handle' should be considered
 * opaque and no presumption made about the meaning of its value.
 */
typedef struct {
	uint32_t handle;
	uint32_t product_id;
	char udid[44];
	enum usbmux_connection_type conn_type;
	char conn_data[200];
} usbmuxd_device_info_t;

/**
 * event types for event callback function
 */
enum usbmuxd_event_type {
    UE_DEVICE_ADD = 1,
    UE_DEVICE_REMOVE,
    UE_DEVICE_PAIRED
};

/**
 * Event structure that will be passed to the callback function.
 * 'event' will contains the type of the event, and 'device' will contains
 * information about the device.
 */
typedef struct {
    int event;
    usbmuxd_device_info_t device;
} usbmuxd_event_t;

/**
 * Contacts usbmuxd and retrieves a list of connected devices.
 *
 * @param device_list A pointer to an array of usbmuxd_device_info_t
 *      that will hold records of the connected devices. The last record
 *      is a null-terminated record with all fields set to 0/NULL.
 * @note The user has to free the list returned.
 *
 * @return number of attached devices, zero on no devices, or negative
 *   if an error occured.
 */
#ifdef _WIN32
typedef int (*usbmuxd_get_device_list_t)(usbmuxd_device_info_t **);
#else
int usbmuxd_get_device_list(usbmuxd_device_info_t **device_list);
#endif

/**
 * Frees the device list returned by an usbmuxd_get_device_list call
 *
 * @param device_list A pointer to an array of usbmuxd_device_info_t to free.
 *
 * @return 0 on success, -1 on error.
 */
#ifdef _WIN32
typedef int (*usbmuxd_device_list_free_t)(usbmuxd_device_info_t **);
#else
int usbmuxd_device_list_free(usbmuxd_device_info_t **device_list);
#endif

/**
 * Request proxy connection to the specified device and port.
 *
 * @param handle returned in the usbmux_device_info_t structure via
 *      usbmuxd_get_device() or usbmuxd_get_device_list().
 *
 * @param tcp_port TCP port number on device, in range 0-65535.
 *	common values are 62078 for lockdown, and 22 for SSH.
 *
 * @return socket file descriptor of the connection, or a negative errno
 *    value on error.
 */
#ifdef _WIN32
typedef int (*usbmuxd_connect_t)(const uint32_t, const unsigned short);
#else
int usbmuxd_connect(const uint32_t handle, const unsigned short tcp_port);
#endif

/**
 * Disconnect. For now, this just closes the socket file descriptor.
 *
 * @param sfd socket file descriptor returned by usbmuxd_connect()
 *
 * @return 0 on success, -1 on error.
 */
#ifdef _WIN32
typedef int (*usbmuxd_disconnect_t)(int);
#else
int usbmuxd_disconnect(int sfd);
#endif

/**
 * Send data to the specified socket.
 *
 * @param sfd socket file descriptor returned by usbmuxd_connect()
 * @param data buffer to send
 * @param len size of buffer to send
 * @param sent_bytes how many bytes sent
 *
 * @return 0 on success, a negative errno value otherwise.
 */
#ifdef _WIN32
typedef int (*usbmuxd_send_t)(int);
#else
int usbmuxd_send(int sfd, const char *data, uint32_t len, uint32_t *sent_bytes);
#endif

/**
 * Receive data from the specified socket.
 *
 * @param sfd socket file descriptor returned by usbmuxd_connect()
 * @param data buffer to put the data to
 * @param len number of bytes to receive
 * @param recv_bytes number of bytes received
 * @param timeout how many milliseconds to wait for data
 *
 * @return 0 on success, a negative errno value otherwise.
 */
//int usbmuxd_recv_timeout(int sfd, char *data, uint32_t len, uint32_t *recv_bytes, unsigned int timeout);

/**
 * Receive data from the specified socket with a default timeout.
 *
 * @param sfd socket file descriptor returned by usbmuxd_connect()
 * @param data buffer to put the data to
 * @param len number of bytes to receive
 * @param recv_bytes number of bytes received
 *
 * @return 0 on success, a negative errno value otherwise.
 */
//int usbmuxd_recv(int sfd, char *data, uint32_t len, uint32_t *recv_bytes);

/**
 * Reads the SystemBUID
 *
 * @param buid pointer to a variable that will be set to point to a newly
 *     allocated string with the System BUID returned by usbmuxd
 *
 * @return 0 on success, a negative errno value otherwise.
 */
//int usbmuxd_read_buid(char** buid);

#ifdef _WIN32
typedef void (*libusbmuxd_set_debug_level_t) (int);
#else
void libusbmuxd_set_debug_level(int level);
#endif

#ifdef __cplusplus
}
#endif

#endif /* USBMUXD_H */
