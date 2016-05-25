#ifndef IO_H
#define IO_H

void ioInit(void);
int ICACHE_FLASH_ATTR ioGetButton();
void ioShowIp(uint32_t ip);

#endif