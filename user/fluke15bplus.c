/*
Routines to handle communication with the Fluke15b+/17b+/18b+ multimeters.
*/

#include <esp8266.h>
#include "mm.h"
#include "uart.h"
#include "io.h"


/*
Fluke15B+ (and presumably also 17B+ and 18B+) data:

The serial port is found under the 'Calibration Sticker' in the battery compartment. It's
actually easier to reach by opening the entire meter, but that shows the location.

Relevant connections: WP6 - GND, WP7 - RxD, WP8 - TxD.
Communications happens at 2400,n,8,1, by sending a single byte ascii letter. These are the ones
I have found:
'i' - Turn HV mark on
'q' - Turn HV mark off
'n' - Hold toggle
'd' - Get the current LCD contents as 8 bytes.
More commands are possibly there. WARNING: They may mess with your configuration EEPROM:
I have not tested which commands do that. Best to desolder the I2C EEPROM chip 
(it's an 24c02 or so) and backup it beforehand.

The return code for 'd' is as such:
0  1  2  3  4  5  6  7
I1 I2 D4 D3 D2 D1 I3 I4

For Ix, see the defines down here.

For the digits D4-D3 and D2,D1, the individual bytes are the segments of the 7-segment display
(plus the decimal point):

D4,D3      D2,D1
 444     |  444
5   2    | 5   0
5   2    | 5   0
 111     |  111
6   0    | 6   2
6   0    | 6   2
 333  7  |  333  7

(so bit 0 and 2 are interchanged between the 2x2 digits)

*/
#define I1_AMP		(1<<0)
#define I1_MILLIV	(1<<1)
#define I1_FARAD	(1<<2)
#define I1_HZ		(1<<3)
#define I1_DIODE	(1<<4)
#define I1_PCT		(1<<5)
#define I1_MEGA		(1<<6)
#define I1_KILO		(1<<7)

#define I2_NANOF	(1<<0)
#define I2_MICROF	(1<<1)
#define I2_OHM		(1<<2)
#define I2_VOLT		(1<<4)
#define I2_DC		(1<<5)
#define I2_AC		(1<<6)
#define I2_AUTO		(1<<7)

#define I3_CONT		(1<<0) //continuency test
#define I3_HOLD		(1<<1)
#define I3_MINUS	(1<<3)

#define I4_MILLIA	(1<<0)
#define I4_MICROA	(1<<3)
#define I4_MANUAL	(1<<7)

#define DIGIT_DP 0x80
#define DIGIT_0 0x7D
#define DIGIT_1 0x05
#define DIGIT_2 0x5B
#define DIGIT_3 0x1F
#define DIGIT_4 0x27
#define DIGIT_5 0x3E
#define DIGIT_6 0x7E
#define DIGIT_7 0x15
#define DIGIT_8 0x7F
#define DIGIT_9 0x3F


static ETSTimer mmDispTimer;
static MmDataCb *callback;

static int lcdToDec(int lcd, int digit) {
	const int digits[]={DIGIT_0, DIGIT_1, DIGIT_2, DIGIT_3, DIGIT_4, DIGIT_5, DIGIT_6, 
			DIGIT_7, DIGIT_8, DIGIT_9};
	int x;
	for (x=0; x<10; x++) if (lcd==digits[x]) return x;
	return 0;
}


static void ICACHE_FLASH_ATTR mmDispTimerCb(void *arg) {
	static int initial=1;
	static int shownIp=0;
	static int btnHeld=0;
	int value=0, decPtPos=0, unit=0;
	char pkt[8];
	int b, x=0;
	os_printf("BtnHeld %d %d\n", btnHeld, ioGetButton());
	if (initial) {
		os_timer_disarm(&mmDispTimer);
		os_timer_arm(&mmDispTimer, 200, 1);
//		return;
		if (ioGetButton()) return;			//wait till hold button released
		uartTxd('n');						//kill hold state
		initial=0;
		return;
	}
	while ((b=uartRxd())!=-1) {
		if (x<8) {
			pkt[x]=b;
			x++;
			printf("%02X ", b);
		}
	}
	printf("\n");
	if (x==8) {
		//Got a packet!
		value=lcdToDec(pkt[2]&0x7f, 0);
		value+=lcdToDec(pkt[3]&0x7f, 1)*10;
		value+=lcdToDec(pkt[4]&0x7f, 2)*100;
		value+=lcdToDec(pkt[5]&0x7f, 3)*1000;
		if (pkt[6]&I3_MINUS) value=-value;
		
		if (pkt[2]&0x80) decPtPos=1;
		if (pkt[3]&0x80) decPtPos=2;
		if (pkt[4]&0x80) decPtPos=3;

		if (pkt[0]&I1_MILLIV) unit=MM_U_ML_MILLI;
		if (pkt[0]&I1_MEGA) unit=MM_U_ML_MEGA;
		if (pkt[0]&I1_KILO) unit=MM_U_ML_KILO;
		if (pkt[1]&I2_NANOF) unit=MM_U_ML_NANO;
		if (pkt[1]&I2_MICROF) unit=MM_U_ML_MICRO;
		if (pkt[7]&I4_MILLIA) unit=MM_U_ML_MILLI;
		if (pkt[7]&I4_MICROA) unit=MM_U_ML_MICRO;

		if (pkt[0]&I1_FARAD) unit|=MM_U_FARAD;
		if (pkt[0]&I1_AMP) unit|=MM_U_AMP;
		if (pkt[0]&I1_PCT) unit|=MM_U_PROCENT;
		if (pkt[0]&I1_HZ) unit|=MM_U_HZ;
		if (pkt[1]&I2_OHM) unit|=MM_U_OHM;
		if (pkt[1]&I2_VOLT) unit|=MM_U_VOLT;

		if (pkt[1]&I2_AC) unit|=MM_U_FL_AC;

		callback(value, decPtPos, unit);
	}
	if (x==8 && !shownIp) {
		if (wifi_station_get_connect_status()==STATION_GOT_IP) {
			//Only do this when on amps range.
			if ((pkt[0]&I1_AMP) && !(pkt[7]&I4_MILLIA)) {
					struct ip_info info;
					wifi_get_ip_info(0, &info);
					ioShowIp(info.ip.addr);
			}
			shownIp=1;
		}
	}
	uartTxd('d');

	//Watch for ap reset thing
	if (ioGetButton()) {
		btnHeld++;
		if (btnHeld==3*5) { //>5 sec
			wifi_station_disconnect();
			wifi_set_opmode_current(0x3); //reset to AP+STA mode
			os_printf("Reset to AP+sta mode...\n");
		}
	} else {
		btnHeld=0;
	}
}


void mmInit(MmDataCb *cb) {
	uartInit(2400);
	callback=cb;
	os_timer_disarm(&mmDispTimer);
	os_timer_setfn(&mmDispTimer, mmDispTimerCb, NULL);
	os_timer_arm(&mmDispTimer, 1000, 0);
}

