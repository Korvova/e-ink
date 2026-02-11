#include "Display_EPD_W21_spi.h"
#include "Display_EPD_W21.h"

void delay_xms(unsigned int xms)
{
	delay(xms);
}

// Wait for BUSY with timeout, always continue
void lcd_chkstatus(unsigned long timeout_ms)
{
	unsigned long start = millis();
	while (isEPD_W21_BUSY == 0)
	{
		delay(1);
		if (millis() - start > timeout_ms) {
			Serial.printf("  BUSY timeout (%lums)\n", timeout_ms);
			return;
		}
	}
	Serial.printf("  BUSY OK (%lums)\n", millis() - start);
}

// Try BOTH panel settings: first 3-color, if user sends 'm' switch to mono
// Default: 3-color (0x8F/0x4D, VCOM 0x77) — original GoodDisplay reference
static uint8_t panelByte1 = 0x8F;
static uint8_t panelByte2 = 0x4D;
static uint8_t vcomByte = 0x77;

void EPD_SetMono(bool mono)
{
	if (mono) {
		panelByte1 = 0x9F; panelByte2 = 0x0D; vcomByte = 0x97;
		Serial.println("[EPD] Mode: MONOCHROME (0x9F/0x0D, VCOM 0x97)");
	} else {
		panelByte1 = 0x8F; panelByte2 = 0x4D; vcomByte = 0x77;
		Serial.println("[EPD] Mode: 3-COLOR (0x8F/0x4D, VCOM 0x77)");
	}
}

void EPD_Init(void)
{
	Serial.println("[EPD] Init: reset...");
	delay_xms(100);
	EPD_W21_RST_0;
	delay_xms(10);
	EPD_W21_RST_1;
	delay_xms(10);
	Serial.printf("[EPD] BUSY=%d after reset\n", isEPD_W21_BUSY);
	lcd_chkstatus(3000);

	// Commands to BOTH chips (cascade mode)
	EPD_W21_WriteCMD(0x4D);
	EPD_W21_WriteDATA(0x55);

	EPD_W21_WriteCMD(0xA6);
	EPD_W21_WriteDATA(0x38);

	EPD_W21_WriteCMD(0xB4);
	EPD_W21_WriteDATA(0x5D);

	EPD_W21_WriteCMD(0xB6);
	EPD_W21_WriteDATA(0x80);

	EPD_W21_WriteCMD(0xB7);
	EPD_W21_WriteDATA(0x00);

	EPD_W21_WriteCMD(0xF7);
	EPD_W21_WriteDATA(0x02);

	EPD_W21_WriteCMD2(0xAE);   // Slave only
	EPD_W21_WriteDATA2(0xA0);

	EPD_W21_WriteCMD(0xE0);    // Cascade ON
	EPD_W21_WriteDATA(0x01);

	EPD_W21_WriteCMD(0x00);    // Panel setting
	EPD_W21_WriteDATA(panelByte1);
	EPD_W21_WriteDATA(panelByte2);
	Serial.printf("[EPD] Panel: 0x%02X 0x%02X, VCOM: 0x%02X\n", panelByte1, panelByte2, vcomByte);

	EPD_W21_WriteCMD(0x06);    // Booster
	EPD_W21_WriteDATA(0x57);
	EPD_W21_WriteDATA(0x24);
	EPD_W21_WriteDATA(0x28);
	EPD_W21_WriteDATA(0x32);
	EPD_W21_WriteDATA(0x08);
	EPD_W21_WriteDATA(0x48);

	EPD_W21_WriteCMD(0x61);    // Resolution: 680x480
	EPD_W21_WriteDATA(0x02);
	EPD_W21_WriteDATA(0xA8);
	EPD_W21_WriteDATA(0x01);
	EPD_W21_WriteDATA(0xE0);

	EPD_W21_WriteCMD(0x62);
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteDATA(0x00);

	EPD_W21_WriteCMD(0x60);
	EPD_W21_WriteDATA(0x31);

	EPD_W21_WriteCMD(0x50);    // VCOM
	EPD_W21_WriteDATA(vcomByte);

	EPD_W21_WriteCMD(0xE8);
	EPD_W21_WriteDATA(0x01);

	EPD_W21_WriteCMD(0x04);    // Power on
	Serial.println("[EPD] Power on...");
	delay_xms(200);
	Serial.printf("[EPD] BUSY=%d (waiting up to 5s...)\n", isEPD_W21_BUSY);
	lcd_chkstatus(5000);
	// Continue regardless
	Serial.printf("[EPD] BUSY=%d, continuing.\n", isEPD_W21_BUSY);
}

void EPD_DeepSleep(void)
{
	EPD_W21_WriteCMD(0x02);
	delay_xms(1000);
	EPD_W21_WriteCMD(0x07);
	EPD_W21_WriteDATA(0xA5);
	Serial.println("[EPD] Sleep.");
}

void EPD_Update(void)
{
	EPD_W21_WriteCMD(0x12);
	delay_xms(1);
	Serial.println("[EPD] Refreshing (waiting 15s)...");
	delay_xms(15000);
	Serial.printf("[EPD] BUSY=%d after refresh.\n", isEPD_W21_BUSY);
}

// White screen — send 0xFF to both chips
void EPD_WhiteScreen_White(void)
{
	unsigned int i;
	Serial.println("[EPD] Data: WHITE to both chips...");

	EPD_W21_WriteCMD1(0x10);
	for (i = 0; i < EPD_ARRAY; i++) EPD_W21_WriteDATA1(0xFF);
	EPD_W21_WriteCMD1(0x13);
	for (i = 0; i < EPD_ARRAY; i++) EPD_W21_WriteDATA1(0xFF);

	EPD_W21_WriteCMD2(0x10);
	for (i = 0; i < EPD_ARRAY; i++) EPD_W21_WriteDATA2(0xFF);
	EPD_W21_WriteCMD2(0x13);
	for (i = 0; i < EPD_ARRAY; i++) EPD_W21_WriteDATA2(0xFF);

	EPD_Update();
}

// Black screen — send 0x00 to both chips
void EPD_WhiteScreen_Black(void)
{
	unsigned int i;
	Serial.println("[EPD] Data: BLACK to both chips...");

	EPD_W21_WriteCMD1(0x10);
	for (i = 0; i < EPD_ARRAY; i++) EPD_W21_WriteDATA1(0x00);
	EPD_W21_WriteCMD1(0x13);
	for (i = 0; i < EPD_ARRAY; i++) EPD_W21_WriteDATA1(0x00);

	EPD_W21_WriteCMD2(0x10);
	for (i = 0; i < EPD_ARRAY; i++) EPD_W21_WriteDATA2(0x00);
	EPD_W21_WriteCMD2(0x13);
	for (i = 0; i < EPD_ARRAY; i++) EPD_W21_WriteDATA2(0x00);

	EPD_Update();
}

// Frame buffer to both chips
void EPD_WhiteScreen_ALL(const uint8_t *datas)
{
	unsigned int i, j;
	Serial.println("[EPD] Data: frame buffer to both chips...");

	EPD_W21_WriteCMD1(0x10);
	for (j = 0; j < 480; j++)
		for (i = 0; i < 85; i++)
			EPD_W21_WriteDATA1(~datas[170 * j + i]);
	EPD_W21_WriteCMD1(0x13);
	for (j = 0; j < 480; j++)
		for (i = 0; i < 85; i++)
			EPD_W21_WriteDATA1(~datas[170 * j + i]);

	EPD_W21_WriteCMD2(0x10);
	for (j = 0; j < 480; j++)
		for (i = 85; i < 170; i++)
			EPD_W21_WriteDATA2(~datas[170 * j + i]);
	EPD_W21_WriteCMD2(0x13);
	for (j = 0; j < 480; j++)
		for (i = 85; i < 170; i++)
			EPD_W21_WriteDATA2(~datas[170 * j + i]);

	EPD_Update();
}
