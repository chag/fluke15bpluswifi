
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
#define HOLDBTN		(1<<13)
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

static ETSTimer ipHzTimer;
static ETSTimer btnTimer;

static int ticksa, ticksb;

//Timer interrupt; toggles an io pin to do pwm
static void timerInt(void *arg) {
	static int i=0;
	if (i) {
		RTC_REG_WRITE(FRC1_LOAD_ADDRESS, ticksa & TIMER1_COUNT_MASK);
		gpio_output_set(0, HZIND, 0, 0);
	} else {
		RTC_REG_WRITE(FRC1_LOAD_ADDRESS, ticksb & TIMER1_COUNT_MASK);
		gpio_output_set(HZIND, 0, 0, 0);
	}
	i=!i;
}

//Do PWM at 312Hz. For some reason, the multimeter acts most stable around this frequency.
#define TICKSTOT 256000

//Generate a PWM signal (using timerInt) with a duty cycle of dc/1000
void ioGenSignal(int dc) {
	if (dc==0) {
		TM1_EDGE_INT_DISABLE();
		ETS_FRC1_INTR_DISABLE();
	} else {
		ticksa=(TICKSTOT*dc)/1000;
		ticksb=TICKSTOT-ticksa;
		RTC_REG_WRITE(FRC1_LOAD_ADDRESS, ticksa & TIMER1_COUNT_MASK);
		RTC_REG_WRITE(FRC1_CTRL_ADDRESS, TIMER1_AUTO_LOAD | TIMER1_ENABLE_TIMER);
		RTC_CLR_REG_MASK(FRC1_INT_ADDRESS, FRC1_INT_CLR_MASK);
		ETS_FRC_TIMER1_INTR_ATTACH(timerInt, NULL);
		TM1_EDGE_INT_ENABLE();
		ETS_FRC1_INTR_ENABLE();
	}
}

//This routine abuses the pin that controls the Hz/% pin to switch to duty cycle mode
//and show the IP as four separate numbers, then switches back.

static uint32_t ipToShow;

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
		ioGenSignal((ipToShow>>0)&0xff);
		os_timer_arm(&ipHzTimer, DIGITTIME, 0);
	} else if (state==6) {
		ioGenSignal((ipToShow>>8)&0xff);
		os_timer_arm(&ipHzTimer, DIGITTIME, 0);
	} else if (state==7) {
		ioGenSignal((ipToShow>>16)&0xff);
		os_timer_arm(&ipHzTimer, DIGITTIME, 0);
	} else if (state==8) {
		ioGenSignal((ipToShow>>24)&0xff);
		os_timer_arm(&ipHzTimer, DIGITTIME, 0);
	} else if (state==9) {
		ioGenSignal(0);
		gpio_output_set(HZREL, 0, 0, 0);
		os_timer_arm(&ipHzTimer, 200, 0);
	} else if (state==10) {
		gpio_output_set(0, HZREL, 0, 0);
		state=0;
	}
}

//Kicks off the above routine.
void ioShowIp(uint32_t ip) {
	ipToShow=ip;
	os_timer_disarm(&ipHzTimer);
	os_timer_setfn(&ipHzTimer, ipHzTimerCb, NULL);
	os_timer_arm(&ipHzTimer, 100, 0);
}


//Return the status of the hold button.
int ICACHE_FLASH_ATTR ioGetButton() {
	//Hold-button pulls down the gate of the mosfet
	//Mosfet releases GPIO, which goes high.
	if (gpio_input_get()&HOLDBTN) return 1; else return 0;
}


void ICACHE_FLASH_ATTR btnTimerCb(void *arg) {
	gpio_output_set(0, HZREL, 0, 0);
}

//Actually only accepts the Hz/% btn for now.
void ioPressBtn(int btn) {
	gpio_output_set(HZREL, 0, 0, 0);
	
	os_timer_disarm(&btnTimer);
	os_timer_setfn(&btnTimer, btnTimerCb, NULL);
	os_timer_arm(&btnTimer, 200, 0);
}

void ioInit() {
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13);
	gpio_output_set(PWRON|HOLDBTN, HZREL|HZIND, PWRON|HZREL|HZIND, HOLDBTN);
}

