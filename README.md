# Olfactory Biometric Access Control

AI nose based on [Benjamin Cabé's artificial nose project](https://github.com/kartben/artificial-nose) and also based on [Shawn Hymel's Interpretation](https://github.com/ShawnHymel/ai-nose)

# Instructions

Before you start live inference you will need to collect your own data. To do so please upload the data collection Ardruino script to the Wio Terminal, close down the program and then run the associates data collection python script. 

Run the python script by navigating to the directory of the repository and using the following terminal command:

python3 serial-data-collect-csv.py -p (usb connection) -b 115200 -d (directory to create) -l (label of file)

Here, you want to run the script for around a minute in atleast 5 different envrioments to get a good chance of classification.

Upload data to [Shawn Hymel's Google Colab Notebook](https://github.com/ShawnHymel/ai-nose/blob/main/ai-nose-dataset-curation.ipynb)

Take note of the min and range of your dataset here. Then take the preprocessed data to [Edge Impulse](https://edgeimpulse.com/). Here you can upload your CSV data and follow the instructions to create an Ardruino classification package. Add this package to your Ardruino library and input the name of your .h file into the live classification source code, under packages.

Hardware Needed:

[Wio Terminal](https://www.digikey.com/en/products/detail/seeed-technology-co-ltd/102991299/11689373)
[Multichannel Gas Sensor V2](https://wiki.seeedstudio.com/Grove-Multichannel-Gas-Sensor-V2/)
[Temperature, Pressure](https://wiki.seeedstudio.com/Grove-Temperature_Humidity_Pressure_Gas_Sensor_BME680/)
[VOC and CO2 Gas Sensor](https://wiki.seeedstudio.com/Grove-VOC_and_eCO2_Gas_Sensor-SGP30/)
[Goove Smart Bulb](https://uk.govee.com/products/wi-fi-led-bulb)
SD card
Appropriate connection wires

You should let the gas sensors preheat for >24 hours, as less than this they may not be as accurate. 

Install the libraries outlined in the source code using the Ardruino package manager. Input your own SSID, password and API Key.

Connect the listed sensors to the Wio Terminal. Upload the live classification source code to the Wio Terminal and look for the predicted odor on the LCD screen. You can also open a serial terminal to view readings.

## License

Unless otherwise specified, all code is licensed under the [Zero-Cluase BSD license (0BSD)](https://opensource.org/licenses/0BSD).

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.