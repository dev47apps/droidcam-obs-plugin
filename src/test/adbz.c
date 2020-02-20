#include "stdio.h"
int main(int x, char** y) {
	printf("List of devices attached\n");
	printf("\n");
	printf("10a3a5185d8ac3b1       device_usb:337641472X_product:occam_model:Nexus_4 device:mako transport_id:1\n");
	printf("20a3a5185d8ac3b100a3a5185d8ac300a3a5185d8ac3b100a3a5185d8ac3b1 zdevice\n");
	printf("30a3a5185d8ac3b100a3a5185d8ac3b0a3a5185d8ac3b100a3a5185d8ac3b1 xdevice_usb:337641472X_product:occam_model:Nexus_4_device:mako transport_id:1");
	printf("garbage\n");
	printf("  garbage\n");
	printf("garbage  \n");
}