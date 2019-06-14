/*! @file
 *
 *  @brief calculation of voltages and currents
 *
 *
 *  @author Lucien Tran
 *  @date 2019-04-16
 */


#include "calculation.h"
#include "OS.h"

void Sliding_Voltage(float data, TChannelData* channelData)
{
  OS_DisableInterrupts();
  channelData->Voltage[15] = data;
  channelData->VoltageSqr[15] = data*data;
  channelData->TotalVoltageSqr += channelData->VoltageSqr[15];
  channelData->TotalVoltageSqr -= channelData->VoltageSqr[0];
  for(int i = 1; i < 16; i++)
  {
    channelData->Voltage[i-1] = channelData->Voltage[i];
    channelData->VoltageSqr[i-1] = channelData->Voltage[i];
  }
  OS_EnableInterrupts();
}


float Real_RMS(TChannelData* channelData)
{
  OS_DisableInterrupts();
  float SqRootRMS = ((channelData->TotalVoltageSqr)/16); // Dividing the total of v^2 by the number of sample per period N = 16
  float voltageRMS = sqrt(SqRootRMS); // Using equation from math.h
//  float voltageRMS = ((SqRootRMS/0.707)+0.707)/2; // equation given in Fixed-point processing for square root of a value
  OS_EnableInterrupts();
  return voltageRMS;
}

float Current_RMS(float voltageRMS)
{
  return (voltageRMS*0.350); // 350mV RMS = 1 A RMS (project notes)
}




