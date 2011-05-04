/***************************************************************************
*   Copyright (C) 2009 by Steven Collins                                  *
*   kbranch@kbranch.net                                                   *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 3 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
***************************************************************************/

#ifndef MAINFILE
	#define EXTERN extern
#else
	#define EXTERN
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

//#define KBRANCH_BOARD
//#undef HAVE_LIBG15RENDER
//#undef HAVE_LIBG15DAEMON_CLIENT

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <math.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <boost/regex.hpp>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <linux/soundcard.h>
#include "EasyBMP.h"

#ifdef HAVE_LIBSENSORS
	#include <sensors/sensors.h>
#endif

#if defined HAVE_LIBG15RENDER && defined HAVE_LIBG15DAEMON_CLIENT
	#include <libg15.h>
	#include <libg15render.h>
	#include <g15daemon_client.h>
#else
	enum { G15_KEY_L2 = 1<<23, G15_KEY_L3 = 1<<24, G15_KEY_L4 = 1<<25,
		G15_KEY_L5 = 1<<26 };
#endif

#ifdef KBRANCH_BOARD
	const int screenWidth = 128;
	const int screenHeight = 64;
#else
	const int screenWidth = 160;
	const int screenHeight = 43;
#endif

#ifdef HAVE_LIBGTOP_2_0
	#include <glibtop/mem.h>
	#include <glibtop/swap.h>
	#include <glibtop/netload.h>
	#include <glibtop/netlist.h>
	#include <glibtop/mountlist.h>
	#include <glibtop/fsusage.h>
#else
	typedef unsigned long long int guint64;
	typedef long long int gint64;
#endif

#define FRAME_TIME 50000
#define SECTOR_SIZE 512
#define MAXSCREENS 10
#define MAXBARS 50
#define MAXSETTINGS 20

using namespace std;

enum { CPU, NET, DOWNLOAD, UPLOAD, DISK, MEM, TIME, TIMER, SENSOR, UPTIME,
	LOADAVG, DISKSPACE, SWAP, USERSCRIPT, VOLUME }; // Bar types
enum { TEMP, FAN }; // Sensor types
enum { LEFT, RIGHT, CENTER }; // Text justifications
enum { SMALL_FONT, MEDIUM_FONT, LARGE_FONT }; // Font sizes

struct Timer
{
	guint64 startTime;
	guint64 stopTime;
	int stopped;
	int processed;
	int reset;
};

struct ScreenText
{
	int x;
	int y;
	int xMod;
	int yMod;
	int xFixed;
	int yFixed;
	int just;
	string text;
};

struct Bar
{
	double values[MAXSETTINGS];
	double speeds[MAXSETTINGS];
	double max[MAXSETTINGS];
	double samples[MAXSETTINGS][2];
	guint64 tempData[MAXSETTINGS][MAXSETTINGS];
	ScreenText str[MAXSETTINGS];
	ScreenText label;
	string timeFormat;
	string stringFormat[MAXSETTINGS];
	int x;
	int y;
	int xMod;
	int yMod;
	int xFixed;
	int yFixed;
	int h;
	int w;
	int wMod;
	int wFixed;
	int numStrings;
	int fontSize;
	int numSubBars;
	int type;
	int hasBar;
	int line;
	int linePop;
	int sectionNum;
	int sensorChip[MAXSETTINGS];
	int sensorType[MAXSETTINGS];
	int sensorNum[MAXSETTINGS];
	int numSensors;
	int buttons[MAXSETTINGS];
	int hideButton[MAXSETTINGS];
	int hideBorder;
	int numButtons;
	int isLocal;
	int oldLeftWidth;
	int oldRightWidth;
	int oldCenterWidth[MAXSETTINGS];
	int manualLayout;
	FILE *fileHandle;
	string sensorSpacer;
	timeval sampleTime;
	Timer timer;
	int reset;
	unsigned int device;
	unsigned int numDevices;
	int isStereo[50];
	int isAvailable[50];
	string devices[50];
	string defaultDevice;
	string clientName;
	int socket;
};

struct Socket
{
	int fd;
	int connected;
	int connections[10];
	int lastError;
	guint64 lastTry;
	addrinfo *res;
	string address;
	string port;
	string clientNames[10];
	
	int numBars;
	Bar bars[MAXBARS];
	unsigned int keyState;
};

struct Screen
{
	int numBars;
	int autoLayout;
	int button;
	string lStrings[4];
	Bar *bars;
};

EXTERN int autoLayout;
EXTERN int daemonize;
EXTERN int numBars;
EXTERN int numScreens;
EXTERN int currentScreen;
EXTERN int screen;
EXTERN int quiet;
EXTERN int fontHeights[3];
EXTERN int fontWidths[3];
EXTERN int lastOutErrors[10];
EXTERN int lastInErrors[10];
EXTERN int volume;
EXTERN string lStrings[4];
EXTERN string configPath;
EXTERN string myName;
EXTERN Socket inSocket;
EXTERN Socket outSockets[10];
EXTERN guint64 rotateDelay;
const string typeStrings[15] = { "CPU", "NET", "DOWNLOAD", "UPLOAD", "DISK",
	"MEM", "TIME", "TIMER", "SENSOR", "UPTIME", "LOADAVG", "DISKSPACE", "SWAP",
	"USERSCRIPT", "VOLUME" };

EXTERN Screen screens[MAXSCREENS];

#if defined HAVE_LIBG15RENDER && defined HAVE_LIBG15DAEMON_CLIENT
EXTERN g15canvas canvas;
#endif

#ifdef KBRANCH_BOARD
void LCDInit();
void LCDClear();
void LCDFill();
void LCDSetPixel(uint8_t x, uint8_t y, uint8_t value);
void LCDDrawRect(int x1, int y1, int x2, int y2, int value, int fill);
void LCDDrawBar(int x1, int y1, int x2, int y2, int border, long long int value, long long int max);
void LCDPutCh(uint8_t ch, int charX, int charY, int size, int screen);
void LCDPutStr(const char* str, int x, int y, int size);
void ColorLCDPutStr(const char* str, int x, int y, int size);
void LCDSendBuffer();
unsigned int LCDGetInput();
void LCDCleanup();
void ColorLCDClear();
void ColorLCDFill();
void ColorLCDSendBuffer(guint64 timeout);
void CheckWeather();
void ColorLCDSetPixel(uint8_t x, uint8_t y, uint16_t value);
void ColorLCDDrawImage(BMP image, int xDest, int yDest, int width, int height, int xOffset, int yOffset);
void PutBitmapC(BMP bitmap, char character, int xDest, int yDest, int width, int height);

EXTERN FILE *weatherFile;
EXTERN BMP weather;
EXTERN BMP bigFont;
#endif

Bar *Init();
void InitBar(Bar &bar);
void InitButtonStrings(Bar *bars);
void InitNetwork();
int LoadConfig(string configPath, int silent);
void WriteConfig(string configFile);
void LayoutBars(Bar *bars);
Bar *CheckInput(Bar *bars);
void OutputReport(Bar *bars);
void UpdateBars(Bar *bars);
void UpdateNetUsage(Bar &usage, int updateStrings);
void UpdateCpuUsage(Bar &usage, int updateStrings);
void UpdateMemUsage(Bar &usage, int updateStrings);
void UpdateSwapUsage(Bar &usage, int updateStrings);
void UpdateDiskUsage(Bar &usage, int updateStrings);
void UpdateSensors(Bar &bar, int updateStrings);
void UpdateUptime(Bar &bar, int updateStrings);
void UpdateLoadAvg(Bar &bar, int updateStrings);
void UpdateDiskSpace(Bar &bar, int updateStrings);
void UpdateVolume(Bar &bar, int updateStrings);
void UpdateTime(Bar &bar);
void UpdateTimer(Bar &bar);
void UpdateUserScript(Bar &bar);
string ResizeNum(double num, string suffix);
unsigned char* ftoa(double number, int precision);
unsigned char* itoa(gint64 number, int width, char fill);
guint64 GetCurrentTime();
timeval ConvertTimeFormat(guint64 time);
void HandleOptions(int argc, char *argv[]);
void HandleSignal(int signal);
double ParseUnits(string &text, string format, double number);
double ParseUnits(string text);
int CheckButton(int button, unsigned int keyState);
Bar *ChangeScreen(int screen);
Socket CreateSocket(const char *address, const char *port);
int ConnectSocket(Socket &sock);
int AcceptConnection(Socket &sock);
int ListenSocket(Socket &sock);
int SendMessage(const char *message, int fd);
string ReceiveMessage(int fd);
void CheckNetwork(guint64 timeout);
int Handshake(int fd);
int SendNeedString(int fd, string clientName);
int SendNetworkBars();
void ParseMessage(string message, Socket &socket, int num);
void RepositionBar(Bar &bar);
int FindNextLine(int lineIndices[MAXBARS], int index);
void CheckBarInput(Bar &bar, unsigned int keyState, int updateLocalStrings);
