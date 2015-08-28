/*
 * twi.c
 *
 * Created: 20.08.2014 14:01:40
 *  Author: Hubert
 */ 

#include <util/twi.h> 	    //enth�lt z.B. die Bezeichnungen f�r die Statuscodes in TWSR
#include <avr/interrupt.h>  //dient zur Behandlung der Interrupts
#include <stdint.h>         //definiert den Datentyp uint8_t       
#include "twi_slave.h"

//%%%%%%%% Globale Variablen, die vom Hauptprogramm genutzt werden %%%%%%%%
/*Der Buffer, in dem die Daten gespeichert werden. 
Aus Sicht des Masters l�uft der Zugrif auf den Buffer genau wie bei einem I2C-EEPROm ab.
F�r den Slave ist es eine globale Variable
*/
volatile uint8_t buffer_adr; //"Adressregister" f�r den Buffer

/*Initaliserung des TWI-Inteface. Muss zu Beginn aufgerufen werden, sowie bei einem Wechsel der Slave Adresse
Parameter adr: gew�nschte Slave-Adresse
*/
void init_twi_slave(uint8_t adr)
{
        TWAR= adr; //Adresse setzen
	TWCR &= ~(1<<TWSTA)|(1<<TWSTO);
	TWCR|= (1<<TWEA) | (1<<TWEN)|(1<<TWIE); 	
	buffer_adr=0xFF;  
	sei();
}


//Je nach Statuscode in TWSR m�ssen verschiedene Bitmuster in TWCR geschreiben werden(siehe Tabellen im Datenblatt!). 
//Makros f�r die verwendeten Bitmuster:

//ACK nach empfangenen Daten senden/ ACK nach gesendeten Daten erwarten
#define TWCR_ACK TWCR = (1<<TWEN)|(1<<TWIE)|(1<<TWINT)|(1<<TWEA)|(0<<TWSTA)|(0<<TWSTO)|(0<<TWWC);  

//NACK nach empfangenen Daten senden/ NACK nach gesendeten Daten erwarten     
#define TWCR_NACK TWCR = (1<<TWEN)|(1<<TWIE)|(1<<TWINT)|(0<<TWEA)|(0<<TWSTA)|(0<<TWSTO)|(0<<TWWC);

//switch to the non adressed slave mode...
#define TWCR_RESET TWCR = (1<<TWEN)|(1<<TWIE)|(1<<TWINT)|(1<<TWEA)|(0<<TWSTA)|(1<<TWSTO)|(0<<TWWC);  


/*ISR, die bei einem Ereignis auf dem Bus ausgel�st wird. Im Register TWSR befindet sich dann 
ein Statuscode, anhand dessen die Situation festgestellt werden kann.
*/
ISR (TWI_vect)  {
	uint8_t data=0;

	switch (TW_STATUS) //TWI-Statusregister pr�fen und n�tige Aktion bestimmen 
	{

	// Slave Receiver 

	case TW_SR_SLA_ACK: // 0x60 Slave Receiver, Slave wurde adressiert	
		TWCR_ACK; // n�chstes Datenbyte empfangen, ACK danach senden
		buffer_adr=0xFF; //Bufferposition ist undefiniert
	break;
	
	case TW_SR_DATA_ACK: // 0x80 Slave Receiver, ein Datenbyte wurde empfangen
		data=TWDR; //Empfangene Daten auslesen
		if (buffer_adr == 0xFF) //erster Zugriff, Bufferposition setzen
			{
				//Kontrolle ob gew�nschte Adresse im erlaubten bereich
				if(data<i2c_buffer_size+1)
					{
						buffer_adr= data; //Bufferposition wie adressiert setzen
					}
				else
					{
						buffer_adr=0; //Adresse auf Null setzen. Ist das sinnvoll? TO DO!
					}				
				TWCR_ACK;	// n�chstes Datenbyte empfangen, ACK danach, um n�chstes Byte anzufordern
			}
		else //weiterer Zugriff, nachdem die Position im Buffer gesetzt wurde. NUn die Daten empfangen und speichern
			{
		
				if(buffer_adr<i2c_buffer_size+1)
					{
							i2crxdata[buffer_adr]=data; //Daten in Buffer schreibe	
					}
				buffer_adr++; //Buffer-Adresse weiterz�hlen f�r n�chsten Schreibzugriff
				
				TWCR_ACK;	
			}
	break;


	//Slave transmitter

	case TW_ST_SLA_ACK: //0xA8 Slave wurde im Lesemodus adressiert und hat ein ACK zur�ckgegeben.
		//Hier steht kein break! Es wird also der folgende Code ebenfalls ausgef�hrt!
	
	case TW_ST_DATA_ACK: //0xB8 Slave Transmitter, Daten wurden angefordert

		if (buffer_adr == 0xFF) //zuvor keine Leseadresse angegeben! 
			{
				buffer_adr=0;
			}	
		
		if(buffer_adr<i2c_buffer_size+1)	
			{
				TWDR = i2ctxdata[buffer_adr]; //Datenbyte senden
				buffer_adr++; //bufferadresse f�r n�chstes Byte weiterz�hlen
			}
		else
			{
				TWDR=0; //Kein Daten mehr im Buffer
			}
		TWCR_ACK;
	break;
	case TW_SR_STOP:
				TWCR_ACK;
			break;
	case TW_ST_DATA_NACK: // 0xC0 Keine Daten mehr gefordert 
	case TW_SR_DATA_NACK: // 0x88 
	case TW_ST_LAST_DATA: // 0xC8  Last data byte in TWDR has been transmitted (TWEA = �0�); ACK has been received
	default: 	
		TWCR_RESET;
	break;
	
	} //end.switch (TW_STATUS)
} //end.ISR(TWI_vect)

////Ende von twislave.c////


/*

#include <avr/io.h>
#include <avr/interrupt.h>
#include "timer.h"
extern volatile struct_COUNTDOWN countdown;
extern volatile struct_TFL tfl;
volatile char[32] twi_data;
volatile uint8_t twi_cnt;


void twi_init(char* name, uint8_t name_length){//calculating address from name
	uint8_t i,address;
	uint16_t sum_name = 0;
	for(i=0; i<name_length; i++){
		sum_name += (2*name[i])>>i;
	}
	address = sum_name / name_length;
	if(address>0x80) address -= 0x80;
	TWAR = 0;
	TWAR = address;//set address
	TWCR = 0;
	TWCR |= (1<<TWEA) | (1<<TWEN) | (1<<TWIE); // enable ack, enable twi, enable interrupts
}

void countdown_anpassen(void){
	if(twi_data[1] & 0x01){
		tfl.ALStatus = AL_EIN;
		tfl.ALStatusOld = AL_EIN;
	}else{
		tfl.ALStatus = AL_AUS;
		tfl.ALStatusOld = AL_AUS;
	}
	if(twi_data[1] & 0x02){
		tfl.Status = TFL_EIN;
		tfl.StatusOld = TFL_EIN;
	}else{
		tfl.Status = TFL_AUS;
		tfl.StatusOld = TFL_AUS;
	}
	countdown.Soll_Aussen = (twi_data[2] << 8) + twi_data[3];
	countdown.Soll_Innen = (twi_data[4] << 8) + twi_data[5];
}	


ISR(TWI_vect){
	
}

*/