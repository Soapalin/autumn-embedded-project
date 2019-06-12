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

const float INVERSE_K = 0.14;
const float INVERSE_ALPHA = 0.02;
const float VERY_INVERSE_K = 13.5;
const float VERY_INVERSE_ALPHA = 1;
const float EXTREMELY_INVERSE_K = 80;
const float EXTREMELY_INVERSE_ALPHA = 2;

void Sliding_Voltage(float data);

void Sliding_VoltageSqr();

float Real_RMS();

float Current_RMS(float voltageRMS);


