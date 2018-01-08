/** 
\file main.c
\brief 		Fichier unique de la carte V-T°-P NMEA183 PIC12F675

\date 	  	Novembre 2004
\version  	PIC12F675
\version  	Carte marine V-T°-P NMEA183 PIC12F675
\version    Compilateur C CCS
\version 	1.0
\author		David PROVENT, modifications Vincent AUBINEAU,  Exxotest, Annecy, Haute-Savoie, FRANCE.
\bug 		Pas de bogues connus pour le moment 
*/

#case

#include <12F675.h>
#include "types.h"

#device ADC=10
#fuses NOCPD, NOPROTECT
#fuses NOWDT, NOPUT, BROWNOUT
#fuses NOMCLR	// RESET
#fuses INTRC_IO	// Horloge
#use delay(clock=4000000)
#use fast_io(A)


// I/O pins
#define MES_TEMP	PIN_A1  
#define MES_SPEED	PIN_A2
#define NMEA		PIN_A4
#define LED			PIN_A5

// Constante en FLASH
const CHAR strTempStart[]	= "$IIMTW,";
const CHAR strTempEnd[]		= ",C\r\n";
const CHAR strSpeedStart[]	= "$IIVHW,,,,,";
const CHAR strSpeedEnd[]	= ",N,,\r\n";

#define AN_VALUE_TMP_MAX	223 ///< 40°C
#define AN_VALUE_TMP_MIN	683 ///< -7°C
#define VALUE_TMP_MAX	    40  ///< 40°C
#define VALUE_TMP_MIN	    (-7)  ///< -7°C

#define NUM_TMP		7
const WORD pwTemp[NUM_TMP] = { AN_VALUE_TMP_MIN, 640, 595, 539, 465, 366, AN_VALUE_TMP_MAX }; ///< Températures en °C, -7, 1, 5, 10, 16, 25, 40    
const CHAR pcTemp[NUM_TMP] = { VALUE_TMP_MIN, 1, 5, 10, 16, 25, VALUE_TMP_MAX }; ///< Températures en °C, -7, 1, 5, 10, 16, 25, 40
const WORD pwDeltaTemp[NUM_TMP] = { 0, AN_VALUE_TMP_MIN-640, 640-595, 595-539, 539-465, 465-366, 366-AN_VALUE_TMP_MAX }; ///< Delta de températures
const CHAR pcDeltaTemp[NUM_TMP] = { 0, 1-VALUE_TMP_MIN, 5-1, 10-5, 16-10, 25-16, VALUE_TMP_MAX-25 }; ///< Delta de températures

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
#define PERIODE			(*((DWORD*)bPeriode))   ///< Periode sur 32 bits
#define PERIODE_L		(*((WORD*)bPeriode))	///< Partie basse de la periode
#define PERIODE_H		(bPeriode[2])			///< Partie haute de la periode
#define PERIODE_N		(bPeriode[3])			///< Non utilisé, doit être à 0


VOID main( VOID )
{
	BYTE i;

	// Setup I/O PORTS
	set_tris_a( 0b11001111 ); // 0 output, 1 input
	ext_int_edge( L_TO_H ); // INT0 edge

	// Setup Timer
	setup_timer_1( T1_INTERNAL | T1_DIV_BY_1 );
	set_timer1( 0 );
	
	// Setup ADC
	setup_adc_ports( AN1_ANALOG ); // A1 (Analog)
	setup_adc( ADC_CLOCK_INTERNAL );
	set_adc_channel(1);
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
	
	output_high( LED );
	
	for(;;)
	{
		///// TEMPERATURE
		wTemp = read_adc();
		
		//printf("$%LX*\n",wTemp);
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
			//i = NUM_TMP-1;
			i = 1;
			while ( wTemp < pwTemp[i])
			 i++;
		    cTemp = ( ((pwTemp[i-1] - wTemp) * pcDeltaTemp[i]) / pwDeltaTemp[i] ) + pcTemp[i-1];			
		  }
		}

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

		// Temperature (Water temperature)
		output_high( LED );
		printf( strTempStart );
		printf( "%d", cTemp );
		printf( strTempEnd );
		output_low( LED );
		delay_ms(250);

		// Vitesse (Water speed)
		output_high( LED );
		printf( strSpeedStart );
		
		printf( "%u.%u", (BYTE)(wSpeed/10), (BYTE)(wSpeed%10) );
		printf( strSpeedEnd );
		output_low( LED );
		delay_ms(250);
	}
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

