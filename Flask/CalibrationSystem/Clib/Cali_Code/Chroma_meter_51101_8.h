#ifndef __CHROMA_METER_51101_8_H
#define __CHROMA_METER_51101_8_H

#define     GET_SENSOR_DATA         0x00
#define     SENSOR_TYPE_SETTING     0x02
#define     GET_DEVICE_ADDRESS      0x10

/* function -----------------------------------------------*/
extern int init_serial(const char *portname);
extern int Read_Packet(int fd, unsigned char command, char param, int response_length, char *response);
extern void Write_Packet(int fd, unsigned char command, char param1, char param2, int response_length);

#endif /* __CHROMA_METER_51101_8_H */
