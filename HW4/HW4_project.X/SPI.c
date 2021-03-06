
#include <stdio.h>
#include <math.h>
#include <xc.h>

// DEVCFG0
#pragma config DEBUG = 11 // no debugging
#pragma config JTAGEN = 0 // no jtag
#pragma config ICESEL = 11 // use PGED1 and PGEC1
#pragma config PWP = 111111111 // no write protect
#pragma config BWP = 0 // no boot write protect
#pragma config CP = 1 // no code protect

// DEVCFG1
#pragma config FNOSC = 011 // use primary oscillator with pll
#pragma config FSOSCEN = 0 // turn off secondary oscillator
#pragma config IESO = 0 // no switching clocks
#pragma config POSCMOD = 10 // high speed crystal mode
#pragma config OSCIOFNC = 1 // free up secondary osc pins
#pragma config FPBDIV = 00 // divide CPU freq by 1 for peripheral bus clock
#pragma config FCKSM = 11 // do not enable clock switch
#pragma config WDTPS = 10100 // slowest wdt
#pragma config WINDIS = 1 // no wdt window
#pragma config FWDTEN = 0 // wdt off by default
#pragma config FWDTWINSZ = 11 // wdt window at 25%

// DEVCFG2 - get the CPU clock to 48MHz
#pragma config FPLLIDIV = 001 // (divide by 2) divide input clock to be in range 4-5MHz (divide by 2)
#pragma config FPLLMUL = 111 // (multiply by 24) multiply clock after FPLLIDIV
#pragma config FPLLODIV = 001 // (divide by 2) divide clock after FPLLMUL to get 48MHz
#pragma config UPLLIDIV = 001 // divider for the 8MHz input clock, then multiply by 12 to get 48MHz for USB
#pragma config UPLLEN = 0 // USB clock on

// DEVCFG3
#pragma config USERID = 0 // some 16bit userid, doesn't matter what
#pragma config PMDL1WAY = 0 // allow multiple reconfigurations
#pragma config IOL1WAY = 0 // allow multiple reconfigurations
#pragma config FUSBIDIO = 1 // USB pins controlled by USB module
#pragma config FVBUSONIO = 1 // USB BUSON controlled by USB module


// Demonstrates SPI
// PIC is the master, DAC is the slave
// Uses microchip MCP4902 chip 
// pin 1 (VDD) -> 3.3V (capacitor 0.1uF)
// pin 2 (NC) -> --
// pin 3 (CS) -> pin12 RA4
// pin 4 (SCK) -> pin25 B14
// pin 5 (SDI) -> pin3 RA1
// pin 6 (NC) -> --
// pin 7 (NC) -> --
// pin 8 (VOUTA) -> Ch1 of NScope
// pin 9 (VREFA) -> 3.3V
// pin 10 (VSS) -> GND
// pin 11 (VREFB) -> 3.3V
// pin 12 (VOUTB) -> Ch2 of NScope
// pin 13 (SHDN) -> 3.3V
// pin 14 (LDAC) -> GND

#define CS LATAbits.LATA4       // chip select pin
#define NUMSAMPS 100            // number of points in waveform
#define PI 3.14159265
#define DELAY 48000000/2/1000

unsigned char spi_io(unsigned char o);
void initSPI1(void);
void setVoltage(unsigned char channel, unsigned char voltage);
void makeSinWave(void);
void makeTriangleWave(void);

unsigned char TriangleWave[NUMSAMPS];
unsigned char SinWave[NUMSAMPS];

int main(void) {
  
  __builtin_disable_interrupts();

    // set the CP0 CONFIG register to indicate that kseg0 is cacheable (0x3)
    __builtin_mtc0(_CP0_CONFIG, _CP0_CONFIG_SELECT, 0xa4210583);

    // 0 data RAM access wait states
    BMXCONbits.BMXWSDRM = 0x0;

    // enable multi vector interrupts
    INTCONbits.MVEC = 0x1;

    // disable JTAG to get pins back
    DDPCONbits.JTAGEN = 0;

    // do your TRIS and LAT commands here
    TRISBbits.TRISB4 = 1; // set pushbutton to an input pin 
    TRISAbits.TRISA4 = 0; // set LED to an output pin
    LATAbits.LATA4 = 1; // set LED on
    
    initSPI1();
    
  __builtin_enable_interrupts();  
    
  
  makeSinWave();
  makeTriangleWave();
  int i = 0;
  _CP0_SET_COUNT(0);
  
  while(1) {
    
    while(_CP0_GET_COUNT()<DELAY){
        ;
    }
    _CP0_SET_COUNT(0);
    
    setVoltage(0, SinWave[i]);
    setVoltage(1, TriangleWave[i]);

    if (i==99){
        i=0;
    } else {
        i=i+1;
    }
  
  }
  return (0);
}


unsigned char spi_io(unsigned char o) {
  SPI1BUF;
  SPI1BUF = o;
  while(!SPI1STATbits.SPIRBF) { // wait to receive the byte
    ;
  }
  return SPI1BUF; 
}

// initialize SPI1
void initSPI1(void) {
  // set up the chip select pin as an output
  // the chip select pin is used by the slave to indicate
  // when a command is beginning (clear CS to low) and when it
  // is ending (set CS high)
  // TRISAbits.TRISA4 = 0;
  CS = 1;

  // Master - SPI1, pins are: SDI4(F4), SDO4(F5), SCK4(F13).  
  // we manually control SS4 as a digital output (F12)
  // since the pic is just starting, we know that SPI is off. We rely on defaults here
 
  // setup SPI1
  SPI1CON = 0;              // turn off the spi module and reset it
  SPI1BUF;                  // clear the rx buffer by reading from it
  SPI1BRG = 0x17;          // baud rate to 10 MHz [SPI2BRG = (80000000/(2*desired))-1] 61A7 3E8
  
  // SPI1CONbits.MODE16=0;
  // SPI1CONbits.MODE32=0;
  SPI1STATbits.SPIROV = 0;  // clear the overflow bit
  SPI1CONbits.CKE = 1;      // data changes when clock goes from hi to lo (since CKP is 0)
  SPI1CONbits.MSTEN = 1;    // master operation
  SPI1CONbits.ON = 1;       // turn on SPI1
  // SS1Rbits.SS1R=0b0000;
  
  RPA1Rbits.RPA1R = 0b0011;
}


void setVoltage(unsigned char channel, unsigned char voltage) {
    
    unsigned char something1, something2;
    
    if (channel==0) {
        something1 = (voltage >> 4) | (0b01110000);
    } else if (channel==1) {
        something1 = (voltage >> 4) | (0b11110000);
    }
    something2 = (voltage << 4);
    
    CS = 0;
    spi_io(something1);
    spi_io(something2);
    CS = 1;   
}

void makeSinWave(void) {
  int i = 0;
  float temp = 0;
  for (i = 0; i < NUMSAMPS; i++) {
    temp = 255.0/2.0+255.0/2.0*sin(2.0*PI*i/100.0);
    // sin(i/NUMSAMPS*2*PI)*255;
    SinWave[i] = (unsigned char) temp;
  }
}

void makeTriangleWave(void) {
  int i = 0;
  float temp = 0;
  for (i = 0; i < (NUMSAMPS/2); i++) {
    temp = (i/50.0)*255.0;
    TriangleWave[i] = (unsigned char) temp;
  }
  for (i = 50; i < NUMSAMPS; i++) {
    temp = (i/50.0)*255.0;
    TriangleWave[i] = (unsigned char) temp;
  }
}