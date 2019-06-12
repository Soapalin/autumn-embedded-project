/*! @file
 *
 *  @brief calculation of voltages and currents
 *
 *
 *  @author Lucien Tran
 *  @date 2019-04-16
 */


#include "calculation.h"

void Sliding_Voltage(float data)
{
  Voltage[15] = data;
  VoltageSqr[15] = data*data;
  TotalVoltageSqr += VoltageSqr[15];
  TotalVoltageSqr -= Voltage[0];
  for(int i = 1; i < 15; i++)
  {
    Voltage[i] = Voltage[i-1];
    VoltageSqr[i] = Voltage[i-1];
  }
}


float Real_RMS()
{
  float SqRootRMS = (TotalVoltageSqr/16); // Dividing the total of v^2 by the number of sample per period N = 16
  float voltageRMS = sqrt(SqRootRMS); // Using equation from math.h
//  float voltageRMS = ((SqRootRMS/0.707)+0.707)/2; // equation given in Fixed-point processing for square root of a value
  return voltageRMS;
}

float Current_RMS(float voltageRMS)
{
  return (voltageRMS*0.350); // 350mV RMS = 1 A RMS (project notes)
}




