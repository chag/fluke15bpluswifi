/*
Telnet interface. Spits out the measurements to anyone connecting.
*/

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */


#include <esp8266.h>

//Listening connection data
static struct espconn telnetLConn;
static esp_tcp telnetTcp;

struct TelnetConn {
	struct espconn *conn;
	char ip[4];
	int port;
	int sending;
};

static struct TelnetConn telnetConn[8];


void telnetBcast(char *str) {
	int x;
	for (x=0; x<8; x++) {
		if (telnetConn[x].conn!=NULL && !telnetConn[x].sending) {
			telnetConn[x].sending=1;
			espconn_sent(telnetConn[x].conn, (uint8_t*)str, strlen(str));
		}
	}
}


static struct TelnetConn *lookupConn(void *arg) {
	int x;
	struct espconn *conn=(struct espconn *)arg;
	for (x=0; x<8; x++) {
		if (memcmp(telnetConn[x].ip, conn->proto.tcp->remote_ip, 4)==0 && telnetConn[x].port==conn->proto.tcp->remote_port) {
			return &telnetConn[x];
		}
	}
	return NULL;
}

static void ICACHE_FLASH_ATTR telnetDisconCb(void *arg) {
	struct TelnetConn *conn=lookupConn(arg);
	if (conn==NULL) return;
	conn->conn=NULL;
}

static void ICACHE_FLASH_ATTR telnetSentCb(void *arg) {
	struct TelnetConn *conn=lookupConn(arg);
	if (conn==NULL) return;
	conn->sending=0;
}

static void ICACHE_FLASH_ATTR telnetConnCb(void *arg) {
	struct espconn *conn=arg;
	int x;
	espconn_regist_disconcb(conn, telnetDisconCb);
	espconn_regist_sentcb(conn, telnetSentCb);
	for (x=0; x<8; x++) {
		if (telnetConn[x].conn==NULL) break;
	}
	telnetConn[x].conn=conn;
	memcpy(telnetConn[x].ip, conn->proto.tcp->remote_ip, 4);
	telnetConn[x].port=conn->proto.tcp->remote_port;
	telnetConn[x].sending=0;
	espconn_regist_time(conn, 7199, 1);
}

//Initialize listening socket, do general initialization
void ICACHE_FLASH_ATTR telnetInit(int port) {
	telnetLConn.type=ESPCONN_TCP;
	telnetLConn.state=ESPCONN_NONE;
	telnetTcp.local_port=port;
	telnetLConn.proto.tcp=&telnetTcp;
	espconn_regist_connectcb(&telnetLConn, telnetConnCb);
	espconn_accept(&telnetLConn);
}

