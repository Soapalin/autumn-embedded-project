/*! @file
 *
 *  @brief calculation of voltages and currents
 *
 *
 *  @author Lucien Tran
 *  @date 2019-04-16
 */
#include "types.h"
#include "calculation.h"

void Sliding_Voltage(float data)
{
  Voltage[15] = data;
  VoltageSqr[15] = data*data;
  TotalVoltageSqr += data;
  TotalVoltageSqr -= Voltage[0];
  for(int i = 1; i < 15; i++)
  {
    Voltage[i] = Voltage[i-1];
    VoltageSqr[i] = Voltage[i-1];
  }
}

float Real_RMS()
{
  float SqRootRMS = (TotalVoltageSqr/16);
  float voltageRMS = ((SqRootRMS/0.707)+0.707)/2;
  return voltageRMS;
}

float Current_RMS(float voltageRMS)
{
  return (voltageRMS*0.350);
}




