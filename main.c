//Multi effects Pedal
//s14896
//RAND Ranaweera

#include <avr/io.h>
#include <math.h>
#include <util/delay.h>
#include <avr/interrupt.h>


//PWM stuff
#define PWM_FREQ 0x00FF // pwm frequency
#define PWM_MODE 0 // Fast PWM (1) OR Phase Correct (0)
#define PWM_QTY 2 // Two PWM output pins

int input;
int counter=0;
int effect=0;

//ADC bytes to obtain the input signal (Left adjusted)
unsigned char ADC_low;
unsigned char ADC_high;


//Effects Variales

int distortion_threshold=6000;
int fuzz_threshold=8000;
int bit_crush_variable=0;


void init_PWM(void);
void init_ADC(void);
void setup_systems(void);
void resetvalues(void);

int main(void){
	
	setup_systems();
	init_PWM();
	init_ADC();
	
	while(1){
		
		
			
		
		
	}
	
}


void setup_systems(void){
	DDRB &= ~(7<<2);// set PB2, PB3 and PB4 as input pins
	//PB2 effect change/run through
	//PB3 effect decrease
	//PB4 effect increase
	DDRC |= (1<<2); //LED output
	
	
}


void init_ADC(void){
	
	//ADC initialization
	DDRA = 0x00;
	
	ADMUX |=1<<ADLAR; //left adjust,ADC0
	// ADC0 is selected by default if no bits are set in MUX bits in ADMUX reg
	ADMUX |=1<<REFS0; //Aref setting
	//forgot to add the cap to the pcb AREF pin to ground ADD ITTTT
	//###########################
	ADCSRA |=((1<<ADPS2)|(1<<ADPS0)); //prescalor C/32
	ADCSRA |=((1<<ADEN)|(1<<ADATE)); //ADC enable and auto trigger 
	//ADEN The ADC will start conversion on the trigger signal
	//ADATE ADC Auto Trigger Enable bit
	SFIOR = 0xe0; //ADC Auto trigger source set to capture event
	
	
	

}

void init_PWM(void){
	
	TCCR1A = ((1<<5)|0x80|(PWM_MODE<<1));
	// clear outputs on compare match, set output non inverting,
	TCCR1B = ((PWM_MODE << 3) | 0x11); //Waveform generation mode ,clk/1s
	//TCCR1B |=(1<<ICNC1); //noise cancelling with 4 cycle testing
	TIMSK = 0x20; // call interrupt on input capture interrupt for timer counter 1A n B
	ICR1H = (PWM_FREQ >> 8); // 0x00FF   input capture trigger source --> to auto trigger in SFIOR 
	ICR1L = (PWM_FREQ & 0xff);
	DDRD |= ((PWM_QTY << 4) | 0x10); // turn on outputs PD4,PD5
	// activate interrupts
	sei();  
	
	
	
	
	
	
}
void resetvalues(void){
	//SETTING DEFAULT VALUES	
	effect = 0;			
	distortion_threshold=6000;
	fuzz_threshold=8000;
	bit_crush_variable=0;
	
}

ISR(TIMER1_CAPT_vect) 
{
	
	// get ADC data
	ADC_low = ADCL; // Fetching the low byte
	ADC_high = ADCH; // Fetching the high byte
  
	// the low byte has to be Fetched first --> the ICR1 page datasheet
  
	//adding the bytes to construct the 16b input signal
  
	input = ((ADC_high << 8) | ADC_low) + 0x8000; // make a signed 16b value
  
  
	if(PINB & (1<<2))
		{
		effect++;
		//PORTC |= (1<<2);
		//PORTC &= ~(1<<2); //LED Blink
		if (effect>3)
			{
			effect=0;
			}
		}
  

	counter++;
	//to save resources, the pushbuttons are checked every 100 times.
	if(counter==100)
	{ 
	counter=0;
	if (PIND & (1<<4)) 
		{
		
		if(distortion_threshold<32768)distortion_threshold=distortion_threshold+100; //increase the distortion
		if (fuzz_threshold<32768)fuzz_threshold=fuzz_threshold+100; //increase the fuzz
		if (bit_crush_variable<16)bit_crush_variable=bit_crush_variable+1; //increase the bit crushing
		if (PIND &(1<<3))
			{
			resetvalues();
			}
		//PORTC |= (1<<2);
		//PORTC &= ~(1<<2); //LED Blink
		}
 
	if (PIND & (1<<3))  
		{
		
		if (distortion_threshold>0)distortion_threshold=distortion_threshold-100; //decrease the distortion
		if (fuzz_threshold>0)fuzz_threshold=fuzz_threshold-100; //decrease the fuzz level
		if (bit_crush_variable>0)bit_crush_variable=bit_crush_variable-1; //decrease the bit crushing
		
		//PORTC |= (1<<2); //LED Blink
		//PORTC &= ~(1<<2); 
		}
	}	
  
  
  
  
  
	if (effect==0) //CLEAN EFFECT
		{
		//clean effect--> do nothing
		}
	
	
	//construct the input sumple summing the ADC low and high byte.
	else if(effect==1) //DISTORTION EFFECT
		{
		if(input>distortion_threshold) input=distortion_threshold;
		}
	else if(effect==2) //FUZZ EFFECT
		{
		if(input>fuzz_threshold) input=32768; //pushing the output to the maximum adc level in 15bit
			else if(input<-fuzz_threshold) input=-32768; //pushing the output to the minimum adc level in 15bit
		}
	else if(effect==3) //BIT CRUSHER EFFECT
		{
	input = input<<bit_crush_variable;
		}
  
  
  
	//"write" the PWM signal to the output pins
	OCR1BL = ((input + 0x8000) >> 8); // convert to unsigned, send out high byte
	OCR1AL = input; // send out low byte

}