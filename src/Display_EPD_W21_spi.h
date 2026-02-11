#ifndef _DISPLAY_EPD_W21_SPI_
#define _DISPLAY_EPD_W21_SPI_
#include "Arduino.h"

// Pin mapping can be overridden from platformio.ini build_flags:
// -DEPD_PIN_BUSY=.. -DEPD_PIN_RST=.. -DEPD_PIN_CS2=.. -DEPD_PIN_CS=.. -DEPD_PIN_CLK=.. -DEPD_PIN_MOSI=..
#ifndef EPD_PIN_BUSY
#define EPD_PIN_BUSY 39
#endif
#ifndef EPD_PIN_RST
#define EPD_PIN_RST 38
#endif
#ifndef EPD_PIN_CS2
#define EPD_PIN_CS2 40
#endif
#ifndef EPD_PIN_CS
#define EPD_PIN_CS 37
#endif
#ifndef EPD_PIN_CLK
#define EPD_PIN_CLK 36
#endif
#ifndef EPD_PIN_MOSI
#define EPD_PIN_MOSI 35
#endif

#define EPD_W21_BUSY EPD_PIN_BUSY
#define EPD_W21_RST EPD_PIN_RST
#define EPD_W21_CS2 EPD_PIN_CS2
#define EPD_W21_CS EPD_PIN_CS
#define EPD_W21_CLK EPD_PIN_CLK
#define EPD_W21_MOSI EPD_PIN_MOSI

//IO settings
#define isEPD_W21_BUSY digitalRead(EPD_W21_BUSY)  //BUSY
#define EPD_W21_RST_0 digitalWrite(EPD_W21_RST,LOW)  //RES
#define EPD_W21_RST_1 digitalWrite(EPD_W21_RST,HIGH)
#define EPD_W21_CS2_0 digitalWrite(EPD_W21_CS2,LOW) //CS2
#define EPD_W21_CS2_1 digitalWrite(EPD_W21_CS2,HIGH)
#define EPD_W21_CS_0 digitalWrite(EPD_W21_CS,LOW) //CS
#define EPD_W21_CS_1 digitalWrite(EPD_W21_CS,HIGH)

#define EPD_W21_CLK_0 digitalWrite(EPD_W21_CLK,LOW)   // CLK pin used by CAMERA !!
#define EPD_W21_CLK_1 digitalWrite(EPD_W21_CLK,HIGH)
#define EPD_W21_MOSI_0  digitalWrite(EPD_W21_MOSI,LOW) // MOSI pin used by CAMERA !!
#define EPD_W21_MOSI_1  digitalWrite(EPD_W21_MOSI,HIGH) 



void SPI_Write(uint8_t value);
void EPD_W21_WriteDATA(uint8_t datas);
void EPD_W21_WriteCMD(uint8_t command);
void EPD_W21_WriteDATA1(uint8_t datas);
void EPD_W21_WriteCMD1(uint8_t command);
void EPD_W21_WriteDATA2(uint8_t datas);
void EPD_W21_WriteCMD2(uint8_t command);
#endif 
