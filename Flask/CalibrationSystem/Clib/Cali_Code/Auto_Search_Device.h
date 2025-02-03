#ifndef __AUTO_SEARCH_DEVICE_H
#define __AUTO_SEARCH_DEVICE_H

#define MAX_DEVICES 10
#define MAX_NAME_LENGTH 256

#define SCPI_IDN "*IDN?\n"

//-------------Meter 51101-8-------------//
#define     GET_SENSOR_DATA         0x00
#define     SENSOR_TYPE_SETTING     0x02
#define     GET_DEVICE_ADDRESS      0x10

#endif /* __AUTO_SEARCH_DEVICE_H */