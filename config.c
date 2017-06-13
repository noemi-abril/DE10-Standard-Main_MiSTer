// config.c

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "errors.h"
#include "hardware.h"
#include "boot.h"
#include "file_io.h"
#include "osd.h"
#include "fdd.h"
#include "hdd.h"
#include "menu.h"
#include "config.h"
#include "user_io.h"

configTYPE config;
char configfilename[32];
char DebugMode = 0;
unsigned char romkey[3072];

void SendFileV2(fileTYPE* file, unsigned char* key, int keysize, int address, int size)
{
	static uint8_t buf[512];
	int i, j;
	unsigned int keyidx = 0;
	iprintf("File size: %dkB\n", size >> 1);
	iprintf("[");
	if (keysize)
	{
		// read header
		FileReadAdv(file, buf, 0xb);
	}
	for (i = 0; i<size; i++)
	{
		if (!(i & 31)) iprintf("*");
		FileReadAdv(file, buf, 512);
		if (keysize)
		{
			// decrypt ROM
			for (j = 0; j<512; j++)
			{
				buf[j] ^= key[keyidx++];
				if (keyidx >= keysize) keyidx -= keysize;
			}
		}
		EnableOsd();
		unsigned int adr = address + i * 512;
		spi8(OSD_CMD_WR);
		spi8(adr & 0xff); adr = adr >> 8;
		spi8(adr & 0xff); adr = adr >> 8;
		spi8(adr & 0xff); adr = adr >> 8;
		spi8(adr & 0xff); adr = adr >> 8;
		for (j = 0; j<512; j = j + 4)
		{
			spi8(buf[j + 0]);
			spi8(buf[j + 1]);
			spi8(buf[j + 2]);
			spi8(buf[j + 3]);
		}
		DisableOsd();
	}
	iprintf("]\n");
}

//// UploadKickstart() ////
char UploadKickstart(char *name)
{
	fileTYPE file;
	int keysize = 0;

	BootPrint("Checking for Amiga Forever key file:");
	if (FileOpen(&file, "ROM.KEY")) {
		keysize = file.size;
		if (file.size<sizeof(romkey))
		{
			FileReadAdv(&file, romkey, keysize);
			BootPrint("Loaded Amiga Forever key file");
		}
		else
		{
			BootPrint("Amiga Forever keyfile is too large!");
		}
		FileClose(&file);
	}
	BootPrint("Loading file: ");
	BootPrint(name);

	if (FileOpen(&file, name)) {
		if (file.size == 0x100000) {
			// 1MB Kickstart ROM
			BootPrint("Uploading 1MB Kickstart ...");
			SendFileV2(&file, NULL, 0, 0xe00000, file.size >> 10);
			SendFileV2(&file, NULL, 0, 0xf80000, file.size >> 10);
			FileClose(&file);
			return(1);
		}
		else if (file.size == 0x80000) {
			// 512KB Kickstart ROM
			BootPrint("Uploading 512KB Kickstart ...");
			SendFileV2(&file, NULL, 0, 0xf80000, file.size >> 9);
			FileClose(&file);
			FileOpen(&file, name);
			SendFileV2(&file, NULL, 0, 0xe00000, file.size >> 9);
			FileClose(&file);
			return(1);
		}
		else if ((file.size == 0x8000b) && keysize) {
			// 512KB Kickstart ROM
			BootPrint("Uploading 512 KB Kickstart (Probably Amiga Forever encrypted...)");
			SendFileV2(&file, romkey, keysize, 0xf80000, file.size >> 9);
			FileClose(&file);
			FileOpen(&file, name);
			SendFileV2(&file, romkey, keysize, 0xe00000, file.size >> 9);
			FileClose(&file);
			return(1);
		}
		else if (file.size == 0x40000) {
			// 256KB Kickstart ROM
			BootPrint("Uploading 256 KB Kickstart...");
			SendFileV2(&file, NULL, 0, 0xf80000, file.size >> 9);
			FileClose(&file);
			FileOpen(&file, name); // TODO will this work
			SendFileV2(&file, NULL, 0, 0xfc0000, file.size >> 9);
			FileClose(&file);
			return(1);
		}
		else if ((file.size == 0x4000b) && keysize) {
			// 256KB Kickstart ROM
			BootPrint("Uploading 256 KB Kickstart (Probably Amiga Forever encrypted...");
			SendFileV2(&file, romkey, keysize, 0xf80000, file.size >> 9);
			FileClose(&file);
			FileOpen(&file, name); // TODO will this work
			SendFileV2(&file, romkey, keysize, 0xfc0000, file.size >> 9);
			FileClose(&file);
			return(1);
		}
		else {
			BootPrint("Unsupported ROM file size!");
		}
		FileClose(&file);
	}
	else {
		printf("No \"%s\" file!\n", name);
	}
	return(0);
}


//// UploadActionReplay() ////
char UploadActionReplay()
{
	fileTYPE file;
	if (FileOpen(&file, "HRTMON.ROM")) {
		int adr, data;
		puts("Uploading HRTmon ROM... ");
		SendFileV2(&file, NULL, 0, 0xa10000, (file.size + 511) >> 9);
		// HRTmon config
		adr = 0xa10000 + 20;
		spi_osd_cmd32le_cont(OSD_CMD_WR, adr);
		data = 0x00800000; // mon_size, 4 bytes
		spi8((data >> 24) & 0xff); spi8((data >> 16) & 0xff);
		spi8((data >> 8) & 0xff); spi8((data >> 0) & 0xff);
		data = 0x00; // col0h, 1 byte
		spi8((data >> 0) & 0xff);
		data = 0x5a; // col0l, 1 byte
		spi8((data >> 0) & 0xff);
		data = 0x0f; // col1h, 1 byte
		spi8((data >> 0) & 0xff);
		data = 0xff; // col1l, 1 byte
		spi8((data >> 0) & 0xff);
		data = 0x01; // right, 1 byte
		spi8((data >> 0) & 0xff);
		data = 0x00; // keyboard, 1 byte
		spi8((data >> 0) & 0xff);
		data = 0x01; // key, 1 byte
		spi8((data >> 0) & 0xff);
		data = config.enable_ide ? 1 : 0; // ide, 1 byte
		spi8((data >> 0) & 0xff);
		data = 0x01; // a1200, 1 byte
		spi8((data >> 0) & 0xff);
		data = config.chipset&CONFIG_AGA ? 1 : 0; // aga, 1 byte
		spi8((data >> 0) & 0xff);
		data = 0x01; // insert, 1 byte
		spi8((data >> 0) & 0xff);
		data = 0x0f; // delay, 1 byte
		spi8((data >> 0) & 0xff);
		data = 0x01; // lview, 1 byte
		spi8((data >> 0) & 0xff);
		data = 0x00; // cd32, 1 byte
		spi8((data >> 0) & 0xff);
		data = config.chipset&CONFIG_NTSC ? 1 : 0; // screenmode, 1 byte
		spi8((data >> 0) & 0xff);
		data = 1; // novbr, 1 byte
		spi8((data >> 0) & 0xff);
		data = 0; // entered, 1 byte
		spi8((data >> 0) & 0xff);
		data = 1; // hexmode, 1 byte
		spi8((data >> 0) & 0xff);
		DisableOsd();
		adr = 0xa10000 + 68;
		spi_osd_cmd32le_cont(OSD_CMD_WR, adr);
		data = ((config.memory & 0x3) + 1) * 512 * 1024; // maxchip, 4 bytes TODO is this correct?
		spi8((data >> 24) & 0xff); spi8((data >> 16) & 0xff);
		spi8((data >> 8) & 0xff); spi8((data >> 0) & 0xff);
		DisableOsd();

		FileClose(&file);
		return(1);
	}
	else {
		puts("\nhrtmon.rom not found!\n");
		return(0);
	}
	return(0);
}


//// SetConfigurationFilename() ////
void SetConfigurationFilename(int config)
{
	if (config)
		siprintf(configfilename, "MINIMIG%d.CFG", config);
	else
		strcpy(configfilename, "MINIMIG.CFG");
}


//// ConfigurationExists() ////
unsigned char ConfigurationExists(char *filename)
{
	if (!filename) {
		// use slot-based filename if none provided
		filename = configfilename;
	}
	fileTYPE f;
	if(FileOpen(&f, filename))
	{
		FileClose(&f);
		return(1);
	}
	return(0);
}


//// LoadConfiguration() ////
unsigned char LoadConfiguration(char *filename)
{
	static const char config_id[] = "MNMGCFG0";
	char updatekickstart = 0;
	char result = 0;
	unsigned char key, i;
	static configTYPE tmpconf;

	if (!filename)
	{
		// use slot-based filename if none provided
		filename = configfilename;
	}

	fileTYPE file;
	// load configuration data
	if (FileOpen(&file, filename)) {
		FileClose(&file);
		BootPrint("Opened configuration file\n");
		printf("Configuration file size: %s, %lu\n", file.name, file.size);
		if (file.size == sizeof(config))
		{
			if (FileLoad(filename, &tmpconf, sizeof(tmpconf)))
			{
				// check file id and version
				if (strncmp(tmpconf.id, config_id, sizeof(config.id)) == 0) {
					// A few more sanity checks...
					if (tmpconf.floppy.drives <= 4) {
						// If either the old config and new config have a different kickstart file,
						// or this is the first boot, we need to upload a kickstart image.
						if (strcmp(tmpconf.kickstart, config.kickstart) != 0) {
							updatekickstart = true;
						}
						memcpy((void*)&config, (void*)&tmpconf, sizeof(config));
						result = 1; // We successfully loaded the config.
					}
					else BootPrint("Config file sanity check failed!\n");
				}
				else BootPrint("Wrong configuration file format!\n");
			}
			else printf("Cannot load configuration file\n");
		}
		else printf("Wrong configuration file size: %lu (expected: %lu)\n", file.size, sizeof(config));
	}
	if (!result) {
		BootPrint("Can not open configuration file!\n");
		BootPrint("Setting config defaults\n");
		// set default configuration
		memset((void*)&config, 0, sizeof(config));  // Finally found default config bug - params were reversed!
		strncpy(config.id, config_id, sizeof(config.id));
		strcpy(config.kickstart, "KICK.ROM");
		config.memory = 0x15;
		config.cpu = 0;
		config.chipset = 0;
		config.floppy.speed = CONFIG_FLOPPY2X;
		config.floppy.drives = 1;
		config.enable_ide = 0;
		config.hardfile[0].enabled = 1;
		strcpy(config.hardfile[0].long_name, "HARDFILE1.HDF");
		config.hardfile[0].long_name[0] = 0;
		strcpy(config.hardfile[1].long_name, "HARDFILE2.HDF");
		config.hardfile[1].long_name[0] = 0;
		config.hardfile[1].enabled = 1;  // Default is access to entire SD card
		updatekickstart = true;
		BootPrint("Defaults set\n");
	}

	// print config to boot screen
	char cfg_str[41];
	siprintf(cfg_str, "CPU:     %s", config_cpu_msg[config.cpu & 0x03]); BootPrintEx(cfg_str);
	siprintf(cfg_str, "Chipset: %s", config_chipset_msg[(config.chipset >> 2) & 7]); BootPrintEx(cfg_str);
	siprintf(cfg_str, "Memory:  CHIP: %s  FAST: %s  SLOW: %s", config_memory_chip_msg[(config.memory >> 0) & 0x03], config_memory_fast_msg[(config.memory >> 4) & 0x03], config_memory_slow_msg[(config.memory >> 2) & 0x03]); BootPrintEx(cfg_str);

	// wait up to 3 seconds for keyboard to appear. If it appears wait another
	// two seconds for the user to press a key
	/*
	int8_t keyboard_present = 0;
	for(i=0;i<3;i++) {
	unsigned long to = GetTimer(1000);
	//while(!CheckTimer(to)) usb_poll();

	// check if keyboard just appeared
	if(!keyboard_present && hid_keyboard_present()) {
	// BootPrintEx("Press F1 for NTSC, F2 for PAL");
	keyboard_present = 1;
	i = 0;
	}
	}
	*/

	key = OsdGetCtrl();
	if (key == KEY_F1) {
		// BootPrintEx("Forcing NTSC video ...");
		// force NTSC mode if F1 pressed
		config.chipset |= CONFIG_NTSC;
	}

	if (key == KEY_F2) {
		// BootPrintEx("Forcing PAL video ...");
		// force PAL mode if F2 pressed
		config.chipset &= ~CONFIG_NTSC;
	}

	ApplyConfiguration(updatekickstart);

	return(result);
}


//// ApplyConfiguration() ////
void ApplyConfiguration(char reloadkickstart)
{
	ConfigCPU(config.cpu);

	if (reloadkickstart)
	{

	}
	else {
		ConfigChipset(config.chipset);
		ConfigFloppy(config.floppy.drives, config.floppy.speed);
	}

	// Whether or not we uploaded a kickstart image we now need to set various parameters from the config.
	if (OpenHardfile(0)) {
		switch (hdf[0].type) {
			// Customise message for SD card acces
		case (HDF_FILE | HDF_SYNTHRDB) :
			printf("\nHardfile 0 (with fake RDB): %s\n", hdf[0].file.name);
			break;
		case HDF_FILE:
			printf("\nHardfile 0: %s\n", hdf[0].file.name);
			break;
		}
		printf("CHS: %u.%u.%u\n", hdf[0].cylinders, hdf[0].heads, hdf[0].sectors);
		printf("Size: %lu MB\n", ((((unsigned long)hdf[0].cylinders) * hdf[0].heads * hdf[0].sectors) >> 11));
		printf("Offset: %ld\n", hdf[0].offset);
	}

	if (OpenHardfile(1)) {
		switch (hdf[1].type) {
		case (HDF_FILE | HDF_SYNTHRDB) :
			printf("\nHardfile 1 (with fake RDB): %s\n", hdf[1].file.name);
			break;
		case HDF_FILE:
			printf("\nHardfile 1: %s\n", hdf[1].file.name);
			break;
		}
		printf("CHS: %u.%u.%u\n", hdf[1].cylinders, hdf[1].heads, hdf[1].sectors);
		printf("Size: %lu MB\n", ((((unsigned long)hdf[1].cylinders) * hdf[1].heads * hdf[1].sectors) >> 11));
		printf("Offset: %ld\n", hdf[1].offset);
	}

	ConfigIDE(config.enable_ide, config.hardfile[0].present && config.hardfile[0].enabled, config.hardfile[1].present && config.hardfile[1].enabled);

	printf("CPU clock     : %s\n", config.chipset & 0x01 ? "turbo" : "normal");
	printf("Chip RAM size : %s\n", config_memory_chip_msg[config.memory & 0x03]);
	printf("Slow RAM size : %s\n", config_memory_slow_msg[config.memory >> 2 & 0x03]);
	printf("Fast RAM size : %s\n", config_memory_fast_msg[config.memory >> 4 & 0x03]);

	printf("Floppy drives : %u\n", config.floppy.drives + 1);
	printf("Floppy speed  : %s\n", config.floppy.speed ? "fast" : "normal");

	printf("\n");

	printf("\nA600/A1200 IDE HDC is %s.\n", config.enable_ide ? "enabled" : "disabled");
	printf("Master HDD is %s.\n", config.hardfile[0].present ? config.hardfile[0].enabled ? "enabled" : "disabled" : "not present");
	printf("Slave HDD is %s.\n", config.hardfile[1].present ? config.hardfile[1].enabled ? "enabled" : "disabled" : "not present");

#if 0
	if (cluster_size < 64) {
		BootPrint("\n***************************************************");
		BootPrint("*  It's recommended to reformat your memory card  *");
		BootPrint("*   using 32 KB clusters to improve performance   *");
		BootPrint("*           when using large hardfiles.           *");  // AMR
		BootPrint("***************************************************");
	}
	iprintf("Bootloading is complete.\n");
#endif

	printf("\nExiting bootloader...\n");

	ConfigMemory(config.memory);
	ConfigCPU(config.cpu);

	ConfigChipset(config.chipset);
	ConfigFloppy(config.floppy.drives, config.floppy.speed);

	if (reloadkickstart)
	{
		UploadActionReplay();

		printf("Reloading kickstart ...\n");
		WaitTimer(1000);
		EnableOsd();
		spi8(OSD_CMD_RST);
		rstval |= (SPI_RST_CPU | SPI_CPU_HLT);
		spi8(rstval);
		DisableOsd();
		if (!UploadKickstart(config.kickstart))
		{
			strcpy(config.kickstart, "KICK.ROM");
			if (!UploadKickstart(config.kickstart))
			{
				FatalError(6);
			}
		}
		EnableOsd();
		spi8(OSD_CMD_RST);
		rstval |= (SPI_RST_USR | SPI_RST_CPU);
		spi8(rstval);
		DisableOsd();
		EnableOsd();
		spi8(OSD_CMD_RST);
		rstval = 0;
		spi8(rstval);
		DisableOsd();
	}
	else
	{
		printf("Resetting ...\n");
		EnableOsd();
		spi8(OSD_CMD_RST);
		rstval |= (SPI_RST_USR | SPI_RST_CPU);
		spi8(rstval);
		DisableOsd();
		EnableOsd();
		spi8(OSD_CMD_RST);
		rstval = 0;
		spi8(rstval);
		DisableOsd();
	}

	ConfigVideo(config.filter.hires, config.filter.lores, config.scanlines);
}

//// SaveConfiguration() ////
unsigned char SaveConfiguration(char *filename)
{
	if (!filename) {
		// use slot-based filename if none provided
		filename = configfilename;
	}
	return FileSave(filename, &config, sizeof(config));
}
