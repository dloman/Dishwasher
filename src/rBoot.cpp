#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include "WifiNetworkData.h"

#define ROM_0_URL  "http://192.168.7.5:80/rom0.bin"
#define ROM_1_URL  "http://192.168.7.5:80/rom1.bin"
#define SPIFFS_URL "http://192.168.7.5:80/spiff_rom.bin"

HttpServer gServer;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void OtaUpdate_CallBack(bool result)
{
	Serial.println("In callback...");
	if(result)
  {
		bool romSlot = rboot_get_current_rom();

    romSlot = !romSlot;
		// set to boot new rom and then reboot
		Serial.printf("Firmware updated, rebooting to rom %d...\r\n", romSlot);
		rboot_set_current_rom(romSlot);
		System.restart();
	}
  else
  {
		Serial.println("Firmware update failed!");
	}
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void OtaUpdate()
{
	Serial.println("Updating...");

	rBootHttpUpdate otaUpdater;

	// select rom slot to flash
	auto bootConfig = rboot_get_config();

	bool romSlot = bootConfig.current_rom;

  romSlot = !romSlot;

	// flash appropriate rom
  auto romUrl = ROM_0_URL;
	if (romSlot)
  {
    romUrl = ROM_1_URL;
	}

  otaUpdater.addItem(bootConfig.roms[romSlot], romUrl);

	// request switch and reboot on success
  otaUpdater.switchToRom(romSlot);
	// and/or set a callback (called on failure or success without switching requested)
	otaUpdater.setCallback(OtaUpdate_CallBack);

	// start update
	otaUpdater.start();
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void switchRoms()
{
	bool romSlot = rboot_get_current_rom();

  romSlot = !romSlot;

	Serial.printf("Swapping from rom %d to rom %d.\r\n", !romSlot, romSlot);

	rboot_set_current_rom(romSlot);

  Serial.println("Restarting...\r\n");

  System.restart();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void status(HttpRequest& request, HttpResponse& response)
{
  response.sendString("Nothing to see here. Move along");
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void updateFirmware(HttpRequest& request, HttpResponse& response)
{
  OtaUpdate();
  response.sendString("trying to update firmware");
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void startmDNS()
{
    struct mdns_info *info =
      (struct mdns_info *)os_zalloc(sizeof(struct mdns_info));
    info->host_name = (char *) "dishwasher";
    info->ipAddr = WifiStation.getIP();
    info->server_name = (char *) "dishwasher";
    info->server_port = 80;
    info->txt_data[0] = (char *) "version = now";
    espconn_mdns_init(info);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void startWebServer()
{
  Serial.println("Connected");
  gServer.listen(80);
  gServer.addPath("/", status);
  gServer.addPath("update", updateFirmware);

	Serial.println("\r\n=== WEB SERVER STARTED ===");
	Serial.println(WifiStation.getIP());
	Serial.println("==============================\r\n");
  startmDNS();
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void init()
{
  Serial.begin(115200); // 115200 by default
  Serial.systemDebugOutput(true); // Debug output to serial

  // mount spiffs
  int Slot = rboot_get_current_rom();
  Serial.printf("\r\nCurrently running rom %d.\r\n", Slot);
  if (Slot)
  {
    Serial.println("fuck");
		debugf("trying to mount spiffs at %x, length %d", 0x40500000, SPIFF_SIZE);
		spiffs_mount_manual(0x40500000, SPIFF_SIZE);
	}
  else
  {
    Serial.println("anus");
		debugf("trying to mount spiffs at %x, length %d", RBOOT_SPIFFS_0 + 0x40200000, SPIFF_SIZE);
		spiffs_mount_manual(RBOOT_SPIFFS_0 + 0x40200000, SPIFF_SIZE);
  }

	WifiAccessPoint.enable(false);

	WifiStation.enable(true);
	WifiStation.config(ssid, password);

	WifiStation.waitConnection(startWebServer);
}
