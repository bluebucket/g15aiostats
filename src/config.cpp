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

int LoadConfig(string path, int silent)
{
	ifstream iFile(path.c_str());
	int error = 1;
	
	if(iFile)
	{
		boost::regex commentRegex("#.*");
		boost::regex signRegex("^([\\+-])");
		boost::regex trailingNumRegex("(\\d+)$");
		boost::regex settingRegex("\\s*(\\w+)\\s*([\\+-]?=)\\s*[\"](.*)[\"]", boost::regex_constants::icase);
		boost::regex positionRegex("(\\d+)\\s*,\\s*(\\d+)", boost::regex_constants::icase);
		boost::regex barRegex("\\s*\\[.*Bar.*\\]", boost::regex_constants::icase);
		boost::regex screenRegex("\\s*\\[.*Screen.*\\]", boost::regex_constants::icase);
		boost::regex newlineRegex("\\s*\\[.*Newline.*\\]", boost::regex_constants::icase);
		boost::regex settingNumRegex("[^\\d]+(\\d+)", boost::regex_constants::icase);
		boost::cmatch result;
		Bar *bars = NULL;
		string replace = "";
		int currentScreen = -1;
		int currentBar = -1;
		int configLineNum = 0;
		int currentLine = 0;
		int linePop[MAXBARS];
		
		for(int i = 0; i < MAXBARS; i++)
			linePop[i] = 0;
		
		while(!iFile.eof() && !iFile.fail())
		{
			char line[256];
			
			iFile.getline(line, 256);
			string lineString = line;
			lineString = boost::regex_replace(lineString, commentRegex, replace); // Removes any text after a comment character (#)
			configLineNum++;
			
			if(boost::regex_search(lineString.c_str(), result, settingRegex))
			{
				string setting = result[1];
				string value = result[3];
				string directionString = result[2];
				string temp = "1";
				int direction = 0;
				
				if(boost::regex_search(setting.c_str(), result, settingNumRegex))
					temp = result[1];
				
				int settingNum = atoi(temp.c_str()) - 1;
				
				if(settingNum > MAXSETTINGS)
				{
					settingNum = MAXSETTINGS;
					
					if(!silent && !quiet)
						cerr << setting << " out of range on line " << configLineNum << ", setting to " << MAXSETTINGS << endl;
				}
				
				setting = boost::regex_replace(setting, trailingNumRegex, replace);
				
				for(int i = 0; setting[i] != '\0'; i++)
					setting[i] = tolower(setting[i]);
				
				if(setting != "timeformat" && setting != "stringformat" && setting != "source" && setting != "myname" && setting != "connectto")
					for(int i = 0; value[i] != '\0'; i++)
						value[i] = tolower(value[i]);
				
				if(directionString == "+=")
					direction = 1;
				else if(directionString == "-=")
					direction = -1;
				
				if(setting == "myname")
				{
					myName = value;
				}
				else if(setting == "connectto")
				{
					for(int i = 0; i < 10; i++)
					{
						if(outSockets[i].address == "")
						{
							outSockets[i].address = value;
							break;
						}
					}
				}
				else if(setting == "port")
				{
					inSocket.port = value;
					for(int i = 0; i < 10; i++)
						outSockets[i].port = value;
				}
				else if(setting == "autorotatedelay")
				{
					rotateDelay = atof(value.c_str()) * 1000000;
				}
				else if(currentBar != -1)
				{
					if(setting == "type")
					{
						if(value == "cpu")
						{
							bars[currentBar].type = CPU;
							bars[currentBar].h = 4;
							bars[currentBar].stringFormat[0] = "%avgcpu%";
							bars[currentBar].numStrings = 1;
						}
						else if(value == "net")
						{
							bars[currentBar].type = NET;
							bars[currentBar].h = 8;
							bars[currentBar].stringFormat[0] = "%inrateKK";
							bars[currentBar].stringFormat[1] = "%outrateKK";
							bars[currentBar].numStrings = 2;
						}
						else if(value == "download")
						{
							bars[currentBar].type = DOWNLOAD;
							bars[currentBar].h = 3;
							bars[currentBar].stringFormat[0] = "%inrateKK";
							bars[currentBar].numStrings = 1;
						}
						else if(value == "upload")
						{
							bars[currentBar].type = UPLOAD;
							bars[currentBar].h = 3;
							bars[currentBar].stringFormat[0] = "%outrateKK";
							bars[currentBar].numStrings = 1;
						}
						else if(value == "disk")
						{
							bars[currentBar].type = DISK;
							bars[currentBar].h = 3;
							bars[currentBar].stringFormat[0] = "%diskact%";
							bars[currentBar].numStrings = 1;
						}
						else if(value == "mem")
						{
							bars[currentBar].type = MEM;
							bars[currentBar].h = 3;
							bars[currentBar].stringFormat[0] = "%memuseMM";
							bars[currentBar].numStrings = 1;
						}
						else if(value == "swap")
						{
							bars[currentBar].type = SWAP;
							bars[currentBar].h = 3;
							bars[currentBar].stringFormat[0] = "%swapuseMM";
							bars[currentBar].numStrings = 1;
						}
						else if(value == "time")
						{
							bars[currentBar].type = TIME;
							bars[currentBar].str[0].just = CENTER;
							bars[currentBar].stringFormat[0] = "%time";
							bars[currentBar].timeFormat = "%a %m/%d %I:%M:%S%P";
							bars[currentBar].fontSize = LARGE_FONT;
							bars[currentBar].numStrings = 1;
						}
						else if(value == "timer")
						{
							bars[currentBar].stringFormat[0] = "%timer";
							bars[currentBar].type = TIMER;
							bars[currentBar].str[0].just = CENTER;
							bars[currentBar].numStrings = 1;
						}
						else if(value == "sensor")
						{
							bars[currentBar].stringFormat[0] = "%sens1";
							bars[currentBar].type = SENSOR;
							bars[currentBar].str[0].just = CENTER;
							bars[currentBar].numStrings = 1;
						}
						else if(value == "uptime")
						{
							bars[currentBar].stringFormat[0] = "up: %dd %H:%M";
							bars[currentBar].type = UPTIME;
							bars[currentBar].str[0].just = CENTER;
							bars[currentBar].numStrings = 1;
						}
						else if(value == "loadavg")
						{
							bars[currentBar].stringFormat[0] = "load: %1m %5m %10m";
							bars[currentBar].type = LOADAVG;
							bars[currentBar].str[0].just = CENTER;
							bars[currentBar].numStrings = 1;
						}
						else if(value == "diskspace")
						{
							bars[currentBar].stringFormat[0] = "%dev1%%";
							bars[currentBar].type = DISKSPACE;
							bars[currentBar].numStrings = 1;
							bars[currentBar].h = 3;
							
							if(!silent)
							{
								bars[currentBar].devices[0] = "/";
								bars[currentBar].numSensors = 1;
							}
						}
						else if(value == "userscript")
						{
							bars[currentBar].type = USERSCRIPT;
							bars[currentBar].numStrings = 0;
							bars[currentBar].numSubBars = 1;
							bars[currentBar].h = 3;
						}
						else if(value == "volume")
						{
							bars[currentBar].type = VOLUME;
							bars[currentBar].h = 3;
							bars[currentBar].stringFormat[0] = "%avg%";
							bars[currentBar].numStrings = 1;
						}
						else if(!silent && !quiet)
							cerr << "Invalid type \"" << value << "\" on line " << configLineNum << endl;
					}
					else if(setting == "timeformat")
					{
						bars[currentBar].timeFormat = value;
					}
					else if(setting == "position")
					{
						if(boost::regex_search(value.c_str(), result, positionRegex))
						{
							string x = result[1];
							string y = result[2];
							if(direction == 0)
							{
								if(screens[currentScreen].autoLayout && !bars[currentBar].manualLayout)
								{
									bars[currentBar].xFixed = atoi(x.c_str());
									bars[currentBar].yFixed = atoi(y.c_str());
								}
								else
								{
									bars[currentBar].x = atoi(x.c_str());
									bars[currentBar].y = atoi(y.c_str());
								}
							}
							else
							{
								if(screens[currentScreen].autoLayout && !bars[currentBar].manualLayout)
								{
									bars[currentBar].xMod += direction * atoi(x.c_str());
									bars[currentBar].yMod += direction * atoi(y.c_str());
								}
								else
								{
									bars[currentBar].x += direction * atoi(x.c_str());
									bars[currentBar].y += direction * atoi(y.c_str());
								}
							}
						}
						else if(!silent && !quiet)
							cerr << "Invalid position \"" << value << "\" on line " << configLineNum << endl;
					}
					else if(setting == "fontsize")
					{
						if(value == "small")
							bars[currentBar].fontSize = SMALL_FONT;
						else if(value == "medium")
							bars[currentBar].fontSize = MEDIUM_FONT;
						else if(value == "large")
							bars[currentBar].fontSize = LARGE_FONT;
						else if(!silent && !quiet)
							cerr << "Invalid font size \"" << value << "\" on line " << configLineNum << endl;
					}
					else if(setting == "label")
					{
						bars[currentBar].label.text = value;
					}
					else if(setting == "labelposition")
					{
						if(boost::regex_search(value.c_str(), result, positionRegex))
						{
							string x = result[1];
							string y = result[2];
							if(direction == 0)
							{
								if(screens[currentScreen].autoLayout && !bars[currentBar].manualLayout)
								{
									bars[currentBar].label.xFixed = atoi(x.c_str());
									bars[currentBar].label.yFixed = atoi(y.c_str());
								}
								else
								{
									bars[currentBar].label.x = atoi(x.c_str());
									bars[currentBar].label.y = atoi(y.c_str());
								}
							}
							else
							{
								if(screens[currentScreen].autoLayout && !bars[currentBar].manualLayout)
								{
									bars[currentBar].label.xMod += direction * atoi(x.c_str());
									bars[currentBar].label.yMod += direction * atoi(y.c_str());
								}
								else
								{
									bars[currentBar].label.x += direction * atoi(x.c_str());
									bars[currentBar].label.y += direction * atoi(y.c_str());
								}
							}
						}
						else if(!silent && !quiet)
							cerr << "Invalid label position \"" << value << "\" on line " << configLineNum << endl;
					}
					else if(setting == "height")
					{
						if(direction == 0)
							bars[currentBar].h = atoi(value.c_str());
						else if(silent)
							bars[currentBar].h += direction * atoi(value.c_str());
					}
					else if(setting == "width")
					{
						if(direction == 0)
						{
							if(screens[currentScreen].autoLayout && !bars[currentBar].manualLayout)
								bars[currentBar].wFixed = atoi(value.c_str());
							else
								bars[currentBar].w = atoi(value.c_str());
						}
						else
						{
							if(screens[currentScreen].autoLayout && !bars[currentBar].manualLayout)
								bars[currentBar].wMod += direction * atoi(value.c_str());
							else
								bars[currentBar].w += direction * atoi(value.c_str());
						}
					}
					else if(setting == "stringposition")
					{
						if(boost::regex_search(value.c_str(), result, positionRegex))
						{
							string x = result[1];
							string y = result[2];
							if(direction == 0)
							{
								if(screens[currentScreen].autoLayout && !bars[currentBar].manualLayout)
								{
									bars[currentBar].str[settingNum].xFixed = atoi(x.c_str());
									bars[currentBar].str[settingNum].yFixed = atoi(y.c_str());
								}
								else
								{
									bars[currentBar].str[settingNum].x = atoi(x.c_str());
									bars[currentBar].str[settingNum].y = atoi(y.c_str());
								}
							}
							else
							{
								if(screens[currentScreen].autoLayout && !bars[currentBar].manualLayout)
								{
									bars[currentBar].str[settingNum].xMod += direction * atoi(x.c_str());
									bars[currentBar].str[settingNum].yMod += direction * atoi(y.c_str());
								}
								else
								{
									bars[currentBar].str[settingNum].x += direction * atoi(x.c_str());
									bars[currentBar].str[settingNum].y += direction * atoi(y.c_str());
								}
							}
						}
						else if(!silent && !quiet)
							cerr << "Invalid string position \"" << value << "\" on line " << configLineNum << endl;
					}
					else if(setting == "readmax" || setting == "downloadmax")
					{
						bars[currentBar].max[0] = ParseUnits(value);
					}
					else if(setting == "uploadmax" || setting == "writemax")
					{
						if(bars[currentBar].type == NET)
							bars[currentBar].max[1] = ParseUnits(value);
						else if(bars[currentBar].type == DISK)
							bars[currentBar].max[2] = ParseUnits(value);
					}
					else if(setting == "defaultdevice")
					{
						bars[currentBar].defaultDevice = value;
					}
					else if(setting == "sensordevice" && !silent)
					{
						bars[currentBar].sensorChip[settingNum] = atoi(value.c_str());
						
						if(settingNum >= bars[currentBar].numSensors)
							bars[currentBar].numSensors = settingNum + 1;
					}
					else if(setting == "sensortype" && !silent)
					{
						int valid = 1;
						
						if(value == "temperature")
							bars[currentBar].sensorType[settingNum] = TEMP;
						else if(value == "fan")
							bars[currentBar].sensorType[settingNum] = FAN;
						else if(!silent && !quiet)
						{
							valid = 0;
							cerr << "Invalid sensor type \"" << value << "\" on line " << configLineNum << endl;
						}
						else
							valid = 0;
						
						if(valid && settingNum >= bars[currentBar].numSensors)
							bars[currentBar].numSensors = settingNum + 1;
					}
					else if(setting == "sensornumber" && !silent)
					{
						bars[currentBar].sensorNum[settingNum] = atoi(value.c_str());
						
						if(settingNum >= bars[currentBar].numSensors)
							bars[currentBar].numSensors = settingNum + 1;
					}
					else if(setting == "stringformat")
					{
						bars[currentBar].stringFormat[settingNum] = value;
						if(settingNum >= bars[currentBar].numStrings)
							bars[currentBar].numStrings = settingNum + 1;
					}
					else if(setting == "numstrings")
					{
						if(bars[currentBar].type == USERSCRIPT)
							bars[currentBar].numStrings = atoi(value.c_str());
						else if(!silent && !quiet)
							cerr << "Invalid setting for this bar type on line " << configLineNum << ", ignoring" << endl;
					}
					else if(setting == "numsubbars")
					{
						if(bars[currentBar].type == USERSCRIPT)
							bars[currentBar].numSubBars = atoi(value.c_str());
						else if(!silent && !quiet)
							cerr << "Invalid setting for this bar type on line " << configLineNum << ", ignoring" << endl;
					}
					else if(setting == "labeljustification")
					{
						if(value == "left")
							bars[currentBar].label.just = LEFT;
						else if(value == "center")
							bars[currentBar].label.just = CENTER;
						else if(value == "right")
							bars[currentBar].label.just = RIGHT;
						else if(!silent && !quiet)
							cerr << "Invalid label justification \"" << value << "\" on line " << configLineNum << endl;
					}
					else if(setting == "stringjustification")
					{
						if(value == "left")
							bars[currentBar].str[settingNum].just = LEFT;
						else if(value == "center")
							bars[currentBar].str[settingNum].just = CENTER;
						else if(value == "right")
							bars[currentBar].str[settingNum].just = RIGHT;
						else if(!silent && !quiet)
							cerr << "Invalid string justification \"" << value << "\" on line " << configLineNum << endl;
					}
					else if(setting == "device" && !silent)
					{
						bars[currentBar].devices[settingNum] = value;
						
						if(settingNum >= bars[currentBar].numSensors)
							bars[currentBar].numSensors = settingNum + 1;
						
						if(bars[currentBar].numSensors > bars[currentBar].h)
							bars[currentBar].h = bars[currentBar].numSensors;
					}
					else if(setting == "mixer")
					{
						bars[currentBar].devices[0] = value;
					}
					else if(setting == "button")
					{
						int buttonNum = atoi(value.c_str()) - 1;
						
						if(buttonNum > 3)
						{
							if(!silent && !quiet)
								cerr << "Button \"" << buttonNum + 1 << "\" out of range on line " << configLineNum << ", setting to 4" << endl;
							
							buttonNum = 3;
						}
						
						bars[currentBar].buttons[settingNum] = buttonNum;
						
						if(settingNum >= bars[currentBar].numButtons)
							bars[currentBar].numButtons = settingNum + 1;
					}
					else if(setting == "hidebutton")
					{
						bars[currentBar].hideButton[settingNum] = atoi(value.c_str());
					}
					else if(setting == "command")
					{
						bars[currentBar].devices[0] = value;
					}
					else if(setting == "source")
					{
						bars[currentBar].isLocal = 0;
						bars[currentBar].clientName = value;
					}
					else if(setting == "manual")
					{
						bars[currentBar].manualLayout = atoi(value.c_str());
						linePop[currentLine]--;
					}
					else if(!silent && !quiet)
					{
						cerr << "Unknown setting \"" << setting << "\" at line " << configLineNum << ", skipping" << endl;
					}
				}
				else
				{
					if(setting == "layout")
					{
						if(value == "auto")
							screens[currentScreen].autoLayout = 1;
						else if(value == "manual")
							screens[currentScreen].autoLayout = 0;
						else if(!silent && !quiet)
							cerr << "Invalid layout type \"" << value << "\" on line " << configLineNum << endl;
					}
					else if(setting == "nextscreenbutton")
					{
						int buttonNum = atoi(value.c_str()) - 1;
						
						if(buttonNum > 3)
						{
							if(!silent && !quiet)
								cerr << "Button \"" << buttonNum + 1 << "\" out of range on line " << configLineNum << ", setting to 1" << endl;
							
							buttonNum = 0;
						}
						
						screens[currentScreen].button = buttonNum;
					}
				}
			}
			else if(currentScreen != -1 && boost::regex_search(lineString.c_str(), result, barRegex))
			{
				currentBar++;
				if(currentBar >= MAXBARS)
				{
					if(!silent && !quiet)
						cerr << "Bar out of range, stopping" << endl;
					screens[currentScreen].numBars = currentBar;
					break;
				}
				else
				{
					bars[currentBar].line = currentLine;
					linePop[currentLine]++;
				}
			}
			else if(boost::regex_search(lineString.c_str(), result, newlineRegex))
			{
				currentLine++;
			}
			else if(boost::regex_search(lineString.c_str(), result, screenRegex))
			{
				if(currentScreen > -1)
					screens[currentScreen].numBars = currentBar + 1;
				
				currentScreen++;
				numScreens = currentScreen + 1;
				currentBar = -1;
				currentLine = 0;
				
				if(bars)
				{
					for(int i = 0; i < screens[currentScreen - 1].numBars; i++)
						bars[i].linePop = linePop[bars[i].line];
					for(int i = 0; i < screens[currentScreen - 1].numBars; i++)
						linePop[i] = 0;
				}
				
				if(screens[currentScreen].bars == NULL)
				{
					screens[currentScreen].bars = new Bar[MAXBARS];
					for(int i = 0; i < MAXBARS; i++)
						InitBar(screens[currentScreen].bars[i]);
				}
				
				bars = screens[currentScreen].bars;
			}
		}
		
		screens[currentScreen].numBars = currentBar + 1;
		
		for(int i = 0; i < screens[currentScreen].numBars; i++)
			bars[i].linePop = linePop[bars[i].line];
		
		if(!silent && !quiet)
			cout << "Read config from " << path << endl;
		
		error = 0;
	}
	
	iFile.close();
	
	return error;
}

void WriteConfig(string path)
{
	ofstream oFile(path.c_str());
	
	if(oFile)
	{
		oFile << "[Screen]" << endl;
		oFile << "	NextScreenButton = \"1\"" << endl;
		oFile << "	[Bar]" << endl;
		oFile << "		Type = \"Time\"" << endl;
		oFile << "	[Newline]" << endl;
		oFile << "	[Bar]" << endl;
		oFile << "		Type = \"CPU\"" << endl;
		oFile << "		Label = \"cpu:\"" << endl;
		oFile << "	[Newline]" << endl;
		oFile << "	[Bar]" << endl;
		oFile << "		Type = \"Net\"" << endl;
		oFile << "		Label = \"net:\"" << endl;
		oFile << "	[Newline]" << endl;
		oFile << "	[Bar]" << endl;
		oFile << "		Type = \"Disk\"" << endl;
		oFile << "		Label = \"dsk:\"" << endl;
		oFile << "		Button = \"4\"" << endl;
		oFile << "	[Newline]" << endl;
		oFile << "	[Bar]" << endl;
		oFile << "		Type = \"Volume\"" << endl;
		oFile << "		Label = \"vol:\"" << endl;
		oFile << "	[Bar]" << endl;
		oFile << "		Type = \"Mem\"" << endl;
		oFile << "		Label = \"mem:\"" << endl;
		oFile << "	[Newline]" << endl;
		oFile << "	[Bar]" << endl;
		oFile << "		Type = \"Timer\"" << endl;
		oFile << "		Button1 = \"2\"" << endl;
		oFile << "		Button2 = \"3\"" << endl;
		
		if(!quiet)
			cout << "Successfully created " << path << endl;
		
		LoadConfig(path, 0);
	}
	else if(!quiet)
	{
		cerr << "Unable to create " << path << ", using defaults" << endl;
	}
	
	oFile.close();
}
