#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include "WifiNetworkData.h"

constexpr int BootloaderRom = 0;
constexpr int ApplicationRom = 1;

constexpr int InputPin = 0;

bool gConnected(false);

rBootHttpUpdate* gpOtaUpdater;

HttpServer gServer;

Timer gTimer;

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
    gTimer.restart();
		Serial.println("Firmware update failed!");
	}
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void OtaUpdate(String Url)
{
  gTimer.stop();

	Serial.println("Updating...");

  if (gpOtaUpdater)
  {
    delete gpOtaUpdater;
  }
	gpOtaUpdater = new rBootHttpUpdate();

  auto bootConfig = rboot_get_config();

  { //Testing
    bool romSlot = bootConfig.current_rom;

    Serial.printf(
      "trying to update, currently on %d rebooting to rom %d...url = %s\r\n",
      romSlot,
      !romSlot,
      String(Url + ":8000/rom0.bin").c_str());
  } //Testing

  gpOtaUpdater->addItem(bootConfig.roms[ApplicationRom], Url + ":8000/rom0.bin");
  gpOtaUpdater->addItem(RBOOT_SPIFFS_1, Url + ":8000/spiff_rom.bin");
	// request switch and reboot on success
  gpOtaUpdater->switchToRom(ApplicationRom);
	// and/or set a callback (called on failure or success without switching requested)
	gpOtaUpdater->setCallback(OtaUpdate_CallBack);

	// start update
	gpOtaUpdater->start();
  Serial.println("started");
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void status(
  HttpRequest& request,
  HttpResponse& response,
  HttpServerConnection& connection)
{
	auto DishwasherStatus = digitalRead(InputPin);

	TemplateFileStream *tmpl = new TemplateFileStream("index.html");
	auto &vars = tmpl->variables();
	vars["status"] = String(DishwasherStatus);
	response.sendTemplate(tmpl);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void updateFirmware(
  HttpRequest& request,
  HttpResponse& response,
  HttpServerConnection& connection)
{
  OtaUpdate("http://" + connection.getRemoteIp().toString());
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

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void restartToApplication()
{
  if (gConnected)
  {
    Serial.println("no new application rom exiting bootloader now");

    rboot_set_current_rom(ApplicationRom);

    System.restart();
  }
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

  if (rboot_get_current_rom() == BootloaderRom)
  {
    gTimer.initializeMs(60 * 2000, restartToApplication).start();
    Serial.println("countdown started");
  }
  else
  {
    rboot_set_current_rom(BootloaderRom);
  }
  gConnected = true;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void init()
{
  Serial.begin(115200); // 115200 by default
  Serial.systemDebugOutput(true); // Debug output to serial

  bool CurrentRom = rboot_get_current_rom();

  Serial.printf("\r\nCurrently running rom %d.\r\n", CurrentRom);

	WifiAccessPoint.enable(false);

	WifiStation.enable(true);
	WifiStation.config(ssid, password);

	WifiStation.waitConnection(startWebServer);
}
