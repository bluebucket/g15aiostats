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

void CheckNetwork(guint64 timeout)
{
	guint64 startTime = GetCurrentTime();
	guint64 currentTime = startTime;
	fd_set readFDs;
	int highestFD = 0;
	
	for(int i = 0; i < 10; i++)
	{
		if(currentTime >= outSockets[i].lastTry + 1000000)
		{
			if(outSockets[i].address != "" && !outSockets[i].connected)
			{
				if(outSockets[i].fd >= 0)
				{
					if(ConnectSocket(outSockets[i]) >= 0)
					{
						cerr << "Connected to " << outSockets[i].address << endl;
						lastOutErrors[i] = -1;
					}
					else
					{
						if(lastOutErrors[i] != errno)
							cerr << "Error connecting to " << outSockets[i].address << ": " << strerror(errno) << endl;
						
						lastOutErrors[i] = errno;
					}
					
					outSockets[i].lastTry = currentTime;
				}
			}
		}
	}
	
	FD_ZERO(&readFDs);
	
	if(inSocket.fd > highestFD)
		highestFD = inSocket.fd;
	
	if(inSocket.fd >= 0)
		FD_SET(inSocket.fd, &readFDs);
	
	for(int i = 0; i < 10; i++)
	{
		if(inSocket.connections[i] >= 0)
		{
			if(inSocket.connections[i] > highestFD)
				highestFD = inSocket.connections[i];
			
			FD_SET(inSocket.connections[i], &readFDs);
		}
		
		if(outSockets[i].fd >= 0 && outSockets[i].connected)
		{
			if(outSockets[i].fd > highestFD)
				highestFD = outSockets[i].fd;
			
			FD_SET(outSockets[i].fd, &readFDs);
		}
	}
	
	while((currentTime = GetCurrentTime()) - startTime < timeout)
	{
		timeval tempTime = ConvertTimeFormat(timeout - (currentTime - startTime));
		
		select(highestFD + 1, &readFDs, NULL, NULL, &tempTime);
		if(inSocket.fd >= 0 && FD_ISSET(inSocket.fd, &readFDs))
		{
			int connectionNum;
			if((connectionNum = AcceptConnection(inSocket)) >= 0)
			{
				cerr << "Accepted connection" << endl;
				lastInErrors[connectionNum] = -1;
			}
			else
			{
				if(lastInErrors[connectionNum] != errno)
					cerr << "Failed to accept connection: " << strerror(errno) << endl;
				
				lastInErrors[connectionNum] = errno;
			}
		}
		
		for(int i = 0; i < 10; i++)
		{
			if(outSockets[i].fd >= 0 && outSockets[i].connected && FD_ISSET(outSockets[i].fd, &readFDs))
			{
				string message = ReceiveMessage(outSockets[i].fd);
				if(message == "")
				{
					for(int j = 0; j < outSockets[i].numBars; j++)
					{
						if(outSockets[i].bars[j].fileHandle)
						{
							close(fileno(outSockets[i].bars[j].fileHandle));
							outSockets[i].bars[j].fileHandle = NULL;
						}
					}
					
					FD_CLR(outSockets[i].fd, &readFDs);
					close(outSockets[i].fd);
					outSockets[i].connected = 0;
					cerr << "Disconnected from " << outSockets[i].address << endl;
					
					outSockets[i] = CreateSocket(outSockets[i].address.c_str(), outSockets[i].port.c_str());
					
					if(outSockets[i].fd < 0)
						cerr << "Failed to create outgoing socket " << i << ": " << strerror(errno) << endl;
				}
				else
				{
					ParseMessage(message, outSockets[i], 0);
				}
			}
			if(inSocket.connections[i] >= 0 && FD_ISSET(inSocket.connections[i], &readFDs))
			{
				string message = ReceiveMessage(inSocket.connections[i]);
				
				if(message == "")
				{
					FD_CLR(inSocket.connections[i], &readFDs);
					close(inSocket.connections[i]);
					inSocket.connections[i] = -1;
					cerr << "Connection " << i + 1 << " closed" << endl;
				}
				else
				{
					ParseMessage(message, inSocket, inSocket.connections[i]);
				}
			}
		}
	}
}

Socket CreateSocket(const char *address, const char *port)
{
	Socket sock;
	addrinfo hints, *res;
	int fd = 1;
	int temp;
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	
	if((temp = getaddrinfo(address, port, &hints, &res)) < 0)
		fd = temp;
	
	if(fd >= 0)
		fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	
	int yes = 1;
	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
		perror("setsockopt");
	
	if(address == NULL && socket >= 0)
	{
		if((temp = bind(fd, res->ai_addr, res->ai_addrlen)) < 0)
			fd = temp;
	}
	
	sock.fd = fd;
	sock.res = res;
	sock.port = port;
	sock.connected = 0;
	sock.lastTry = 0;
	sock.lastError = -1;
	sock.numBars = 0;
	sock.keyState = 0;
	
	for(int i = 0; i < 10; i++)
		sock.connections[i] = -1;
	
	if(address)
		sock.address = address;
	else
		sock.address = "";
	
	return sock;
}

int ConnectSocket(Socket &sock)
{
	int retval = connect(sock.fd, sock.res->ai_addr, sock.res->ai_addrlen);
	
	if(retval >= 0)
	{
		if(Handshake(sock.fd))
		{
			sock.connected = 1;
			SendMessage(myName.c_str(), sock.fd);
		}
		else
		{
			close(sock.fd);
			sock = CreateSocket(sock.address.c_str(), sock.port.c_str());
			
			if(sock.fd < 0)
				cerr << "Failed to create outgoing socket " << sock.fd << ": " << strerror(errno) << endl;
			
			retval = -1;
			errno = EPROTOTYPE;
		}
	}
	
	return retval;
}

int Handshake(int fd)
{
	int retval = 0;
	double version = 0;
	string id = PACKAGE_NAME;
	string name = "";
	fd_set readFDs;
	
	id += '\2';
	id += PACKAGE_VERSION;
	
	SendMessage(id.c_str(), fd);
	
	FD_ZERO(&readFDs);
	FD_SET(fd, &readFDs);
	
	timeval timeout;
	
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	
	select(fd + 1, &readFDs, NULL, NULL, &timeout);
	
	if(FD_ISSET(fd, &readFDs))
	{
		id = ReceiveMessage(fd);
		
		char *token = strtok((char*) id.c_str(), "\2");
		if(token != NULL)
		{
			name = token;
			token = strtok(NULL, "\2");
			
			if(token != NULL)
				version = atof(token);
		}
		
		if(name == "g15aiostats")
		{
			if(version >= 0.22)
			{
				SendMessage("GOGO", fd);
				timeout.tv_sec = 1;
				timeout.tv_usec = 0;
				select(fd + 1, &readFDs, NULL, NULL, &timeout);
				if(FD_ISSET(fd, &readFDs) && ReceiveMessage(fd) == "GOGO")
					retval = 1;
			}
			else
			{
				SendMessage("GTFO", fd);
			}
		}
	}
	
	return retval;
}

int SendNeedString(int fd, string clientName)
{
	Bar *bars = screens[currentScreen].bars;
	string needString = "INEED\2";
	int retval = 0;
	
	for(int j = 0; j < numBars; j++)
	{
		if(bars[j].clientName == clientName)
		{
			bars[j].socket = fd;
			
			switch(bars[j].type)
			{
				case CPU:
					needString += "CPU";
					break;
				case NET:
					needString += "NET\2" + bars[j].defaultDevice + "\2";
					needString += (char *) itoa(bars[j].buttons[0], 0, ' ');
					break;
				case DOWNLOAD:
					needString += "DOWNLOAD\2" + bars[j].defaultDevice + "\2";
					needString += (char *) itoa(bars[j].buttons[0], 0, ' ');
					break;
				case UPLOAD:
					needString += "UPLOAD\2" + bars[j].defaultDevice + "\2";
					needString += (char *) itoa(bars[j].buttons[0], 0, ' ');
					break;
				case DISK:
					needString += "DISK\2" + bars[j].devices[bars[j].device] + "\2";
					needString += (char *) itoa(bars[j].buttons[0], 0, ' ');
					break;
				case MEM:
					needString += "MEM";
					break;
				case TIME:
					needString += "TIME\2" + bars[j].timeFormat;
					break;
				case SENSOR:
					needString += "SENSOR\2";
					needString += (char *) itoa(bars[j].numSensors, 0, ' ');
					
					for(int k = 0; k < bars[j].numSensors; k++)
					{
						needString += "\2";
						needString += (char *) itoa(bars[j].sensorChip[k], 0, ' ');
						needString += "\2" ;
						needString += (char *) itoa(bars[j].sensorType[k], 0, ' ');
						needString += "\2" ;
						needString += (char *) itoa(bars[j].sensorNum[k], 0, ' ');
					}
					
					break;
				case UPTIME:
					needString += "UPTIME";
					break;
				case LOADAVG:
					needString += "LOADAVG";
					break;
				case DISKSPACE:
					needString += "DISKSPACE\2" ;
					needString += (char *) itoa(bars[j].numSubBars, 0, ' ');
					
					for(int k = 0; k < bars[j].numSubBars; k++)
						needString += "\2" + bars[j].devices[k];
					
					break;
				case SWAP:
					needString += "SWAP";
					break;
				case USERSCRIPT:
					needString += "USERSCRIPT\2" + bars[j].devices[0];
					needString += "\2";
					needString += (char *) itoa(bars[j].numSubBars, 0, ' ');
					needString += "\2";
					needString += (char *) itoa(bars[j].numStrings, 0, ' ');
					break;
				case VOLUME:
					needString += "VOLUME\2" + bars[j].devices[0] + "\2";
					needString += bars[j].defaultDevice;
					break;
			}
			
			needString += "\2";
		}
	}
	
	if(needString != "INEED\2")
		retval = SendMessage(needString.c_str(), fd);
	
	return retval;
}

int AcceptConnection(Socket &sock)
{
	sockaddr_storage theirAddress;
	socklen_t addressSize;
	int retval = 0;
	
	addressSize = sizeof(theirAddress);
	for(int i = 0; i < 10; i++)
	{
		if(sock.connections[i] < 0)
		{
			sock.connections[i] = accept(sock.fd, (sockaddr *)&theirAddress, &addressSize);
			if(sock.connections[i] < 0)
			{
				retval = sock.connections[i];
			}
			else
			{
				if(Handshake(sock.connections[i]))
				{
					sock.clientNames[i] = ReceiveMessage(sock.connections[i]);
					SendNeedString(sock.connections[i], sock.clientNames[i]);
				}
				else
				{
					close(sock.connections[i]);
					sock.connections[i] = -1;
					retval = -1;
					errno = EPROTOTYPE;
				}
			}
			
			break;
		}
	}
	
	return retval;
}

int ListenSocket(Socket &sock)
{
	return listen(sock.fd, 10);
}

int SendMessage(const char *message, int fd)
{
	int len = strlen(message) + 3;
	int bytesOut = 0;
	unsigned char packet[len];
	int temp = len - 2;
	
	packet[0] = temp >> 8;
	packet[1] = temp;
	
	for(int i = 2; i < len; i++)
		packet[i] = message[i - 2];
	
	temp = 0;
	while(bytesOut < len)
	{
		temp = send(fd, packet + bytesOut, len - bytesOut, MSG_NOSIGNAL);
		if(temp <= 0)
		{
			bytesOut = temp;
			break;
		}
		else
		{
			bytesOut += temp;
		}
	}
	
	return bytesOut;
}

string ReceiveMessage(int fd)
{
	int bytesIn = 0;
	unsigned char temp[2];
	int len = 0;
	string message = "";
	
	if(recv(fd, &temp, 2, 0) > 0)
	{
		len = temp[0] << 8;
		len += temp[1];
		
		char buffer[len];
		
		while(bytesIn < len)
		{
			bytesIn = recv(fd, buffer + bytesIn, len - bytesIn, 0);
			if(bytesIn == 0)
			{
				message = "";
				break;
			}
		}
		
		message = buffer;
	}
	
	return message;
}

int SendNetworkBars()
{
	int retval = 0;
	Socket *sock;
	
	for(int currentSocket = 0; currentSocket < 10; currentSocket++)
	{
		string sendString = "HERE\2";
		sock = &outSockets[currentSocket];
		if(sock->fd >= 0 && retval >= 0)
		{
			for(int i = 0; i < sock->numBars; i++)
			{
				if(sock->bars[i].type == CPU)
				{
					UpdateCpuUsage(sock->bars[i], 0);
					
					sendString += "CPU\2";
					sendString += (char *)itoa(sock->bars[i].numDevices, 0, ' ');
					
					for(int j = 0; j < sock->bars[i].numDevices; j++)
					{
						sendString += "\2";
						sendString += (char *) ftoa(sock->bars[i].samples[j][0], 2);
					}
				}
				else if(sock->bars[i].type == NET)
				{
					UpdateNetUsage(sock->bars[i], 0);
					
					sendString += "NET\2";
					sendString += (char *) ftoa(sock->bars[i].samples[0][0], 2);
					sendString += "\2";
					sendString += (char *) ftoa(sock->bars[i].samples[1][0], 2);
				}
				else if(sock->bars[i].type == DOWNLOAD)
				{
					UpdateNetUsage(sock->bars[i], 0);
					
					sendString += "DOWNLOAD\2";
					sendString += (char *) ftoa(sock->bars[i].samples[0][0], 2);
				}
				else if(sock->bars[i].type == UPLOAD)
				{
					UpdateNetUsage(sock->bars[i], 0);
					
					sendString += "UPLOAD\2";
					sendString += (char *) ftoa(sock->bars[i].samples[0][0], 2);
				}
				else if(sock->bars[i].type == DISK)
				{
					UpdateDiskUsage(sock->bars[i], 0);
					sendString += "DISK\2";
					sendString += (char *) ftoa(sock->bars[i].samples[0][0], 2);
					sendString += "\2";
					sendString += (char *) ftoa(sock->bars[i].samples[1][0], 2);
					sendString += "\2";
					sendString += (char *) ftoa(sock->bars[i].samples[2][0], 2);
				}
				else if(sock->bars[i].type == MEM)
				{
					UpdateMemUsage(sock->bars[i], 0);
					sendString += "MEM\2";
					sendString += (char *) itoa(sock->bars[i].samples[0][0], 0, ' ');
					sendString += "\2";
					sendString += (char *) itoa(sock->bars[i].samples[1][0], 0, ' ');
				}
				else if(sock->bars[i].type == TIME)
				{
					UpdateTime(sock->bars[i]);
					sendString += "TIME\2" + sock->bars[i].devices[0];
				}
				else if(sock->bars[i].type == SENSOR)
				{
					UpdateSensors(sock->bars[i], 1);
					sendString += "SENSOR";
					
					for(int j = 0; j < sock->bars[i].numSensors; j++)
					{
						sendString += "\2";
						sendString += (char *) itoa(sock->bars[i].samples[j][0], 0, ' ');
					}
				}
				else if(sock->bars[i].type == UPTIME)
				{
					UpdateUptime(sock->bars[i], 1);
					sendString += "UPTIME\2";
					sendString += (char *) ftoa(sock->bars[i].samples[0][0], 2);
					sendString += "\2";
					sendString += (char *) ftoa(sock->bars[i].samples[1][0], 2);
					sendString += "\2";
					sendString += (char *) ftoa(sock->bars[i].samples[2][0], 2);
					sendString += "\2";
					sendString += (char *) ftoa(sock->bars[i].samples[3][0], 2);
					sendString += "\2";
					sendString += (char *) ftoa(sock->bars[i].samples[4][0], 2);
					
				}
				else if(sock->bars[i].type == LOADAVG)
				{
					UpdateLoadAvg(sock->bars[i], 1);
					sendString += "LOADAVG\2";
					sendString += (char *) ftoa(sock->bars[i].samples[0][0], 2);
					sendString += "\2";
					sendString += (char *) ftoa(sock->bars[i].samples[1][0], 2);
					sendString += "\2";
					sendString += (char *) ftoa(sock->bars[i].samples[2][0], 2);
				}
				else if(sock->bars[i].type == DISKSPACE)
				{
					UpdateDiskSpace(sock->bars[i], 0);
					sendString += "DISKSPACE";
					
					for(int j = 0; j < sock->bars[i].numSensors; j++)
					{
						sendString += "\2";
						sendString += (char *) ftoa(sock->bars[i].samples[j][0], 2);
						sendString += "\2";
						sendString += (char *) ftoa(sock->bars[i].tempData[j][0], 2);
						sendString += "\2";
						sendString += (char *) ftoa(sock->bars[i].tempData[j][1], 2);
					}
				}
				else if(sock->bars[i].type == SWAP)
				{
					UpdateSwapUsage(sock->bars[i], 0);
					sendString += "SWAP\2";
					sendString += (char *) itoa(sock->bars[i].samples[0][0], 0, ' ');
					sendString += "\2";
					sendString += (char *) itoa(sock->bars[i].samples[1][0], 0, ' ');
				}
				else if(sock->bars[i].type == USERSCRIPT)
				{
					UpdateUserScript(sock->bars[i]);
					sendString += "USERSCRIPT";
					
					for(int j = 0; j < sock->bars[i].numSubBars; j++)
					{
						sendString += "\2";
						sendString += (char *) itoa(sock->bars[i].samples[j][0], 0, ' ');
					}
					
					for(int j = 0; j < sock->bars[i].numStrings; j++)
					{
						sendString += "\2" + sock->bars[i].str[j].text;
					}
				}
				else if(sock->bars[i].type == VOLUME)
				{
					UpdateVolume(sock->bars[i], 0);
					sendString += "VOLUME\2";
					sendString += (char *) itoa(sock->bars[i].numSubBars, 0, ' ');
					sendString += "\2";
					sendString += (char *) itoa(sock->bars[i].samples[0][0], 0, ' ');
					sendString += "\2";
					sendString += (char *) itoa(sock->bars[i].samples[1][0], 0, ' ');
				}
				sendString += "\2";
			}
		}
		
		if(sendString != "HERE\2")
		{
			retval = SendMessage(sendString.c_str(), sock->fd);
		}
	}
	
	return retval;
}

void ParseMessage(string message, Socket &socket, int num)
{
	char *token = strtok((char*) message.c_str(), "\2");
	int i = 0;
	
	if(!strcmp(token, "INEED"))
	{
		token = strtok(NULL, "\2");
		
		while(token != NULL && i < MAXBARS)
		{
			InitBar(socket.bars[i]);
			socket.bars[i].socket = socket.fd;
			
			if(!strcmp(token, "CPU"))
			{
				socket.bars[i].type = CPU;
			}
			else if(!strcmp(token, "NET") || !strcmp(token, "DOWNLOAD") || !strcmp(token, "UPLOAD"))
			{
				if(!strcmp(token, "NET"))
					socket.bars[i].type = NET;
				else if(!strcmp(token, "DOWNLOAD"))
					socket.bars[i].type = DOWNLOAD;
				else if(!strcmp(token, "UPLOAD"))
					socket.bars[i].type = UPLOAD;
				
				token = strtok(NULL, "\2");
				#ifdef HAVE_LIBGTOP_2_0
				glibtop_netlist interfaceInfo;
				char **temp = glibtop_get_netlist(&interfaceInfo);
				
				socket.bars[i].defaultDevice = token;
				socket.bars[i].numDevices = interfaceInfo.number;
				
				for(unsigned int j = 0; j < socket.bars[i].numDevices; j++) // Retrieves the names of all network interfaces
				{
					socket.bars[i].devices[j] = temp[j];
					if(socket.bars[i].devices[j] == socket.bars[i].defaultDevice)
						socket.bars[i].device = j;
				}
				#endif
				
				token = strtok(NULL, "\2");
				socket.bars[i].buttons[0] = atoi(token);
				
				if(socket.bars[i].buttons[0] >= 0)
					socket.bars[i].numButtons = 1;
			}
			else if(!strcmp(token, "DISK"))
			{
				boost::regex re("(\\s*\\d+){2}\\s+(\\w+)");
				boost::cmatch result;
				
				socket.bars[i].type = DISK;
				token = strtok(NULL, "\2");
				socket.bars[i].defaultDevice = token;
				
				ifstream iFile("/proc/diskstats");
				
				while(!iFile.eof() && !iFile.fail()) // Retrieves the names of all disk devices
				{
					char line[256];
					
					iFile.getline(line, 256);
					if(boost::regex_search(line, result, re))
					{
						socket.bars[i].devices[socket.bars[i].numDevices] = result[2];
						if(result[2] == socket.bars[i].defaultDevice)
							socket.bars[i].device = socket.bars[i].numDevices;
						
						socket.bars[i].numDevices++;
					}
				}
				
				iFile.close();
				
				token = strtok(NULL, "\2");
				socket.bars[i].buttons[0] = atoi(token);
				
				if(socket.bars[i].buttons[0] >= 0)
					socket.bars[i].numButtons = 1;
			}
			else if(!strcmp(token, "MEM"))
			{
				socket.bars[i].type = MEM;
			}
			else if(!strcmp(token, "TIME"))
			{
				socket.bars[i].type = TIME;
				token = strtok(NULL, "\2");
				socket.bars[i].timeFormat = token;
			}
			else if(!strcmp(token, "SENSOR"))
			{
				socket.bars[i].type = SENSOR;
				token = strtok(NULL, "\2");
				socket.bars[i].numSensors = atoi(token);
				
				for(int j = 0; j < socket.bars[i].numSensors; j++)
				{
					socket.bars[i].sensorChip[j] = atoi(strtok(NULL, "\2"));
					socket.bars[i].sensorType[j] = atoi(strtok(NULL, "\2"));
					socket.bars[i].sensorNum[j] = atoi(strtok(NULL, "\2"));
				}
			}
			else if(!strcmp(token, "UPTIME"))
			{
				socket.bars[i].type = UPTIME;
			}
			else if(!strcmp(token, "LOADAVG"))
			{
				socket.bars[i].type = LOADAVG;
			}
			else if(!strcmp(token, "DISKSPACE"))
			{
				socket.bars[i].type = DISKSPACE;
				token = strtok(NULL, "\2");
				socket.bars[i].numSensors = atoi(token);
				
				for(int j = 0; j < socket.bars[i].numSensors; j++)
				{
					socket.bars[i].devices[j] = strtok(NULL, "\2");
				}
				
				#ifdef HAVE_LIBGTOP_2_0
				
				glibtop_mountentry *mountList;
				glibtop_mountlist mountInfo;
				
				mountList = glibtop_get_mountlist(&mountInfo, 0);
				for(int j = 0; j < socket.bars[i].numSensors; j++)
				{
					for(unsigned int k = 0; k < mountInfo.number; k++)
					{
						string tempString = mountList[k].devname;
						if(tempString == socket.bars[i].devices[j])
						{
							socket.bars[i].devices[j] = mountList[k].mountdir;
						}
					}
				}
				
				#endif
			}
			else if(!strcmp(token, "SWAP"))
			{
				socket.bars[i].type = SWAP;
			}
			else if(!strcmp(token, "USERSCRIPT"))
			{
				socket.bars[i].type = USERSCRIPT;
				token = strtok(NULL, "\2");
				socket.bars[i].devices[0] = token;
				
				socket.bars[i].numSubBars = atoi(strtok(NULL, "\2"));
				socket.bars[i].numStrings = atoi(strtok(NULL, "\2"));
				
				socket.bars[i].fileHandle = popen(socket.bars[i].devices[0].c_str(), "r");
				fcntl(fileno(socket.bars[i].fileHandle), F_SETFL, fcntl(fileno(socket.bars[i].fileHandle), F_GETFL, 0) | O_NONBLOCK);
			}
			else if(!strcmp(token, "VOLUME"))
			{
				socket.bars[i].type = VOLUME;
				token = strtok(NULL, "\2");
				socket.bars[i].devices[0] = token;
				token = strtok(NULL, "\2");
				socket.bars[i].defaultDevice = token;
				
				int devMask = 0;
				int stereoDevs = 0;
				int fd = -1;
				const char *names[] = SOUND_DEVICE_NAMES;
				
				if(socket.bars[i].devices[0] == "")
					socket.bars[i].devices[0] = "/dev/mixer";
				
				if(socket.bars[i].defaultDevice == "")
					socket.bars[i].defaultDevice = "vol";
				
				socket.bars[i].device = 1;
				fd = open(socket.bars[i].devices[0].c_str(), O_RDONLY);
				
				ioctl(fd, SOUND_MIXER_READ_STEREODEVS, &stereoDevs);
				ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devMask);
				
				for(int j = 0; j < SOUND_MIXER_NRDEVICES; j++)
				{
					if((1 << j) & devMask)
					{
						socket.bars[i].numSubBars = 1;
						socket.bars[i].hasBar = 1;
						socket.bars[i].devices[j + 1] = names[j];
						socket.bars[i].isAvailable[j + 1] = 1;
						
						if(socket.bars[i].defaultDevice == names[j])
							socket.bars[i].device = j + 1;
						
						if((1 << j) & stereoDevs)
						{
							socket.bars[i].isStereo[j + 1] = 1;
							socket.bars[i].numSubBars = 2;
						}
					}
				}
				
				close(fd);
				
				UpdateVolume(socket.bars[i], 1);
			}
			
			token = strtok(NULL, "\2");
			i++;
		}
		
		socket.numBars = i;
	}
	else if(!strcmp("HERE", token))
	{
		Bar *bars = screens[currentScreen].bars;
		
		token = strtok(NULL, "\2");
		
		while(token != NULL)
		{
			Bar *bar = NULL;
			for(int i = 0; i < screens[currentScreen].numBars; i++)
			{
				if(typeStrings[bars[i].type] == token &&  bars[i].socket == num)
				{
					bar = &(bars[i]);
				}
			}
			
			if(bar != NULL)
			{
				if(bar->type == CPU)
				{
					token = strtok(NULL, "\2");
					bar->numSubBars = atoi(token);
					
					for(int i = 0; i < bar->numSubBars; i++)
					{
						token = strtok(NULL, "\2");
						bar->samples[i][1] = bar->samples[i][0];
						bar->samples[i][0] = atof(token);
					}
				}
				else if(bar->type == NET)
				{
					token = strtok(NULL, "\2");
					bar->samples[0][1] = bar->samples[0][0];
					bar->samples[0][0] = atoi(token);
					token = strtok(NULL, "\2");
					bar->samples[1][1] = bar->samples[1][0];
					bar->samples[1][0] = atoi(token);
				}
				else if(bar->type == DOWNLOAD || bar->type == UPLOAD)
				{
					token = strtok(NULL, "\2");
					bar->samples[0][1] = bar->samples[0][0];
					bar->samples[0][0] = atoi(token);
				}
				else if(!strcmp(token, "DISK"))
				{
					token = strtok(NULL, "\2");
					bar->samples[0][1] = bar->samples[0][0];
					bar->samples[0][0] = atoi(token);
					token = strtok(NULL, "\2");
					bar->samples[1][1] = bar->samples[1][0];
					bar->samples[1][0] = atoi(token);
					token = strtok(NULL, "\2");
					bar->samples[2][1] = bar->samples[2][0];
					bar->samples[2][0] = atoi(token);
				}
				else if(!strcmp(token, "MEM"))
				{
					token = strtok(NULL, "\2");
					bar->samples[0][1] = bar->samples[0][0];
					bar->samples[0][0] = atoi(token);
					token = strtok(NULL, "\2");
					bar->max[0] = atoi(token);
					
				}
				else if(!strcmp(token, "TIME"))
				{
					token = strtok(NULL, "\2");
					bar->devices[0] = token;
				}
				else if(!strcmp(token, "SENSOR"))
				{
					for(int i = 0; i < bar->numSensors; i++)
					{
						token = strtok(NULL, "\2");
						bar->samples[i][0] = atof(token);
					}
				}
				else if(!strcmp(token, "UPTIME"))
				{
					token = strtok(NULL, "\2");
					bar->samples[0][0] = atoi(token);
					token = strtok(NULL, "\2");
					bar->samples[1][0] = atoi(token);
					token = strtok(NULL, "\2");
					bar->samples[2][0] = atoi(token);
					token = strtok(NULL, "\2");
					bar->samples[3][0] = atoi(token);
					token = strtok(NULL, "\2");
					bar->samples[4][0] = atoi(token);
				}
				else if(!strcmp(token, "LOADAVG"))
				{
					token = strtok(NULL, "\2");
					bar->samples[0][0] = atof(token);
					token = strtok(NULL, "\2");
					bar->samples[1][0] = atof(token);
					token = strtok(NULL, "\2");
					bar->samples[2][0] = atof(token);;
				}
				else if(!strcmp(token, "DISKSPACE"))
				{
					for(int i = 0; i < bar->numSensors; i++)
					{
						token = strtok(NULL, "\2");
						bar->samples[i][0] = atof(token);
						token = strtok(NULL, "\2");
						bar->tempData[i][0] = atof(token);
						token = strtok(NULL, "\2");
						bar->tempData[i][1] = atof(token);
					}
				}
				else if(!strcmp(token, "SWAP"))
				{
					token = strtok(NULL, "\2");
					bar->samples[0][1] = bar->samples[0][0];
					bar->samples[0][0] = atoi(token);
					token = strtok(NULL, "\2");
					bar->max[0] = atoi(token);
				}
				else if(!strcmp(token, "USERSCRIPT"))
				{
					for(int i = 0; i < bar->numSubBars; i++)
					{
						token = strtok(NULL, "\2");
						bar->samples[i][0] = atoi(token);
					}
					
					for(int i = 0; i < bar->numStrings; i++)
					{
						token = strtok(NULL, "\2");
						bar->str[i].text = token;
					}
				}
				else if(!strcmp(token, "VOLUME"))
				{
					token = strtok(NULL, "\2");
					bar->numSubBars = atoi(token);
					token = strtok(NULL, "\2");
					bar->samples[0][0] = atof(token);
					token = strtok(NULL, "\2");
					bar->samples[1][0] = atof(token);
				}
			}
			
			token = strtok(NULL, "\2");
		}
	}
	else if(!strcmp("BUTTONPRESS", token))
	{
		token = strtok(NULL, "\2");
		
		switch(token[0])
		{
			case '0':
				socket.keyState = socket.keyState | G15_KEY_L2;
				break;
			case '1':
				socket.keyState = socket.keyState | G15_KEY_L3;
				break;
			case '2':
				socket.keyState = socket.keyState | G15_KEY_L4;
				break;
			case '3':
				socket.keyState = socket.keyState | G15_KEY_L5;
				break;
		}
	}
	else if(!strcmp("BUTTONSTRING", token))
	{
		token = strtok(NULL, "\2");
		int stringNum = atoi(token);
		
		token = strtok(NULL, "\2");
		
		lStrings[stringNum] = token;
		
		for(int i = 0; i < screens[currentScreen].numBars; i++)
		{
			if(screens[currentScreen].bars[i].buttons[0] == stringNum)
				screens[currentScreen].bars[i].defaultDevice = token;
		}
	}
}
