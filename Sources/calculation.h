/*! @file
 *
 *  @brief calculation of voltages and currents
 *
 *
 *  @author Lucien Tran
 *  @date 2019-04-16
 */

float Voltage[16];
float VoltageSqr[16];
float VoltageRMS;
float TotalVoltageSqr;

typedef enum
{
  INVERSE,
  VERY_INVERSE,
  EXTREMELY_INVERSE
}TCharacteristic;

TCharacteristic Current_Charac;

void Sliding_Voltage(float data);

void Sliding_VoltageSqr();

float Real_RMS();

float Current_RMS(float voltageRMS);


