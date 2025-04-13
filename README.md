# homey-alarm-keypad

This is a work in progress.

## WiFi Config

To compile this project, create a file named `wifi_config.h` in the `src` folder with this content:

```c
#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#define WIFI_SSID "YourWiFiSSID"
#define WIFI_PASSWORD "YourWiFiPassword"

#endif
```

## Compile and Upload to ESP32

To compile and upload the code to the ESP32, you can use the PlatformIO command line interface. Make
sure you have PlatformIO installed and then run the following command in the root directory of your
project:

```bash
pio run -t upload && pio device monitor
```

## Use in Neovim

In order to use this project in Neovim, you will have to create the file `compile_commands.json`
file by running the following command:

```bash
pio run -t compiledb
```
