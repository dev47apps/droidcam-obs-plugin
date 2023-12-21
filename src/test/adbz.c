#include "stdio.h"
#include "string.h"
void list_props(void);
void list_devices(void);

int main(int argc, char** argv) {
	if (argc == 2 && strcmp(argv[1], "start-server") == 0) {
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "version") == 0) {
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
	printf("\r\n");
	printf("\n\r");
	printf("\n\n");
	printf("  garbage\n");
	printf("empty1  \n");
	printf("empty2\t\n");
	printf("long1 devicedevicedevicedevicedevicedevicedevicedevicedevicedevicedevicedevice\r\n");
	printf("111a3a5185d8ac device\r\n");
	printf("111a3a5185d8ac offline\r\n");
	printf("111a3a5185d8ac \r\n");
	printf("222a3a5185d8ac device\ttransport_id:1\r\n");
	printf("333a3a5185d8ac\toffline\n");
	printf("long2long2long2long2long2long2long2long2long2long2long2long2long2long2long2long2long2long2long2long2 device\n");
	printf("extra1 \n");
	printf("extra2 \n");
	printf("extra3 \n");
}
