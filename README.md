# METAR_Map

<p align="center">
  <img src=https://github.com/schu-lab/METAR_Map/blob/main/IMG_9553.jpg height="600" />
  <img src=https://github.com/schu-lab/METAR_Map/blob/main/IMG_9548.jpg height="400" />
</p>

Based on Rick Gouin Metar Map: https://www.rickgouin.com/make-your-own-metar-weather-map/

Features:
1. LED indication for various weather stations in the Los Angeles Region.
2. OLED Screen for additional data for John Wayne/Santa Ana Airport Weather Station.

Hardware:
1. NodeMCU ESP-12E with Wifi Module
2. WS2812 LEDs
3. OLED 128 * 64
4. 12" * 12" Shadowbox
5. 5.0V, 2.4 USB Power Supply

Legend:
1.  Green = Normal Ceeling and Visibility
2.  Yellow = Really Windy - Set to > 20 knots if it's a good time to fly a drone
3.  Blue = Marginal (MVFR) -Ceiling 1k to 3k feet / Visibility 3 to 5 miles
4.  Red = Instrument (IFR) - Ceiling 500' to 1k / Visibility 1 to 3 miles
5.  Purple/Magenta = Low Instrument (LIFR) - Ceeling < 500' / Visibility < 1 mile
