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

void UpdateVolume(Bar &bar, int updateStrings)
{
	if(bar.isLocal)
	{
		int volume = 0;
		int fd = open(bar.devices[0].c_str(), O_RDONLY);
		
		if(bar.isAvailable[bar.device])
		{
			ioctl(fd, MIXER_READ(bar.device - 1), &volume);
			
			if(bar.isStereo[bar.device])
			{
				bar.samples[0][0] = volume & 0xFF;
				bar.samples[1][0] = (volume & 0xFF00) >> 8;
			}
			else
			{
				bar.samples[0][0] = volume & 0xFF;
				bar.samples[1][0] = volume & 0xFF;
			}
		}
		
		close(fd);
	}
	
	if(updateStrings)
	{
		boost::regex formatRegex;
		boost::cmatch result;
		
		int numFormats = 3;
		double tempNum[numFormats];
		string formats[numFormats];
		
		formats[0] = "%left";
		formats[1] = "%right";
		formats[2] = "%avg";
		
		tempNum[0] = bar.samples[0][0];
		tempNum[1] = bar.samples[1][0];
		tempNum[2] = (bar.samples[0][0] + bar.samples[1][0]) / 2.0;
		
		for(int i = 0; i < bar.numStrings; i++)
		{
			bar.str[i].text = bar.stringFormat[i];
			for(int j = 0; j < numFormats; j++)
			{
				formatRegex = formats[j];
				string tempString = ResizeNum(tempNum[j], "");
				bar.str[i].text = boost::regex_replace(bar.str[i].text, formatRegex, tempString);
			}
		}
	}
}

void UpdateUserScript(Bar &bar)
{
	if(bar.fileHandle && bar.isLocal)
	{
		char line[256];
		int done = 0;
		int temp = 0;
		
		while(!done)
		{
			temp++;
			if(bar.tempData[0][0] == 0)
			{
				bar.tempData[0][0] = 1;
				done = 1;
			}
			
			for(int i = 0; i < bar.numSubBars; i++)
			{
				fgets(line, 256, bar.fileHandle);
				
				if(!ferror(bar.fileHandle) && !feof(bar.fileHandle))
				{
					bar.samples[i][0] = atoi(line);
				}
				else
				{
					done = 1;
					break;
				}
			}
			
			for(int i = 0; i < bar.numStrings; i++)
			{
				fgets(line, 256, bar.fileHandle);
				
				if(!ferror(bar.fileHandle) && !feof(bar.fileHandle))
				{
					int len = strlen(line);
					for(int j = 0; j < len; j++)
						if(line[j] == '\n' || line[j] == '\r')
							line[j] = '\0';
					
					bar.str[i].text = line;
				}
				else
				{
					done = 1;
					break;
				}
			}
		}
		
		clearerr(bar.fileHandle);
	}
}

void UpdateDiskSpace(Bar &bar, int updateStrings)
{
	#ifdef HAVE_LIBGTOP_2_0
	enum { FREE, TOTAL };
	
	if(bar.isLocal)
	{
		glibtop_fsusage fsUsage;
		
		for(int i = 0; i < bar.numSensors; i++)
		{
			glibtop_get_fsusage(&fsUsage, bar.devices[i].c_str());
			if(fsUsage.blocks <= 0)
			{
				if(!quiet)
					cerr << "Invalid device " << bar.devices[i] << ", attempting to change to /" << endl;
				
				bar.devices[i] = "/";
				glibtop_get_fsusage(&fsUsage, bar.devices[i].c_str());
			}
			bar.tempData[i][FREE] = fsUsage.bavail * fsUsage.block_size;
			bar.tempData[i][TOTAL] = fsUsage.blocks * fsUsage.block_size;
			bar.samples[i][0] = (1 - double(bar.tempData[i][FREE]) / double(bar.tempData[i][TOTAL])) * 100;
		}
	}
	
	if(updateStrings)
	{
		boost::regex formatRegex;
		boost::cmatch result;
		
		int numFormats = 5 * bar.numSensors;
		double tempNum[numFormats];
		string formats[numFormats];
		string temp = "%dev";
		string temp2 = "";
		
		for(int i = 0; i < numFormats; i += 5)
		{
			temp2 = ((char *)itoa(i / 3 + 1, 0, ' '));
			temp2 = temp + temp2;
			formats[i] = temp2 + "free%";
			formats[i + 1] = temp2 + "used";
			formats[i + 2] = temp2 + "total";
			formats[i + 3] = temp2 + "%";
			formats[i + 4] = temp2 + "free";
			tempNum[i] = 100 - bar.samples[i / 5][0];
			tempNum[i + 1] = bar.tempData[i / 5][TOTAL] - bar.tempData[i / 5][FREE];
			tempNum[i + 2] = bar.tempData[i / 5][TOTAL];
			tempNum[i + 3] = bar.samples[i / 5][0];
			tempNum[i + 4] = bar.tempData[i / 5][FREE];
		}
		
		for(int i = 0; i < bar.numStrings; i++)
		{
			bar.str[i].text = bar.stringFormat[i];
			for(int j = 0; j < numFormats; j++)
			{
				formatRegex = formats[j];
				tempNum[j] = ParseUnits(bar.str[i].text, formats[j], tempNum[j]);
				string tempString = ResizeNum(tempNum[j], "");
				bar.str[i].text = boost::regex_replace(bar.str[i].text, formatRegex, tempString);
			}
		}
	}
	#endif
}

void UpdateLoadAvg(Bar &bar, int updateStrings)
{
	if(!updateStrings)
		return;
	
	if(bar.isLocal)
	{
		ifstream iFile("/proc/loadavg");
		
		iFile >> bar.samples[0][0] >> bar.samples[1][0] >> bar.samples[2][0];
		
		iFile.close();
	}
	
	boost::regex formatRegex;
	
	int numFormats = 3;
	string formats[numFormats];
	string tempString;
	
	formats[0] = "%1m";
	formats[1] = "%5m";
	formats[2] = "%10m";
	
	for(int i = 0; i < bar.numStrings; i++)
	{
		bar.str[i].text = bar.stringFormat[i];
		for(int j = 0; j < numFormats; j++)
		{
			formatRegex = formats[j];
			tempString = (char *)ftoa(bar.samples[j][0], 2);
			bar.str[i].text = boost::regex_replace(bar.str[i].text, formatRegex, tempString);
		}
	}
}

void UpdateSensors(Bar &bar, int updateStrings)
{
	#ifdef HAVE_LIBSENSORS
	
	if(!updateStrings)
		return;
	
	if(bar.isLocal)
	{
		int chipNum = 0;
		const sensors_chip_name *chip;
		const sensors_subfeature *subFeature;
		const sensors_feature *feature;
		string tempString = "";
		sensors_feature_type featureType;

		for(int i = 0; i < bar.numSensors; i++)
		{
			int sensorNum = 0;
			
			chipNum = bar.sensorChip[i];
			
			if(bar.sensorType[i] == TEMP)
				featureType = SENSORS_FEATURE_TEMP;
			else if(bar.sensorType[i] == FAN)
				featureType = SENSORS_FEATURE_FAN;
			
			chip = sensors_get_detected_chips(NULL, &chipNum);
			
			int featureNum = 0;
			while ((feature = sensors_get_features(chip, &featureNum)))
			{
				if(feature->type == featureType)
				{
					if(sensorNum == bar.sensorNum[i])
					{
						sensors_subfeature_type subFeatureType;
						
						if(bar.sensorType[i] == TEMP)
						{
							subFeatureType = SENSORS_SUBFEATURE_TEMP_INPUT;
						}
						else if(bar.sensorType[i] == FAN)
						{
							subFeatureType = SENSORS_SUBFEATURE_FAN_INPUT;
						}
						
						subFeature = sensors_get_subfeature(chip, feature, subFeatureType);
						
						if(subFeature)
						{
							double value = 0;
							sensors_get_value(chip, subFeature->number, &value);
							bar.samples[i][0] = value;
							break;
						}
					}
					
					sensorNum++;
				}
			}
		}
	}
	
	boost::regex formatRegex;
	boost::cmatch result;
	
	int numFormats = 10;
	int width[MAXSETTINGS];
	double tempNum[numFormats];
	string formats[numFormats];
	
	for(int i = 0; i < MAXSETTINGS; i++)
	{
		if(bar.sensorType[i] == TEMP)
			width[i] = 2;
		else if(bar.sensorType[i] == FAN)
			width[i] = 4;
		else
			width[i] = 0;
	}
	
	for(int i = 0; i < numFormats; i++)
	{
		formats[i] = "%sens";
		string temp = (char *)itoa(i + 1, 0, ' ');
		formats[i] += temp;
		tempNum[i] = bar.samples[i][0];
	}
	
	for(int i = 0; i < bar.numStrings; i++)
	{
		bar.str[i].text = bar.stringFormat[i];
		for(int j = 0; j < numFormats; j++)
		{
			formatRegex = formats[j];
			string tempString = (char *)itoa(tempNum[j], width[j], ' ');
			bar.str[i].text = boost::regex_replace(bar.str[i].text, formatRegex, tempString);
		}
	}
	
	#endif
}

void UpdateUptime(Bar &bar, int updateStrings)
{
	enum { DAYS, HOURS, MINUTES, SECONDS, IDLEPERCENT };
	
	if(!updateStrings)
		return;
	
	if(bar.isLocal)
	{
		double uptime = 0;
		double idleTime = 0;
		ifstream iFile("/proc/uptime");
		boost::regex timeRegex("%d");
		boost::cmatch result;
		
		iFile >> uptime >> idleTime;
		
		iFile.close();
		
		bar.samples[IDLEPERCENT][0] = idleTime / uptime * 100;
		
		bar.samples[DAYS][0] = int(uptime / (86400));
		uptime -= bar.samples[0][0] * 86400;
		bar.samples[HOURS][0] = int(uptime / (3600));
		uptime -= bar.samples[1][0] * 3600;
		bar.samples[MINUTES][0] = int(uptime / 60);
		uptime -= bar.samples[2][0] * 60;
		bar.samples[SECONDS][0] = int(uptime);
	}
	
	boost::regex formatRegex;
	
	int numFormats = 5;
	string formats[numFormats];
	string tempString[numFormats];
	
	formats[0] = "%d";
	formats[1] = "%H";
	formats[2] = "%M";
	formats[3] = "%S";
	formats[4] = "%idle";
	
	tempString[0] = (char *)itoa(bar.samples[DAYS][0], 0, ' ');
	tempString[1] = (char *)itoa(bar.samples[HOURS][0], 2, '0');
	tempString[2] = (char *)itoa(bar.samples[MINUTES][0], 2, '0');
	tempString[3] = (char *)itoa(bar.samples[SECONDS][0], 2, '0');
	tempString[4] = (char *)ftoa(bar.samples[IDLEPERCENT][0], 1);
	
	for(int i = 0; i < bar.numStrings; i++)
	{
		bar.str[i].text = bar.stringFormat[i];
		for(int j = 0; j < numFormats; j++)
		{
			formatRegex = formats[j];
			bar.str[i].text = boost::regex_replace(bar.str[i].text, formatRegex, tempString[j]);
		}
	}
}

void UpdateTime(Bar &bar)
{
	if(bar.isLocal)
	{
		char clockbuf[100];
		time_t time_now; struct tm tm_now;
		
		time(&time_now);
		
		if(localtime_r(&time_now, &tm_now) != NULL && strftime(clockbuf, 100, bar.timeFormat.c_str(), &tm_now))
		{
			bar.devices[0] = clockbuf;
		}
	}
	
	boost::regex formatRegex;
	boost::cmatch result;
	
	int numFormats = 1;
	string formats[numFormats];
	
	formats[0] = "%time";
	
	for(int i = 0; i < bar.numStrings; i++)
	{
		bar.str[i].text = bar.stringFormat[i];
		for(int j = 0; j < numFormats; j++)
		{
			formatRegex = formats[j];
			bar.str[i].text = boost::regex_replace(bar.str[i].text, formatRegex, bar.devices[0]);
		}
	}
}

void UpdateSwapUsage(Bar &usage, int updateStrings)
{
	#ifdef HAVE_LIBGTOP_2_0
	
	if(usage.isLocal)
	{
		glibtop_swap swap;
		
		glibtop_get_swap(&swap);
		
		usage.samples[0][1] = usage.samples[0][0];
		usage.samples[0][0] = swap.used;
		usage.samples[1][0] = swap.total;
	}
	
	if(updateStrings)
	{
		boost::regex formatRegex;
		boost::cmatch result;
		
		int numFormats = 5;
		int widths[numFormats];
		double tempNum[numFormats];
		string formats[numFormats];
		double tempUsed = (usage.samples[0][0] + usage.samples[0][1]) / 2.0;
		guint64 tempTotal = usage.samples[1][0];
		
		formats[0] = "%swapuse%";
		formats[1] = "%swapuse";
		formats[2] = "%swapfree%";
		formats[3] = "%swapfree";
		formats[4] = "%swaptotal";
		tempNum[0] = tempUsed / tempTotal * 100;
		tempNum[1] = tempUsed;
		tempNum[2] = (tempTotal - tempUsed) / tempTotal * 100;
		tempNum[3] = tempTotal - tempUsed;
		tempNum[4] = tempTotal;
		
		for(int i = 0; i < usage.numStrings; i++)
		{
			usage.str[i].text = usage.stringFormat[i];
			for(int j = 0; j < numFormats; j++)
			{
				formatRegex = formats[j];
				tempNum[j] = ParseUnits(usage.str[i].text, formats[j], tempNum[j]);
				string tempString = ResizeNum(tempNum[j], "");
				usage.str[i].text = boost::regex_replace(usage.str[i].text, formatRegex, tempString);
			}
		}
	}
	
	#endif
}

void UpdateMemUsage(Bar &usage, int updateStrings)
{
	#ifdef HAVE_LIBGTOP_2_0
	
	if(usage.isLocal)
	{
		glibtop_mem mem;
		
		glibtop_get_mem(&mem);
		
		usage.samples[0][1] = usage.samples[0][0];
		usage.samples[0][0] = (mem.used - mem.buffer - mem.cached);
		usage.samples[1][0] = mem.total;
	}
	
	if(updateStrings)
	{
		boost::regex formatRegex;
		boost::cmatch result;
		
		int numFormats = 5;
		int widths[numFormats];
		double tempNum[numFormats];
		string formats[numFormats];
		double tempUsed = (usage.samples[0][0] + usage.samples[0][1]) / 2.0;
		guint64 tempTotal = usage.samples[1][0];
		
		formats[0] = "%memuse%";
		formats[1] = "%memuse";
		formats[2] = "%memfree%";
		formats[3] = "%memfree";
		formats[4] = "%memtotal";
		tempNum[0] = tempUsed / tempTotal * 100;
		tempNum[1] = tempUsed;
		tempNum[2] = (tempTotal - tempUsed) / tempTotal * 100;
		tempNum[3] = tempTotal - tempUsed;
		tempNum[4] = tempTotal;
		
		for(int i = 0; i < usage.numStrings; i++)
		{
			usage.str[i].text = usage.stringFormat[i];
			for(int j = 0; j < numFormats; j++)
			{
				formatRegex = formats[j];
				tempNum[j] = ParseUnits(usage.str[i].text, formats[j], tempNum[j]);
				string tempString = ResizeNum(tempNum[j], "");
				usage.str[i].text = boost::regex_replace(usage.str[i].text, formatRegex, tempString);
			}
		}
	}
	
	#endif
}

void UpdateCpuUsage(Bar &usage, int updateStrings)
{
	enum { TOTAL, IDLE };
	enum { IDLETIME = 3, IOTIME = 4 };
	
	if(usage.isLocal)
	{
		static boost::regex re("cpu(\\d+)\\s+(\\d+)\\s+(\\d+)\\s+(\\d+)\\s+(\\d+)\\s+(\\d+)\\s+(\\d+)\\s+(\\d+)");
		boost::cmatch result;
		int coreNum = 0;
		
		ifstream iFile("/proc/stat");
		
		while(!iFile.eof() && !iFile.fail())
		{
			char line[256];
			
			iFile.getline(line, 256);
			if(boost::regex_search(line, result, re))
			{
				string tempString[7];
				string tempCore = "";
				guint64 tempTotal = 0;
				
				tempCore = result[1];
				coreNum = atoi(tempCore.c_str());
				
				for(int j = 2; j <= 8; j++)
				{
					tempString[j - 2] = result[j];
					tempTotal += atof(tempString[j - 2].c_str());
				}
				
				guint64 tempIdle = atof(tempString[IDLETIME].c_str()) + atof(tempString[IOTIME].c_str());
				
				usage.samples[coreNum][1] = usage.samples[coreNum][0];
				
				if(tempTotal - usage.tempData[TOTAL][coreNum] == 0)
					usage.samples[coreNum][0] = 0;
				else
					usage.samples[coreNum][0] = (1 - (tempIdle - usage.tempData[IDLE][coreNum]) / double((tempTotal - usage.tempData[TOTAL][coreNum]))) * 100;
				
				usage.numDevices = coreNum + 1;
				
				usage.tempData[TOTAL][coreNum] = tempTotal;
				usage.tempData[IDLE][coreNum] = tempIdle;
			}
		}
		
		iFile.close();
	}
	
	if(updateStrings)
	{
		boost::regex formatRegex;
		boost::cmatch result;
		
		int numFormats = 5;
		double tempNum[numFormats];
		string formats[numFormats];
		
		formats[0] = "%core1";
		formats[1] = "%core2";
		formats[2] = "%core3";
		formats[3] = "%core4";
		formats[4] = "%avgcpu";
		
		double temp = 0;
		for(int i = 0; i < usage.numSubBars; i++)
		{
			tempNum[i] = (usage.samples[i][0] + usage.samples[i][1]) / 2.0;
			temp += tempNum[i];
		}
		
		temp /= usage.numSubBars;
		
		tempNum[4] = temp;
		
		for(int i = 0; i < usage.numStrings; i++)
		{
			usage.str[i].text = usage.stringFormat[i];
			for(int j = 0; j < numFormats; j++)
			{
				formatRegex = formats[j];
				string tempString = ResizeNum(tempNum[j], "");
				usage.str[i].text = boost::regex_replace(usage.str[i].text, formatRegex, tempString);
			}
		}
	}
}

void UpdateNetUsage(Bar &usage, int updateStrings)
{
	#ifdef HAVE_LIBGTOP_2_0
	enum { CURRENT, LAST };
	
	int INBYTES = 0;
	int OUTBYTES = 1;
	
	if(usage.type == UPLOAD)
	{
		INBYTES = 1;
		OUTBYTES = 0;
	}
	
	if(usage.isLocal && usage.devices[usage.device] != "")
	{
		timeval currentTime;
		glibtop_netload net;
		
		glibtop_get_netload(&net, usage.devices[usage.device].c_str());
		
		if(!(net.if_flags & GLIBTOP_IF_FLAGS_RUNNING))
		{
			cerr << "Invalid network interface " << usage.devices[usage.device] << ", setting to lo" << endl;
			usage.devices[usage.device] = "lo";
			glibtop_get_netload(&net, usage.devices[usage.device].c_str());
			if(!(net.if_flags & GLIBTOP_IF_FLAGS_RUNNING))
			{
				cerr << "Invalid network interface " << usage.devices[usage.device] << ", disabling" << endl;
				usage.devices[usage.device] = "";
			}
		}
		
		if(usage.reset)
		{
			usage.tempData[INBYTES][CURRENT] = net.bytes_in;
			usage.tempData[OUTBYTES][CURRENT] = net.bytes_out;
			usage.tempData[INBYTES][LAST] = net.bytes_in;
			usage.tempData[OUTBYTES][LAST] = net.bytes_out;
			
			usage.reset = 0;
		}
		
		usage.tempData[INBYTES][LAST] = usage.tempData[INBYTES][CURRENT];
		usage.tempData[OUTBYTES][LAST] = usage.tempData[OUTBYTES][CURRENT];
		
		usage.tempData[INBYTES][CURRENT] = net.bytes_in;
		usage.tempData[OUTBYTES][CURRENT] = net.bytes_out;
		
		gettimeofday(&currentTime, NULL);
		
		double tempLast = usage.sampleTime.tv_sec + usage.sampleTime.tv_usec / 1000000.0;
		double tempTime = currentTime.tv_sec + currentTime.tv_usec / 1000000.0;
		
		usage.samples[INBYTES][LAST] = usage.samples[INBYTES][CURRENT];
		usage.samples[OUTBYTES][LAST] = usage.samples[OUTBYTES][CURRENT];
		usage.samples[INBYTES][CURRENT] = (usage.tempData[INBYTES][CURRENT] - usage.tempData[INBYTES][LAST]) * (1.0 / (tempTime - tempLast));
		usage.samples[OUTBYTES][CURRENT] = (usage.tempData[OUTBYTES][CURRENT] - usage.tempData[OUTBYTES][LAST]) * (1.0 / (tempTime - tempLast));
		
		usage.sampleTime = currentTime;
	}
	
	if(updateStrings)
	{
		boost::regex formatRegex;
		boost::cmatch result;
		
		int numFormats = 2;
		double tempNum[numFormats];
		string formats[numFormats];
		
		formats[INBYTES] = "%inrate";
		formats[OUTBYTES] = "%outrate";
		
		tempNum[0] = (usage.samples[0][CURRENT] + usage.samples[0][LAST]) / 2.0;
		tempNum[1] = (usage.samples[1][CURRENT] + usage.samples[1][LAST]) / 2.0;
		
		for(int i = 0; i < usage.numStrings; i++)
		{
			usage.str[i].text = usage.stringFormat[i];
			for(int j = 0; j < numFormats; j++)
			{
				formatRegex = formats[j];
				tempNum[j] = ParseUnits(usage.str[i].text, formats[j], tempNum[j]);
				string tempString = ResizeNum(tempNum[j], "");
				usage.str[i].text = boost::regex_replace(usage.str[i].text, formatRegex, tempString);
			}
		}
	}
	
	#endif
}

void UpdateDiskUsage(Bar &usage, int updateStrings)
{
	enum { READBYTES, IOTIME, WRITEBYTES };
	enum { CURRENT, LAST };
	
	if(usage.isLocal)
	{
		timeval currentTime;
		
		static boost::regex re("\\s*\\d+\\s+\\d+\\s+(\\w+)(\\s+\\d+){2}\\s+(\\d+)(\\s+\\d+){3}\\s+(\\d+)(\\s+\\d+){2}\\s+(\\d+)");
		boost::cmatch result;
		
		ifstream iFile("/proc/diskstats");
		
		while(!iFile.eof() && !iFile.fail())
		{
			char line[256];
			
			iFile.getline(line, 256);
			if(boost::regex_search(line, result, re) && result[1] == usage.devices[usage.device])
			{
				usage.tempData[READBYTES][LAST] = usage.tempData[READBYTES][CURRENT];
				usage.tempData[WRITEBYTES][LAST] = usage.tempData[WRITEBYTES][CURRENT];
				usage.tempData[IOTIME][LAST] = usage.tempData[IOTIME][CURRENT];
				
				string temp = result[3];
				usage.tempData[READBYTES][CURRENT] = atof(temp.c_str()) * SECTOR_SIZE;
				temp = result[5];
				usage.tempData[WRITEBYTES][CURRENT] = atof(temp.c_str()) * SECTOR_SIZE;
				temp = result[7];
				usage.tempData[IOTIME][CURRENT] = atof(temp.c_str());
			}
		}
		
		iFile.close();
		
		gettimeofday(&currentTime, NULL);
		
		if(usage.reset)
		{
			usage.tempData[READBYTES][LAST] = usage.tempData[READBYTES][CURRENT];
			usage.tempData[WRITEBYTES][LAST] = usage.tempData[WRITEBYTES][CURRENT];
			usage.tempData[IOTIME][LAST] = usage.tempData[IOTIME][CURRENT];
			
			usage.reset = 0;
		}
		
		double tempLast = usage.sampleTime.tv_sec + usage.sampleTime.tv_usec / 1000000.0;
		double tempTime = currentTime.tv_sec + currentTime.tv_usec / 1000000.0;
		
		usage.samples[READBYTES][LAST] = usage.samples[READBYTES][CURRENT];
		usage.samples[IOTIME][LAST] = usage.samples[IOTIME][CURRENT];
		usage.samples[WRITEBYTES][LAST] = usage.samples[WRITEBYTES][CURRENT];
		usage.samples[READBYTES][CURRENT] = (usage.tempData[READBYTES][CURRENT] - usage.tempData[READBYTES][LAST]) * (1.0 / (tempTime - tempLast));
		usage.samples[IOTIME][CURRENT] = (usage.tempData[IOTIME][CURRENT] - usage.tempData[IOTIME][LAST]) / 1000.0 * (1.0 / (tempTime - tempLast)) * 100;
		usage.samples[WRITEBYTES][CURRENT] = (usage.tempData[WRITEBYTES][CURRENT] - usage.tempData[WRITEBYTES][LAST]) * (1.0 / (tempTime - tempLast));
		
		usage.sampleTime = currentTime;
	}
	
	if(updateStrings)
	{
		boost::regex formatRegex;
		boost::cmatch result;
		
		int numFormats = 3;
		double tempNum[numFormats];
		string formats[numFormats];
		
		formats[0] = "%inrate";
		formats[1] = "%diskact";
		formats[2] = "%outrate";
		
		tempNum[0] = (usage.samples[0][CURRENT] + usage.samples[0][LAST]) / 2.0;
		tempNum[1] = (usage.samples[1][CURRENT] + usage.samples[1][LAST]) / 2.0;
		tempNum[2] = (usage.samples[2][CURRENT] + usage.samples[2][LAST]) / 2.0;
		
		for(int i = 0; i < usage.numStrings; i++)
		{
			usage.str[i].text = usage.stringFormat[i];
			for(int j = 0; j < numFormats; j++)
			{
				formatRegex = formats[j];
				tempNum[j] = ParseUnits(usage.str[i].text, formats[j], tempNum[j]);
				string tempString = ResizeNum(tempNum[j], "");
				usage.str[i].text = boost::regex_replace(usage.str[i].text, formatRegex, tempString);
			}
		}
	}
}

void UpdateTimer(Bar &bar)
{
	string timerString = "";
	
	guint64 tempStop;
	int minutes = 0;
	int seconds = 0;
	int hundredths = 0;
	
	if(!bar.timer.stopped)
	{
		tempStop = GetCurrentTime();
	}
	else
	{
		tempStop = bar.timer.stopTime;
		bar.timer.processed = 1;
	}
	
	minutes = (tempStop - bar.timer.startTime) / 60000000;
	seconds = (tempStop - bar.timer.startTime) % 60000000 / 1000000;
	hundredths = round((tempStop - bar.timer.startTime - (tempStop - bar.timer.startTime) / 1000000 * 1000000) / 10000.0);
	
	if(hundredths < 0)
		hundredths += 100;
	
	string minutesString = (char*)itoa(minutes, 2, '0');
	string secondsString = (char*)itoa(seconds, 2, '0');
	string hundredthsString = (char*)itoa(hundredths, 2, '0');
	
	timerString = minutesString + ":" + secondsString + ":" + hundredthsString;
	
	boost::regex formatRegex;
	boost::cmatch result;
	
	int numFormats = 1;
	string formats[numFormats];
	
	formats[0] = "%timer";
	
	for(int i = 0; i < bar.numStrings; i++)
	{
		bar.str[i].text = bar.stringFormat[i];
		for(int j = 0; j < numFormats; j++)
		{
			formatRegex = formats[j];
			bar.str[i].text = boost::regex_replace(bar.str[i].text, formatRegex, timerString);
		}
	}
}
