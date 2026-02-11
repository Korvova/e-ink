#ifndef _DISPLAY_EPD_W21_H_
#define _DISPLAY_EPD_W21_H_

#define EPD_WIDTH   1360/2
#define EPD_HEIGHT  480
#define EPD_ARRAY  EPD_WIDTH*EPD_HEIGHT/8

// Full screen refresh display (monochrome GDEM1085T51)
void EPD_Init(void);
void EPD_SetMono(bool mono);
void EPD_WhiteScreen_ALL(const uint8_t* datas);
void EPD_WhiteScreen_White(void);
void EPD_WhiteScreen_Black(void);
void EPD_DeepSleep(void);

#endif
