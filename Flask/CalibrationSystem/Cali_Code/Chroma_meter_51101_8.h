#ifndef __CHROMA_METER_51101_8_H
#define __CHROMA_METER_51101_8_H

#define     GET_SENSOR_DATA         0x00
#define     SENSOR_TYPE_SETTING     0x02
#define     GET_DEVICE_ADDRESS      0x10

#define     VA_480      0x08
#define     VA_10       0x0B

/* function -----------------------------------------------*/
extern int init_serial(const char *portname);
extern int Read_Packet(int fd, unsigned char command, uint8_t param, uint8_t response_length, char *response);
extern int Write_Packet(int fd, unsigned char command, uint8_t param1, uint8_t param2, uint8_t response_length);

#endif /* __CHROMA_METER_51101_8_H */
