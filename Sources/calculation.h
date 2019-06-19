/*! @file
 *
 *  @brief calculation of voltages and currents
 *
 *
 *  @author Lucien Tran
 *  @date 2019-04-16
 */

#include <math.h>
#include "types.h"

typedef struct
{
  float Voltage[16];
  float VoltageSqr[16];
  float TotalVoltageSqr;
  float voltageRMS;
  float currentRMS;
}TChannelData;

#define ADC_RATE            3276.7
#define ANALOG_TO_VOLT(X)   (float) X / (float) ADC_RATE
#define VOLT_TO_ANALOG(X)   (uint16_t) (float) X * (float) ADC_RATE
typedef enum
{
  INVERSE,
  VERY_INVERSE,
  EXTREMELY_INVERSE
}TCharacteristic;

TCharacteristic Current_Charac; // Keeping track of the current mode INVERSE, VERY INVERSE, EXTREMELY INVERSE


/*! @brief Keeps the 16 samples of voltages, voltages square and running total of the square
 *
 * @param data - newest sample taken by uC
 * @param channelData - tracking of indepedent voltages for each channel
 */
void Sliding_Voltage(float data,TChannelData* channelData);

/*! @brief Returns the voltage RMS, calculated from global variable TotalVoltageSqr
 *
 *  @param channelData - tracking of indepedent voltages for each channel
 *
 *  @return float - value of voltage RMS
 */
float Real_RMS(TChannelData* channelData);

/*! @brief Converts voltage RMS to current RMS
 *
 *  @return float - current RMS
 */
float Current_RMS(float voltageRMS);

//
