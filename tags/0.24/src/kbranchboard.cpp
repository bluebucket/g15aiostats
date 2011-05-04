#include "common.h"

#ifdef KBRANCH_BOARD

#include <fcntl.h>
#include "font.h" // 6x4, 7x5, 8x8

uint8_t monoBuffer[screenWidth][screenHeight / 8];
uint8_t lastMonoBuffer[screenWidth][screenHeight / 8];
uint16_t colorBuffer[132][132];
uint16_t lastColorBuffer[132][132];

unsigned int buttons = 0;
int defaultMode;
FILE *iFile;

void PutCLarge(unsigned char character, unsigned int sx, unsigned int sy, int screen);
void PutCMedium(unsigned char character, unsigned int sx, unsigned int sy, int screen);
void PutCSmall(unsigned char character, unsigned int sx, unsigned int sy, int screen);
void FlipBuffer();

void LCDInit()
{
	for(int x = 0; x < screenWidth; x++)
	{
		for(int y = 0; y < screenHeight / 8; y++)
		{
			monoBuffer[x][y] = 0;
			lastMonoBuffer[x][y] = 0;
		}
	}
	
	for(int x = 0; x < 132; x++)
	{
		for(int y = 0; y < 132; y++)
		{
			colorBuffer[x][y] = 0;
			lastColorBuffer[x][y] = 0;
		}
	}
	
	iFile = fopen("/dev/avr_serial", "r+");
	
	if(iFile)
	{
		defaultMode = fcntl(fileno(iFile), F_GETFL, 0);
		setvbuf(iFile, NULL, _IONBF, NULL);
	}
	else
		cout << "Error opening serial port" << endl;
}

void ColorLCDClear()
{
	for(uint8_t x = 0; x < 132; x++)
	{
		for(uint8_t y = 0; y < 132; y++)
		{
			colorBuffer[x][y] = 0;
		}
	}
}

void LCDClear()
{
	for(uint8_t x = 0; x < screenWidth; x++)
	{
		for(uint8_t y = 0; y < screenHeight / 8; y++)
		{
			monoBuffer[x][y] = 0;
		}
	}
}

void ColorLCDFill()
{
	for(uint8_t x = 0; x < 132; x++)
	{
		for(uint8_t y = 0; y < 132; y++)
		{
			colorBuffer[x][y] = 0xFFF;
		}
	}
}

void LCDFill()
{
	for(uint8_t x = 0; x < screenWidth; x++)
	{
		for(uint8_t y = 0; y < screenHeight / 8; y++)
		{
			monoBuffer[x][y] = 0xFF;
		}
	}
}

void ColorLCDSetPixel(uint8_t x, uint8_t y, uint16_t value)
{
	if(x < 0 || x >= 132 || y < 0 || y >= 132)
		return;
	
	colorBuffer[x][y] = value;
}

void LCDSetPixel(uint8_t x, uint8_t y, uint8_t value)
{
	if(x < 0 || x >= screenWidth || y < 0 || y >= screenHeight)
		return;
	
	uint8_t bit = y % 8;
	uint8_t line = y / 8;
	
	if(value)
		monoBuffer[x][line] |= 1 << bit;
	else
		monoBuffer[x][line] &= ~(1 << bit);
}

void LCDDrawRect(int x1, int y1, int x2, int y2, int value, int fill)
{
	if(x2 < x1 || y2 < y1)
		return;
	
	if(fill)
	{
		for(int x = x1; x <= x2; x++)
		{
			for(int y = y1; y <= y2; y++)
			{
				LCDSetPixel(x, y, value);
			}
		}
	}
	else
	{
		for(int x = x1; x <= x2; x++)
		{
			LCDSetPixel(x, y1 - 1, value);
			LCDSetPixel(x, y2 + 1, value);
		}
		
		for(int y = y1; y <= y2; y++)
		{
			LCDSetPixel(x1, y, value);
			LCDSetPixel(x2, y, value);
		}
	}
}

void LCDDrawBar(int x1, int y1, int x2, int y2, int border, long long int value, long long int max)
{
	if(border)
		LCDDrawRect(x1, y1, x2, y2, 1, 0);
	
	int barLength = (float(value) / float(max)) * (x2 - x1);
	
	LCDDrawRect(x1, y1, x1 + barLength, y2, 1, 1);
}

void LCDPutCh(uint8_t ch, int charX, int charY, int size, int screen)
{
	if (ch < 32) ch = 32;
	if (ch > 128) ch = 128;
	
	switch(size)
	{
		case 0:
			PutCSmall(ch, charX, charY, screen);
			break;
		case 1:
			PutCMedium(ch, charX, charY, screen);
			break;
		case 2:
			PutCLarge(ch, charX, charY, screen);
			break;
		case 3:
			PutBitmapC(bigFont, ch, charX, charY, 15, 24);
	}
}

void PutCLarge(unsigned char character, unsigned int sx, unsigned int sy, int screen)
{
	int helper = character * 8;	/* for our font which is 8x8 */
	
	int top_left_pixel_x = sx; /* 1 pixel spacing */
	int top_left_pixel_y = sy; /* once again 1 pixel spacing */
	
	int x, y;
	for (y = 0; y < 8; ++y)
	{
		for (x = 0; x < 8; ++x)
		{
			char font_entry = fontdata_8x8[helper + y];
			
			if (font_entry & 1 << (7 - x))
			{
				if(screen == 0)
					LCDSetPixel(top_left_pixel_x + x, top_left_pixel_y + y, 1);
				else
				{
					ColorLCDSetPixel(top_left_pixel_x + x, top_left_pixel_y + y, 0xFFF);
				}
			}
			else
			{
				if(screen == 0)
					LCDSetPixel(top_left_pixel_x + x, top_left_pixel_y + y, 0);
				else
				{
					ColorLCDSetPixel(top_left_pixel_x + x, top_left_pixel_y + y, 0);
				}
			}
		}
	}
}

void PutCMedium(unsigned char character, unsigned int sx, unsigned int sy, int screen)
{
	uint8_t x, y;
	const uint8_t* chp;
	uint8_t b;
	
	chp = fontdata_7x5 + 5 * (character-32);
	
	for(x = 0; x < 6; ++x)
	{
		b = *(chp + x);
		for(y = 0; y < 8; ++y)
		{
			if (x < 5 && y < 7)
			{
				if(screen == 0)
					LCDSetPixel(sx + x, sy + y, b & (1<<y));
				else
				{
					if(b & (1 << y))
						ColorLCDSetPixel(sx + x, sy + y, 0xFFF);
					else
						ColorLCDSetPixel(sx + x, sy + y, 0);
				}
			}
			else
			{
				if(screen == 0)
					LCDSetPixel(sx + x, sy + y, 0);
				else
				{
					ColorLCDSetPixel(sx + x, sy + y, 0);
				}
			}
		}
	}
}

void PutCSmall(unsigned char character, unsigned int sx, unsigned int sy, int screen)
{
	int helper = character * 6 * 4;	/* for our font which is 6x4 */
	
	int top_left_pixel_x = sx;	/* 1 pixel spacing */
	int top_left_pixel_y = sy;	/* once again 1 pixel spacing */
	
	int x, y;
	for (y = 0; y < 6; ++y)
	{
		for (x = 0; x < 4; ++x)
		{
			char font_entry = fontdata_6x4[helper + y * 4 + x];
			if(font_entry)
			{
				if(screen == 0)
					LCDSetPixel(top_left_pixel_x + x, top_left_pixel_y + y, 1);
				else
				{
					ColorLCDSetPixel(top_left_pixel_x + x, top_left_pixel_y + y, 0xFFF);
				}
			}
			else
			{
				if(screen == 0)
					LCDSetPixel(top_left_pixel_x + x, top_left_pixel_y + y, 0);
				else
				{
					ColorLCDSetPixel(top_left_pixel_x + x, top_left_pixel_y + y, 0);
				}
			}
		}
	}
}

void PutBitmapC(BMP bitmap, char character, int xDest, int yDest, int width, int height)
{
	uint16_t pixel;
	int row, col;
	
	if(character < 32)
		character = 32;
	if(character > 127)
		character = 127;
	
// 	cout << character << endl;
	
	character -= 32;
	
	row = character / (bitmap.TellWidth() / width);
	col = character % (bitmap.TellWidth() / width);
	
	for(int x = 0; x < width; x++)
	{
		for(int y = 0; y < height; y++)
		{
			int xTemp = col * width + x;
			int yTemp = row * height + y;
			
			pixel = (bitmap(xTemp, yTemp)->Red & 0xF0) << 4;
			pixel |= (bitmap(xTemp, yTemp)->Green & 0xF0);
			pixel |= (bitmap(xTemp, yTemp)->Blue & 0xF0) >> 4;
			
// 			cout << col << ", " << row << ", " << xTemp << ", " << yTemp << ", " << pixel << endl;
// 			cout << xDest + x << ", " << yDest + y << endl;
			ColorLCDSetPixel(xDest + x, yDest + y, pixel);
		}
	}
}

void ColorLCDPutStr(const char* str, int x, int y, int size)
{
	while(*str)
	{
		LCDPutCh(*str++, x, y, size, 1);
		
		if(size < 3)
			x += fontWidths[size];
		else
			x += 15;
	}
}

void LCDPutStr(const char* str, int x, int y, int size)
{
	while(*str)
	{
		LCDPutCh(*str++, x, y, size, 0);
		x += fontWidths[size];
	}
}

void ColorLCDSendBuffer(guint64 timeout)
{
	if(!iFile || timeout <= 0)
		return;
	
	guint64 startTime = GetCurrentTime();
	guint64 currentTime = startTime;
	
	int blockStart = -1;
	uint8_t header[6] = { 'C', 0, 0, 0, 0, 0 };
	
	fcntl(fileno(iFile), F_SETFL, defaultMode);
	
	FlipBuffer();
	
	for(int y = 0; y < 132; y++)
	{
		for(int x = 0; x < 132; x++)
		{
			int changed = 0;
			int nextChanged = 0;
			
			if(colorBuffer[x][y] != lastColorBuffer[x][y])
				changed = 1;
			
			if(x + 1 < 132 && colorBuffer[x + 1][y] != lastColorBuffer[x + 1][y])
				nextChanged = 1;
			
			if(blockStart == -1 && changed)
			{
				blockStart = x;
			}
			
			if(blockStart > -1 && changed && !nextChanged)
			{
				int length = x - blockStart + 1;
				int blockEnd;
				
				if((length == 1 || length % 2) && x < 131)
				{
					blockEnd = x + 1;
					length++;
				}
				else if((length == 1 || length % 2) && x == 131)
				{
					blockEnd = x;
					blockStart = x - 1;
					length++;
				}
				else
				{
					blockEnd = x;
				}
				
				
				header[1] = length * 1.5;
				header[2] = blockStart;
				header[3] = y;
				header[4] = blockEnd;
				header[5] = y;
				
				for(int i = 0; i < 6; i++)
				{
					putc(header[i], iFile);
				}
				
				if((blockEnd - blockStart + 1) % 2)
					cout << blockStart << ", " << blockEnd << ", " << length << endl;
				
				for(int i = blockStart; i <= blockEnd; i += 2)
				{
					uint8_t bytes[3];
					bytes[0] = colorBuffer[i][y] >> 4;
					bytes[1] = ((colorBuffer[i][y] & 0x000F) << 4) | (colorBuffer[i + 1][y] >> 8);
					bytes[2] = colorBuffer[i + 1][y] & 0x00FF;
					
					putc(bytes[0], iFile);
					putc(bytes[1], iFile);
					putc(bytes[2], iFile);
					
					lastColorBuffer[i][y] = colorBuffer[i][y];
					lastColorBuffer[i + 1][y] = colorBuffer[i + 1][y];
				}
				
				blockStart = -1;
				
				if((currentTime = GetCurrentTime()) - startTime >= timeout)
				{
					break;
				}
			}
		}
	}
	
	FlipBuffer();
}

void LCDSendBuffer()
{
	if(!iFile)
		return;
	
	int blockStart = -1;
	uint8_t header[5] = { 'D', 0, 64, 0, 0 };
	
	fcntl(fileno(iFile), F_SETFL, defaultMode);
	
	for(int y = 0; y < screenHeight / 8; y++)
	{
		for(int chip = 0; chip <= 1; chip++)
		{
			int start = chip * (screenWidth / 2);
			int end = screenWidth / 2 + chip * (screenWidth / 2);
 			
			for(int x = start; x < end; x++)
			{
				int changed = 0;
				int nextChanged = 0;
				
				if(monoBuffer[x][y] != lastMonoBuffer[x][y])
					changed = 1;
				
				if(x + 1 < end && monoBuffer[x + 1][y] != lastMonoBuffer[x + 1][y])
					nextChanged = 1;
				
				if(blockStart == -1 && changed)
				{
					blockStart = x;
				}
				
				if(blockStart > -1 && changed && !nextChanged)
				{
					int length = x - blockStart + 1;
					
					header[1] = chip;
					header[2] = length;
					header[3] = blockStart - start;
					header[4] = y;
					
					for(int i = 0; i < 5; i++)
					{
						putc(header[i], iFile);
					}
					
					for(int i = blockStart; i <= x; i++)
					{
						putc(monoBuffer[i][y], iFile);
					}
					
					blockStart = -1;
				}
			}
		}
	}
	
	for(int x = 0; x < screenWidth; x++)
	{
		for(int y = 0; y < screenHeight / 8; y++)
		{
			lastMonoBuffer[x][y] = monoBuffer[x][y];
		}
	}
}

void LCDCleanup()
{
	fclose(iFile);
}

unsigned int LCDGetInput()
{
	if(!iFile)
		return 0;
	
	int8_t byte;
	static uint8_t lastButtons;
	buttons = 0;
	
	fcntl(fileno(iFile), F_SETFL, defaultMode);
// 	fcntl(fileno(iFile), F_SETFL, defaultMode | O_NONBLOCK);
	
	putc('B', iFile);
	byte = getc(iFile);
	
	if(byte != lastButtons)
	{
		buttons = byte;
		lastButtons = byte;
	}
	
	putc('V', iFile);
	byte = getc(iFile);
	byte *= (100.0/127.0);
	volume = byte << 8;
	volume |= byte;
	
// 	do
// 	{
// 		byte = getc(iFile);
// 		if(byte == 'B')
// 		{
// 			do
// 			{
// 				byte = getc(iFile);
// 			} while(byte < 0);
// 			
// 			buttons = byte;
// 		}
// 		else if(byte == 'V')
// 		{
// 			do
// 			{
// 				byte = getc(iFile);
// 			} while(byte < 0);
// 			
// 			byte *= (100.0/127.0);
// 			volume = byte << 8;
// 			volume |= byte;
// 		}
// 	} while(byte >= 0);
	
	return buttons;
}

void ColorLCDDrawImage(BMP image, int xDest, int yDest, int width, int height, int xOffset, int yOffset)
{
	for(int x = xOffset; x < width + xOffset; x++)
	{
		for(int y = yOffset; y < height + yOffset; y++)
		{
			uint16_t color = ((weather(x, y)->Red & 0xF0) << 4);
			color |= (weather(x, y)->Green & 0xF0);
			color |= (weather(x, y)->Blue >> 4);
			
			ColorLCDSetPixel(x + xDest - xOffset, y + yDest - yOffset, color);
		}
	}
}

void CheckWeather()
{
	static int temp;
	static int conditions;
	
	if(weatherFile)
	{
		char line[256];
		int done = 0;
		
		fgets(line, 256, weatherFile);
		
		if(!ferror(weatherFile) && !feof(weatherFile))
		{
			for(int i = 0; i < 256; i++)
			{
				if(line[i] <= ' ')
				{
					line[i] = 'F';
					break;
				}
			}
			
			ColorLCDClear();
			ColorLCDPutStr(line, 10, 84, 3);
			temp = atoi(line);
			
			while(!done)
			{
				fgets(line, 256, weatherFile);
				
				if(!ferror(weatherFile) && !feof(weatherFile))
				{
					conditions = atoi(line);
					ColorLCDDrawImage(weather, 66, 66, 64, 64, conditions * 64, 0);
					done = 1;
				}
				
				clearerr(weatherFile);
			}
		}
		
		char clockbuf[100];
		time_t time_now; struct tm tm_now;
		
		time(&time_now);
		
		if(localtime_r(&time_now, &tm_now) != NULL && strftime(clockbuf, 100, "%a", &tm_now))
		{
			int clockX = 66 - (strlen(clockbuf) * 15) / 2;
			ColorLCDPutStr(clockbuf, clockX, 8, 3);
			
			strftime(clockbuf, 100, "%m/%d", &tm_now);
			clockX = 66 - (strlen(clockbuf) * 15) / 2;
			ColorLCDPutStr(clockbuf, clockX, 32, 3);
		}
	}
	
	clearerr(weatherFile);
}

void FlipBuffer()
{
	uint16_t tempBuffer[132][132];
	for(int x = 0; x < 132; x++)
	{
		for(int y = 0; y < 132; y++)
		{
			tempBuffer[x][y] = colorBuffer[x][y];
		}
	}
	
	int i = 131;
	int j;
	for(int x = 0; x < 132; x++)
	{
		j = 131;
		for(int y = 0; y < 132; y++)
		{
			colorBuffer[x][y] = tempBuffer[i][j];
			j--;
		}
		i--;
	}
}

#endif