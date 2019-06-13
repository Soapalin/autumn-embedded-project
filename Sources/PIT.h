/*! @file
 *
 *  @brief Routines for controlling Periodic Interrupt Timer (PIT) on the TWR-K70F120M.
 *
 *  This contains the functions for operating the periodic interrupt timer (PIT).
 *
 *  @author PMcL
 *  @date 2015-08-22
 */

#ifndef PIT_H
#define PIT_H

// new types
#include "types.h"

uint16union_t Tripped;

/*! @brief Sets up the PIT before first use.
 *
 *  Enables the PIT and freezes the timer when debugging.
 *  @param moduleClk The module clock rate in Hz.
 *  @param userFunction is a pointer to a user callback function.
 *  @param userArguments is a pointer to the user arguments to use with the user callback function.
 *  @return bool - TRUE if the PIT was successfully initialized.
 *  @note Assumes that moduleClk has a period which can be expressed as an integral number of nanoseconds.
 */
bool PIT_Init(const uint32_t moduleClk, void (*userFunction)(void*), void* userArguments);

/*! @brief Sets the value of the desired period of the PIT.
 *
 *  @param period The desired value of the timer period in nanoseconds.
 *  @param restart TRUE if the PIT is disabled, a new value set, and then enabled.
 *                 FALSE if the PIT will use the new value after a trigger event.
 *  @param channel - desired channel to manipulate
 *  @note The function will enable the timer and interrupts for the PIT.
 */
void PIT_Set(const uint32_t period, const bool restart, uint8_t channel);

/*! @brief Enables or disables the PIT.
 *
 *  @param enable - TRUE if the PIT is to be enabled, FALSE if the PIT is to be disabled.
 *  @param channel - desired channel to manipulate
 */
void PIT_Enable(const bool enable, uint8_t channel);

/*! @brief Interrupt service routine for the PIT.
 *
 *  The periodic interrupt timer has timed out.
 *  The user callback function will be called.
 *  @note Assumes the PIT has been initialized.
 */
void __attribute__ ((interrupt)) PIT0_ISR(void);
void __attribute__ ((interrupt)) PIT1_ISR(void);
//void __attribute__ ((interrupt)) PIT2_ISR(void);
//void __attribute__ ((interrupt)) PIT3_ISR(void);


/*! @brief PIT Thread
 *
 *Every period, this function is triggered and values of the accelerometer is sent
 */
void PIT0Thread(void* pData);
void PIT1Thread(void* pData);

//void PIT2Thread(void* pData);
//void PIT3Thread(void* pData);
#endif
