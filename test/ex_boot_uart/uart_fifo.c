/*
 * File:   uart_fifo.c
 * Author: Howard Schlunder
 *
 * Created on December 17, 2013
 *
 * An easily ported interrupt based UART TX and RX buffering implementation.
 * Implements independent software FIFOs for TX and RX channels.
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

#define UART_FIFO_C // Unique identifier in case if a #include .h header needs to know which file it is included in

#include <xc.h>     // Needed for UART SFR definitions
#include "fifo.h"   // For FIFO_FEATURES flag definitions


// Configuration parameters
#define UART_MODULE_NUM         2       // Hardware UART module # to apply this 
                                        // software FIFO to. stdout will be 
                                        // redirected to this UART. 1 means 
                                        // UART1, 2 means UART2, etc.
#define UART_TX_FIFO_SIZE       8u      // Bytes for software TX FIFO buffering
#define UART_RX_FIFO_SIZE       96u     // Bytes for software RX FIFO buffering


// Static token concatenation macros that make the code easier to read.
// Don't directly change these unless porting to different hardware with 
// different register names. Instead change the UART_MODULE_NUM macro above to
// change which physical UART hardware module this FIFOing library applies to.
#define UxCON           CAT3(U,UART_MODULE_NUM,CON)
#define UxCONbits       CAT3(U,UART_MODULE_NUM,CONbits)
#define UxMODE          CAT3(U,UART_MODULE_NUM,MODE)
#define UxMODEbits      CAT3(U,UART_MODULE_NUM,MODEbits)
#define UxBRG           CAT3(U,UART_MODULE_NUM,BRG)
#define UxSTA           CAT3(U,UART_MODULE_NUM,STA)
#define UxSTAbits       CAT3(U,UART_MODULE_NUM,STAbits)
#define UxRXREG         CAT3(U,UART_MODULE_NUM,RXREG)
#define UxTXREG         CAT3(U,UART_MODULE_NUM,TXREG)
#define _UxTXIF         CAT3(_U,UART_MODULE_NUM,TXIF)
#define _UxRXIF         CAT3(_U,UART_MODULE_NUM,RXIF)
#define _UxTXIE         CAT3(_U,UART_MODULE_NUM,TXIE)
#define _UxRXIE         CAT3(_U,UART_MODULE_NUM,RXIE)
#define _UxTXIP         CAT3(_U,UART_MODULE_NUM,TXIP)
#define _UxRXInterrupt  CAT3(_U,UART_MODULE_NUM,RXInterrupt)
#define _UxTXInterrupt  CAT3(_U,UART_MODULE_NUM,TXInterrupt)


// Instantiate UART TX FIFO
#define FIFO_NAME               UART_TX_FIFO_
#define FIFO_SIZE               UART_TX_FIFO_SIZE
#define FIFO_FEATURES           FIFO_FEATURE_WRITABLE | FIFO_FEATURE_ON_WRITE | FIFO_FEATURE_ON_WRITE_SUSPEND
#define UART_TX_FIFO_OnWrite(sourcePtr, writeLength)    \
    if(!UxSTAbits.UTXBF)                                \
    {                                                   \
        _UxTXIF = 1;                                    \
    }
#define UART_TX_FIFO_OnWriteSuspend(bytesFreeNeeded)    \
    UART_TX_FIFO_OnWrite(0,0)
#include "fifo.c.h"


// Instantiate UART RX FIFO
#define FIFO_NAME               UART_RX_FIFO_
#define FIFO_SIZE               UART_RX_FIFO_SIZE
#define FIFO_FEATURES           FIFO_FEATURE_READABLE | FIFO_FEATURE_PEEKABLE
#define FIFO_EXTRA_VARS         volatile unsigned int errors;   // Add an extra variable for capturing RX framing and hardware + software FIFO overrun errors
#include "fifo.c.h"




// Special UART function for printf(), stdout, stderr, etc. redirection
int __attribute__((__section__(".libc.write"))) write(int handle, void *buffer, unsigned int len);


/** 
 * Resets the hardware UART and software FIFOs. All FIFO data is lost. The UART
 * is configured and enabled afterwards and ready for both TX and RX. This
 * function will disarm auto-baud if armed and 0 is not specified for baudRate.
 *
 * @param peripheralClockSpeed Frequency, in hertz, that the UART peripheral is 
 *                             operating with. On PIC24 and dsPIC products, this 
 *                             is the same as the instructions executed/second 
 *                             while not dozing. Ex: set to 70000000 if running 
 *                             at 70 MIPS.
 * @param baudRate Desired baud rate for the UART RX and TX operations, or 0 for
 *                 auto-baud mode. Ex: set to 115200 for 115200 bits/second
 *                 (11520 bytes/second with 1 start bit, 8 data bits, and 1 stop
 *                 bit). The actual baud rate programed will be as close as
 *                 possible to the requested value, but could still result in
 *                 appreciable error if specifying very fast baud rates or when
 *                 operating at a slow peripheral clock speed. See the UART data
 *                 sheet or Family Reference Manual documentation to understand
 *                 the underlying error limits. This function always enables the
 *                 UART in high-speed (4x Baud-clock) mode to minimize high-baud
 *                 rate error.
 *
 *                 For auto-baud mode, a 0x55 character must be the first
 *                 character received for the hardware to properly compute the
 *                 baud clock divider.
 */
void UART_Reset(unsigned long peripheralClockSpeed, unsigned long baudRate)
{
int i;
    _UxTXIE = 0;            // Clear interrupt enable; we are about to clear the corresponding FIFO
    _UxRXIE = 0;            // Clear interrupt enable; we are about to clear the corresponding FIFO
    UxMODE = 0x0000;        // Disable the UART module, in case if it has already been enabled and we are at risk of receiving an interrupt while reseting the FIFOs
    UART_RX_FIFO_vars.errors = 0;   // Clear overflow or other logged RX errors

    UART_TX_FIFO_Reset();
    UART_RX_FIFO_Reset();

    _UxTXIP = 1;            // Set TX interrupt to Priority 1 (0 is main context, 7 is time-critical highest priority)
    UxMODEbits.UARTEN = 1;  // Set UARTEN and BRGH.  Note: this must be done before setting UTXEN

    UxMODEbits.BRGH = 1;    // Set UARTEN and BRGH.  Note: this must be done before setting UTXEN
    if(baudRate == 0u)      // Turn on auto-baud if 0 is specified (instead of causing a divide by 0 error trap!)
    {
        UxMODEbits.ABAUD = 1;
    }
    else
    {
        //UxBRG = (peripheralClockSpeed + (baudRate<<1))/(baudRate<<2) - 1u;    // Program baud rate generator to what was set outside this file
        i = (peripheralClockSpeed + (baudRate<<1))/(baudRate<<2) - 1u;    // Program baud rate generator to what was set outside this file
        UxBRG = i;
    }
    UxSTAbits.UTXEN = 1;    // UTXEN set
    UxSTAbits.OERR = 0;     // Make sure there is no RX overflow present
    _UxRXIF = 0;            // Clear Interrupt Flags
    _UxTXIF = 0;            // Clear Interrupt Flags
    _UxRXIE = 1;            // Enable Interrupts and normal operation
    _UxTXIE = 1;            // Enable Interrupts and normal operation
}


/**
 * Blocks execution until everything pending is finished being physically
 * transmitted. The software TX FIFO and the hardware UART TX FIFO are drained
 * to 0 bytes before returning.
 */
void UART_TX_FIFO_WaitUntilFlushed(void)
{
    volatile unsigned int i;
    while(UART_TX_FIFO_vars.dataCount);     // Wait for software TX FIFO to empty
    while(UxSTAbits.TRMT);                  // Wait for TX hardware FIFO to empty
    for(i = UxBRG*32u; i != 0x0000u; i--);  // Wait a little longer for the stop bit and any other residual info to exit
}


/**
 * Enables interrupts used by the UART FIFO TX and RX routines.
 */
void UART_FIFO_EnableInterrupts(void)
{
    _UxRXIE = 1;
    _UxTXIE = 1;
}


/**
 * Disables all interrupts used by the UART FIFO routines.
 */
void UART_FIFO_DisableInterrupts(void)
{
    _UxRXIE = 0;
    _UxTXIE = 0;
}


/**
 * Turns on hardware Auto-baud rate detection. Next character received must be 
 * 0x55 ('U'), after which UART RX will begin operating normally. UART TX rate
 * can change if transmitting while the auto-baud character is received. The
 * 0x55 character used for baud rate detection is not returned in the software
 * RX FIFO (nor hardware RX FIFO either).
 */
void UART_FIFO_ArmAutoBaud(void)
{
    // Clear framing error accumulator if going to auto-baud mode
    UART_RX_FIFO_vars.errors &= ~_U1STA_FERR_MASK;

    // Enable hardware Auto-baud mode
    UxMODEbits.ABAUD = 1;
}


/**
 * Tests if the RX hardware is armed for Auto-baud detection. If armed, the next
 * RX character is used to set the baud-rate and therefore must be the value 
 * 0x55 ('U'). This character does not get inserted into the RX stream when
 * it is used for auto-baud.
 *
 * @return 1 if auto-baud mode is set (UART_FIFO_ArmAutoBaud() has been called
 *         and a timing character has not been received yet). 0 if auto-baud has
 *         completed or has never been armed.
 */
unsigned int UART_FIFO_QueryAutoBaud(void)
{
    return UxMODEbits.ABAUD;
}

/**
 * Generic write() function for printf() and other C stdlib file I/O operations.
 * This function overrides the internal write() function of the library so
 * whenever a higher layer printf() or similar call is made, this function gets
 * called automatically so we can beam the stdout data into this software TX
 * FIFO instead of directly to a UART peripheral.
 *
 * For further information and info on how to modify this function, see the
 * XC16 C library user's guide (DS50002071), ex:
 * C:\Program Files (x86)\Microchip\xc16\v1.21\docs\MPLAB_XC16_C_Compiler_Users_Guide.pdf
 *
 * @param handle File handle specifying where the write is intended to go (ex:
 *               stdout, stderr, file, etc.). handle is not checked here, so
 *               this function will wrap ALL writes to stdout, stderr, and all
 *               other file handles. This function should be modified or
 *               replaced if needing other handle destinations besides this
 *               UART module.
 *
 * @param *buffer Pointer to source data being written to the handle.
 *
 * @param len Number of bytes that need to be copied from *buffer to the output
 *            handle.
 *
 * @return int Number of bytes copied from *buffer to the UART TX FIFO. This
 *         function blocks if there is insufficient space directly in the
 *         software TX FIFO until space is freed up. However, it returns
 *         immediately after all bytes have been buffered allowing efficient
 *         asynchronous and piplined transmission.
 */
#if defined(__XC16__)
int __attribute__((__section__(".libc.write"))) write(int handle, void *buffer, unsigned int len)
{
    UART_TX_FIFO_Write(buffer, len);
    return len;
}
#elif defined(__XC32__)
void _mon_putc(char c)
{
    UART_TX_FIFO_Write8(c);
}

int _mon_getc(int canblock)
{
    if(canblock == 0)
    {
        if(UART_RX_FIFO_ReadableLength() == 0u)
            return -1;
    }
    return (int)(unsigned int)UART_RX_FIFO_Read8();        
}
#endif


/*********************************************************************
 * Function:        void __attribute__((weak, vector(_UARTx_VECTOR), interrupt(IPL4SOFT), keep)) UxRXInterrupt(void)
 *                  void _ISR _UxRXInterrupt(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          Records hardware overflow/framing/etc errors
 *                  into UART_RX_FIFO_vars.errors.
 *
 * Side Effects:    None
 *
 * Overview:        Receives a physical RX byte from the UART and
 *                  places it in a local RAM FIFO for software to
 *                  read it at its leisure.
 *
 * Note:            None
 ********************************************************************/
#if defined(__XC32__)
void __attribute__((weak, vector(CAT3(_UART,UART_MODULE_NUM,_RX_VECTOR)), interrupt(IPL4SOFT), keep)) UxRXInterrupt(void)
#else
void __attribute__((interrupt, __no_auto_psv__)) _UxRXInterrupt(void)
#endif
{
    unsigned int i;

    // Clear the interrupt flag so we don't keep entering this ISR
    _UxRXIF = 0;


    // Read all available bytes. This is checked before reading anything because
    // in the event of an auto-baud 0x55 reception, we'll get this RX interrupt,
    // but won't actually have any data to put into the software RX FIFO.
    while(UxSTAbits.URXDA)
    {
        // Get the byte
        i = UxRXREG;

        // Collect any Parity (not used, so never should be set), Framing, 
        // and Overrun errors. Parity/Framing errors persist only while the 
        // applicable byte is at the top of the hardware RX FIFO, so we need to 
        // collect it for every byte.
        UART_RX_FIFO_vars.errors |= UxSTA & (_U1STA_PERR_MASK | _U1STA_FERR_MASK | _U1STA_OERR_MASK);

        // Throw this byte away if it would cause overflow
        if(UART_RX_FIFO_vars.dataCount >= sizeof(UART_RX_FIFO_vars.fifoRAM))
        {
            UART_RX_FIFO_vars.errors |= 0x0001; // Flag for our software RX FIFO overflow
            continue;
        }

        // Copy the byte into the local FIFO
        // NOTE: The FIFO internal data structures are being accessed directly
        // here rather than calling the UART_RX_FIFO_Write*() functions because
        // any function call in an ISR will trigger a whole lot of compiler
        // context saving overhead. The compiler has no way of knowing what
        // registers any given function will clobber, so it has to save them
        // all. For efficiency, the needed write-one-byte code is duplicated
        // here.
        *UART_RX_FIFO_vars.headPtr++ = i;
        if(UART_RX_FIFO_vars.headPtr >= UART_RX_FIFO_vars.fifoRAM + sizeof(UART_RX_FIFO_vars.fifoRAM))
        {
            UART_RX_FIFO_vars.headPtr = UART_RX_FIFO_vars.fifoRAM;
        }
        ATOMIC_ADD(UART_RX_FIFO_vars.dataCount, 1);
    }


    // Clear overflow flag if it it has become set. This bit prevents future
    // reception so it must be cleared. If set, we (most likely) were able to
    // record it earlier in UART_RX_FIFO_vars.errors.
    UxSTAbits.OERR = 0;
}


/*********************************************************************
 * Function:        void __attribute__((weak, vector(_UARTx_TX_VECTOR), interrupt(IPL4SOFT), keep)) UxTXInterrupt(void)
 *                  void _ISR _UxTXInterrupt(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        Transmits physical TX FIFOed bytes out of the UART
 *
 * Note:            Any flow control requested (on or off) externally will
 *                  inject an XON or XOFF into the TX stream immediately rather
 *                  than placing it in the TX FIFO.
 ********************************************************************/
#if defined(__XC32__)
void __attribute__((weak, vector(CAT3(_UART,UART_MODULE_NUM,_TX_VECTOR)), interrupt(IPL4SOFT), keep)) UxTXInterrupt(void)
#else
void __attribute__((interrupt, __no_auto_psv__)) _UxTXInterrupt(void)
#endif
{
    // Clear the TX interrupt flag before transmitting again
    _UxTXIF = 0;

    // Transmit a byte, if possible, if pending
    // NOTE: The FIFO internal data structures are being accessed directly
    // here rather than calling the UART_TX_FIFO_Read*() functions because
    // any function call in an ISR will trigger a whole lot of compiler 
    // context saving overhead. The compiler has no way of knowing what 
    // registers any given function will clobber, so it has to save them 
    // all. For efficiency, the needed read-one-byte code is duplicated here.
    while(!UxSTAbits.UTXBF)
    {
        // Stop loading UART TX REG if there is nothing left to transmit; duh!
        if(UART_TX_FIFO_vars.dataCount == 0u)
            return;

        // Get a byte from the TX buffer
        UxTXREG = *UART_TX_FIFO_vars.tailPtr++;
        if(UART_TX_FIFO_vars.tailPtr >= UART_TX_FIFO_vars.fifoRAM + sizeof(UART_TX_FIFO_vars.fifoRAM))
            UART_TX_FIFO_vars.tailPtr = UART_TX_FIFO_vars.fifoRAM;
        ATOMIC_SUB(UART_TX_FIFO_vars.dataCount, 1);
    }
}
