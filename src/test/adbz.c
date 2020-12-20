#include "stdio.h"
#include "string.h"
void list_props(void);
void list_devices(void);

int main(int argc, char** argv) {
	if (argc == 2 && strcmp(argv[1], "start-server") == 0) {
		return 0;
	}

	if (argc == 3 && strcmp(argv[1], "reconnect") == 0 && strcmp(argv[2], "offline") == 0) {
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "devices") == 0) {
		list_devices();
		return 0;
	}

	if (argc > 4 && strcmp(argv[3], "shell") == 0) {
		if (strcmp(argv[4], "getprop") == 0) {
			printf("Nexus X\n\n");
			return 0;
		}
	}

	return 1;
}

void list_devices(void) {
	printf("List of devices attached\r\n");
	printf("10a3a5185d8ac3b1       device_usb:337641472X_product:occam_model:Nexus_4 device:mako transport_id:1\n");
	printf("20a3a5185d8ac3b100a3a5185d8ac300a3a5185d8ac3b100a3a5185d8ac3b1 zdevice\n");
	printf("30a3a5185d8ac3b100a3a5185d8ac3b0a3a5185d8ac3b100a3a5185d8ac3b1 xdevice_usb:337641472X_product:occam_model:Nexus_4_device:mako transport_id:1\n");
	printf("100a3a5185d8ac111 device\r\n");
	printf("100a3a5185d8ac222 device\r\n");
	printf("100a3a5185d8ac333 device\n");
	printf("garbage\n");
	printf("  garbage\n");
	printf("garbage  \n");
	printf("\n\n\n");
}
