#ifndef SD_BROWSER_H
#define SD_BROWSER_H

#include <stdint.h>

void SD_Browser_Init(void);
void SD_Browser_Poll(void);
/* A, B, Select, Start, Up, Down, Left, Right in bits 0 through 7. */
uint8_t SD_Browser_NesPad(void);

#endif
