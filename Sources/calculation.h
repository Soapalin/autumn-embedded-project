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
  float VoltageRMS;
  float TotalVoltageSqr;
}TChannelVoltage;


typedef enum
{
  INVERSE,
  VERY_INVERSE,
  EXTREMELY_INVERSE
}TCharacteristic;

TCharacteristic Current_Charac; // Keepint track of the current mode INVERSE, VERY INVERSE, EXTREMELY INVERSE
TChannelVoltage ChannelVoltages[NB_ANALOG_CHANNELS]; // keeping track of voltages of each channel independently


/*! @brief Keeps the 16 samples of voltages, voltages square and running total of the square
 *
 * @param data - newest sample taken by uC
 * @param channelData - tracking of indepedent voltages for each channel
 */
void Sliding_Voltage(float data,TChannelVoltage channelData);

/*! @brief Returns the voltage RMS, calculated from global variable TotalVoltageSqr
 *
 *  @param channelData - tracking of indepedent voltages for each channel
 *
 *  @return float - value of voltage RMS
 */
float Real_RMS(TChannelVoltage channelData);

/*! @brief Converts voltage RMS to current RMS
 *
 *  @return float - current RMS
 */
float Current_RMS(float voltageRMS);


