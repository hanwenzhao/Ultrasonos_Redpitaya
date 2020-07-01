/* ########################################## */
/* INCLUDES */
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <errno.h>
#include <arpa/inet.h> 
#include "redpitaya/rp.h"
#include <linux/spi/spidev.h>
#include <linux/types.h>
#include <stdint.h>
#include <fcntl.h>
#include <memory.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>


/* ########################################## */
/* COMMUNICATION OPTION */
//#define TXTFILE
//#define TCPIP
#define UDP

/* ########################################## */
/* TCP */
#define SERVER_IP "169.254.225.178"

/* ########################################## */
/* UDP */
#define SERVER_LOCAL_IP "169.254.187.68"
//#define SERVER_PUBLIC_IP "98.15.72.232"
//#define CLIENT_PUBLIC_IP "71.127.254.212"


/* ########################################## */
/* RED PITAYA VARIABLES FOR TX AND ADC */
#define TX_FREQ 5000.0
#define TX_AMP 1.0
#define TX_OFFSET 1.0
#define TX_DUTYCYCLE 0.1/(1000000/TX_FREQ) // 0.1us(100ns) / 200us
#define ADC_TRIG_LEVEL 2.0
#define ADC_DECIMATION RP_DEC_8
#define BUFF_SIZE 2500
#define ADC_TRIG_DELAY BUFF_SIZE-8192 // pitaya has a internal 8192 tigger delay
#define TRIG_SOURCE RP_TRIG_SRC_CHB_PE
#define TRIF_LEVEL_SOURCE RP_CH_2
#define BUFFER_FILL_TIME BUFF_SIZE/15.6 + 10 // need a little longer time than just about right

/* ########################################## */
/* I2C DEFINES */
#define I2CBUS "/dev/i2c-0"

/* ########################################## */
/* I2C VARIABLES */
int i2cfd;
int _stat = 0x0b;
int _raw_ang_hi = 0x0c;
int _raw_ang_lo = 0x0d;

/* ########################################## */
/* VARIABLES FOR SPI */
int spi_fd = -1;
static uint32_t mode;
static uint8_t bits = 8;
static uint32_t speed = 1000000;
static uint16_t delay;

/* ########################################## */
/* VARIABLES FOR DATA PROCESSING - DECIMAL*/
struct timeval tv;
unsigned long time_stamp;
uint16_t sweeping_angle;
uint16_t rotation_angle;
uint32_t crc_result;

/* ########################################## */
/* VARIABLES FOR DATA PROCESSING - BYTE */
const unsigned char marker[10] = {0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01};
unsigned char version_number = 0x08;
unsigned char time_stamp_char[4];
unsigned char information_byte = 0xE1;
unsigned char sweeping_angle_char[2];
unsigned char rotation_angle_char[2];
unsigned char qua_wxyz_char[16];
unsigned char adc_char[2*BUFF_SIZE];
unsigned char crc_char[4];

unsigned char crc_input[1+4+1+2+2+16+2*BUFF_SIZE];
unsigned char message_buff[10+1+4+1+2+2+16+2*BUFF_SIZE+4];

/* ########################################## */
/* VARIABLES - OTHER*/
rp_acq_trig_state_t state = RP_TRIG_STATE_WAITING;
int i,e;
char lo;
char hi;
int16_t temp;
uint32_t buff_size = BUFF_SIZE;
int angle_index;
float servo_angle;

/* ########################################## */
/* Functions */
static int SPI_Init();
void TX_Init();
void ADC_Init();
void System_Init();
unsigned long changed_endian_4Bytes(unsigned long num);
int16_t changed_endian_2Bytes(int16_t value);
uint16_t read_encoder();
uint32_t rc_crc32(uint32_t crc, unsigned char *buf, size_t len);
static void transfer(int fd, uint8_t const *tx, uint8_t const *rx, size_t len);
void print_bytes(unsigned char num[], int size);
void TX_Init_Chrip();
int control_servo(float angle);
void get_i2cbus(char *i2cbus, char *i2caddr);
int readOneByte(int in_adr);
int detectMagnet();
int getMagnetStrength();
uint16_t readTwoBytes(int in_adr_hi, int in_adr_lo);
float getAngle();


int main(int argc, char **arg){
    /* piezo servo setup */

    /* "./test <servo_resolution> <servo_start> <servo_end> <lx16_resolution> <ls16_start> <ls16_end>" */
    int SR;
    float SS;
    float SE;
    int LR;
    float LS;
    float LE;
    char* target;
    if (argc < 7)
    {
        SR = 200;
        SS = 140 - 40;
        SE = 140 + 40;
        LR = 90;
        LS = 150;// - 90;
        LE = 150;// + 90;
    } else{
        SR = atoi(arg[1]);
        SS = strtof(arg[2], &target);
        SE = strtof(arg[3], &target);
        LR = atoi(arg[4]);
        LS = strtof(arg[5], &target);
        LE = strtof(arg[6], &target);
    }


    int servo_resolution = SR;
    float servo_angles[servo_resolution];
    float servo_start = SS;
    float servo_end = SE;
    float servo_step = (servo_end-servo_start)/(0.5*servo_resolution);

    for (int ii = 0; ii < servo_resolution/2+1; ii++){
        servo_angles[ii] = servo_start+servo_step*ii;
        servo_angles[servo_resolution-ii] = servo_angles[ii];
        printf
    }

    return 0;

    /* lx-16a servo setup */
    int lx16_resolution = LR;
    float lx16_angles[lx16_resolution];
    float lx16_start = LS;
    float lx16_end = LE;
    float lx16_step = (lx16_end-lx16_start)/(lx16_resolution);

    for (int ii = 0; ii <= lx16_resolution; ii++){
        lx16_angles[ii] = lx16_start+lx16_step*ii;
    } 

    #ifdef TCPIP
        // prepare for TCP communication
        int sockfd, portno = 51717, n;
        char serverIp[] = SERVER_IP;
        struct sockaddr_in serv_addr;
        struct hostent *server;
        fprintf(stdout, "contacting %s on port %d\n", serverIp, portno);
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
            printf( "ERROR opening socket" );
        }
        if ((server = gethostbyname( serverIp)) == NULL){
            printf( "ERROR, no such host\n");
        }
        bzero( (char *) &serv_addr, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        bcopy( (char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
        serv_addr.sin_port = htons(portno);
        if ( connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) 
            printf( "ERROR connecting\n");
    #endif

    #ifdef UDP
        int fd1;
        socklen_t length = sizeof(struct sockaddr);
        struct sockaddr_in addrSrv;
        addrSrv.sin_family = AF_INET;
        addrSrv.sin_port = htons(8888);
        addrSrv.sin_addr.s_addr = inet_addr(SERVER_LOCAL_IP);

        struct sockaddr_in addrClt;
        addrClt.sin_family = AF_INET;
        addrClt.sin_port = htons(8000);
        addrClt.sin_addr.s_addr = INADDR_ANY;//inet_addr(CLIENT_PUBLIC_IP);

        if ((fd1 = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
        {
            perror("Client socket dgram\n");
            exit(EXIT_FAILURE);
        }
        if(bind(fd1, (struct sockaddr*)&addrClt, length) == -1)
        {
            perror("Cient bind INADDR_ANY\n");
            exit(EXIT_FAILURE);
        }

        char recvbuf[200];
    #endif

    // initialize all systems
    System_Init();
    // prepare text file to write
    FILE * fp;
    fp = fopen("./RedPitaya_WhiteFin_V3.3_V0.8_WirePhantom_Grantry+Glycerin+10mmLens.dat", "w");
    // set trigger delay
    if (rp_AcqSetTriggerDelay((int32_t)ADC_TRIG_DELAY) != RP_OK){
        fprintf(stderr, "Error: Sets the number of decimated data after trigger written into memory failed!\n");
    }
    // main loop
    clock_t start, end;
    double cpu_time_used;
    start = clock();
    for (int n = 0; n < lx16_resolution; n++){
        char command_buffer[50];
        sprintf(command_buffer, "python3 ./control_lx16a.py %f", lx16_angles[n]);
        printf("%s\n", command_buffer);
        system(command_buffer);
        for (int j = 0; j < servo_resolution; j++){
            // start adc acquiring
            if(rp_AcqStart() != RP_OK){
                fprintf(stderr, "Error: Start of the Acquisition failed!\n");
            }
            // reset trigger state
            state = RP_TRIG_STATE_WAITING;
            // adc acquiring loop
            while(1){
                // set trigger source
                if(rp_AcqSetTriggerSrc(TRIG_SOURCE) != RP_OK){
                    fprintf(stderr, "Error: Set of the adquisition trigger source failed!\n");
                }
                // get trigger state
                if (rp_AcqGetTriggerState(&state) != RP_OK){
                    fprintf(stderr, "Error: Returns the trigger state failed!\n");
                }
                if (state == RP_TRIG_STATE_TRIGGERED){
                    // time for adc to stop after trigger (2500-8192)
                    //fprintf(stdout, "TRIG\n");
                    usleep(BUFFER_FILL_TIME);
                    break;
                }
                //fprintf(stdout, "Waiting for TRIG\n");
            }
            int16_t *buff = (int16_t *)malloc(buff_size * sizeof(int16_t));
            // get lastest data in raw
            if(rp_AcqGetLatestDataRaw(RP_CH_1, &buff_size, buff) != RP_OK){
                fprintf(stderr, "Returns the ADC buffer in Volt units from the oldest sample to the newest one failed!\n");
            }
            /* ##################### Time Stamp ##################### */
            // get the timestamp
            gettimeofday(&tv, NULL);
            // put microsecond timestamp into unsigned long
            time_stamp = tv.tv_usec;
            //printf("Time stamp is %lu\n", time_stamp);
            // convert small endian to big endian
            time_stamp = changed_endian_4Bytes(time_stamp);
            // put number in bytes
            memcpy(time_stamp_char, (unsigned char *)&time_stamp, sizeof time_stamp_char);
            //printf("Time stamp in bytes:\n");
            //print_bytes(time_stamp_char, sizeof time_stamp_char);

            /* ##################### Sweeping ##################### */
            // read encoder
            angle_index = j%servo_resolution;
            servo_angle = servo_angles[angle_index];
            printf("Designed Sweeping Angle: %f \t", servo_angle);
            control_servo(servo_angle);
            //usleep(10000);
            sweeping_angle = readTwoBytes(_raw_ang_hi, _raw_ang_lo);
            printf("Actual Sweeping Angle: %d (%f)\n", sweeping_angle, sweeping_angle*0.087);
            // convert small endian to big endian
            sweeping_angle = changed_endian_2Bytes(sweeping_angle);
            // put number in bytes
            memcpy(sweeping_angle_char, &sweeping_angle, sizeof sweeping_angle_char);
            /* ##################### Rotating ##################### */
            rotation_angle = (int)(lx16_angles[n] * 4096.0/360.0);
            rotation_angle = changed_endian_2Bytes(rotation_angle);
            memcpy(rotation_angle_char, &rotation_angle, sizeof rotation_angle_char);

            /* ##################### ADC ##################### */
            // convert all adc reading to big endian and put into unsigned char
            for (i = 0; i < BUFF_SIZE; i++){
                temp = buff[i];
                temp = changed_endian_2Bytes(temp);
                memcpy(adc_char+i*sizeof(temp), &temp, sizeof temp);
            }

            /* ##################### CRC ##################### */
            // concatenation everything we have now for crc calculation
            memcpy(crc_input, &version_number, sizeof(version_number));
            memcpy(crc_input+sizeof(version_number), time_stamp_char, sizeof(time_stamp_char));
            memcpy(crc_input+sizeof(version_number)+sizeof(time_stamp_char), &information_byte, sizeof(information_byte));
            memcpy(crc_input+sizeof(version_number)+sizeof(time_stamp_char)+sizeof(information_byte), sweeping_angle_char, sizeof(sweeping_angle_char));
            memcpy(crc_input+sizeof(version_number)+sizeof(time_stamp_char)+sizeof(information_byte)+sizeof(sweeping_angle_char), rotation_angle_char, sizeof(rotation_angle_char));
            memcpy(crc_input+sizeof(version_number)+sizeof(time_stamp_char)+sizeof(information_byte)+sizeof(sweeping_angle_char)+sizeof(rotation_angle_char), qua_wxyz_char, sizeof(qua_wxyz_char));
            memcpy(crc_input+sizeof(version_number)+sizeof(time_stamp_char)+sizeof(information_byte)+sizeof(sweeping_angle_char)+sizeof(rotation_angle_char)+sizeof(qua_wxyz_char), adc_char, sizeof(adc_char));
                //print_bytes(crc_input, sizeof(crc_input));
            // calculate crc32 checksum 
            crc_result = rc_crc32(0, crc_input, sizeof(crc_input));
            //printf("CRC is %X\n", crc_result);
            // change crc result to big endian
            crc_result = changed_endian_4Bytes(crc_result);
            // convert to unsigned char
            memcpy(crc_char, (unsigned char *)&crc_result, sizeof crc_char);
            //printf("CRC in bytes:\n");
            // put everything into message buffer
            memcpy(message_buff, marker, sizeof(marker));
            memcpy(message_buff+sizeof(marker), crc_input, sizeof(crc_input));
            memcpy(message_buff+sizeof(marker)+sizeof(crc_input), crc_char, sizeof(crc_char));
            //print_bytes(message_buff, sizeof(message_buff));
            #ifdef TXTFILE
                fwrite(&message_buff, sizeof(message_buff), 1, fp);
            #endif
            #ifdef TCPIP
            printf("Sending one Line\n");
            if ((e = write(sockfd, &message_buff, sizeof(message_buff))) < 0){
                    fprintf(stdout, "ERROR writing to socket");
                }
            #endif
            #ifdef UDP
            sendto(fd1, message_buff, sizeof message_buff, MSG_NOSIGNAL, (struct socaddr*)&addrSrv, length);
            recvfrom(fd1, recvbuf, sizeof recvbuf, MSG_NOSIGNAL, (struct sockaddr*)&addrClt, &length);
            printf("###################Received one line##################\n");
            #endif
        }
    }
    
    end = clock();
    fclose(fp);
#ifdef UDP
    close(fd1); /* close the UDP socket*/
#endif
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("%f seconds to execute \n", cpu_time_used);
    printf("%f Hz\n", 1.0/(cpu_time_used/1000));
}

int16_t changed_endian_2Bytes(int16_t value){
    return ((value >> 8) & 0x00ff) | ((value & 0x00ff) << 8);
}

void print_bytes(unsigned char num[], int size){
    for (int i = 0 ; i < size; i++){
            printf("%02X ",*(num + i));
    }
    printf("\n");
}

unsigned long changed_endian_4Bytes(unsigned long num){
    int byte0, byte1, byte2, byte3;
    byte0 = (num & 0x000000FF) >> 0 ;
    byte1 = (num & 0x0000FF00) >> 8 ;
    byte2 = (num & 0x00FF0000) >> 16 ;
    byte3 = (num & 0xFF000000) >> 24 ;
    return((byte0 << 24) | (byte1 << 16) | (byte2 << 8) | (byte3 << 0));
}

static int SPI_Init(){
    int mode = 0;
    /* Opening file stream */
    // RDWR means it can read and write
    spi_fd = open("/dev/spidev1.0", O_RDWR | O_NOCTTY);
    if(spi_fd < 0){
        fprintf(stderr, "Error opening spidev0.1. Error: %s\n", strerror(errno));
        return -1;
    }
    /* Setting mode (CPHA, CPOL) */
    if(ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) < 0){
        fprintf(stderr, "Error setting SPI_IOC_RD_MODE. Error: %s\n", strerror(errno));
        return -1;
    }
    /* Setting SPI bus speed */
    int spi_speed = 1000000;
    if(ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi_speed) < 0){
        fprintf(stderr, "Error setting SPI_IOC_WR_MAX_SPEED_HZ. Error: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

void I2C_Init(){
    char senaddr[256] = "0x36";
    char i2c_bus[256] = I2CBUS;
    get_i2cbus(i2c_bus, senaddr);
}

void TX_Init(){
    // Generate Tx signal
	rp_GenFreq(RP_CH_1, TX_FREQ);
	rp_GenAmp(RP_CH_1, TX_AMP);
	rp_GenWaveform(RP_CH_1, RP_WAVEFORM_PWM);
	rp_GenOffset(RP_CH_1, TX_OFFSET);
	rp_GenDutyCycle(RP_CH_1, TX_DUTYCYCLE);
	rp_GenOutEnable(RP_CH_1);
}

void TX_Init_Chrip(){
    int buff_size = 15000;
    float *x = (float *)malloc(buff_size * sizeof(float));
    for (int i = 0; i < buff_size; i++){
        x[i] = 0;
    }
    for (int i = 0; i < 10; i++){
        x[i] = 1;
    }
    for (int i = 20; i < 30; i++){
        x[i] = 1;
    }
    for (int i = 40; i < 50; i++){
        x[i] = 1;
    }
    for (int i = 60; i < 70; i++){
        x[i] = 1;
    }
    rp_GenWaveform(RP_CH_1, RP_WAVEFORM_ARBITRARY);
    rp_GenArbWaveform(RP_CH_1, x, buff_size);
    rp_GenFreq(RP_CH_1, 5000.0);
    rp_GenAmp(RP_CH_1, 1.0);
    rp_GenOutEnable(RP_CH_1);
    free(x);
}

void ADC_Init(){
    // rest adc
    if (rp_AcqReset() != RP_OK){
        fprintf(stderr, "Error: Resets the acquire writing state machine failed!\n");
    }
    rp_acq_decimation_t rp_dec = ADC_DECIMATION;
    // set the decimation
    if (rp_AcqSetDecimation(rp_dec) != RP_OK){
        fprintf(stderr, "Error: Sets the decimation used at acquiring signal failed!\n");
    }
    // set trigger threshold
	if (rp_AcqSetTriggerLevel(TRIF_LEVEL_SOURCE, ADC_TRIG_LEVEL) != RP_OK){
        fprintf(stderr, "Error: Sets the trigger threshold value in volts failed!\n");
    }
}

void System_Init(){
    // check red pitaya api initialization
    if (rp_Init() != RP_OK){
        fprintf(stderr, "RP api init failed!\n");
    }
    // start generating Tx signal
    //TX_Init();
    TX_Init_Chrip();
    // initialize ADC
    ADC_Init();
    /* Init the spi resources */
    if(SPI_Init() < 0){
        fprintf(stderr, "Initialization of SPI failed. Error: %s\n", strerror(errno));
    }
    I2C_Init();
}

uint32_t rc_crc32(uint32_t crc, unsigned char *buf, size_t len){
	static uint32_t table[256];
	static int have_table = 0;
	uint32_t rem;
	uint8_t octet;
	int i, j;
	unsigned char *p, *q;
	/* This check is not thread safe; there is no mutex. */
	if (have_table == 0) {
        //fprintf(stdout, "Table\n");
		/* Calculate CRC table. */
		for (i = 0; i < 256; i++) {
			rem = i;  /* remainder from polynomial division */
			for (j = 0; j < 8; j++) {
				if (rem & 1) {
					rem >>= 1;
					rem ^= 0xedb88320;
				} else
					rem >>= 1;
			}
			table[i] = rem;
		}
		have_table = 1;
	}
	crc = ~crc;
	q = buf + len;
	for (p = buf; p < q; p++) {
		octet = *p;  /* Cast to unsigned octet. */
		crc = (crc >> 8) ^ table[(crc & 0xff) ^ octet];
	}
	return ~crc;
}

static void transfer(int fd, uint8_t const *tx, uint8_t const *rx, size_t len){
	int ret;
	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)rx,
		.len = len,
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
	};

	if (mode & SPI_TX_QUAD)
		tr.tx_nbits = 4;
	else if (mode & SPI_TX_DUAL)
		tr.tx_nbits = 2;
	if (mode & SPI_RX_QUAD)
		tr.rx_nbits = 4;
	else if (mode & SPI_RX_DUAL)
		tr.rx_nbits = 2;
	if (!(mode & SPI_LOOP)) {
		if (mode & (SPI_TX_QUAD | SPI_TX_DUAL))
			tr.rx_buf = 0;
		else if (mode & (SPI_RX_QUAD | SPI_RX_DUAL))
			tr.tx_buf = 0;
	}
    
	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1)
		fprintf(stderr, "can't send spi message\n");
    
}

uint16_t read_encoder(){
	uint8_t rd_pos = 0x10;
    uint8_t nop_a5 = 0x00;
    //uint8_t zero_set = 0x70;
    uint8_t rx_buff = 0xA5;
	uint8_t temp[2]; // temp variale to store MSB and LSB
	uint16_t ABSposition = 0;
	//float deg = 0.0;
	// send the first rd_pos command
	transfer(spi_fd, &rd_pos, &rx_buff, 1);
	// if the encoder is not ready, continue to send 0x00 command until it's ready
	while (rx_buff != rd_pos){
		//fprintf(stdout, "%02X\n", rx_buff);
		transfer(spi_fd, &nop_a5, &rx_buff, 1);
		usleep(1);
        //printf("Waiting for Encoder.\n");
	}
	// get the MSB
	transfer(spi_fd, &nop_a5, &rx_buff, 1);
	temp[0] = rx_buff;
	// get LSB
	transfer(spi_fd, &nop_a5, &rx_buff, 1);
	temp[1] = rx_buff;
	// calculate the abs position
	// mask out first 4 bits
	temp[0] &= ~ 0xF0; 
	// shift MSB to ABS position
	ABSposition = temp[0] << 8;
	// add LSB
	ABSposition += temp[1];
	// convert to angle
	//deg = ABSposition * 360.0 / 4096.0;
	// wait a bit
	usleep(1);
	return ABSposition;	
}

int control_servo(float angle){
    if ((angle > 180) && (angle < 0)){
		printf("Angle %f out of range", angle);
		return -1;
	}
    //angle = abs(180-angle);
	float duty_cycle = 0.5 * (angle/180.0);
    rp_GenFreq(RP_CH_2, 250);
	rp_GenAmp(RP_CH_2, 1.0);
	rp_GenWaveform(RP_CH_2, RP_WAVEFORM_PWM);
	rp_GenOffset(RP_CH_2, 1.0);
	rp_GenDutyCycle(RP_CH_2, duty_cycle);
	rp_GenOutEnable(RP_CH_2);

	return 0;
}

void get_i2cbus(char *i2cbus, char *i2caddr){
   if((i2cfd = open(i2cbus, O_RDWR)) < 0) {
      printf("Error failed to open I2C bus [%s].\n", i2cbus);
      exit(-1);
   }
   /* set I2C device */
   int addr = (int)strtol(i2caddr, NULL, 16);

   if(ioctl(i2cfd, I2C_SLAVE, addr) != 0) {
      printf("Error can't find sensor at address [0x%02X].\n", addr);
      exit(-1);
   }
   /* I2C communication test */
   char reg = 0x00;
   if(write(i2cfd, &reg, 1) != 1) {
      printf("Error: I2C write failure register [0x%02X], sensor addr [0x%02X]?\n", reg, addr);
      exit(-1);
   }
}

int readOneByte(int in_adr){
  int retVal = -1;
  if(write(i2cfd, &in_adr, 1) != 1) {
      printf("Error: Request one byte\n");
      return(-1);
    }
  if(read(i2cfd, &retVal, 1) != 1) {
      printf("Error: Receive one byte\n");
      return(-1);
    }
  return retVal;
}

int detectMagnet(){
  int magStatus;
  int retVal = 0;

  magStatus = readOneByte(_stat);
  if(magStatus & 0x20)
    retVal = 1; 
  
  return retVal;
}

int getMagnetStrength(){
  int magStatus;
  int retVal = 0;
  /*0 0 MD ML MH 0 0 0*/
  /* MD high = AGC minimum overflow, Magnet to strong */
  /* ML high = AGC Maximum overflow, magnet to weak*/ 
  /* MH high = magnet detected*/ 
  magStatus = readOneByte(_stat);
  if(detectMagnet() ==1){
      retVal = 2; /*just right */
      if(magStatus & 0x10)
        retVal = 1; /*to weak */
      else if(magStatus & 0x08)
        retVal = 3; /*to strong */
  }
  
  return retVal;
}

uint16_t readTwoBytes(int in_adr_hi, int in_adr_lo){
  uint16_t retVal = -1;

  /* read low byte */
  uint8_t low = readOneByte(in_adr_lo);

  /* read high byte */
  uint16_t high = readOneByte(in_adr_hi);
  high = high << 8;

  retVal = high | low;

  return retVal;
}

float getAngle(){
  uint16_t rawAngle = readTwoBytes(_raw_ang_hi, _raw_ang_lo);
  float angle = rawAngle * 0.087;
  return angle;
}
