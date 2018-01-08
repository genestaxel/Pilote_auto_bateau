/** 
\file main.c
\brief 		Fichier unique de la carte V-T°-P NMEA183 PIC16F87x

\date 	  	Novembre 2004
\version  	PIC16F873
\version  	Carte marine V-T°-P NMEA183 PIC16F873
\version    Compilateur C CCS
\version 	1.0
\author		David Provent, modifications et adaptation au PIC16F876 Vincent AUBINEAU,  Exxotest, Annecy, Haute-Savoie, FRANCE.
\bug 		Pas de bogues connus pour le moment 
*/


#case

#include <16F876A.h>
#include "types.h"

#device ADC=10
#fuses NOCPD, NOPROTECT, NODEBUG
#fuses NOWDT, NOLVP, NOPUT, NOBROWNOUT
#fuses HS
#use delay(clock=16000000)
#use fast_io(A)
#use fast_io(B)
#use fast_io(C)

// I/O pins
#define MES_TEMP	PIN_A0  ///< Entrée mesure de température (analogique)
#define MES_ADC1    PIN_A1  ///< Entrée mesure ADC1 (analogique) du connecteur orange 2 voies
#define MES_ADC2    PIN_A5  ///< Entrée mesure ADC2 (analogique) du connecteur orange 2 voies
#define MES_SPEED	PIN_B0  ///< Mesure de la vitesse
#define LED		    PIN_B5  ///< LED verte
#define NMEA		PIN_C6  ///< TX NMEA183

#define ADC_TEMP 0  ///< ADC entrée température du MUX
#define ADC_1    1  ///< ADC 1 entrée connecteur orange du MUX
#define ADC_2    4  ///< ADC 2 entrée connecteur orange du MUX

#define AN_VALUE_TMP_MAX	223 ///< Température maximale de l'eau (ADC) : 40°C
#define AN_VALUE_TMP_MIN	706 ///< Température minimale de l'eau (ADC) : -7°C
#define VALUE_TMP_MAX	    40  ///< Température maximale de l'eau : 40°C
#define VALUE_TMP_MIN	    -7  ///< Température minimale de l'eau :-7°C

#define NUM_TMP		7
const WORD pwTemp[NUM_TMP] = { AN_VALUE_TMP_MIN, 640, 595, 539, 465, 366, AN_VALUE_TMP_MAX }; // -7, 1, 5, 10, 16, 25, 40
const CHAR pcTemp[NUM_TMP] = { VALUE_TMP_MIN, 1, 5, 10, 16, 25, VALUE_TMP_MAX }; // -7, 1, 5, 10, 16, 25, 40

BYTE bFrameBuf[60];
BYTE bFrameCrc;
CHAR cTemp;
WORD wTemp;
BOOL SpeedNull;
BOOL SpeedValid;
BOOL UpdateSpeed;
BOOL SpeedChange;
WORD wSpeed;

typedef struct {
	WORD wLow;
	BYTE bHigh;
} ST_PERIODE;
ST_PERIODE stPeriode;

BYTE bPeriode[4];
#define PERIODE			(*((DWORD*)bPeriode))
#define PERIODE_L		(*((WORD*)bPeriode))	///< Partie basse de la periode
#define PERIODE_H		(bPeriode[2])			///< Partie haute de la periode
#define PERIODE_N		(bPeriode[3])			///< Non utilisé, doit être à 0

BYTE CalcCRC( VOID );   ///< Calcul du CRC de la trame NMEA183
VOID SendFrame( VOID ); ///< Envoie de la trame NMEA183


VOID main( VOID )
{
	BYTE i;

	// Setup I/O PORTS
	set_tris_a( 0b11111111 ); // 0 output, 1 input
	set_tris_b( 0b11011111 ); // 0 output, 1 input
	set_tris_c( 0b10110011 ); // 0 output, 1 input
	ext_int_edge( L_TO_H ); // INT0 edge

	// Setup Timer
	setup_timer_1( T1_INTERNAL | T1_DIV_BY_4 );
	set_timer1( 0 );
	
	// Setup ADC
	setup_adc_ports( A_ANALOG_RA3_RA2_REF ); // A0, A1, A5 (Analog) RA2 RA3 (Vref)
	setup_adc( ADC_CLOCK_INTERNAL );
	set_adc_channel(ADC_TEMP);
	delay_us(10);

	// Setup RS232
	#use rs232( BAUD=4800, XMIT=NMEA, PARITY=N, BITS=8 )

	enable_interrupts( INT_TIMER1 );      
	enable_interrupts( INT_EXT );
	enable_interrupts( GLOBAL );

	// Init variable
	stPeriode.wLow  = 0;
	stPeriode.bHigh	= 0;
	SpeedNull		= TRUE;
	SpeedValid		= FALSE;
	UpdateSpeed		= FALSE;
	SpeedChange		= TRUE;
	PERIODE_N		= 0;
	
	output_low( LED );
	
	for(;;)
	{
		/*
		// Test de ADC1 et ADC2
		set_adc_channel(ADC_1);
		delay_ms(100);
		wSpeed = read_adc();
		set_adc_channel(ADC_2);
		delay_ms(100);
		wTemp = read_adc();
		set_adc_channel(ADC_TEMP);
		delay_ms(100);*/
		
	    set_adc_channel(ADC_TEMP);
		delay_ms(100);
		wTemp = read_adc();

		if( wTemp <= AN_VALUE_TMP_MAX )
		{		
			cTemp = VALUE_TMP_MAX;
		}
		else
		{
           if( wTemp >= AN_VALUE_TMP_MIN )
           {
			 cTemp = VALUE_TMP_MIN;
		   }
		   else
		   {
			  i = 1;
			  while ( wTemp < pwTemp[i])
			    i++;
			  cTemp = (pwTemp[i-1] - wTemp) * (pcTemp[i] - pcTemp[i-1]) / (pwTemp[i-1] - pwTemp[i]);
			  cTemp += pcTemp[i-1];
		   }
		}

		///// TEMPERATURE
		sprintf( bFrameBuf, "IIMTW,%d,C", cTemp ); // Température (Water temperature)
		bFrameCrc = CalcCRC();

		output_high( LED );
		SendFrame();
		output_low( LED );
		delay_ms(250);

		///// VITESSE
		if( SpeedChange )
		{
			UpdateSpeed = TRUE;
			if( !SpeedNull )
				wSpeed = 1724138/PERIODE;
			else
				wSpeed = 0;
			SpeedChange = FALSE;
			UpdateSpeed = FALSE;
		}
		sprintf( bFrameBuf, "IIVHW,,,,,%lu.%lu,N,,", wSpeed/10, wSpeed%10 ); // Vitesse (Water speed)
		bFrameCrc = CalcCRC();		
		SendFrame();
		
		
		// ADC
		set_adc_channel(ADC_1);
		delay_ms(100);
		wSpeed = read_adc();
		set_adc_channel(ADC_2);
		delay_ms(100);
		wTemp = read_adc();
		sprintf( bFrameBuf, "AETXT,01,01,01, ADC1=%04lu ADC2=%04lu", wSpeed, wTemp);
	    bFrameCrc = CalcCRC();		
		SendFrame();
		
		
		delay_ms(250);
	}
}


//-----------------------------------------------------------------------------
// Nom : CalcCRC()
// Desc: Calcul le CRC de la trame NMEA
//-----------------------------------------------------------------------------
BYTE CalcCRC( VOID )
{
	BYTE i, bCrc;

	bCrc = 0;
	i = 0;
	while( bFrameBuf[i] != 0 )
	{
		bCrc ^= bFrameBuf[i];
		i++;
	}
	
	return bCrc;
}


//-----------------------------------------------------------------------------
// Nom : SendFrame()
// Desc: Emet la trame NMEA
//-----------------------------------------------------------------------------
VOID SendFrame( VOID )
{
	printf( "$%s*%02x\r\n", bFrameBuf, bFrameCrc );
}


//-----------------------------------------------------------------------------
// Nom : ISR_Ext()
// Desc: Lecture de la fréquence du capteur de vitesse
//-----------------------------------------------------------------------------
#int_ext
VOID ISR_Ext( VOID )
{
	stPeriode.wLow = get_timer1()-2;
	set_timer1( 0 );
	
	if( !UpdateSpeed )
	{
		if( SpeedValid )
		{
			SpeedNull = FALSE;
			PERIODE_L = stPeriode.wLow;
			PERIODE_H = stPeriode.bHigh;
			
			SpeedChange = TRUE;
		}
		else
			SpeedValid = TRUE;
	}
	
	stPeriode.bHigh = 0;
}

//-----------------------------------------------------------------------------
// Nom : ISR_Timer1()
// Desc: Time out de la lecture de fréquence du capteur de vitesse
//-----------------------------------------------------------------------------
#int_timer1
VOID ISR_Timer1( VOID )
{
	if( stPeriode.bHigh >= 64 )
	{
		SpeedNull  = TRUE;
		SpeedValid = FALSE;
		
		SpeedChange = TRUE;
	}
	else
		stPeriode.bHigh++;
}
