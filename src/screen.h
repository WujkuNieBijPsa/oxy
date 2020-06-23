#include <arduino.h>
#include <TFT_eSPI.h>
class Screen
{
private:
    bool displayed;
    int bgColor;
    int uiMainColor;
    int textColorMain;
    int textColor1;
    int textColor2;
    int errorColor;

public:
    Screen(int _bgColor, int _uiMainColor, int _textColorMain, int _textColor1, int _textColor2, int _errorColor)
    {
        displayed = false;
        bgColor = _bgColor;
        uiMainColor = _uiMainColor;
        textColorMain = _textColorMain;
        textColor1 = _textColor1;
        textColor2 = _textColor2;
        errorColor = _errorColor;
    }
    void draw() {
        
    }
};