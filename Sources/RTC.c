/*! @file
 *
 *  @brief Routines for controlling the Real Time Clock (RTC) on the TWR-K70F120M.
 *
 *  This contains the functions for operating the real time clock (RTC).
 *
 *  @author Lucien Tran & Angus Ryan
 *  @date 2019-05-05
 */

/*!
 *  @addtogroup RTC_module RTC module documentation
 *  @{
 */

#include "types.h"
#include "RTC.h"
#include "MK70F12.h"
#include "LEDs.h"
#include "OS.h"
#include "packet.h"


static void *RTCArguments;
static void (*RTCCallback)(void* RTCArguments);

OS_ECB* RTCSemaphore; //Declare Semaphore

bool RTC_Init(void (*userFunction)(void*), void* userArguments)
{
  RTCCallback = userFunction; /*!< Make the user function equal to RTC callback and accessible as global variable*/
  RTCArguments = userArguments; /*!< Make the user Argument a global variable*/

  SIM_SCGC6 |= SIM_SCGC6_RTC_MASK; /*!< Enable Clock Gate Control Register for RTC */

  /*!< Need to configure the capacitors - from schematics: use 18pF ==> 16 +2 */
  RTC_CR |= RTC_CR_SC16P_MASK;
  RTC_CR |= RTC_CR_SC2P_MASK;

  RTC_CR |= RTC_CR_OSCE_MASK; /*!<  oscillator is enabled. After setting this bit */
   /*!<wait the oscillator startup time before enabling the time counter to allow the 32.768 kHz clock time to stabilize. */
  for(int i= 0; i < 1000000; i++)
  {
  }

  RTC_LR &= ~RTC_LR_CRL_MASK; /*! <Lock the control register after setting it p1398 - needs to be cleared to lock the register*/

  RTC_IER |= RTC_IER_TSIE_MASK; /*!<Enable every second interrupt*/
  //RTC_TSR = 0; Need to be reset or not ???
  RTC_SR |= RTC_SR_TCE_MASK; /*!< Enable Time Counter from RTC status Register (p1395)*/

  /*!< IRQ RTC seconds = 67
   * 67%32 = 3 */
  NVICICPR2 = (1 << 3);  // Clear any pending interrupts with NVIC
  NVICISER2 = (1 << 3); // Enable interrupts with NVIC
  RTCSemaphore = OS_SemaphoreCreate(0); //Create a semaphore

  return true;

}


void RTC_Set(const uint8_t hours, const uint8_t minutes, const uint8_t seconds)
{
  uint32_t timeSet = (hours*3600)+(minutes*60)+ seconds; /*hours minutes and seconds converted to seconds*/
  RTC_SR &= ~RTC_SR_TCE_MASK; /*!<isable counter before writing to it */
  RTC_TSR = timeSet; /*!<write to counter with the time calculated above */
  RTC_SR |= RTC_SR_TCE_MASK;/*!<restart the counter from the new time */
}


void RTC_Get(uint8_t* const hours, uint8_t* const minutes, uint8_t* const seconds)
{
  uint32_t currentTime = RTC_TSR; /*!< current time stored in RTC Time Seconds Register p1390 of reference manual*/
  uint32_t matchTime = RTC_TSR; /*!< Read it a second time to check if they are equal */
  /* Read the RTC time twice and compare them. If they are not equal, error. Try until they are equal*/
  while(currentTime != matchTime)
  {
    /*!< If they are not equal, keep going through the loop until they are equal*/
    currentTime = RTC_TSR;
    matchTime = RTC_TSR;
  }
  *hours = ((currentTime/3600)%24); /*!< Convert seconds into hours  and found the modulus of 24 to not go over 24 hours*/
  *minutes = (currentTime%3600) /60; /*!< Convert seconds to minutes */
  *seconds = (currentTime%3600)%60; /*!< Convert seconds of the day to second of the minute */
}

void __attribute__ ((interrupt)) RTC_ISR(void)
{
  OS_ISREnter();
//  if (RTCCallback)
//  { /*!< Hint 3 in Lab3 Notes - Callback is run every ISR (yellow LED toggle every second) */
//    (*RTCCallback)(RTCArguments);
//  }
  while(OS_SemaphoreSignal(RTCSemaphore) != OS_NO_ERROR); //Signal I2C Semaphore (triggering I2C thread) and ensure it returns no error
  OS_ISRExit();
}

void RTCThread(void* pData)
{

  for(;;)
  {
    OS_SemaphoreWait(RTCSemaphore, 0); //Wait for semaphore to be signaled
    LEDs_Toggle(LED_YELLOW);
    uint8_t hours, minutes, seconds;
    RTC_Get(&hours, &minutes, &seconds);
    Packet_Put(SET_TIME_COMMAND, hours, minutes, seconds);
  }
}
/*!
* @}
*/
