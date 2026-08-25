#ifndef __UTILS_H__
#define __UTILS_H__

char nibbleToChar(short int  byte);
char* stristr( char* str1, const char* str2 );
void debug(const char * format, ...);
void Delay_Micro(uint32_t period);
void delay_us(uint32_t period, void *intf_ptr);

#endif
