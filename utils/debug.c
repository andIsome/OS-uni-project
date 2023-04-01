#include "debug.h"

#include <stdio.h>
#include <stdlib.h>



void hexDump_(const char* desc, const void* addr, const size_t len, int perLine) {
	if (perLine < 4 || perLine > 64) perLine = 16;
	int i;
	unsigned char* buff = malloc((size_t)(perLine + 1));
	const unsigned char* pc = (const unsigned char*)addr;
	if (desc != NULL) printf("%s:\n", desc);
	if (len == 0) {
		printf("  ZERO LENGTH\n");
		return;
	}
	for (i = 0; i < len; i++) {
		if ((i % perLine) == 0) {
			if (i != 0) printf("  %s\n", buff);
			printf("  %04x ", i);
		}
		printf(" %02x", pc[i]);
		if ((pc[i] < 0x20) || (pc[i] > 0x7e))
			buff[i % perLine] = '.';
		else
			buff[i % perLine] = pc[i];
		buff[(i % perLine) + 1] = '\0';
	}
	while ((i % perLine) != 0) {
		printf("   ");
		i++;
	}
	printf("  %s\n", buff);
	free(buff);
}