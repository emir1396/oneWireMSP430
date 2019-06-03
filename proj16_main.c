#include <msp430.h> 

#define SET |= BIT1
#define CLEAR &= ~BIT1
//
#define PAYLOAD_MASK 0x0001
//
#define ONEWIRE_OUTPUT      P2DIR SET
#define ONEWIRE_INPUT       P2DIR CLEAR
#define ONEWIRE_LOW         P2OUT CLEAR
#define ONEWIRE_HIGH        P2OUT SET
#define ONEWIRE_READ        P2IN & BIT1
#define ONEWIRE_ENABLE_INT  P2IE SET
#define ONEWIRE_DISABLE_INT P2IE CLEAR
//
#define TIMER_SET(num)      TA0CCR0 = num
#define TIMER_START         TA0CTL |= MC__UP
#define TIMER_STOP          TA0CTL |= MC__STOP
//
void GpioSetup(void);
void TimerSetup(void);
void startOneWire(void);

unsigned int start = 0, getTemp = 0, convertTemp = 0,updateTemp = 0, doNext = 1;
int timerIntrCount = 0;
unsigned long payload = 0x00000000;
unsigned int bytesExpected = 0x0000;
unsigned int semafor = 0;
unsigned int readBufferIndex = 0;
unsigned int readBuffer[10] = {0,0,0,0,0,0,0,0,0,0};
float temperature = 0;


int main(void)
{
	WDTCTL = WDTPW | WDTHOLD;	// stop WDT
	GpioSetup();
	TimerSetup();
	__enable_interrupt();
	while (1){
        if (start){
            startOneWire();
            //payload = 0x0000017F55554ECC;
            bytesExpected = 0;
            start = 0;
        }
        if (convertTemp){
            startOneWire();
            //stavi se 1 pre 44 jer ne bi poslao ceo bajt poslednje cetvorke
            payload = 0x144CC;
            bytesExpected = 9;
            getTemp = 1;
            convertTemp = 0;
        }
        if (getTemp && doNext){
            startOneWire();
            payload = 0xBECC;
            bytesExpected = 9;
            updateTemp = 1;
            getTemp = 0;
        }
        if (updateTemp && doNext){
            //convertToCelsius();
            updateTemp = 0;
        }

    }

}

void startOneWire(void)
{
    doNext = 0;
    unsigned int i=0;
    for (i = 10; i > 0 ; i--){
        readBuffer[i-1] = 0x0000;
    }
    ONEWIRE_OUTPUT;
    TA0CCTL0 = CCIE;            //enable cc interrupt
    TIMER_SET(480);
    TIMER_STOP;
    ONEWIRE_LOW;                //pull the line low
    TIMER_START;                //wait for 480uS
}

void GpioSetup(void)
{
    ONEWIRE_OUTPUT;
    ONEWIRE_HIGH;               //oneWire idle
    P2IES SET;                  //int na falling edge
    ONEWIRE_DISABLE_INT;        //disable interrupt
    P2REN CLEAR;                //disable pullup/down
}

void TimerSetup(void)
{
    TA0CTL = TACLR;             //clear timer & divide logic
    TA0CTL |= TASSEL__SMCLK;
    TA0CTL &= ~TAIE;
    TIMER_SET(480);
}


void __attribute__ ((interrupt(PORT2_VECTOR))) P2ISR (void)
{
    if (ONEWIRE_READ)           // P2.1 izazvao int?
    {
        semafor++;
        ONEWIRE_DISABLE_INT;
        P2IFG CLEAR;            // P2.1 flag clr
    }
    return;
}


void __attribute__ ((interrupt(TIMER0_A0_VECTOR))) CCR0ISR (void)
{

    switch (semafor) {
            case 0:
                //poslat RESET
                //staviti pin u HIGH-Z
                ONEWIRE_INPUT;
                ONEWIRE_ENABLE_INT;
                if (timerIntrCount++){
                    //nema senzora na magistrali
                    timerIntrCount = 0;
                    ONEWIRE_DISABLE_INT;
                    TIMER_STOP;
                }
                break;
            case 1:
                //CpuTimer1.InterruptCount = 0;
                ONEWIRE_INPUT;
                TIMER_STOP;
                TIMER_SET(2);
                if (payload || bytesExpected){
                //ako saljem ili primam
                    semafor++;
                }else{
                //none
                    semafor = 6;
                }
                TIMER_START;
                break;
            case 2:
                //pull the line LOW
                ONEWIRE_OUTPUT;
                ONEWIRE_LOW;
                TIMER_STOP;
                TIMER_SET(2);
                if(payload){
                    semafor++;
                }
                else if(bytesExpected){
                    semafor = 4;
                } else {
                    semafor = 1;
                }
                TIMER_START;
                break;
            case 3:
                TIMER_STOP;
                //mogu samo iz 2 da dodjem tako da je pin vec OUT i LOW
                //maskiram MSB
                if(payload & PAYLOAD_MASK ) {
                    ONEWIRE_HIGH;
                }
                payload>>=1;
                //takvo stanje mora da se ostavi na 60uS
                TIMER_SET(60);
                TIMER_START;
                semafor = 1;
                break;
            case 4:
                //mogu samo iz 2 da dodjem tako da moram da pin stavim u HIGHZ
                TIMER_STOP;
                ONEWIRE_INPUT;
                //ostavim vremena slave-u da postavi nivo na magistralu
                TIMER_SET(8);
                TIMER_START;
                semafor++;
                break;
            case 5:
                //citanje i upis podatka
                readBuffer[bytesExpected] |= (ONEWIRE_READ << readBufferIndex++);
                if (readBufferIndex == 8){
                    readBufferIndex = 0;
                    bytesExpected--;
                }
                TIMER_SET(60);
                TIMER_START;
                semafor = 1;
                break;
            case 6:
                if (timerIntrCount++ == 10){
                //nema ni payloada ni bytesExpecteda 500mS
                //kraj
                    timerIntrCount = 0;
                    TIMER_STOP;
                    semafor=0;
                    doNext = 1;
                    break;
                }
                TIMER_STOP;
                TIMER_SET(50000);
                if (payload || bytesExpected){
                    semafor = 1;
                }
                TIMER_START;
                break;
            default:
                break;
        }
    return;
}
