#include "Display_EPD_W21_spi.h"
#include <SPI.h>

//SPI write byte
void SPI_Delay(uint8_t xrate)
{
	while(xrate--);

}

void SPI_Write(uint8_t value)                                    
{                                                           
	uint8_t i;
	EPD_W21_CLK_1;
	for(i=0; i<8; i++)   
	{
		if(value & 0x80)
			EPD_W21_MOSI_1;
		else
			EPD_W21_MOSI_0;		
		EPD_W21_CLK_0;
		SPI_Delay(1);	
    EPD_W21_CLK_1;	
		SPI_Delay(1);	
	  value = (value << 1);				
	}
}
//SPI write command
void EPD_W21_WriteCMD(uint8_t command)
{
  EPD_W21_CS_1;
  EPD_W21_CS2_1;
  EPD_W21_CS_0;  
  EPD_W21_CS2_0;        
	//DC--0
  EPD_W21_MOSI_0; // command write
  EPD_W21_CLK_0;
  SPI_Delay(1);	
  EPD_W21_CLK_1;
  SPI_Write(command);
  EPD_W21_CS_1;
  EPD_W21_CS2_1;
}
//SPI write data
void EPD_W21_WriteDATA(uint8_t datas)
{
  EPD_W21_CS_1;
  EPD_W21_CS2_1;
  EPD_W21_CS_0;  
  EPD_W21_CS2_0;                  
	//DC--1
	EPD_W21_MOSI_1; // data write
  EPD_W21_CLK_0;
  SPI_Delay(1);	
  EPD_W21_CLK_1;
  SPI_Write(datas);
  EPD_W21_CS_1;
  EPD_W21_CS2_1;
}

void EPD_W21_WriteCMD1(uint8_t command)
{
  EPD_W21_CS2_1;
  EPD_W21_CS_0;                   
	//DC--0
  EPD_W21_MOSI_0; // command write
  EPD_W21_CLK_0;
  SPI_Delay(1);	
  EPD_W21_CLK_1;
  SPI_Write(command);
  EPD_W21_CS_1;
}
void EPD_W21_WriteDATA1(uint8_t datas)
{
  EPD_W21_CS2_1;
  EPD_W21_CS_0;                   
	//DC--1
	EPD_W21_MOSI_1; // data write
  EPD_W21_CLK_0;
  SPI_Delay(1);	
  EPD_W21_CLK_1;
  SPI_Write(datas);
  EPD_W21_CS_1;
}

void EPD_W21_WriteCMD2(uint8_t command)
{
  EPD_W21_CS_1;
  EPD_W21_CS2_0;                   
	//DC--0
  EPD_W21_MOSI_0; // command write
  EPD_W21_CLK_0;
  SPI_Delay(1);	
  EPD_W21_CLK_1;
  SPI_Write(command);
  EPD_W21_CS2_1;
}
void EPD_W21_WriteDATA2(uint8_t datas)
{
  EPD_W21_CS_1;
  EPD_W21_CS2_0;                   
	//DC--1
	EPD_W21_MOSI_1; // data write
  EPD_W21_CLK_0;
  SPI_Delay(1);	
  EPD_W21_CLK_1;
  SPI_Write(datas);
  EPD_W21_CS2_1;
}
