/* ###################################################################
 **     Filename    : main.c
 **     Project     : Lab6
 **     Processor   : MK70FN1M0VMJ12
 **     Version     : Driver 01.01
 **     Compiler    : GNU C Compiler
 **     Date/Time   : 2015-07-20, 13:27, # CodeGen: 0
 **     Abstract    :
 **         Main module.
 **         This module contains user's application code.
 **     Settings    :
 **     Contents    :
 **         No public methods
 **
 ** ###################################################################*/
/*!
 ** @file main.c
 ** @version 6.0
 ** @brief
 **         Main module.
 **         This module contains user's application code.
 */
/*!
 **  @addtogroup main_module main module documentation
 **  @{
 */
/* MODULE main */

// CPU module - contains low level hardware initialization routines
#include "Cpu.h"

// Simple OS
#include "OS.h"

// Analog functions
#include "analog.h"

// Modules
#include "UART.h"
#include "packet.h"
#include "Flash.h"
#include "PIT.h"
#include "RTC.h"
#include "LEDs.h"
#include "FTM.h"
#include "calculation.h"

// Global variables and macro definitions
const uint32_t BAUDRATE = 115200; /*!< Baud Rate specified in project */
const uint32_t MODULECLK = CPU_BUS_CLK_HZ; /*!< Clock Speed referenced from Cpu.H */
const uint16_t STUDENT_ID = 0x22E2; /*!< Student Number: 7533 */
const uint8_t PACKET_ACK_MASK = 0x80; /*!< Packet Acknowledgment mask, referring to bit 7 of the Packet */
static volatile uint16union_t *TowerNumber; /*!< declaring static TowerNumber Pointer */
static volatile uint16union_t *TowerMode; /*!< declaring static TowerMode Pointer */
static volatile uint16union_t *Tripped; /*!< declaring static TowerMode Pointer */
static volatile uint8_t *CharacFlash;
uint16union_t NumberTripped;
const uint32_t PIT_Period = 1000000000; /*!< 1 second in nano */
bool ResetMode;
float Frequency;




// Prototypes functions
bool TowerInit(void);
void PacketHandler(void);
bool StartupPackets(void);
bool VersionPackets(void);
bool TowerNumberPackets(void);
bool TowerModePackets(void);
bool TowerTimePackets(void);
bool ProgramBytePackets(void);
bool ReadBytePackets(void);
bool DORPackets (void);
void PIT0Callback(void);


// ----------------------------------------
// Thread set up
// ----------------------------------------
// Arbitrary thread stack size - big enough for stacking of interrupts and OS use.
#define THREAD_STACK_SIZE 100
#define NB_ANALOG_CHANNELS 3

OS_ECB* PacketHandlerSemaphore; //Declare a semaphore, to be signaled.

TChannelData ChannelsData[NB_ANALOG_CHANNELS]; // keeping track of voltages of each channel independently

// Thread stacks
OS_THREAD_STACK(InitModulesThreadStack, THREAD_STACK_SIZE); /*!< The stack for the LED Init thread. */
static uint32_t AnalogThreadStacks[NB_ANALOG_CHANNELS][THREAD_STACK_SIZE] __attribute__ ((aligned(0x08)));
OS_THREAD_STACK(UARTRXStack, THREAD_STACK_SIZE);
OS_THREAD_STACK(UARTTXStack, THREAD_STACK_SIZE);
OS_THREAD_STACK(PacketHandlerStack, THREAD_STACK_SIZE);
OS_THREAD_STACK(PIT0Stack, THREAD_STACK_SIZE);

// ----------------------------------------
// Thread priorities
// 0 = highest priority
// ----------------------------------------
const uint8_t ANALOG_THREAD_PRIORITIES[NB_ANALOG_CHANNELS] = { 3, 4, 5};

/*! @brief Data structure used to pass Analog configuration to a user thread
 *
 */
typedef struct AnalogThreadData
{
  OS_ECB* semaphore;
  uint8_t channelNb;
} TAnalogThreadData;

/*! @brief Analog thread configuration data
 *
 */
static TAnalogThreadData AnalogThreadData[NB_ANALOG_CHANNELS] =
{
  {
    .semaphore = NULL,
    .channelNb = 0
  },
  {
    .semaphore = NULL,
    .channelNb = 1
  },
  {
    .semaphore = NULL,
    .channelNb = 2
  }
};

/*! @brief The Packet Handler Thread
 *
 *  @note - Loops until interrupted by thread of higher priority
 */
void PacketHandlerThread(void* pData)
{
  OS_SemaphoreWait(PacketHandlerSemaphore, 0); //Wait until triggered by Semaphore Signal
  StartupPackets();
  for(;;)
  {
    if (Packet_Get())
    {
      PacketHandler(); /*!<  When a complete packet is finally formed, handle the packet accordingly */
    }
  }
}

/*! @brief Initialise the Low Power Timer
 *
 *  @param count - the period of the low power timer in millisecond
 *
 *  @note - Loops until interrupted by thread of higher priority
 */
void LPTMRInit(const int count)
{
  // Enable clock gate to LPTMR module
  SIM_SCGC5 |= SIM_SCGC5_LPTIMER_MASK;

  // Disable the LPTMR while we set up
  // This also clears the CSR[TCF] bit which indicates a pending interrupt
  LPTMR0_CSR &= ~LPTMR_CSR_TEN_MASK;

  // Enable LPTMR interrupts
  LPTMR0_CSR |= LPTMR_CSR_TIE_MASK;
  // Reset the LPTMR free running counter whenever the 'counter' equals 'compare'
  LPTMR0_CSR &= ~LPTMR_CSR_TFC_MASK;
  // Set the LPTMR as a timer rather than a counter
  LPTMR0_CSR &= ~LPTMR_CSR_TMS_MASK;

  // Bypass the prescaler
  LPTMR0_PSR |= LPTMR_PSR_PBYP_MASK;
  // Select the prescaler clock source
  LPTMR0_PSR = (LPTMR0_PSR & ~LPTMR_PSR_PCS(0x3)) | LPTMR_PSR_PCS(1);

  // Set compare value
  LPTMR0_CMR = LPTMR_CMR_COMPARE(count);

  // Initialize NVIC
  // see p. 91 of K70P256M150SF3RM.pdf
  // Vector 0x65=101, IRQ=85
  // NVIC non-IPR=2 IPR=21
  // Clear any pending interrupts on LPTMR
  NVICICPR2 = NVIC_ICPR_CLRPEND(1 << 21);
  // Enable interrupts from LPTMR module
  NVICISER2 = NVIC_ISER_SETENA(1 << 21);

//  LPTMR0_CSR |= LPTMR_CSR_TEN_MASK;
}

/*! @brief LPTimer ISR - used for Reset mode - outputting the trip for a second and re-init the DOR with all values equaling to zero
 *
 *  @note - Loops until interrupted by thread of higher priority
 */
void __attribute__ ((interrupt)) LPTimer_ISR(void)
{
  // Clear interrupt flag
  LPTMR0_CSR |= LPTMR_CSR_TCF_MASK;
  LPTMR0_CSR &= ~LPTMR_CSR_TEN_MASK;
  ResetMode = true;

  for (uint8_t analogNb = 0; analogNb < NB_ANALOG_CHANNELS; analogNb++)
  {
    ChannelsData[analogNb].totalVoltageSqr = 0;
    ChannelsData[analogNb].currentRMS = 0;
    ChannelsData[analogNb].voltageRMS = 0;
    for(uint8_t i = 0; i < 16; i++)
    {
      ChannelsData[analogNb].voltage[i] = 0;
      ChannelsData[analogNb].voltageSqr[i] = 0;
    }
  }


}

/*! @brief Initialises modules.
 *
 */
static void InitModulesThread(void* pData)
{
  OS_DisableInterrupts();
  // Analog
  (void)Analog_Init(CPU_BUS_CLK_HZ);

  // Generate the global analog semaphores
  for (uint8_t analogNb = 0; analogNb < NB_ANALOG_CHANNELS; analogNb++)
    AnalogThreadData[analogNb].semaphore = OS_SemaphoreCreate(0);

  LPTMRInit(1000); // Set the Low power timer to a period of 1 second
  Current_Charac = INVERSE; // Set the default mode to inverse
  TowerInit(); // Initialise tower modules used in previous labs
  Analog_Put(0, 0); 
  Analog_Put(1, 0);
  PIT_Set(1250000, true, 0); // Set the sample period to 1.25 ms
  OS_EnableInterrupts();
  while (OS_SemaphoreSignal(PacketHandlerSemaphore) != OS_NO_ERROR); // Signal Packet Handler Thread 

  // We only do this once - therefore delete this thread
  OS_ThreadDelete(OS_PRIORITY_SELF);
}

/*! @brief Samples a value on an ADC channel and sends it to the corresponding DAC channel.
 *
 */
void AnalogLoopbackThread(void* pData)
{
  // Make the code easier to read by giving a name to the typecast'ed pointer
  #define analogData ((TAnalogThreadData*)pData)


  static uint32_t oldCurrent[NB_ANALOG_CHANNELS];
  static uint32_t counterTrip[NB_ANALOG_CHANNELS];
  static uint32_t goalTrip[NB_ANALOG_CHANNELS];
  static uint32_t oldGoal[NB_ANALOG_CHANNELS];

  for (;;)
  {
    (void) OS_SemaphoreWait(analogData->semaphore, 0);
    int16_t analogInputValue;
    OS_DisableInterrupts();
    // Get analog sample
    Analog_Get(analogData->channelNb, &analogInputValue); 
    if (ResetMode)
    {
      // Resetting the circuit breaker and the code after tripping 
      counterTrip[0] = counterTrip[2] = counterTrip[1] = 0;
      goalTrip[0] = goalTrip[1] = goalTrip[2] = 0;
      LEDs_Off(LED_GREEN);
      LEDs_Off(LED_BLUE);
      ResetMode = false;
    }
    Sliding_voltage(ANALOG_TO_VOLT(analogInputValue), &ChannelsData[analogData->channelNb]); // Adding the new sample value to the structure
    ChannelsData[analogData->channelNb].voltageRMS = Real_RMS(&ChannelsData[analogData->channelNb]); // Calculate voltage RMS of the last 16 samples
    ChannelsData[analogData->channelNb].currentRMS =  Current_RMS(ChannelsData[analogData->channelNb].voltageRMS); // Finding and storing the current RMS in the structure
    if (ChannelsData[analogData->channelNb].currentRMS > 1.03) //&& (oldCurrent != (uint32_t) ChannelsData[analogData->channelNb].currentRMS*100)
    {
      if ((ChannelsData[analogData->channelNb].currentRMS != oldCurrent[analogData->channelNb]) || (!goalTrip[analogData->channelNb]))
      {
        goalTrip[analogData->channelNb] = Calculate_TripGoal(ChannelsData[analogData->channelNb].currentRMS); // Calculate the goal to reach before tripping
        if(oldGoal[analogData->channelNb])
          counterTrip[analogData->channelNb] = (uint32_t) (((float) (counterTrip[analogData->channelNb]))/ ((float) (oldGoal[analogData->channelNb])))*100/(goalTrip[analogData->channelNb]);
        oldGoal[analogData->channelNb] = goalTrip[analogData->channelNb];
      }
      oldCurrent[analogData->channelNb] = ChannelsData[analogData->channelNb].currentRMS;


      Analog_Put(0, VOLT_TO_ANALOG(5)); // Detecting a currentRMS over 1.03, outputting in time channel
      LEDs_On(LED_BLUE); // Using LED so don't have to check on DSO
      counterTrip[analogData->channelNb]++; // Incremetnting the count to reach the goal
      if (counterTrip[analogData->channelNb] >= goalTrip[analogData->channelNb]) // If goal is reached or beyond
      {
        Analog_Put(1, VOLT_TO_ANALOG(5)); // Output in channel 2 after trip "delay"
        LEDs_On(LED_GREEN); // Using LED to check without DSO
        LPTMR0_CSR |= LPTMR_CSR_TEN_MASK; // Start timer for reset mode 
        NumberTripped.l++; // Incrementing the number of time Tripped
        OS_EnableInterrupts();
//        Flash_Write16((volatile uint16_t *) Tripped, NumberTripped.l); // Write the number of time tripped to Flash
      }
      else 
      {
        OS_EnableInterrupts();
      }
    }
    if (PeriodComplete == 16) // Finding the frequency after every 16 samples, PeriodComplete is incremented in PIT_ISR
    {
      TCrossing crossing; // structure that stores the zero crossings of waveform
      if (Zero_Crossings(ChannelsData[analogData->channelNb].voltage, &crossing))
        Frequency = Calculate_Frequency(&crossing); // Frequency is a global variable storing the current frequency of the wave
      OS_EnableInterrupts();
    }
    else if (ChannelsData[analogData->channelNb].currentRMS < 1.03) // If the currentRMS in under 1.03, the circuit breaking should not be tripped
    {
      Analog_Put(0, 0);
      LEDs_Off(LED_BLUE);
      OS_EnableInterrupts();
    }
    else 
      OS_EnableInterrupts();
  }
}

/*lint -save  -e970 Disable MISRA rule (6.3) checking. */
int main(void)
/*lint -restore Enable MISRA rule (6.3) checking. */
{
  OS_ERROR error;

  // Initialise low-level clocks etc using Processor Expert code
  PE_low_level_init();

  // Initialize the RTOS
  OS_Init(CPU_CORE_CLK_HZ, true);

  // Create module initialisation thread
  error = OS_ThreadCreate(InitModulesThread,
                          NULL,
                          &InitModulesThreadStack[THREAD_STACK_SIZE - 1],
                          0); // Highest priority

  while (OS_ThreadCreate(UARTRXThread, NULL, &UARTRXStack[THREAD_STACK_SIZE-1], 1) != OS_NO_ERROR); //UARTRX Thread
  while (OS_ThreadCreate(UARTTXThread, NULL, &UARTTXStack[THREAD_STACK_SIZE-1], 2) != OS_NO_ERROR); //UARTTX Thread


  // Create threads for 3 analog loopback channels
  for (uint8_t threadNb = 0; threadNb < NB_ANALOG_CHANNELS; threadNb++)
  {
    error = OS_ThreadCreate(AnalogLoopbackThread,
                            &AnalogThreadData[threadNb],
                            &AnalogThreadStacks[threadNb][THREAD_STACK_SIZE - 1],
                            ANALOG_THREAD_PRIORITIES[threadNb]);
  }

  while (OS_ThreadCreate(PIT0Thread, NULL, &PIT0Stack[THREAD_STACK_SIZE-1], 6) != OS_NO_ERROR); //PIT Thread
  while (OS_ThreadCreate(PacketHandlerThread, NULL, &PacketHandlerStack[THREAD_STACK_SIZE-1], 7) != OS_NO_ERROR); //Packet Handler Thread
  PacketHandlerSemaphore = OS_SemaphoreCreate(0);

  // Start multithreading - never returns!
  OS_Start();
}


/*! @brief Process the packet that has been received
 *
 *  @note Assumes that Packet_Init and Packet_Get was called
 */
void PacketHandler(void)
{ /*!<  Packet Handler used after Packet Get */
  bool actionSuccess;  /*!<  Acknowledge is false as long as the package isn't acknowledge or if it's not required */
  switch (Packet_Command & ~PACKET_ACK_MASK)
  {
    case TOWER_STARTUP_COMMAND:
      actionSuccess = StartupPackets();
      break;

    case TOWER_VERSION_COMMAND:
      actionSuccess = VersionPackets();
      break;

    case TOWER_NUMBER_COMMAND:
      actionSuccess = TowerNumberPackets();
      break;

    case TOWER_MODE_COMMAND:
      actionSuccess = TowerModePackets();
      break;

    case SET_TIME_COMMAND:
      actionSuccess = TowerTimePackets();
      break;

    case FLASH_PROGRAM_COMMAND:
      actionSuccess = ProgramBytePackets();
      break;

    case FLASH_READ_COMMAND:
      actionSuccess = ReadBytePackets();
      break;

    case DOR_COMMAND:
      actionSuccess = DORPackets();
      break;


  }

  if (Packet_Command & PACKET_ACK_MASK) /*!< if ACK bit is set, need to send back ACK packet if done successfully and NAK packet with bit7 cleared */
  {
    if (actionSuccess)
    {
      Packet_Put(Packet_Command, Packet_Parameter1, Packet_Parameter2, Packet_Parameter3);
    }
    else
    {
      Packet_Put((Packet_Command |=PACKET_ACK_MASK),Packet_Parameter1, Packet_Parameter2, Packet_Parameter3);
    }
  }

}

/*! @brief saves in Flash the TowerNumber and the TowerMode
 *
 *
 *  @return bool - TRUE if packet has been sent successfully
 *  @note Assumes that Packet_Init was called
 */
bool TowerInit(void)
{
  /*!<  Allocate var for both Tower Number and Mode, if succcessful, FlashWrite16 them with the right values */
  Flash_Init();
  bool towerModeInit = Flash_AllocateVar( (volatile void **) &TowerMode, sizeof(*TowerMode));
  bool towerNumberInit = Flash_AllocateVar((volatile void **) &TowerNumber, sizeof(*TowerNumber));
  bool trippedInit = Flash_AllocateVar((volatile void **) &Tripped, sizeof(*Tripped));
  bool characInit = Flash_AllocateVar((volatile void **) &CharacFlash, sizeof(*CharacFlash));
  LEDs_Init();
  if (towerModeInit && towerNumberInit && trippedInit && characInit)
  {
    if (Tripped->l == 0xffff) /* if unprogrammed, value = 0xffff, and therefore start writing to it with a value of 1 */
      Flash_Write16((volatile uint16_t *) Tripped, 0x1);
    if (*CharacFlash == 0xff) /* Writing the current IDMT characteristics to Flash */
      Flash_Write8((volatile uint8_t *) CharacFlash, Current_Charac);
    if (TowerMode->l == 0xffff) /* when unprogrammed, value = 0xffff, announces in hint*/
    {
      Flash_Write16((volatile uint16_t *) TowerMode, 0x1); /*!< Parsing through the function: typecast volatile uint16_t pointer from uint16union_t pointer, and default towerMode = 1 */
    }
    if (TowerNumber->l == 0xffff) /* when unprogrammed, value = 0xffff, announces in hint*/
    {
      Flash_Write16((volatile uint16_t *) TowerNumber, STUDENT_ID); /*Like above, but with towerNumber set to our student ID = 7533*/
    }

  }
  PIT_Init(MODULECLK, (void*) &PIT0Callback , NULL);
  return Packet_Init(BAUDRATE, MODULECLK);
}


/*! @brief Send the packets needed on startup

 *  @return bool - TRUE if packet has been sent successfully
 *  @note Assumes that Packet_Init was called
 */
bool StartupPackets(void)
{
  if (Packet_Put(TOWER_STARTUP_COMMAND, TOWER_STARTUP_PARAMETER1, TOWER_STARTUP_PARAMETER2, TOWER_STARTUP_PARAMETER3))
  {
    if (Packet_Put(TOWER_VERSION_COMMAND, TOWER_VERSION_PARAMETER1, TOWER_VERSION_PARAMETER2, TOWER_VERSION_PARAMETER3))
    {
      if (Packet_Put(TOWER_NUMBER_COMMAND, TOWER_NUMBER_GET, TowerNumber->s.Lo, TowerNumber->s.Hi))
      {
        return Packet_Put(TOWER_MODE_COMMAND,TOWER_MODE_GET, TowerMode->s.Lo, TowerMode->s.Hi);
      }
    }
  }
}

/*! @brief Send the tower number packet to the PC
 *
 *  @return bool - TRUE if packet has been sent successfully
 *  @note Assumes that Packet_Init was called
 */
bool TowerNumberPackets(void)
{
  if (Packet_Parameter1 == (uint8_t) 1)
  {
    // if Parameter1 = 1 - get the tower number and send it to PC
    return Packet_Put(TOWER_NUMBER_COMMAND, TOWER_NUMBER_GET, TowerNumber->s.Lo, TowerNumber->s.Hi);
  }
  else if (Packet_Parameter1 == (uint8_t) 2) // if Parameter1 =2 - write new TowerNumber to Flash and send it to interface
  {
    uint16union_t newTowerNumber; /*! < create a union variable to combine the two Parameters*/
    newTowerNumber.s.Lo = Packet_Parameter2;
    newTowerNumber.s.Hi = Packet_Parameter3;
    Flash_Write16((volatile uint16_t *) TowerNumber, newTowerNumber.l);
    return Packet_Put(TOWER_NUMBER_COMMAND, TOWER_NUMBER_SET, TowerNumber->s.Lo, TowerNumber->s.Hi);
  }
}

/*! @brief Send the tower mode packet to the PC
 *
 *  @return bool - TRUE if packet has been sent successfully
 *  @note Assumes that Packet_Init was called
 */
bool TowerModePackets(void)
{
  if (Packet_Parameter1 == 1) // if paramater1 = 1 - get the towermode and send it to PC
  {
    return Packet_Put(TOWER_MODE_COMMAND,TOWER_MODE_GET, TowerMode->s.Lo, TowerMode->s.Hi);
  }
  else if (Packet_Parameter1 == 2) // if parameter1 = 2 - set the towermode, write to Flash and send it back to PC
  {
    uint16union_t newTowerMode; /* !< Create a union variable to combine parameter2 and 3*/
    newTowerMode.s.Lo = Packet_Parameter2;
    newTowerMode.s.Hi = Packet_Parameter3;
    Flash_Write16((volatile uint16_t *) TowerMode, newTowerMode.l);
    return Packet_Put(TOWER_MODE_COMMAND,TOWER_MODE_SET, TowerMode->s.Lo, TowerMode->s.Hi);
  }
  return false;
}


/*! @brief Send the version packet to the PC
 *
 *  @return bool - TRUE if packet has been sent successfully
 *  @note Assumes that Packet_Init was called
 */
bool VersionPackets(void)
{
  return Packet_Put(TOWER_VERSION_COMMAND,TOWER_VERSION_PARAMETER1,TOWER_VERSION_PARAMETER2, TOWER_VERSION_PARAMETER3);
}

/*! @brief Handles the packet to program bytes in FLASH
 *
 *  @return bool - TRUE if packet has been sent and handled successfully
 *  @note Assumes that Packet_Init was called
 */
bool ProgramBytePackets(void)
{
  if (Packet_Parameter1 == 8)
  {
    return Flash_Erase(); /*! < if Parameter1  = 8 - erase the whole sector */
  }
  else if (Packet_Parameter1 > 8)
  {
    return false; //data sent is obsolete
  }
  else /*!< if offset (Parameter1) is between 0 and 7 inclusive, check the offset */
  {
    volatile uint8_t *address = (uint8_t *)(FLASH_DATA_START + Packet_Parameter1);
    return Flash_Write8(address, Packet_Parameter3); //Write in the Flash
  }
  return false;
}

/*! @brief Handles the packet to read bytes from FLASH
 *
 *  @return bool - TRUE if packet has been sent and handled successfully
 *  @note Assumes that Packet_Init was called
 */
bool ReadBytePackets(void)
{
  uint8_t readByte = _FB(FLASH_DATA_START + Packet_Parameter1); /* !< fetching the Byte at offset Parameter1 and send it to PCc*/
  return Packet_Put(FLASH_READ_COMMAND, Packet_Parameter1, 0x0, readByte);
}


/*! @brief Handles the packet RTC time - sends back ther packet to PC if setting time is successful
 *
 *  @return bool - TRUE if packet has been sent and handled successfully
 *  @note Assumes that Packet_Init and RTC_Init was called
 */
bool TowerTimePackets(void)
{
  /*!< Checking if input is valid, if not, return false */
  if (Packet_Parameter1 <= 23)
  {
    if (Packet_Parameter2 <= 59)
    {
      if (Packet_Parameter3 <= 59)
      {
        /*!< sets the time with packet parameters given by PC */
        RTC_Set(Packet_Parameter1, Packet_Parameter2, Packet_Parameter3);
        /*!< returns the original packet to the PC if successful */
        return Packet_Put(SET_TIME_COMMAND, Packet_Parameter1, Packet_Parameter2, Packet_Parameter3);
      }
    }
  }
  return false;
}

/*! @brief Handles the DOR command packets
 *
 *  @return bool - TRUE if packet has been sent and handled successfully
 *  @note Assumes that Packet_Init was called
 */
bool DORPackets (void)
{
  float decimalCurrents[NB_ANALOG_CHANNELS]; // To store the decimal part of the current when sending packet
  float decimalFrequency; // To store the decimal part of the frequency when sending packet
  switch (Packet_Parameter1)
  {
    case DOR_IDMT_CHAR:
      if (Packet_Parameter2 == DOR_IDMT_GET)
      {
  /*GET IDMT CHARACTERISTICS*/
        return Packet_Put(DOR_COMMAND, DOR_IDMT_CHAR, DOR_IDMT_GET, Current_Charac);
      }
      else if (Packet_Parameter2 == DOR_IDMT_SET)
      {
  /*SET IDMT CHARACTERISTICS */
        Current_Charac = Packet_Parameter3;
//        Flash_Write8((volatile uint8_t *) CharacFlash, Current_Charac);
        return Packet_Put(DOR_COMMAND, DOR_IDMT_CHAR, DOR_IDMT_GET, Current_Charac);
      }
      break;


    case DOR_GET_CURRENTS:
      // convert  the current RMS and ouputting it as a packet. MSB is the int part and LSB the float part
      decimalCurrents[0] =  (ChannelsData[0].currentRMS - ((uint8_t) (ChannelsData[0].currentRMS)))*100;
      decimalCurrents[1] =  (ChannelsData[1].currentRMS - ((uint8_t) (ChannelsData[1].currentRMS)))*100;
      decimalCurrents[2] =  (ChannelsData[2].currentRMS - ((uint8_t) (ChannelsData[2].currentRMS)))*100;
      Packet_Put(DOR_COMMAND_CURRENT, 0, (uint8_t) (decimalCurrents[0]), (uint8_t) ChannelsData[0].currentRMS);
      Packet_Put(DOR_COMMAND_CURRENT, 1, (uint8_t) (decimalCurrents[1]), (uint8_t) ChannelsData[1].currentRMS);
      Packet_Put(DOR_COMMAND_CURRENT, 2, (uint8_t) (decimalCurrents[2]), (uint8_t) ChannelsData[2].currentRMS);
      break;

    case DOR_GET_FREQUENCY:
      // convert the frequency and outputting it as a packet. MSB is the int part and LSB is the decimal part
      decimalFrequency =  (Frequency - ((uint8_t) (Frequency)))*100;
      Packet_Put(DOR_COMMAND, 2, (uint8_t) (decimalCurrents[2]), (uint8_t) Frequency);
      break;

    case DOR_GET_TRIPPED:
      //Send the packet including the number of times tripped
      Packet_Put(DOR_COMMAND, DOR_GET_TRIPPED,   NumberTripped.s.Lo,   NumberTripped.s.Hi);
      break;

    case DOR_GET_FAULT:
      break;
  }
}


/*! @brief Triggered during interrupt, toggles green LED
 * Signaling the analog thread for each channel A, B and C 
 *
 *  @note Assumes that PIT_Init called
 */
void PIT0Callback()
{
  //Signals the 3 analog threads semaphores
  for (uint8_t analogNb = 0; analogNb < NB_ANALOG_CHANNELS; analogNb++)
    OS_SemaphoreSignal(AnalogThreadData[analogNb].semaphore);
}


/*!
 ** @}
 */
