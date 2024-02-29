# sht31d_esp32

Code for an sht31-d sensor running on an esp-32 as an aws iot device. Publishes readings via device shadow.

This sensor is meant to integrate with a Greengrass core device. The sensor publishes to its local device shadow and the
Greengrass core can interact with it and decides how frequently to publish the data to the cloud.

When you first boot the device, it will setup an access point. Enter in your wifi ssid and password along with a unique
identifier for the device. This device ID will be stored via LittleFS and published to the device shadow. 

## Development

Copy `secrets.h.example` to `secrets.h` and update with variables created from your AWS account. See
their [blog post](https://aws.amazon.com/blogs/compute/building-an-aws-iot-core-device-using-aws-serverless-and-an-esp32/)
for more details.

## Dependencies

The following libraries must be installed in Arduino IDE:

1. NTPClient by Fabrice Weinberg
2. Adafruit BusIO by Adafruit
3. Adafruit SHT31 Library by Adafruit
4. ArduinoJson by Benoit Blanchon
5. Cooperative Multitasking by Andreas Motzek
6. MQTT by Joel Gaehwiler
7. Time by Michael Margolis
8. WiFiManager by tzapu

## API

This sensor will publish data in the following format to its named device shadow, "readings". Temperature is in celsius.

```json
{
  "state": {
    "reported": {
      "time": "2024-02-29T20:02:53Z",
      "temperature": 22.63999939,
      "relativeHumidity": 45.81999969,
      "sensorId": "your-sensor-id"
    }
  }
}
```


