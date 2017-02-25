/** 
 * File:   main.c
 *
 * Author: Howard Schlunder
 *
 * Created on July ‎09, ‎2014, ‏‎11:17 AM
 *
 * Example LED blinking application to demonstrate successful building of an 
 * application containing an EZBL bootloader and also how bootloader functions 
 * can be called directly from an application project (with appropriate headers 
 * for function prototypes and extern variable declarations).
 *
 */

// DOM-IGNORE-BEGIN
/*******************************************************************************
  Copyright (C) 2015 Microchip Technology Inc.

  MICROCHIP SOFTWARE NOTICE AND DISCLAIMER:  You may use this software, and any
  derivatives created by any person or entity by or on your behalf, exclusively
  with Microchip's products.  Microchip and its licensors retain all ownership
  and intellectual property rights in the accompanying software and in all
  derivatives here to.

  This software and any accompanying information is for suggestion only.  It
  does not modify Microchip's standard warranty for its products.  You agree
  that you are solely responsible for testing the software and determining its
  suitability.  Microchip has no obligation to modify, test, certify, or
  support the software.

  THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS".  NO WARRANTIES, WHETHER
  EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED
  WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A PARTICULAR
  PURPOSE APPLY TO THIS SOFTWARE, ITS INTERACTION WITH MICROCHIP'S PRODUCTS,
  COMBINATION WITH ANY OTHER PRODUCTS, OR USE IN ANY APPLICATION.

  IN NO EVENT, WILL MICROCHIP BE LIABLE, WHETHER IN CONTRACT, WARRANTY, TORT
  (INCLUDING NEGLIGENCE OR BREACH OF STATUTORY DUTY), STRICT LIABILITY,
  INDEMNITY, CONTRIBUTION, OR OTHERWISE, FOR ANY INDIRECT, SPECIAL, PUNITIVE,
  EXEMPLARY, INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, FOR COST OR EXPENSE OF
  ANY KIND WHATSOEVER RELATED TO THE SOFTWARE, HOWSOEVER CAUSED, EVEN IF
  MICROCHIP HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.
  TO THE FULLEST EXTENT ALLOWABLE BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL
  CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF
  FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.

  MICROCHIP PROVIDES THIS SOFTWARE CONDITIONALLY UPON YOUR ACCEPTANCE OF THESE
  TERMS.
*******************************************************************************/
// DOM-IGNORE-END

#include <xc.h>
#include <stdio.h>
#include <stdlib.h>
#include <libpic30.h>
#include <string.h>
#include "ezbl_integration/ezbl.h"
#include "ezbl_integration/uart_fifo.h"
#include "ezbl_integration/i2c_fifo.h"
#include "hardware_initializers/hardware_initializer.h"

#include "hardware_initializers/flower.h"


#define     TRUE                1
#define     FALSE               0
#define     SUCCESS             1
#define     FAILED              0
#define     ENABLED             1
#define     DISABLED            0
#define		EXIST				1
#define		NOT_EXIST			0
#define		IN					1
#define		OUT					0




//__prog__ unsigned char __attribute__((space(prog), keep)) dummy[10000];    // Dummy items in program memory for bootloader testing purposes
//__prog__ unsigned char __attribute__((space(prog), keep)) dummy2[20000];   // Dummy items in program memory for bootloader testing purposes
//__prog__ unsigned char __attribute__((space(prog), keep)) dummy3[30000];   // Dummy items in program memory for bootloader testing purposes


void __attribute__((interrupt, no_auto_psv)) _DefaultInterrupt(void)
{
    // If this interrupt is unexpected, you should check all of the INTCON1 bits
    // to see if a trap is responsible.
    __builtin_software_breakpoint();

    // Clear trap(s), if any, so we can return to see where the PC was when this
    // interrupt was triggered
    INTCON1 &= 0x8700;
}

void initBluetooth(void)
{
    BEN_DIR = OUT;
    BEN = 0;        // 0:模块开始广播 1：sleep
    BRTS_DIR = OUT;
    BRTS = 1;       //idle
    BCTS_DIR = IN;  //等待模块数据
    BCTS = 1;              
    BRESTORE_DIR = OUT;
    BRESTORE = 1;   //常态
}

// main() function
int main(void)
{
    unsigned long ledTimer = 0;
    unsigned long now;
    char i=1;
    
    
    initBluetooth();
    
    
    // Optionally take over the UART2 module interrupts if we want to disable 
    // bootloader ISR handling and use our Application defined ones instead.
//    {
//        unsigned long isrForwardMaskBit;
//        EZBL_GetSymbol(isrForwardMaskBit, EZBL_FORWARD_MASK_U2RX);
//        EZBL_ForwardBootloaderISR |= isrForwardMaskBit;
//        EZBL_GetSymbol(isrForwardMaskBit, EZBL_FORWARD_MASK_U2TX);
//        EZBL_ForwardBootloaderISR |= isrForwardMaskBit;
//    }
    
    int t0,t1;
 
    while(1)
    {
        // Periodically let the bootloader check for incoming messages. This 
        // function does nothing and returns immediately if the bootloader's 
        // RX communications channel interrupt is being handled by this 
        // application instead of the bootloader (i.e. you have the applicable 
        // bit in EZBL_ForwardBootloaderISR set to '1').
        
        t1=EZBL_BootloaderComTask(1);

        // Every 500ms toggle an LED
        now = NOW_32();
        if(now - ledTimer > (NOW_second)>>3)   //>>3))
        {
            ledTimer = now;
            t0=LEDToggle(0x01);
        }
    }
}

