#include <Arduino.h>
#define NSAMPLE 24
#define NSLOT 4

class MAF
{
private:
    long buff[NSLOT];
    int next;

public:
    MAF()
    {
        next = 0;
    }
    //MOVING AVERAGE FILTER
    long filter(long val)
    {
        buff[next] = val;
        next = (next + 1) % NSLOT;
        long total = 0;
        for (int ctr = 0; ctr < NSLOT; ctr++)
        {
            total += buff[ctr];
        }
        return total / NSLOT;
    }
};
class DCF
{
private:
    long totalAvg;

public:
    DCF()
    {
        totalAvg = 0;
    }
    //DC COMPONENT REMOVAL
    long filter(long sample)
    {
        totalAvg += (sample - totalAvg / NSAMPLE);
        return (sample - totalAvg / NSAMPLE);
    }
    long avgDC()
    {
        return totalAvg / NSAMPLE;
    }
};
class Pulse
{
private:
    DCF dc;
    MAF ma;
    long ampTotal;
    long cycleMin;
    long cycleMax;
    bool pos;
    long prevSig;

public:
    Pulse()
    {
        ampTotal = 0;
        cycleMin = 0;
        cycleMax = 0;
        pos = false;
        prevSig = 0;
    }
    long DCFilter(long sample)
    {
        return dc.filter(sample);
    }
    long MAFilter(long sample)
    {
        return ma.filter(sample);
    }
    long avgDC()
    {
        return dc.avgDC();
    }
    long avgAC()
    {
        return ampTotal / 4;
    }
};