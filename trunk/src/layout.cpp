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

void LayoutBars(Bar *bars)
{
	int screenWidth = 160;
	int screenHeight = 43;
	int totalLineHeight = 0;
	double lineSpacing = 0;
	double currentLineY = 0;
	int numLines = 0;
	int lineIndices[MAXBARS];
	int lineHeights[MAXBARS];
	int leftWidth[MAXBARS];
	int rightWidth[MAXBARS];
	int numJust[3][MAXBARS];
	
	for(int i = 0; i < MAXBARS; i++)
	{
		lineIndices[i] = -1;
		lineHeights[i] = 0;
		leftWidth[i] = 0;
		rightWidth[i] = 1;
		
		for(int j = 0; j < 3; j++)
			numJust[j][i] = 0;
	}
	
	int lastLine = -1;
	for(int i = 0; i < numBars; i++)
	{
		if(bars[i].line != lastLine)
		{
			lineIndices[bars[i].line] = i;
			lastLine = bars[i].line;
			if(bars[i].line + 1 > numLines)
				numLines = bars[i].line + 1;
		}
	}
	
	lineIndices[numLines] = numBars;
	
	for(int i = 0; i < numLines; i++)
	{
		for(int j = lineIndices[i]; j < FindNextLine(lineIndices, i); j++)
		{
			if(j != -1 && !bars[j].manualLayout)
			{
				int tempHeight = 0;
				for(int k = 0; k < bars[j].numStrings; k++)
				{
					int tempWidth = bars[j].str[k].text.length() * fontWidths[bars[j].fontSize];
					if(bars[j].str[k].just == LEFT && tempWidth > leftWidth[j])
						leftWidth[j] = tempWidth;
					else if(bars[j].str[k].just == RIGHT && tempWidth > rightWidth[j])
						rightWidth[j] = tempWidth;
					
					numJust[bars[j].str[k].just][j]++;
				}
				
				int tempWidth = bars[j].label.text.length() * fontWidths[bars[j].fontSize] + 1;
				if(bars[j].label.just == LEFT && tempWidth > leftWidth[j] && tempWidth > 1)
					leftWidth[j] = tempWidth;
				else if(bars[j].label.just == RIGHT && tempWidth > rightWidth[j])
					rightWidth[j] = tempWidth;
				
				if(bars[j].label.text.length() > 0)
					numJust[bars[j].label.just][j]++;
				
				if(leftWidth[j] > 0)
					leftWidth[j]++;
				if(rightWidth[j] > 1)
					rightWidth[j] += 2;
				
				bars[j].oldLeftWidth = leftWidth[j];
				bars[j].oldRightWidth = rightWidth[j];
				
				int tempNumJust = 0;
				for(int k = 0; k < 3; k++)
				{
					if(numJust[k][j] > tempNumJust)
						tempNumJust = numJust[k][j];
				}
				
				tempHeight = fontHeights[bars[j].fontSize] * tempNumJust;
				
				if(bars[j].h + 2 > tempHeight)
					tempHeight = bars[j].h + 2;
				
				if(tempHeight > lineHeights[i])
					lineHeights[i] = tempHeight;
			}
			else if(j == -1)
			{
				lineHeights[i] = 5;
				break;
			}
		}
		totalLineHeight += lineHeights[i];
	}
	
	lineSpacing = double(screenHeight - totalLineHeight) / double(numLines - 1);
	
	for(int i = 0; i < numLines; i++) // Loops through all lines
	{
		for(int j = lineIndices[i]; j < FindNextLine(lineIndices, i); j++) // Loops through all bars within the line
		{
			if(j != -1 && !bars[j].manualLayout)
			{
				int sectionNum = j - lineIndices[i]; // Which section number within the line we're on
				int sectionStart = screenWidth / bars[j].linePop * sectionNum; // X coordinate of the start of the current section
				int nextSectionStart = screenWidth / bars[j].linePop * (sectionNum + 1); // X coordinate of the start of the next section
				int sectionWidth = nextSectionStart - sectionStart; // Total width available for the current section
				
				bars[j].sectionNum = sectionNum;
				
				if(bars[j].xFixed)
					bars[j].x = bars[j].xFixed;
				else
					bars[j].x = sectionStart + leftWidth[j]; // Sets the bar's X coord to the end of the label
				
				if(bars[j].yFixed)
					bars[j].y = bars[j].yFixed;
				else
					bars[j].y = currentLineY + lineHeights[i] / 2.0 - bars[j].h / 2.0; // Vertically centers the bar on the line
				
				bars[j].x += bars[j].xMod;
				bars[j].y += bars[j].yMod;
				
				int extraSpacer = 0;
				if(lineHeights[i] + lineSpacing * 2 >= (fontHeights[bars[j].fontSize] + 1) * numJust[bars[j].label.just][j])
					extraSpacer = 1; // Adds a pixel between strings if there's space
				
				if(bars[j].label.yFixed)
					bars[j].label.y = bars[j].label.yFixed;
				else
					bars[j].label.y = currentLineY + lineHeights[i] / 2.0 - ((fontHeights[bars[j].fontSize] + extraSpacer) * numJust[bars[j].label.just][j]) / 2.0 + 1; //Vertically centers the label on the line
				
				if(bars[j].label.xFixed)
				{
					bars[j].label.x = bars[j].label.xFixed;
				}
				else
				{
					switch(bars[j].label.just)
					{
						case RIGHT:
							bars[j].label.x = nextSectionStart - bars[j].label.text.length() * fontWidths[bars[j].fontSize] - 1;
							break;
						
						case CENTER:
							bars[j].label.x = sectionStart + (sectionWidth - bars[j].label.text.length() * fontWidths[bars[j].fontSize]) / 2.0 ;
							break;
						
						case LEFT:
							bars[j].label.x = sectionStart;
							break;
					}
				}
				
				bars[j].label.x += bars[j].label.xMod;
				bars[j].label.y += bars[j].label.yMod;
				
				int currentString[3];
				for(int k = 0; k < 3; k++)
				{
					currentString[k] = 0;
					if(bars[j].label.just == k && bars[j].label.text.length() > 0)
						currentString[k] = 1;
				}
				
				for(int k = 0; k < bars[j].numStrings; k++) // Loops through all strings in the bar
				{
					if(bars[j].str[k].xFixed)
					{
						bars[j].str[k].x = bars[j].str[k].xFixed;
					}
					else
					{
						switch(bars[j].str[k].just)
						{
							case RIGHT:
								bars[j].str[k].x = nextSectionStart - bars[j].str[k].text.length() * fontWidths[bars[j].fontSize];
								break;
							
							case CENTER:
								bars[j].str[k].x = sectionStart + (sectionWidth - double(bars[j].str[k].text.length()) * fontWidths[bars[j].fontSize]) / 2.0;
								break;
							
							case LEFT:
								bars[j].str[k].x = sectionStart;
								break;
						}
						
						if(j == lineIndices[i + 1] - 1)
							bars[j].str[k].x++; // Removes the padding pixel if this is the last section on the line
					}
					
					if(bars[j].str[k].yFixed)
					{
						bars[j].str[k].y = bars[j].str[k].yFixed;
					}
					else
					{
						extraSpacer = 0;
						if(lineHeights[i] + lineSpacing * 2 >= (fontHeights[bars[j].fontSize] + 1) * numJust[bars[j].str[k].just][j])
							extraSpacer = 1; // Adds a pixel between strings if there's space
						
						double lineMid = currentLineY + lineHeights[i] / 2.0; // Vertical middle of the line
						int singleStrHeight = fontHeights[bars[j].fontSize] + extraSpacer; // Height of a single string
						double halfTotalStrHeight = (singleStrHeight * numJust[bars[j].str[k].just][j] - extraSpacer) / 2.0; // Half the height of all strings combined
						bars[j].str[k].y = lineMid - halfTotalStrHeight + singleStrHeight * currentString[bars[j].str[k].just];
						currentString[bars[j].str[k].just]++;
					}
					
					bars[j].str[k].x += bars[j].str[k].xMod;
					bars[j].str[k].y += bars[j].str[k].yMod;
				}
				
				if(bars[j].wFixed)
					bars[j].w = bars[j].wFixed;
				else
					bars[j].w = nextSectionStart - rightWidth[j] - bars[j].x; // Sets the bar's width so it's up against the edge of the left most string
				
				bars[j].w += bars[j].wMod;
			}
			else if(j == -1)
			{
				break;
			}
		}
		
		currentLineY += lineHeights[i] + lineSpacing; // Increments the line
	}
	
	for(int i = 0; i < numBars; i++)
	{
		if(!autoLayout || bars[i].manualLayout)
		{
			for(int j = 0; j < bars[i].numStrings; j++)
			{
				bars[i].str[j].x = bars[i].str[j].xFixed;
				bars[i].str[j].y = bars[i].str[j].yFixed;
				
				bars[i].str[j].x += bars[i].str[j].xMod;
				bars[i].str[j].y += bars[i].str[j].yMod;
			}
			
			bars[i].label.x = bars[i].label.xFixed;
			bars[i].label.y = bars[i].label.yFixed;
			
			bars[i].label.x += bars[i].label.xMod;
			bars[i].label.y += bars[i].label.yMod;
			
			bars[i].x = bars[i].xFixed;
			bars[i].y = bars[i].yFixed;
			
			bars[i].x += bars[i].xMod;
			bars[i].y += bars[i].yMod;
		}
	}
}

void RepositionBar(Bar &bar)
{
	if(autoLayout && !bar.manualLayout)
	{
		int screenWidth = 160;
		int leftWidth = 0;
		int rightWidth = 1;
		int textChanged = 0;
		
		for(int i = 0; i < bar.numStrings; i++)
		{
			int tempWidth = bar.str[i].text.length() * fontWidths[bar.fontSize];
			if(bar.str[i].just == LEFT && tempWidth > leftWidth)
				leftWidth = tempWidth;
			else if(bar.str[i].just == RIGHT && tempWidth > rightWidth)
				rightWidth = tempWidth;
			
			if(bar.str[i].just == CENTER)
			{
				if(bar.str[i].text.length() != bar.oldCenterWidth[i])
					textChanged = 1;
			}
		}
		
		int tempWidth = bar.label.text.length() * fontWidths[bar.fontSize] + 1;
		if(bar.label.just == LEFT && tempWidth > leftWidth && tempWidth > 1)
			leftWidth = tempWidth;
		else if(bar.label.just == RIGHT && tempWidth > rightWidth)
			rightWidth = tempWidth;
		
		if(leftWidth > 0)
			leftWidth++;
		if(rightWidth > 1)
			rightWidth += 2;
		
		if(leftWidth != bar.oldLeftWidth || rightWidth != bar.oldRightWidth)
			textChanged = 1;
		
		if(textChanged)
		{
			int sectionNum = bar.sectionNum;
			int sectionStart = screenWidth / bar.linePop * sectionNum; // X coordinate of the start of the current section
			int nextSectionStart = screenWidth / bar.linePop * (sectionNum + 1); // X coordinate of the start of the next section
			int sectionWidth = nextSectionStart - sectionStart; // Total width available for the current section
			
			if(bar.xFixed)
				bar.x = bar.xFixed;
			else
				bar.x = sectionStart + leftWidth; // Sets the bar's X coord to the end of the label
			
			bar.x += bar.xMod;
			
			if(bar.label.xFixed)
			{
				bar.label.x = bar.label.xFixed;
			}
			else
			{
				switch(bar.label.just)
				{
					case RIGHT:
						bar.label.x = nextSectionStart - bar.label.text.length() * fontWidths[bar.fontSize] - 1;
						break;
						
					case CENTER:
						bar.label.x = sectionStart + (sectionWidth - bar.label.text.length() * fontWidths[bar.fontSize]) / 2.0 ;
						break;
						
					case LEFT:
						bar.label.x = sectionStart;
						break;
				}
			}
			
			bar.label.x += bar.label.xMod;
			
			for(int i = 0; i < bar.numStrings; i++) // Loops through all strings in the bar
			{
				if(bar.str[i].xFixed)
				{
					bar.str[i].x = bar.str[i].xFixed;
				}
				else
				{
					switch(bar.str[i].just)
					{
						case RIGHT:
							bar.str[i].x = nextSectionStart - bar.str[i].text.length() * fontWidths[bar.fontSize];
							break;
							
						case CENTER:
							bar.str[i].x = sectionStart + (sectionWidth - double(bar.str[i].text.length()) * fontWidths[bar.fontSize]) / 2.0 ;
							break;
							
						case LEFT:
							bar.str[i].x = sectionStart;
							break;
					}
					
					if(sectionNum == bar.linePop)
						bar.str[i].x++; // Removes the padding pixel if this is the last section on the line
				}
				
				bar.str[i].x += bar.str[i].xMod;
			}
			
			if(bar.wFixed)
				bar.w = bar.wFixed;
			else
				bar.w = nextSectionStart - rightWidth - bar.x; // Sets the bar's width so it's up against the edge of the left most string
			
			bar.w += bar.wMod;
			
			bar.oldLeftWidth = leftWidth;
			bar.oldRightWidth = rightWidth;
		}
	}
}

int FindNextLine(int lineIndices[MAXBARS], int index)
{
	int nextLine = -1;
	

	for(int i = index + 1; i < MAXBARS; i++)
	{
		if(lineIndices[i] != -1)
		{
			nextLine = lineIndices[i];
			break;
		}
	}
	
	if(nextLine == -1)
		nextLine = lineIndices[index] + 1;
	
	return nextLine;
}
