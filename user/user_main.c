/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */


#include <esp8266.h>
#include "httpd.h"
#include "io.h"
#include "httpdespfs.h"
#include "cgiwifi.h"
#include "cgiflash.h"
#include "auth.h"
#include "espfs.h"
#include "captdns.h"
#include "webpages-espfs.h"
#include "cgiwebsocket.h"
#include "mm.h"
#include "telnetif.h"

static ETSTimer websockTimer;

static char sendBuf[2048]={0};
static int sendBufPos=0;

static void ICACHE_FLASH_ATTR stdoutPutcharWs(char c) {
	if (c=='\n') {
		stdoutPutcharWs('<');
		stdoutPutcharWs('B');
		stdoutPutcharWs('R');
		stdoutPutcharWs('>');
		return;
	}
	sendBuf[sendBufPos++]=c;
	if (sendBufPos>=sizeof(sendBuf)) sendBufPos=sizeof(sendBuf)-1;
}

//Broadcast the uptime in seconds every second over connected websockets
static void ICACHE_FLASH_ATTR websockTimerCb(void *arg) {
	if (sendBufPos!=0) cgiWebsockBroadcast("/websocket/ws.cgi", sendBuf, sendBufPos, WEBSOCK_FLAG_NONE);
	sendBufPos=0;
}

void mmWsRecv(Websock *ws, char *data, int len, int flags) {
	if (strcmp(data, "hz")==0) ioPressBtn(0);
}

//On reception of a message, send "You sent: " plus whatever the other side sent
void myWebsocketRecv(Websock *ws, char *data, int len, int flags) {
	int i;
	char buff[128];
	os_sprintf(buff, "You sent: ");
	for (i=0; i<len; i++) buff[i+10]=data[i];
	buff[i+10]=0;
	cgiWebsocketSend(ws, buff, os_strlen(buff), WEBSOCK_FLAG_NONE);
}

//Websocket connected. Install reception handler and send welcome message.
void myWebsocketConnect(Websock *ws) {
	ws->recvCb=myWebsocketRecv;
	cgiWebsocketSend(ws, "Hi, Websocket!", 14, WEBSOCK_FLAG_NONE);
}


//Multimeter websocket connected.
void mmWsConnect(Websock *ws) {
	ws->recvCb=mmWsRecv;
}


#ifdef ESPFS_POS
CgiUploadFlashDef uploadParams={
	.type=CGIFLASH_TYPE_ESPFS,
	.fw1Pos=ESPFS_POS,
	.fw2Pos=0,
	.fwSize=ESPFS_SIZE,
};
#define INCLUDE_FLASH_FNS
#endif
#ifdef OTA_FLASH_SIZE_K
CgiUploadFlashDef uploadParams={
	.type=CGIFLASH_TYPE_FW,
	.fw1Pos=0x1000,
	.fw2Pos=((OTA_FLASH_SIZE_K*1024)/2)+0x1000,
	.fwSize=((OTA_FLASH_SIZE_K*1024)/2)-0x1000,
	.tagName=OTA_TAGNAME
};
#define INCLUDE_FLASH_FNS
#endif

/*
This is the main url->function dispatching data struct.
In short, it's a struct with various URLs plus their handlers. The handlers can
be 'standard' CGI functions you wrote, or 'special' CGIs requiring an argument.
They can also be auth-functions. An asterisk will match any url starting with
everything before the asterisks; "*" matches everything. The list will be
handled top-down, so make sure to put more specific rules above the more
general ones. Authorization things (like authBasic) act as a 'barrier' and
should be placed above the URLs they protect.
*/
HttpdBuiltInUrl builtInUrls[]={
	{"*", cgiRedirectApClientToHostname, "esp8266.nonet"},
	{"/", cgiRedirect, "/index.html"},
	{"/mmws.cgi", cgiWebsocket, mmWsConnect},

#ifdef INCLUDE_FLASH_FNS
	{"/flash/next", cgiGetFirmwareNext, &uploadParams},
	{"/flash/upload", cgiUploadFirmware, &uploadParams},
#endif
	{"/flash/reboot", cgiRebootFirmware, NULL},

	//Routines to make the /wifi URL and everything beneath it work.

//Enable the line below to protect the WiFi configuration with an username/password combo.
//	{"/wifi/*", authBasic, myPassFn},

	{"/wifi", cgiRedirect, "/wifi/wifi.tpl"},
	{"/wifi/", cgiRedirect, "/wifi/wifi.tpl"},
	{"/wifi/wifiscan.cgi", cgiWiFiScan, NULL},
	{"/wifi/wifi.tpl", cgiEspFsTemplate, tplWlan},
	{"/wifi/connect.cgi", cgiWiFiConnect, NULL},
	{"/wifi/connstatus.cgi", cgiWiFiConnStatus, NULL},
	{"/wifi/setmode.cgi", cgiWiFiSetMode, NULL},

	{"/websocket/ws.cgi", cgiWebsocket, myWebsocketConnect},

	{"*", cgiEspFsHook, NULL}, //Catch-all cgi function for the filesystem
	{NULL, NULL, NULL}
};


void mmData(int value, int decPtPos, int unit) {
	const char mls[]=" fpnumKMGT";
	const char *units[]={"ohm","V","A","F","C","X","H","Hz", "%"};
	int x;
	char buf[12];
	char json[128];
	int p=11;
	int val=value;
	if (val<0) val=-val;
	buf[p--]=0;
	for (x=0; x<10; x++) {
		if (x>decPtPos && val==0) break;
		if (decPtPos==x && x!=0) buf[p--]='.';
		buf[p--]='0'+val%10;
		val/=10;
	}
	if (value<0) buf[p--]='-';
	//Abuse json buffer for normal stuff
	sprintf(json, "%s %c%s%s\n", &buf[p+1], mls[unit>>8], units[unit&127], (unit&MM_U_FL_AC)?" AC":"");
	telnetBcast(json);

	//Populate json buffer
	x=sprintf(json, "{ \"value\": \"%s\", \"ml\": \"%c\", \"unit\": \"%s\", \"acdc\": \"%s\" }",
		&buf[p+1], mls[unit>>8], units[unit&127], (unit&MM_U_FL_AC)?"AC":"DC");
	
	cgiWebsockBroadcast("/mmws.cgi", json, x, WEBSOCK_FLAG_NONE);
}

//Main routine. Initialize stdout, the I/O, filesystem and the webserver and we're done.
void user_init(void) {
	mmInit(mmData);
	//Redirect stdout to websocket
	os_install_putc1((void *)stdoutPutcharWs);
	ioInit();
	captdnsInit();
	telnetInit(333);

	// 0x40200000 is the base address for spi flash memory mapping, ESPFS_POS is the position
	// where image is written in flash that is defined in Makefile.
#ifdef ESPFS_POS
	espFsInit((void*)(0x40200000 + ESPFS_POS));
#else
	espFsInit((void*)(webpages_espfs_start));
#endif
	httpdInit(builtInUrls, 80);
	os_timer_disarm(&websockTimer);
	os_timer_setfn(&websockTimer, websockTimerCb, NULL);
	os_timer_arm(&websockTimer, 1000, 1);
	os_printf("\nReady\n");
	wifi_set_sleep_type(LIGHT_SLEEP_T);
}

void user_rf_pre_init() {
	//Not needed, but some SDK versions want this defined.
}
