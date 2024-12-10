/*				The tasks of power meter : 
 * 1. set the voltage range to a value(V500, V300, V150 or AUTO)
 * 2. get the measured voltage value x, where x is a global variable in this project.
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define DEVICE "/dev/usbtmc0"

int main(){
	int fd;
	//~ char write_buffer[] = "*IDN?\n"; //SCPI command, query device ID
	//~ char write_buffer[] = "*STB?\n"; //query the status byte register
	//~ char write_buffer[] = "SYSTem:VERsion?\n"; //query SCPI version
	char Command_set_voltage_range[] = "VOLTage:RANGe AUTO\n"; //set the voltage range to AUTO, there is no need to read response
	char Command_get_voltage_value[] = "MEASure:VOLTage:RMS?\n"; //get the volrage rms value
	char read_buffer[256];
	ssize_t bytes_written, bytes_read;
	
	//open USBTMC device
	fd = open(DEVICE, O_RDWR);
	if(fd < 0){
		perror("Failed to open USBTMC device");
		return 1;
	}
	printf("[Find Deivce]\n");
	
	bytes_written = write(fd, Command_set_voltage_range, strlen(Command_set_voltage_range));
	if(bytes_written < 0){
		perror("Failed to write to USBTMC device");
		close(fd);
		return 1;
	}
	printf("[Set voltage range]\n");
	
	usleep(20000);
	
	bytes_written = write(fd, Command_get_voltage_value, strlen(Command_get_voltage_value));
	if(bytes_written < 0){
		perror("Failed to write to USBTMC device");
		close(fd);
		return 1;
	}
	printf("[Get voltage value]\n");
	
	//~ bytes_written = write(fd, write_buffer, strlen(write_buffer));
	//~ if(bytes_written < 0){
		//~ perror("Failed to write to USBTMC device");
		//~ close(fd);
		//~ return 1;
	//~ }
	//~ printf("[Get voltage value]\n");
	
	
	bytes_read = read(fd, read_buffer, sizeof(read_buffer) - 1);
	if(bytes_read < 0){
		perror("Failed to read from USBTMC device");
		close(fd);
		return 1;
	}
	printf("[Read voltage value]\n");
	read_buffer[bytes_read] = '\0';
	printf("Response from device: %s\n", read_buffer);
	
	close(fd);
	
	return 0;
}
