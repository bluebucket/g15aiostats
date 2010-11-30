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

#define MAINFILE

#include "common.h"

int main(int argc, char *argv[])
{
	int done = 0;
	bool updateStrings = 1;
	Bar *bars = NULL;
	configPath = "";
	guint64 startTime = 0;
	guint64 timeTaken = 0;
	guint64 lastCheck = 0;
	guint64 totalTime = 0;
	guint64 lastRotate = 0;
	gint64 timeout = 0;
	
	HandleOptions(argc, argv);
	
	bars = Init();
	
	lastRotate = GetCurrentTime();
	
	int i = 5;
	while(!done)
	{
		startTime = GetCurrentTime();
		
		if(i % 5 == 0)
		{
			if(i >= 10)
			{
				#ifdef KBRANCH_BOARD
				CheckWeather();
				#endif
				
				updateStrings = 1;
				i = 0;
			}
			
			for(int j = 0; j < numBars; j++)
			{
				switch(bars[j].type)
				{
					case NET:
					case UPLOAD:
					case DOWNLOAD:
						UpdateNetUsage(bars[j], updateStrings);
						break;
					case CPU:
						UpdateCpuUsage(bars[j], updateStrings);
						break;
					case MEM:
						UpdateMemUsage(bars[j], updateStrings);
						break;
					case SWAP:
						UpdateSwapUsage(bars[j], updateStrings);
						break;
					case DISK:
						UpdateDiskUsage(bars[j], updateStrings);
						break;
					case DISKSPACE:
						UpdateDiskSpace(bars[j], updateStrings);
						break;
					case USERSCRIPT:
						UpdateUserScript(bars[j]);
						break;
					case SENSOR:
						UpdateSensors(bars[j], updateStrings);
						break;
					case UPTIME:
						UpdateUptime(bars[j], updateStrings);
						break;
					case LOADAVG:
						UpdateLoadAvg(bars[j], updateStrings);
						break;
					case VOLUME:
						UpdateVolume(bars[j], 1);
						break;
				}
				
				if(bars[j].hasBar)
				{
					for(int k = 0; k < bars[j].numSubBars; k++)
						bars[j].speeds[k] = 1000;
				}
				
				RepositionBar(bars[j]);
			}
			
			SendNetworkBars();
		}
		
		for(int j = 0; j < numBars; j++)
		{
			if(bars[j].type == TIME)
				UpdateTime(bars[j]);
			else if(bars[j].type == TIMER)
				UpdateTimer(bars[j]);
		}
		
		
		UpdateBars(bars);
		
		OutputReport(bars);
		
		bars = CheckInput(bars);
		
		updateStrings = 0;
		i++;
		
		guint64 currentTime = GetCurrentTime();
		
		if(rotateDelay > 0 && lastRotate + rotateDelay <= currentTime)
		{
			bars = ChangeScreen(currentScreen + 1);
			lastRotate = currentTime;
		}
		
		timeTaken = currentTime - startTime;
		timeout = FRAME_TIME - timeTaken;
		
		if(timeout < 0)
			timeout = 0;
		
		#ifdef KBRANCH_BOARD
		ColorLCDSendBuffer(timeout - 5000);
		#endif
		
		currentTime = GetCurrentTime();
		timeTaken = currentTime - startTime;
		timeout = FRAME_TIME - timeTaken;
		
		if(timeout < 0)
			timeout = 0;
		
		CheckNetwork(timeout);
	}
	
	return EXIT_SUCCESS;
}

void OutputReport(Bar *bars)
{
	if(bars == NULL)
		return;
	
	#if defined HAVE_LIBG15RENDER && defined HAVE_LIBG15DAEMON_CLIENT
	
	g15r_clearScreen(&canvas, G15_COLOR_WHITE);
	
	int stringPos[4] = { 12, 38, 109, 135 };
	
	for(int i = 0; i < numBars; i++)
	{
		if(bars[i].label.text.length() > 0)
			g15r_renderString(&canvas, (unsigned char*) bars[i].label.text.c_str(), 0, bars[i].fontSize, bars[i].label.x, bars[i].label.y);
		
		for(int j = 0; j < bars[i].numStrings; j++) // Draw strings
		{
			g15r_renderString(&canvas, (unsigned char*)bars[i].str[j].text.c_str(), 0, bars[i].fontSize, bars[i].str[j].x, bars[i].str[j].y);
		}
		
		if(bars[i].hasBar)
		{
			int barHeight = bars[i].h / bars[i].numSubBars;
			int extraPixel = bars[i].h % bars[i].numSubBars;
			
			if(!bars[i].hideBorder)
				g15r_drawBar(&canvas, bars[i].x, bars[i].y, bars[i].x + bars[i].w, bars[i].y + bars[i].h - 1, 1, 0, 100, 1);
			
			for(int j = 0; j < bars[i].numSubBars; j++) // Draw individual sub bars
			{
				if(extraPixel && j == bars[i].numSubBars / 2)
				{
					g15r_drawBar(&canvas, bars[i].x, bars[i].y + j * barHeight, bars[i].x + bars[i].w, bars[i].y + j * barHeight + barHeight, 1, bars[i].values[j], 100, 0);
				}
				else if(extraPixel && j > bars[i].numSubBars / 2)
				{
					g15r_drawBar(&canvas, bars[i].x, bars[i].y + j * barHeight + 1, bars[i].x + bars[i].w, bars[i].y + j * barHeight + barHeight, 1, bars[i].values[j], 100, 0);
				}
				else
				{
					g15r_drawBar(&canvas, bars[i].x, bars[i].y + j * barHeight, bars[i].x + bars[i].w, bars[i].y + j * barHeight + barHeight - 1, 1, bars[i].values[j], 100, 0);
				}
			}
		}
		
		for(int j = 0; j < bars[i].numButtons; j++) // Draw LCD button strings
		{
			if(!bars[i].hideButton[j] && bars[i].buttons[j] > -1)
				g15r_renderString(&canvas, (unsigned char*)lStrings[bars[i].buttons[j]].c_str(), 0, G15_TEXT_SMALL, stringPos[bars[i].buttons[j]], 38);
		}
	}
	
	g15_send(screen, (char*)canvas.buffer, G15_BUFFER_LEN);
	
	#endif
	#ifdef KBRANCH_BOARD
	
	LCDClear();
	
	int stringPos[4] = { 0, 24, 84, 109 };
	
	for(int i = 0; i < numBars; i++)
	{
		if(bars[i].label.text.length() > 0)
			LCDPutStr(bars[i].label.text.c_str(), bars[i].label.x, bars[i].label.y, bars[i].fontSize);
		
		for(int j = 0; j < bars[i].numStrings; j++) // Draw strings
		{
			LCDPutStr(bars[i].str[j].text.c_str(), bars[i].str[j].x, bars[i].str[j].y, bars[i].fontSize);
// 			cout << bars[i].str[j].text.c_str() << endl;
		}
		
		if(bars[i].hasBar)
		{
			int barHeight = bars[i].h / bars[i].numSubBars;
			int extraPixel = bars[i].h % bars[i].numSubBars;
			
			if(!bars[i].hideBorder)
				LCDDrawBar(bars[i].x, bars[i].y, bars[i].x + bars[i].w, bars[i].y + bars[i].h - 1, 1, 0, 100);
			
			for(int j = 0; j < bars[i].numSubBars; j++) // Draw individual sub bars
			{
				if(extraPixel && j == bars[i].numSubBars / 2)
				{
					LCDDrawBar(bars[i].x, bars[i].y + j * barHeight, bars[i].x + bars[i].w, bars[i].y + j * barHeight + barHeight, 0, bars[i].values[j], 100);
				}
				else if(extraPixel && j > bars[i].numSubBars / 2)
				{
					LCDDrawBar(bars[i].x, bars[i].y + j * barHeight + 1, bars[i].x + bars[i].w, bars[i].y + j * barHeight + barHeight, 0, bars[i].values[j], 100);
				}
				else
				{
					LCDDrawBar(bars[i].x, bars[i].y + j * barHeight, bars[i].x + bars[i].w, bars[i].y + j * barHeight + barHeight - 1, 0, bars[i].values[j], 100);
				}
			}
			
// 			cout << bars[i].x << ", " << bars[i].w  << endl;
		}
		
		for(int j = 0; j < bars[i].numButtons; j++) // Draw LCD button strings
		{
			if(!bars[i].hideButton[j] && bars[i].buttons[j] > -1)
				LCDPutStr(lStrings[bars[i].buttons[j]].c_str(), stringPos[bars[i].buttons[j]], screenHeight - 5, 0);
		}
	}
	
	LCDSendBuffer();
	
	#endif
}

Bar *CheckInput(Bar *bars)
{
	unsigned int keyState = 0;
	
	#if defined HAVE_LIBG15RENDER && defined HAVE_LIBG15DAEMON_CLIENT
	
	read(screen, &keyState, sizeof(keyState));
	
	#endif
	#ifdef KBRANCH_BOARD
	
	keyState = LCDGetInput();
	
	#endif
	
	if(CheckButton(screens[currentScreen].button, keyState))
		bars = ChangeScreen(currentScreen + 1);
	
	for(int i = 0; i < numBars; i++)
		CheckBarInput(bars[i], keyState, 1);
	
	for(int i = 0; i < 10; i++)
	{
		if(outSockets[i].connected)
		{
			for(int j = 0; j < outSockets[i].numBars; j++)
				CheckBarInput(outSockets[i].bars[j], outSockets[i].keyState, 0);
			
			outSockets[i].keyState = 0;
		}
	}
	
	return bars;
}

void CheckBarInput(Bar &bar, unsigned int keyState, int updateLocalStrings)
{
	for(int i = 0; i < bar.numButtons; i++)
	{
		if(CheckButton(bar.buttons[i], keyState))
		{
			if(bar.type == TIMER)
			{
				if(i == 0)
				{
					if(bar.timer.stopped)
					{
						if(bar.timer.reset)
							bar.timer.startTime = GetCurrentTime();
						else
							bar.timer.startTime += GetCurrentTime() - bar.timer.stopTime;
						
// 						ColorLCDFill();
						
						if(updateLocalStrings)
							lStrings[bar.buttons[i]] = "stop";
					}
					else
					{
						bar.timer.stopTime = GetCurrentTime();
						
// 						ColorLCDClear();
						
						if(updateLocalStrings)
							lStrings[bar.buttons[i]]= "start";
					}
					
					bar.timer.stopped = !bar.timer.stopped;
					bar.timer.processed = 0;
					bar.timer.reset = 0;
				}
				else if(i == 1)
				{
					bar.timer.startTime = GetCurrentTime();
					bar.timer.stopTime = GetCurrentTime();
					
					if(bar.timer.stopped)
						bar.timer.reset = 1;
					
					bar.timer.processed = 0;
				}
			}
			else if(bar.type == NET || bar.type == DOWNLOAD || bar.type == UPLOAD)
			{
				if(!bar.isLocal)
				{
					string message = "BUTTONPRESS\2";
					message += (char*) itoa(bar.buttons[i], 0, ' ');
					SendMessage(message.c_str(), bar.socket);
				}
				else
				{
					bar.reset = 1;
					bar.device++;
					if(bar.device >= bar.numDevices)
						bar.device = 0;
					
					if(updateLocalStrings)
					{
						lStrings[bar.buttons[i]] = bar.devices[bar.device];
					}
					else
					{
						string message = "BUTTONSTRING\2";
						message += (char*) itoa(bar.buttons[i], 0, ' ');
						message += "\2" + bar.devices[bar.device];
						SendMessage(message.c_str(), bar.socket);
					}
				}
			}
			else if(bar.type == DISK)
			{
				if(!bar.isLocal)
				{
					string message = "BUTTONPRESS\2";
					message += (char*) itoa(bar.buttons[i], 0, ' ');
					SendMessage(message.c_str(), bar.socket);
				}
				else
				{
					bar.reset = 1;
					bar.device++;
					if(bar.device >= bar.numDevices)
						bar.device = 0;
					
					if(updateLocalStrings)
					{
						lStrings[bar.buttons[i]] = bar.devices[bar.device];
					}
					else
					{
						string message = "BUTTONSTRING\2";
						message += (char*) itoa(bar.buttons[i], 0, ' ');
						message += "\2" + bar.devices[bar.device];
						SendMessage(message.c_str(), bar.socket);
					}
				}
			}
		}
	}
}

void UpdateBars(Bar *bars)
{
	if(bars == NULL)
		return;
	
	for(int i = 0; i < numBars; i++)
	{
		for(int j = 0; j < bars[i].numSubBars; j++)
		{
			if(bars[i].max[j] > 0)
			{
				double temp = bars[i].samples[j][0];
				
				if(temp > bars[i].max[j])
					temp = bars[i].max[j];
				
				temp = temp / bars[i].max[j] * 100;
				
				if(bars[i].speeds[j] == 1000)
					bars[i].speeds[j] = (temp - bars[i].values[j]) / 5.0;
				
				if(fabs(bars[i].speeds[j]) < 0.0001)
					bars[i].speeds[j] = 0;
				
				bars[i].values[j] += bars[i].speeds[j];
			}
		}
	}
}

guint64 GetCurrentTime()
{
	timeval temp;
	
	gettimeofday(&temp, NULL);
	
	return temp.tv_sec * 1000000 + temp.tv_usec;
}

timeval ConvertTimeFormat(guint64 time)
{
	timeval temp;
	
	temp.tv_sec = time / 1000000;
	temp.tv_usec = time % 1000000;
	
	return temp;
}

unsigned char* ftoa(double number, int precision)
{
	ostringstream sout;
	
	sout.setf(ios::fixed,ios::floatfield);
	sout.precision(precision);
	sout << setw(precision) << std::showpoint << number;
	string val = sout.str();
	
	return (unsigned char*) val.c_str();
}

unsigned char* itoa(gint64 number, int width, char fill)
{
	ostringstream sout;
	
	if(width > 0)
		sout << setw(width) << setfill(fill);
	
	sout << number;
	string val = sout.str();
	
	return (unsigned char*) val.c_str();
}

string ResizeNum(double num, string suffix)
{
	string str = "";
	
	if(num >= 100)
		str = (char*)itoa(round(num), 4, ' ');
	else if(num >= 10)
		str = (char*)ftoa(round(num * 10) / 10.0, 1);
	else
		str = (char*)ftoa(num, 2);
	
	return str + suffix;
}

double ParseUnits(string text)
{
	boost::regex unitRegex;
	boost::cmatch result;
	string temp = "";
	double number = 0;
	
	unitRegex = "(\\d+)k";
	if(boost::regex_search(text.c_str(), result, unitRegex))
	{
		temp = result[1];
		number = atoi(text.c_str()) * 1000;
	}
	
	unitRegex = "(\\d+)m";
	if(boost::regex_search(text.c_str(), result, unitRegex))
	{
		temp = result[1];
		number = atoi(text.c_str()) * 1000000;
	}
	
	unitRegex = "(\\d+)g";
	if(boost::regex_search(text.c_str(), result, unitRegex))
	{
		temp = result[1];
		number = atoi(text.c_str()) * 1000000000;
	}
	
	return number;
}

double ParseUnits(string &text, string format, double number)
{
	boost::regex unitRegex;
	boost::cmatch result;
	
	unitRegex = format + "K";
	if(boost::regex_search(text.c_str(), result, unitRegex))
	{
		text = boost::regex_replace(text, unitRegex, format);
		number /= 1000.0;
	}
	
	unitRegex = format + "M";
	if(boost::regex_search(text.c_str(), result, unitRegex))
	{
		text = boost::regex_replace(text, unitRegex, format);
		number /= 1000000.0;
	}
	
	unitRegex = format + "G";
	if(boost::regex_search(text.c_str(), result, unitRegex))
	{
		text = boost::regex_replace(text, unitRegex, format);
		number /= 1000000000.0;
	}
	
	return number;
}

int CheckButton(int button, unsigned int keyState)
{
	int pressed = 0;
	
	#if defined HAVE_LIBG15RENDER && defined HAVE_LIBG15DAEMON_CLIENT
	static int buttons[4] = { G15_KEY_L2, G15_KEY_L3, G15_KEY_L4, G15_KEY_L5 };
	#elif defined KBRANCH_BOARD
	static int buttons[4] = { (1<<0), (1<<1), (1<<2), (1<<3) };
	#else
	static int buttons[4];
	return 0;
	#endif
	
	if(keyState & buttons[button])
		pressed = 1;
	
	return pressed;
}

Bar *ChangeScreen(int screen)
{
	Bar *bars;
	
	if(screen >= numScreens)
		currentScreen = 0;
	else
		currentScreen = screen;
	
	bars = screens[currentScreen].bars;
	numBars = screens[currentScreen].numBars;
	autoLayout = screens[currentScreen].autoLayout;
	
	InitButtonStrings(bars);
	
	for(int i = 0; i < 10; i++)
	{
		if(inSocket.connections[i] != -1)
			SendNeedString(inSocket.connections[i], inSocket.clientNames[i]);
	}
	
	return bars;
}

void HandleSignal(int sig)
{
	if(!quiet)
		cerr << "Recieved signal " << sig << ", exiting" << endl;
	
	for(int i = 0; i < numScreens; i++)
	{
		if(screens[i].bars)
		{
			for(int j = 0; j < screens[i].numBars; j++)
			{
				if(screens[i].bars[j].fileHandle)
					close(fileno(screens[i].bars[j].fileHandle));
			}
			
			delete [] screens[i].bars;
		}
	}
	
	if(inSocket.fd >= 0)
	{
		close(inSocket.fd);
		freeaddrinfo(inSocket.res);
	}
	
	for(int i = 0; i < 10; i++)
	{
		if(outSockets[i].fd >= 0)
		{
			if(outSockets[i].bars)
			{
				for(int j = 0; j < outSockets[i].numBars; j++)
				{
					if(outSockets[i].bars[j].fileHandle)
						close(fileno(outSockets[i].bars[j].fileHandle));
				}
			}
			
			close(outSockets[i].fd);
			freeaddrinfo(outSockets[i].res);
		}
	}
	
	#ifdef KBRANCH_BOARD
	LCDCleanup();
	#endif
	
	signal(sig, SIG_DFL);
	raise(sig);
}
