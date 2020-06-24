#include <arduino.h>
#include <TFT_eSPI.h>
class Graph
{
private:
    int sizeX, sizeY, dataSeries, xScaleCount, yScaleCount, xOffset, yOffset, bgColor;
    String *xScale;
    String *yScale;
    String title;
    int graphPos[2]; // 0 x, 1 y

public:
    int dataOffset, startPos, lastPos;
    Graph(int _sizeX, int _sizeY, int _dataSeries, int _xScaleCount, int _yScaleCount, int _xOffset, int _yOffset, String _title, int _bgColor)
    {
        dataOffset = 0;
        sizeX = _sizeX;
        sizeY = _sizeY;
        dataSeries = _dataSeries;
        xScaleCount = _xScaleCount;
        yScaleCount = _yScaleCount;
        title = _title;
        xScale = new String[xScaleCount];
        yScale = new String[yScaleCount];
        xOffset = _xOffset;
        yOffset = _yOffset;
        bgColor = _bgColor;
    }
    /* Draw graph area with set parameters */
    void draw(TFT_eSPI tft, int scaleColor, int posX, int posY)
    {
        graphPos[0] = posX;
        graphPos[1] = posY;
        /* Draw background in chosen color */
        tft.fillRect(posX, posY, sizeX, sizeY, bgColor);
        /* Draw scales in chosen color */
        tft.drawLine(posX + xOffset, posY - yOffset, posX + xOffset, posY + sizeY - yOffset, scaleColor);          //vertical scale
        tft.drawLine(posX + xOffset, posY + sizeY - yOffset, posX + sizeX - xOffset, sizeY - yOffset, scaleColor); //horizontal scale
        if (xScaleCount)
        {
            dataOffset = (sizeX - 2 * xOffset - 1) / xScaleCount;
            int xPos = posX + xOffset;
            for (int ctr = 0; ctr < xScaleCount; ctr++)
            {
                tft.drawLine(xPos, posY + sizeY - yOffset, xPos, posY + sizeY - yOffset + 5, scaleColor);
                xPos += dataOffset;
            }
        }
        if (yScaleCount)
        {
            dataOffset = (sizeY - 2 * xOffset - 1) / yScaleCount;
            int yPos = posY + sizeY - yOffset;
            for (int ctr = 0; ctr < xScaleCount; ctr++)
            {
                tft.drawLine(posX + xOffset, yPos, posX + xOffset - 5, yPos, scaleColor);
                yPos -= dataOffset;
            }
        }
        startPos = posX + xOffset + 1;
        lastPos = startPos;
    }
    /* Draw data to graph */
    void dataDraw(TFT_eSPI tft, int val, int color)
    {
        int pos = 0;
        if (lastPos == startPos)
        {
            pos = startPos;
        }
        else
        {
            pos = lastPos + dataOffset;
        }
        tft.fillRect(pos, map(val, 0, 100, graphPos[1] + sizeY - yOffset - 1, graphPos[1] - yOffset), 1, 1, color);
        lastPos = pos;
    }
    /* Clear graph */
    void clear(TFT_eSPI tft)
    {
        tft.fillRect(graphPos[0] + xOffset + 1, graphPos[1], sizeX - xOffset - 2, sizeY - yOffset - 2, bgColor);
        lastPos = startPos;
    }
};