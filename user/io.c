
/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */


#include <esp8266.h>

#define PWRON		(1<<5)
#define HOLDBTN		(1<<4)
#define HZREL		(1<<14)
#define HZIND		(1<<12)

// definitions for RTC Timer1
#define TIMER1_DIVIDE_BY_1              0x0000
#define TIMER1_DIVIDE_BY_16             0x0004
#define TIMER1_DIVIDE_BY_256            0x0008

#define TIMER1_AUTO_LOAD                0x0040
#define TIMER1_ENABLE_TIMER             0x0080
#define TIMER1_FLAGS_MASK               0x00cc

#define TIMER1_NMI                      0x8000

#define TIMER1_COUNT_MASK               0x007fffff        // 23 bit timer

static ETSTimer resetBtntimer;
static ETSTimer ipHzTimer;

void timerInt(void *arg) {
	static int i=0;
	if (i) {
		gpio_output_set(0, HZIND, 0, 0);
	} else {
		gpio_output_set(HZIND, 0, 0, 0);
	}
	i=!i;
}

//My hw seems to have a slight offset between mm and esp. This corrects for that.
#define OFFSET -2

void ioGenSignal(int hz) {
	if (hz==0) {
		TM1_EDGE_INT_DISABLE();
		ETS_FRC1_INTR_DISABLE();
	} else {
		int ticks=(80000000/(2*hz))-1+OFFSET;
		if (ticks<160) return;
		RTC_REG_WRITE(FRC1_LOAD_ADDRESS, ticks & TIMER1_COUNT_MASK);
		RTC_REG_WRITE(FRC1_CTRL_ADDRESS, TIMER1_AUTO_LOAD | TIMER1_ENABLE_TIMER);
		RTC_CLR_REG_MASK(FRC1_INT_ADDRESS, FRC1_INT_CLR_MASK);
		ETS_FRC_TIMER1_INTR_ATTACH(timerInt, NULL);
		TM1_EDGE_INT_ENABLE();
		ETS_FRC1_INTR_ENABLE();
	}
}

static uint32_t ipToShow;

void sendDigit(int digit) {
	if (digit!=0) {
		ioGenSignal(digit*10+5000);
	} else {
		ioGenSignal(1000);
	}
}

#define DIGITTIME 4000

static void ICACHE_FLASH_ATTR ipHzTimerCb(void *arg) {
	static int state=0;
	os_timer_disarm(&ipHzTimer);
	state++;
	if (state==1) {
		gpio_output_set(HZREL, 0, 0, 0);
		os_timer_arm(&ipHzTimer, 200, 0);
	} else if (state==2) {
		gpio_output_set(0, HZREL, 0, 0);
		os_timer_arm(&ipHzTimer, 400, 0);
	} else if (state==3) {
		gpio_output_set(HZREL, 0, 0, 0);
		os_timer_arm(&ipHzTimer, 200, 0);
	} else if (state==4) {
		gpio_output_set(0, HZREL, 0, 0);
		os_timer_arm(&ipHzTimer, 200, 0);
	} else if (state==5) {
		sendDigit((ipToShow>>0)&0xff);
		os_timer_arm(&ipHzTimer, DIGITTIME, 0);
	} else if (state==6) {
		sendDigit((ipToShow>>8)&0xff);
		os_timer_arm(&ipHzTimer, DIGITTIME, 0);
	} else if (state==7) {
		sendDigit((ipToShow>>16)&0xff);
		os_timer_arm(&ipHzTimer, DIGITTIME, 0);
	} else if (state==8) {
		sendDigit((ipToShow>>24)&0xff);
		os_timer_arm(&ipHzTimer, DIGITTIME, 0);
	} else if (state==9) {
		ioGenSignal(0);
		gpio_output_set(HZREL, 0, 0, 0);
		os_timer_arm(&ipHzTimer, 200, 0);
	} else if (state==10) {
		gpio_output_set(0, HZREL, 0, 0);
	}
}


void ioShowIp(uint32_t ip) {
	ipToShow=ip;
	os_timer_disarm(&ipHzTimer);
	os_timer_setfn(&ipHzTimer, ipHzTimerCb, NULL);
	os_timer_arm(&ipHzTimer, 100, 0);
}


int ICACHE_FLASH_ATTR ioGetButton() {
	if (gpio_input_get()&HOLDBTN) return 1; else return 0;
}



static void ICACHE_FLASH_ATTR resetBtnTimerCb(void *arg) {
/*
	static int resetCnt=0;
	if (!GPIO_INPUT_GET(BTNGPIO)) {
		resetCnt++;
	} else {
		if (resetCnt>=6) { //3 sec pressed
			wifi_station_disconnect();
			wifi_set_opmode(0x3); //reset to AP+STA mode
			os_printf("Reset to AP mode. Restarting system...\n");
			system_restart();
		}
		resetCnt=0;
	}
*/
}

void ioInit() {
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
	gpio_output_set(PWRON|HOLDBTN, HZREL|HZIND, PWRON|HZREL|HZIND, HOLDBTN);
	
	os_timer_disarm(&resetBtntimer);
	os_timer_setfn(&resetBtntimer, resetBtnTimerCb, NULL);
	os_timer_arm(&resetBtntimer, 500, 1);

	os_timer_disarm(&ipHzTimer);
	os_timer_setfn(&ipHzTimer, ipHzTimerCb, NULL);
	os_timer_arm(&ipHzTimer, 10000, 0);
}

