# include <htc.h>

# define _XTAL_FREQ	4000000L

__CONFIG(0X3F50);

//========================================================================
// impostazioni di base
# define ADDRESS_RBUS	0x01 // 000G AAAA
# define ADDRESS_XBUS	30
# define SOFTWARE_VER	0x7A

# define SERIAL_STRB	0xF0
# define FB_INFO_BYTE	0x00
# define SW_INFO_BYTE	0x80
# define AD_INFO_BYTE	0xC0

# define OCCUPE_COUNT	100
# define RELASE_COUNT	50

# define IN0 			RB3
# define IN1 			RB4
# define IN2 			RB5
# define IN3 			RB6
# define IN4 			RB7
# define IN5 			RA6
# define IN6 			RA7
# define IN7 			RA0

# define LED			RA1
# define LED_ON		(RA1 = 1)
# define LED_OFF	(RA1 = 0)

# define RS485_RX 	(RB0 = 0)
# define RS485_TX 	(RB0 = 1)

# define ERROR_STA			0
# define ADR_BYTE_STA		1	
# define START_BYTE_STA		2
# define INFO_BYTE_STA		3
# define MODIN_BYTE_STA		4
# define MODSW_BYTE_STA		5

//========================================================================

// impostazioni iniziali eeprom integrata
__EEPROM_DATA(0XFF,ADDRESS_RBUS,ADDRESS_XBUS,SOFTWARE_VER,OCCUPE_COUNT,RELASE_COUNT,0XFF,0XFF);

# include "product_info.h"
# include "sci.c"

//========================================================================
// programma principale

void main(void) {
	PCON=0xFF; // 4MHz
	CMCON=0x07; // all digital I/O
	INTCON=0; // int disabilitati
	TRISA=0b11111101;
	TRISB=0b11111110;

	LED_ON;
	RS485_RX; // imposta il MAX485 in ascolto

	unsigned char k = eeprom_read(0x0001);
	unsigned char R_BUS_ADR = k & 0x0F;
	unsigned char MY_GROUP = k & 0x10;
	unsigned char X_BUS_ADR = ADDRESS_XBUS;//eeprom_read(0x0002);
	unsigned char SOFTW_VER = eeprom_read(0x0003);
	unsigned char OCC_COUNT = eeprom_read(0x0004);
	unsigned char REL_COUNT = eeprom_read(0x0005);

	// estrae indirizzo e gruppo del modulo, se l'eeprom non è inizializzata 
	// la inizializza con i valori di default: rbus_id=1, gruppo=0
	if(R_BUS_ADR==0 || R_BUS_ADR > 10) {
		eeprom_write(0x0001,ADDRESS_RBUS);
		R_BUS_ADR = ADDRESS_RBUS & 0x0F;
		MY_GROUP = ADDRESS_RBUS & 0x10;
	}

	// verifica l'X-Bus address, se l'eeprom non è inizializzata inserisce
	// i valori di default: xbus_id=30
	if(X_BUS_ADR > 31 || X_BUS_ADR < 1) {
		eeprom_write(0x0002,ADDRESS_XBUS);
		X_BUS_ADR = ADDRESS_XBUS;
	}
	X_BUS_ADR |= 0xC0;

	// input
	unsigned char INPUT_LOADER = 0;
	unsigned char INPUT_BUFFER[8];
	unsigned char z1 = 0;
	unsigned char z2 = 0;
	k = 0;

	INPUT_BUFFER[0] = 0;
	INPUT_BUFFER[1] = 0;
	INPUT_BUFFER[2] = 0;
	INPUT_BUFFER[3] = 0;
	INPUT_BUFFER[4] = 0;
	INPUT_BUFFER[5] = 0;
	INPUT_BUFFER[6] = 0;
	INPUT_BUFFER[7] = 0;
	
	// inizializza la seriale a 9bit
	unsigned char SERIAL_BYTE = 0;
	unsigned char SERIAL_STATE = ERROR_STA;
	unsigned char SERIAL_SHNB = 0;
	unsigned char SERIAL_MOD_N = 0;
	unsigned char NEW_ADR = 0;
	unsigned char INFO_BYTE_TYPE = 0;
	unsigned char GROUP = 0;
	unsigned char MOD = 0;
	unsigned char stop = 0;

	__delay_ms(150);

	sci_Init(62500,SCI_NINE);

	__delay_ms(50);
	
	LED_OFF;

	// loop principale
	while(1) {

		// verifico nuovi byte in arrivo
		if(RCIF) {
			if(sci_GetNinth()) {
				SERIAL_STATE = ADR_BYTE_STA;
			}
			SERIAL_BYTE = sci_GetByte();

			// a seconda dello stato corrente eseguo diverse azioni
			switch(SERIAL_STATE) {
				case ERROR_STA:
					break;

				case ADR_BYTE_STA:
					SERIAL_STATE = START_BYTE_STA;
					if(SERIAL_BYTE != X_BUS_ADR) {
						SERIAL_STATE = ERROR_STA;
					}
					break;

				case START_BYTE_STA:
					SERIAL_SHNB = SERIAL_BYTE & 0xF0;
					if(SERIAL_SHNB == SERIAL_STRB) {
						SERIAL_MOD_N = SERIAL_BYTE & 0x0F;
						SERIAL_MOD_N--;
						SERIAL_STATE = INFO_BYTE_STA;
					}
					else {
						SERIAL_STATE = ERROR_STA;
					}
					break;

				case INFO_BYTE_STA:
					MOD = 0;
					NEW_ADR = SERIAL_BYTE & 0x0F;
					GROUP = SERIAL_BYTE & 0x10;
					INFO_BYTE_TYPE = SERIAL_BYTE & 0xC0;
					
					if(INFO_BYTE_TYPE==FB_INFO_BYTE) {
						SERIAL_STATE = MODIN_BYTE_STA;
						// non faccio parte del gruppo richiesto
						if(GROUP ^ MY_GROUP) { 
							SERIAL_STATE = ERROR_STA;
							break;
						}
						// il mio indirizzo è maggiore del numero di moduli richesti
						if(R_BUS_ADR > SERIAL_MOD_N) {
							SERIAL_STATE = ERROR_STA;
							break;
						}

						MOD++;
						if(MOD == R_BUS_ADR) {
							__delay_us(10);
							RS485_TX; // imposta il MAX485 in scrittura
							__delay_us(20);
							sci_PutByte(INPUT_LOADER);
							stop = 1;
							SERIAL_STATE = ERROR_STA;
						}
						if(MOD == SERIAL_MOD_N) {
							SERIAL_STATE = ERROR_STA;
						}
					}
					else if(INFO_BYTE_TYPE==SW_INFO_BYTE) {
						SERIAL_STATE = MODSW_BYTE_STA;
						// non faccio parte del gruppo richiesto
						if(GROUP ^ MY_GROUP) { 
							SERIAL_STATE = ERROR_STA;
							break;
						}
						// il mio indirizzo è maggiore del numero di moduli richesti
						if(R_BUS_ADR > SERIAL_MOD_N) {
							SERIAL_STATE = ERROR_STA;
							break;
						}

						MOD++;
						if(MOD == R_BUS_ADR) {
							__delay_us(10);
							RS485_TX; // imposta il MAX485 in scrittura
							__delay_us(20);
							sci_PutByte(SOFTW_VER);
							stop = 1;
							SERIAL_STATE = ERROR_STA;
						}
						if(MOD == SERIAL_MOD_N) {
							SERIAL_STATE = ERROR_STA;
						}
					}
					else if(INFO_BYTE_TYPE==AD_INFO_BYTE) {
						LED_ON;
						eeprom_write(0x0001,NEW_ADR|GROUP);
						MY_GROUP = GROUP;
						R_BUS_ADR = NEW_ADR;
						SERIAL_STATE = ERROR_STA;
						__delay_ms(1000);
						LED_OFF;
					}
					else{
						SERIAL_STATE = ERROR_STA;
					}
					break;

				case MODIN_BYTE_STA:
					MOD++;
					if(MOD == R_BUS_ADR) {
						__delay_us(10);
						RS485_TX; // imposta il MAX485 in scrittura
						__delay_us(20);
						sci_PutByte(INPUT_LOADER);
						stop = 1;
						SERIAL_STATE = ERROR_STA;
					}
					if(MOD == SERIAL_MOD_N) {
						SERIAL_STATE = ERROR_STA;
					}
					break;

				case MODSW_BYTE_STA:
					MOD++;
					if(MOD == R_BUS_ADR) {
						__delay_us(10);
						RS485_TX; // imposta il MAX485 in scrittura
						__delay_us(20);
						sci_PutByte(SOFTW_VER);
						stop = 1;
						SERIAL_STATE = ERROR_STA;
					}
					if(MOD == SERIAL_MOD_N) {
						SERIAL_STATE = ERROR_STA;
					}
					break;

				default:
					SERIAL_STATE = ERROR_STA;
					break;
			}
		}

		// check tx stop event
		if(TRMT && stop) {
			__delay_us(15);
			RS485_RX; // imposta il MAX485 in ascolto
			stop = 0;
		}
		// verifica errore di ricezione, cancella e riparte
		if(sci_CheckOERR()) {
			SERIAL_STATE = ERROR_STA;
		}
		SERIAL_BYTE = 0;

		// scan degli input
		if(!IN0) { INPUT_BUFFER[0]++; }
		if(!IN1) { INPUT_BUFFER[1]++; }
		if(!IN2) { INPUT_BUFFER[2]++; }
		if(!IN3) { INPUT_BUFFER[3]++; }
		if(!IN4) { INPUT_BUFFER[4]++; }
		if(!IN5) { INPUT_BUFFER[5]++; }
		if(!IN6) { INPUT_BUFFER[6]++; }
		if(!IN7) { INPUT_BUFFER[7]++; }

		__delay_us(100);

		if(LED == 1)
			z1++;
		if(z1 == 255) {
			z1 = 0;
			z2++;
			if(z2 > 5) {
				LED_OFF;
				z2 = 0;
			}
		}

		// verifica superamento soglia
		k++;
		if(k == 255) {
			k = 0;

			for(char i=0; i<8; i++) {
				char mask = 1<<i;
				if(INPUT_LOADER & mask) {
					// stato precedente 1 quindi verifico se ho un conteggio basso
					if(INPUT_BUFFER[i] < REL_COUNT) {
						INPUT_LOADER &= ~mask;
					}
				}
				else {
					// stato precedente 0 quindi verifico se ho un conteggio alto
					if(INPUT_BUFFER[i] > OCC_COUNT) {
						INPUT_LOADER |= mask;
						LED_ON;
						z1 = 0;
						z2 = 0;
					}
				}
				INPUT_BUFFER[i] = 0;
			}
		}
	}
}