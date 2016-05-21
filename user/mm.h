/*
Generic functions to implement a multimeter interface
*/

#ifndef MM_H
#define MM_H

#define MM_U_OHM		(0<<0)
#define MM_U_VOLT		(1<<0)
#define MM_U_AMP		(2<<0)
#define MM_U_FARAD		(3<<0)
#define MM_U_CELCIUS	(4<<0)
#define MM_U_HFE		(5<<0)
#define MM_U_HENRY		(6<<0)
#define MM_U_HZ			(7<<0)

#define MM_U_FL_AC		(1<<7)

#define MM_U_ML_NONE	(0<<8)
#define MM_U_ML_FEMTO	(1<<8)
#define MM_U_ML_PICO	(2<<8)
#define MM_U_ML_NANO	(3<<8)
#define MM_U_ML_MICRO	(4<<8)
#define MM_U_ML_MILLI	(5<<8)
#define MM_U_ML_KILO	(6<<8)
#define MM_U_ML_MEGA	(7<<8)
#define MM_U_ML_GIGA	(8<<8)
#define MM_U_ML_TERA	(9<<8)


typedef void (MmDataCb)(int value, int decPtPos, int unit);

void mmInit(MmDataCb *b);
void mmShowIp(uint32_t ip);

#endif