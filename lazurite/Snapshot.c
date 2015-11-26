/* FILE NAME: Snapshot
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015  Lapis Semiconductor Co.,Ltd.
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.

Function: 小型シリアルJPEGカメラモジュール(3.3V TTL電圧レベル、ビデオ出力付き)写真を撮る応用例
Notes:
1.  Originally from https://github.com/adafruit/Adafruit-VC0706-Serial-Camera-Library
2.  Modified to fit our products and add Japanese comments
    by New Business Development Project
    Email: lazurite@adm.lapis-semi.com
*/

/* SubGHz *****************************/
#define LED_BL 26                      // pin number of Blue LED
#define LED_RE 25                      // pin number of Red LED
#define SUBGHZ_CH       33          // channel number (frequency)
#define SUBGHZ_PANID    0xABCD      // panid
#define HOST_ADDRESS    0xAC54      // distination address

#define SEND_PKT_LEN    192         // Length of one packet

/* Camera *****************************/
#define REQUEST_CODE            0x56
#define RETURN_CODE             0x76

#define COMMAND_GET_VERSION     0x11
#define COMMAND_MIRROR_CTRL     0x3A
#define COMMAND_FBUF_CTRL       0x36
#define COMMAND_GET_FBUF_LEN    0x34
#define COMMAND_READ_FBUF       0x32
#define COMMAND_HEADER_SIZE     5          

#define FBUF_START_ADDR         0x00, 0x00, 0x00, 0x00
#define FBUF_DATA_LENGTH        0x00, 0x00, 0x00, SEND_PKT_LEN

static unsigned char get_version[]  = {REQUEST_CODE, 0x00, COMMAND_GET_VERSION, 0x00};
static unsigned char mirror_ctrl[]  = {REQUEST_CODE, 0x00, COMMAND_MIRROR_CTRL, 0x02, 0x01, 0x01};
static unsigned char resume_frame[] = {REQUEST_CODE, 0x00, COMMAND_FBUF_CTRL,   0x01, 0x02};
static unsigned char stop_frame[]   = {REQUEST_CODE, 0x00, COMMAND_FBUF_CTRL,   0x01, 0x00};
static unsigned char get_fbuf_len[] = {REQUEST_CODE, 0x00, COMMAND_GET_FBUF_LEN,0x01, 0x00};
static unsigned char read_fbuf[]    = {REQUEST_CODE, 0x00, COMMAND_READ_FBUF,   0x0C, 0x00, 0x0A, FBUF_START_ADDR, FBUF_DATA_LENGTH, 0x00, 0x64 };
//static unsigned char read_fbuf[]    = {REQUEST_CODE, 0x00, COMMAND_READ_FBUF,   0x0C, 0x00, 0x0A, FBUF_START_ADDR, FBUF_DATA_LENGTH, 0x0B, 0xB8 };

unsigned char serial_tx_data[256];
unsigned char serial_rx_data[256];



/* static functions *********************/
static read_serial_buf(unsigned char begin_char, unsigned char len){

    unsigned char i=0;
    int itmp;

    while (1){
        itmp = Serial1.read();
        if (begin_char == itmp) {
            serial_rx_data[i]=itmp;
            break;
        }
    }

    for (i++; i < len;){
        itmp = Serial1.read();
        if (itmp >= 0)
            serial_rx_data[i++]=itmp;
    }
    delay(1);
}


/* setup & loop *********************/
void setup() {

    SUBGHZ_PARAM a;

    // Module initialization
    Serial.begin(115200);
    Serial2.begin(115200);

    /* SubGHz */
    SubGHz.init();                  // initializing Sub-GHz

    SubGHz.getSendMode(&a);
    a.txRetry = 3;
    a.senseTime = 5;
    a.txInterval = 10;
    a.ccaWait = 1;
    SubGHz.setSendMode(&a);

    /* set GPIO */
    pinMode(LED_BL,OUTPUT);            // setting of LED
    pinMode(LED_RE,OUTPUT);            // setting of LED
    digitalWrite(LED_BL,HIGH);         // setting of LED
    digitalWrite(LED_RE,HIGH);         // setting of LED
}


void loop() {

    volatile uint32_t total_len=0x00000000, current_addr=0x00000000;
    unsigned char uctmp,i=0, ii=0;
    int itmp, jtmp;

    // Initializing
    SubGHz.begin(SUBGHZ_CH, SUBGHZ_PANID,  SUBGHZ_100KBPS, SUBGHZ_PWR_1MW);     // start Sub-GHz

    Serial1.write(get_version, sizeof(get_version));
    read_serial_buf(RETURN_CODE, 0x10);
    Serial1.write(resume_frame, sizeof(resume_frame));
    read_serial_buf(RETURN_CODE, 0x04);
    Serial1.write(stop_frame, sizeof(stop_frame));
    read_serial_buf(RETURN_CODE, 0x04);
    Serial1.write(get_fbuf_len, sizeof(get_fbuf_len));
    read_serial_buf(RETURN_CODE, 0x09);

    /* get total length */
    total_len |= (uint32_t)(serial_rx_data[5] << 24);
    total_len |= (uint32_t)(serial_rx_data[6] << 16);
    total_len |= (uint32_t)(serial_rx_data[7] << 8);
    total_len |= (uint32_t)(serial_rx_data[8]);

    // preparing data
    digitalWrite(LED_BL,LOW);                                                      // LED ON

    while(1){
//      volatile uint32_t diff_size; // ssdebug
        volatile uint16_t diff_size;

        /* read address */
        read_fbuf[6]= (uint8_t)(current_addr >> 24);
        read_fbuf[7]= (uint8_t)(current_addr >> 16);
        read_fbuf[8]= (uint8_t)(current_addr >> 8);
        read_fbuf[9]= (uint8_t)current_addr;

        diff_size = (uint16_t)(total_len - current_addr);

        if (diff_size > SEND_PKT_LEN){
            Serial1.write(read_fbuf, sizeof(read_fbuf));
            if (current_addr == 0x00000000) {
                read_serial_buf(RETURN_CODE, SEND_PKT_LEN + COMMAND_HEADER_SIZE);
                SubGHz.send(SUBGHZ_PANID, HOST_ADDRESS, &(serial_rx_data[COMMAND_HEADER_SIZE]), SEND_PKT_LEN ,NULL);
            }else{
                read_serial_buf(RETURN_CODE, SEND_PKT_LEN + COMMAND_HEADER_SIZE + 5); // ssdebug
                SubGHz.send(SUBGHZ_PANID, HOST_ADDRESS, &(serial_rx_data[COMMAND_HEADER_SIZE + 5]), SEND_PKT_LEN ,NULL);
            }
            current_addr+=SEND_PKT_LEN;
        } else {
            Serial1.write(read_fbuf, sizeof(read_fbuf));
            read_serial_buf(RETURN_CODE, diff_size + COMMAND_HEADER_SIZE + 5); // ssdebug
            SubGHz.send(SUBGHZ_PANID, HOST_ADDRESS, &(serial_rx_data[COMMAND_HEADER_SIZE + 5]), diff_size ,NULL);
            break;
        }
    }


    digitalWrite(LED_BL,HIGH);                                                     // LED off
    digitalWrite(LED_RE,LOW);                                                      // LED ON
    
    // close
    SubGHz.close();                                                             // Sub-GHz module sets into power down mode.

    sleep(1000);                                                                // sleep

    for(;;);

    return;
}
