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

#include "common.h"

Bar *Init()
{
	signal(SIGTERM, HandleSignal);
	signal(SIGINT, HandleSignal);
	
	#ifndef HAVE_LIBSENSORS
	if(!quiet)
		cerr << "Warning: lm_sensors support not enabed, \"Sensor\" types will not function" << endl;
	#endif
	#if ! defined HAVE_LIBG15RENDER || ! defined HAVE_LIBG15DAEMON_CLIENT
	if(!quiet)
		cerr << "Warning: g15daemon support not enabed, g15 LCD output will not function" << endl;
	#endif
	#ifndef HAVE_LIBGTOP_2_0
	if(!quiet)
		cerr << "Warning: gtop-2.0 support not enabed, \"Mem\", \"Swap\", \"Net\", and \"DiskSpace\" types will not function" << endl;
	#endif
	
	currentScreen = 0;
	volume = -1;
	
	for(int i = 0; i < 10; i++)
	{
		lastOutErrors[i] = -1;
		lastInErrors[i] = -1;
	}
	
	for(int i = 0; i < MAXSCREENS; i++)
	{
		screens[i].numBars = 0;
		screens[i].autoLayout = 1;
		screens[i].bars = NULL;
		
		for(int j = 0; j < 4; j++)
			screens[i].lStrings[j] = "";
	}
	
	Bar *bars = new Bar[MAXBARS];
	
	for(int i = 0; i < MAXBARS; i++)
	{
		InitBar(bars[i]);
	}
	
	bars = ChangeScreen(0);
	
	
// 	fontHeights[0] = 7;
// 	fontHeights[1] = 7;
// 	fontHeights[2] = 7;
// 	fontWidths[0] = 6;
// 	fontWidths[1] = 6;
// 	fontWidths[2] = 6;
// 	#else
	fontHeights[0] = 5;
	fontHeights[1] = 6;
	fontHeights[2] = 7;
	fontWidths[0] = 4;
	fontWidths[1] = 5;
	fontWidths[2] = 8;
// 	#endif
	
	#ifdef KBRANCH_BOARD
	fontWidths[1] = 6;
	
	weatherFile = popen("perl /home/kbranch/scripts/weather.pl", "r");
	weather.ReadFromFile("/home/kbranch/pictures/weather3.bmp");
	bigFont.ReadFromFile("/home/kbranch/pictures/bigfont.bmp");
	
	if(weatherFile)
	{
// 		CheckWeather();
		fcntl(fileno(weatherFile), F_SETFL, fcntl(fileno(weatherFile), F_GETFL, 0) | O_NONBLOCK);
	}
	
	#endif
	
	rotateDelay = 0;
	
	#ifdef HAVE_LIBSENSORS
	if(sensors_init(NULL) && !quiet)
		cerr << "Error initializing sensors" << endl;
	#endif
	
	#ifdef HAVE_LIBGTOP_2_0
	glibtop_init();
	#endif
	
	#ifdef KBRANCH_BOARD
	LCDInit();
	#endif
	
	if(configPath == "")
	{
		configPath = getenv("HOME");
		configPath += "/.g15aiostats.conf";
	}
	
	int error = LoadConfig(configPath, 0);
	
	if(error)
	{
		if(!quiet)
			cerr << "Unable to open " << configPath << ", trying /etc/g15aiostats.conf" << endl;
		
		error = LoadConfig("/etc/g15aiostats.conf", 0);
	}
	
	if(error)
	{
		if(!quiet)
			cerr << "Unable to open " << configPath << ", attempting to create" << endl;
		
		WriteConfig(configPath);
	}
	
	InitNetwork();
	
	for(int i = 0; i < numScreens; i++)
	{
		bars = ChangeScreen(i);
		
		for(int j = 0; j < numBars; j++)
		{
			if(bars[j].type == CPU)
			{
				boost::regex re("(cpu\\d*)");
				boost::cmatch result;
				
				ifstream iFile("/proc/stat");
				
				while(!iFile.eof() && !iFile.fail() && bars[j].numDevices < 5) // Detects the number of cores
				{
					char line[256];
					
					iFile.getline(line, 256);
					if(boost::regex_search(line, result, re))
						bars[j].numDevices++;
				}
				
				iFile.close();
				
				bars[j].numSubBars = bars[j].numDevices - 1;
				bars[j].hasBar = 1;
				
				for(int k = 0; k < bars[j].numSubBars; k++)
					bars[j].max[k] = 100;
				
				UpdateCpuUsage(bars[j], 1);
			}
			else if(bars[j].type == NET || bars[j].type == UPLOAD || bars[j].type == DOWNLOAD)
			{
				#ifdef HAVE_LIBGTOP_2_0
				glibtop_netlist interfaceInfo;
				char **temp = glibtop_get_netlist(&interfaceInfo);
				
				if(bars[j].defaultDevice == "")
					bars[j].defaultDevice = "eth0";
				
				for(unsigned int k = 0; k < interfaceInfo.number; k++) // Retrieves the names of all network interfaces
				{
					bars[j].devices[k] = temp[k];
					if(bars[j].devices[k] == bars[j].defaultDevice)
						bars[j].device = k;
				}
				
				if(bars[j].type == NET)
				{
					bars[j].numSubBars = 2;
					if(bars[j].max[0] == 100)
						bars[j].max[0] = 900000;
					if(bars[j].max[1] == 100)
						bars[j].max[1] = 60000;
				}
				else if(bars[j].type == DOWNLOAD)
				{
					bars[j].numSubBars = 1;
					if(bars[j].max[0] == 100)
						bars[j].max[0] = 900000;
				}
				else if(bars[j].type == UPLOAD)
				{
					bars[j].numSubBars = 1;
					if(bars[j].max[0] == 100)
						bars[j].max[0] = 60000;
				}
				
				bars[j].numDevices = interfaceInfo.number;
				bars[j].hasBar = 1;
				
				UpdateNetUsage(bars[j], 1);
				#endif
			}
			else if(bars[j].type == DISK)
			{
				boost::regex re("(\\s*\\d+){2}\\s+(\\w+)");
				boost::cmatch result;
				
				if(bars[j].defaultDevice == "")
					bars[j].defaultDevice = "sda";
				
				ifstream iFile("/proc/diskstats");
				
				while(!iFile.eof() && !iFile.fail()) // Retrieves the names of all disk devices
				{
					char line[256];
					
					iFile.getline(line, 256);
					if(boost::regex_search(line, result, re))
					{
						bars[j].devices[bars[j].numDevices] = result[2];
						if(result[2] == bars[j].defaultDevice)
							bars[j].device = bars[j].numDevices;
						
						bars[j].numDevices++;
					}
				}
				
				iFile.close();
				
				if(bars[j].max[0] == 100)
					bars[j].max[0] = 50000000;
				if(bars[j].max[2] == 100)
					bars[j].max[2] = 50000000;
				
				bars[j].numSubBars = 3;
				bars[j].hasBar = 1;
				
				UpdateDiskUsage(bars[j], 1);
			}
			else if(bars[j].type == MEM)
			{
				#ifdef HAVE_LIBGTOP_2_0
				glibtop_mem mem;
				glibtop_get_mem(&mem);
				
				bars[j].numSubBars = 1;
				bars[j].max[0] = mem.total;
				bars[j].hasBar = 1;
				
				UpdateMemUsage(bars[j], 1);
				#endif
			}
			else if(bars[j].type == SWAP)
			{
				#ifdef HAVE_LIBGTOP_2_0
				glibtop_swap swap;
				glibtop_get_swap(&swap);
				
				bars[j].numSubBars = 1;
				bars[j].max[0] = swap.total;
				bars[j].hasBar = 1;
				
				UpdateSwapUsage(bars[j], 1);
				#endif
			}
			else if(bars[j].type == TIME)
			{
				UpdateTime(bars[j]);
			}
			else if(bars[j].type == TIMER)
			{
				bars[j].str[0].text = "00:00:00";
				bars[j].timer.stopped = 1;
				bars[j].timer.processed = 0;
				bars[j].timer.reset = 1;
				bars[j].timer.startTime = GetCurrentTime();
				bars[j].timer.stopTime = GetCurrentTime();
			}
			else if(bars[j].type == SENSOR)
			{
				UpdateSensors(bars[j], 1);
			}
			else if(bars[j].type == UPTIME)
			{
				UpdateUptime(bars[j], 1);
			}
			else if(bars[j].type == LOADAVG)
			{
				UpdateLoadAvg(bars[j], 1);
			}
			else if(bars[j].type == DISKSPACE)
			{
				#ifdef HAVE_LIBGTOP_2_0
				if(bars[j].isLocal)
				{
					glibtop_mountentry *mountList;
					glibtop_mountlist mountInfo;
					
					mountList = glibtop_get_mountlist(&mountInfo, 0);
					for(int k = 0; k < bars[j].numSensors; k++)
					{
						for(unsigned int l = 0; l < mountInfo.number; l++)
						{
							string tempString = mountList[l].devname;
							if(tempString == bars[j].devices[k])
							{
								bars[j].devices[k] = mountList[l].mountdir;
							}
						}
					}
				}
				
				bars[j].numSubBars = bars[j].numSensors;
				bars[j].hasBar = 1;
				
				for(int k = 0; k < bars[j].numSubBars; k++)
					bars[j].max[k] = 100;
				
				UpdateDiskSpace(bars[j], 1);
				#endif
			}
			else if(bars[j].type == USERSCRIPT)
			{
				if(bars[j].numSubBars > 0)
					bars[j].hasBar = 1;
				
				if(bars[j].isLocal)
				{
					bars[j].fileHandle = popen(bars[j].devices[0].c_str(), "r");
					
					UpdateUserScript(bars[j]);
					
					fcntl(fileno(bars[j].fileHandle), F_SETFL, fcntl(fileno(bars[j].fileHandle), F_GETFL, 0) | O_NONBLOCK);
				}
			}
			else if(bars[j].type == VOLUME)
			{
				int devMask = 0;
				int stereoDevs = 0;
				int fd = -1;
				const char *names[] = SOUND_DEVICE_NAMES;
				
				if(bars[j].devices[0] == "")
					bars[j].devices[0] = "/dev/mixer";
				
				if(bars[j].defaultDevice == "")
					bars[j].defaultDevice = "vol";
				
				bars[j].device = 0;
				fd = open(bars[j].devices[0].c_str(), O_RDONLY);
				
				ioctl(fd, SOUND_MIXER_READ_STEREODEVS, &stereoDevs);
				ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devMask);
				
				for(int k = 0; k < SOUND_MIXER_NRDEVICES; k++)
				{
					if((1 << k) & devMask)
					{
						if(bars[j].device == 0)
							bars[j].device = k + 1;
						
						bars[j].numSubBars = 1;
						bars[j].hasBar = 1;
						bars[j].devices[k + 1] = names[k];
						bars[j].isAvailable[k + 1] = 1;
						
						if(bars[j].defaultDevice == names[k])
							bars[j].device = k + 1;
						
						if((1 << k) & stereoDevs)
						{
							bars[j].isStereo[k + 1] = 1;
							bars[j].numSubBars = 2;
						}
					}
				}
				
				close(fd);
				
				UpdateVolume(bars[j], 1);
			}
		}
	}
	
	for(int i = 0; i < numScreens; i++)
	{
		if(screens[i].autoLayout)
			LayoutBars(ChangeScreen(i));
	}
	
// 	LoadConfig(configPath, 1); // Reloads any overrides specified in the config file that were wiped out
	
	bars = ChangeScreen(0);
	
	#if defined HAVE_LIBG15RENDER && defined HAVE_LIBG15DAEMON_CLIENT
	// Initialize the display and permanently draw static elements
	if((screen = new_g15_screen(G15_G15RBUF)) < 0)
	{
		if(!quiet)
			cerr << "Unable to connect to G15daemon\n" << endl;
		return NULL;
	}
	#endif
	
	InitButtonStrings(bars);
	
	if(daemonize && daemon(0, 0))
	{
		if(!quiet)
			cerr << "Error starting daemon" << endl;
	}
	
	return bars;
}

void HandleOptions(int argc, char *argv[])
{
	int option;
	
	daemonize = 1;
	quiet = 0;
	outSockets[0].address = "";
	inSocket.port = "5687";
	
	for(int i = 0; i < 10; i++)
		outSockets[i].port = "5687";
	
	while((option = getopt(argc, argv, "c:dh:l:fp:q")) >= 0)
	{
		switch(option)
		{
			case '?':
				cout << "Usage: g15aiostats [OPTIONS]" << endl;
				cout << "  -c path        Read config from path" << endl;
				cout << "  -h address     Connect to address as a client" << endl;
				cout << "  -p port        Use port for all network connections" << endl;
				cout << "  -l mixer path  Prints all available mixer devices for the specified mixer" << endl;
				cout << "  -d             Run in the background as a daemon" << endl;
				cout << "  -f             Run in the foreground instead of as a daemon" << endl;
				cout << "  -q             Run in quiet mode" << endl;
				
				raise(SIGINT);
				break;
			case 'c':
				configPath = optarg;
				break;
			case 'd':
				daemonize = 1;
				break;
			case 'f':
				daemonize = 0;
				break;
			case 'q':
				quiet = 1;
				break;
			case 'h':
				outSockets[0].address = optarg;
				break;
			case 'p':
				inSocket.port = optarg;
				for(int i = 0; i < 10; i++)
					outSockets[i].port = optarg;
				
				break;
			case 'l':
				int devMask = 0;
				int fd = -1;
				const char *names[] = SOUND_DEVICE_NAMES;
				string mixer = optarg;
				
				fd = open(mixer.c_str(), O_RDONLY);
				
				ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devMask);
				
				cout << "Available mixer options: ";
				
				for(int i = 0; i < SOUND_MIXER_NRDEVICES; i++)
				{
					if((1 << i) & devMask)
					{
						cout << "\"" << names[i] << "\" ";
					}
				}
				
				cout << endl;
				
				close(fd);
				
				raise(SIGINT);
				
				break;
		}
	}
}

void InitBar(Bar &bar)
{
	for(int i = 0; i < MAXSETTINGS; i++)
	{
		bar.values[i] = 0;
		bar.speeds[i] = 1000;
		bar.max[i] = 100;
		bar.samples[i][0] = 0;
		bar.samples[i][1] = 0;
		bar.str[i].text = "";
		bar.stringFormat[i] = "";
		bar.str[i].x = 0;
		bar.str[i].y = 0;
		bar.str[i].xMod = 0;
		bar.str[i].yMod = 0;
		bar.str[i].xFixed = 0;
		bar.str[i].yFixed = 0;
		bar.str[i].just = RIGHT;
		bar.sensorChip[i] = 0;
		bar.sensorType[i] = 0;
		bar.sensorNum[i] = 0;
		bar.buttons[i] = -1;
		bar.hideButton[i] = 0;
		bar.oldCenterWidth[i] = 0;
		
		for(int j = 0; j < 5; j++)
			bar.tempData[i][j] = 0;
	}
	
	for(int i = 0; i < 50; i++)
	{
		bar.devices[i] = "";
		bar.isStereo[i] = 0;
		bar.isAvailable[i] = 0;
	}
	
	bar.label.text = "";
	bar.x = 0;
	bar.y = 0;
	bar.xMod = 0;
	bar.yMod = 0;
	bar.xFixed = 0;
	bar.yFixed = 0;
	bar.h = 0;
	bar.w = 0;
	bar.wMod = 0;
	bar.wFixed = 0;
	bar.label.x = 0;
	bar.label.y = 0;
	bar.label.xMod = 0;
	bar.label.yMod = 0;
	bar.label.xFixed = 0;
	bar.label.yFixed = 0;
	bar.label.just = LEFT;
	bar.numStrings = 0;
	bar.fontSize = 0;
	bar.numSubBars = 0;
	bar.type = 0;
	bar.timer.stopped = 1;
	bar.timer.processed = 0;
	bar.timer.reset = 1;
	bar.timer.startTime = GetCurrentTime();
	bar.timer.stopTime = GetCurrentTime();
	bar.reset = 1;
	bar.device = 0;
	bar.numDevices = 0;
	bar.defaultDevice = "";
	bar.timeFormat = "";
	bar.sensorSpacer = " ";
	bar.hasBar = 0;
	bar.line = 0;
	bar.linePop = 1;
	bar.numSensors = 0;
	bar.numButtons = 0;
	bar.isLocal = 1;
	bar.socket = NULL;
	bar.clientName = "";
	bar.oldLeftWidth = 0;
	bar.oldRightWidth = 0;
	bar.sectionNum = 0;
	bar.manualLayout = 0;
	bar.hideBorder = 0;
}

void InitButtonStrings(Bar *bars)
{
	for(int i = 0; i < numBars; i++)
	{
		for(int j = 0; j < bars[i].numButtons; j++)
		{
			if(bars[i].buttons[j] > -1)
			{
				if(bars[i].type == NET || bars[i].type == DISK ||
					bars[i].type == DOWNLOAD || bars[i].type == UPLOAD)
				{
					if(bars[i].isLocal)
						lStrings[bars[i].buttons[j]] = bars[i].devices[bars[i].device];
					else
						lStrings[bars[i].buttons[j]] = bars[i].defaultDevice;
				}
				else if(bars[i].type == TIMER)
				{
					if(j == 0)
						lStrings[bars[i].buttons[j]] = "start";
					else if(j == 1)
						lStrings[bars[i].buttons[j]] = "reset";
				}
			}
		}
	}	
}

void InitNetwork()
{
	char hostName[100];
	if(myName == "")
	{
		if(gethostname(hostName, 100) >= 0)
			myName = hostName;
		else
			myName = "default";
	}
	
	inSocket.fd = -1;
	for(int i = 0; i < 10; i++)
		outSockets[i].fd = -1;
	
	inSocket = CreateSocket(NULL, inSocket.port.c_str());
	
	if(inSocket.fd < 0)
		cerr << "Error creating incoming socket: " << strerror(errno) << endl;
	else if(ListenSocket(inSocket) < 0)
	{
		cerr << "Error listening to incoming socket: " << strerror(errno) << endl;
		close(inSocket.fd);
		inSocket.fd = -1;
	}
	
	for(int i = 0; i < 10; i++)
	{
		if(outSockets[i].address != "")
		{
			outSockets[i] = CreateSocket(outSockets[i].address.c_str(), outSockets[i].port.c_str());
			if(outSockets[i].fd < 0)
				cerr << "Error creating outgoing socket " << i << ": " << strerror(errno) << endl;
		}
	}
}
