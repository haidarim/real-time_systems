#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include <stdlib.h>
#include <stdio.h>
#define DAC_adr ((volatile unsigned char*) (0x4000741C))

/**
GENERELL INFORMATION
 * Skriver till DAC på adress 0x4000741C.
 * Tone-objektet innehåller variabler som används för tonen.
 * Noise-objektet innehåller variabler som används för förvrängningen av tonen.
 *
ANVÄNDARMANUAL
 * 
 * 
 * Vid tryck på P påbörjas broder Jakob.
 * Inmatning av siffror och sedan knapptryck på T ändrar tempot.
 * Inmatning av siffra och sedan knapptryck på K ändrar key. 
 * + och - ökar respektive sänker volymen.
 * M kontrollerar mute-funktionen.
 * OBS! Innan första ändring av tempo eller key tryck X för att rensa buffern, annars kraschar programmet.
**/

typedef struct {
    Object super;
    int count;
    char c;
    int history[3];
    char buf[30];
    int counter;
    int historyCounter;
    int num;
    int sum;
    int input;
}   App;

typedef struct {
    Object super;
    int counter;
    int key;
    int tempo;
    int beat;
    int musicPeriod;
} MusicPlayer;

typedef struct {
    Object super;
    int tonePeriod;
    int volume;
    int state;
    int muteFlag;
    int toneDeadline;
    int tempVolume;
    int manualMute;
}   Tone;

typedef struct{
        Object super;
        int backgroundLoopRange;
        int noiseState;
        int noiseDeadline;
}   Noise;

App app = { initObject(), 0, 'X', {0}, {0}, 0, 0, 0, 0, 0};
MusicPlayer musicPlayer = {initObject(), 0, 0, 120, 500, 0};
Tone tone = { initObject(), 1136, 5, 0, 0, 0, 0, 0};
Noise noise = { initObject(), 1000, 0, 0};

int bj[32] = {0, 2, 4, 0, 0, 2, 4, 0, 4, 5, 7, 4, 5, 7, 7, 9, 7, 5, 4, 0, 7, 9, 7, 5, 4, 0, 0, -5, 0, 0, -5, 0};
int period[25] = {2024, 1908, 1805, 1706, 1607, 1515, 1432, 1351, 1275, 1204, 1136, 1072, 1012, 956, 902, 851, 803, 758, 716, 675, 637, 601, 568, 536, 506};
float beatArr[32] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 2, 0.5, 0.5, 0.5, 0.5, 1, 1, 0.5, 0.5, 0.5, 0.5, 1, 1, 1, 1, 2, 1, 1, 2};

void reader(App*, int);
void receiver(App*, int);
void threeHistory(App *self, int c);
void key(App *self, int c);
void clear(char *num);
void volumeControl(Tone *self, int c);
void mute(Tone *self, int c);
void toneGenerator(Tone *self, int d);
void noiseGenerator(Noise *self, int d);
void toneDeadlineControl(Tone *self, int unused);
void noiseDeadlineControl(Noise *self, int unused);
void loadControl(Noise *self, int c);
void setKey(MusicPlayer *self, int c);
void setTempo(MusicPlayer *self, int c);
void setPeriod(Tone *self, int c);

Serial sci0 = initSerial(SCI_PORT0, &app, reader);
Can can0 = initCan(CAN_PORT0, &app, receiver);

//Kontrollerar nästan alla variabler och ropar på sig själv rekursivt för att göra detta kontinuerligt.
void player(MusicPlayer *self, int unused){
    ASYNC(&tone, mute, 0);
    self->beat = (60000 / self->tempo);
    self->beat = (beatArr[(self->counter/2)] * self->beat) - 50;
    
    if ((self->counter % 2) == 0){
        self->musicPeriod = period[bj[self->counter/2] + self->key + 10];
        ASYNC(&tone, setPeriod, self->musicPeriod);  
        AFTER(MSEC(self->beat), self, player, 0);
        } 
        else {
            AFTER(MSEC(50), self, player, 0);
            }
            self->counter++;
            if(self->counter == 64){
                self->counter = 0;
            }
    }

//Genererar ton efter period som player bestämmer.
void toneGenerator(Tone *self, int d){
        if(self->state == 0){
             if (self->manualMute == 0){
                 *DAC_adr = self->volume;
                 } 
                 else{
                     *DAC_adr = 0;
                     }
             self->state = 1;
            }
            else if (self->state == 1){
               *DAC_adr = 0;
               self->state = 0;
            }
            
            /*if(self->toneDeadline == 1){
            SEND(USEC(500), USEC(100), self, toneGenerator, 0);
            }*/
            AFTER(USEC(self->tonePeriod), self, toneGenerator, 0);
}

//Hanterar volymen på tonen.
void volumeControl(Tone *self, int c){
    if(c == '+' && self->volume < 20){
        self->volume++;
        }
        else if (c == '-' && self->volume > 0){
            self->volume--;
            }
    }

//Huvudmute som användaren kan uttnyttja.
void toggleMute(Tone *self, int unused){
    if(self->manualMute == 0){
        self->manualMute = 1;
        }
        else {
                self->manualMute = 0;
                self->muteFlag = 0;
            }
    }
    
//Mute-funktion som player använder för att skapa olika noter.
void mute(Tone *self, int c){
    if(self->manualMute == 0){
        if(self->muteFlag == 0){
            self->tempVolume = self->volume;
            self->volume = 0;
            self->muteFlag = 1;
            }
            else{
                self->muteFlag = 0;
                self->volume = self->tempVolume;
                }
    }
}

//Används för att tömma arrays
void clear(char *num){
    for(int i = 0; i<30; ++i){
        num[i] = 0;
        }
}

void setKey(MusicPlayer *self, int c){
    self->key = c;
    }
    
void setTempo(MusicPlayer *self, int c){
    self->tempo = c;
    }
    
void setPeriod(Tone *self, int c){
    self->tonePeriod = c;
    }

void receiver(App *self, int unused) {
    CANMsg msg;
    CAN_RECEIVE(&can0, &msg);
    SCI_WRITE(&sci0, "Can msg received: ");
    SCI_WRITE(&sci0, msg.buff);
}

void reader(App *self, int c) {
    SCI_WRITE(&sci0, "Rcv: \'");
    SCI_WRITECHAR(&sci0, c);
    SCI_WRITE(&sci0, "\'\n");
    
    self->buf[self->counter++] = c;
    
    switch (c){
    case 'x': clear(self->buf);
                self->counter = 0;
                SCI_WRITE(&sci0, "Buffer cleared");
                break;
    case '+': ASYNC(&tone, volumeControl, c);
                break;
    case '-': ASYNC(&tone, volumeControl, c);
                break;
    case 'm': ASYNC(&tone, toggleMute, 0);
                break;
    case 'k': self->input = atoi(self->buf);
                ASYNC(&musicPlayer, setKey, self->input);
                 clear(self->buf);
                self->counter = 0;
                break;
    case 't': self->input = atoi(self->buf);
                ASYNC(&musicPlayer, setTempo, self->input);
                clear(self->buf);
                self->counter = 0;
                break;
    case 'p': ASYNC(&tone, toneGenerator, 0);
                ASYNC(&musicPlayer, player, 0);
                break;
    default: break;
    }
    
}

void startApp(App *self, int arg) {
    CANMsg msg;

    CAN_INIT(&can0);
    SCI_INIT(&sci0);
    SCI_WRITE(&sci0, "Hello, hello...\n");

    msg.msgId = 1;
    msg.nodeId = 1;
    msg.length = 6;
    msg.buff[0] = 'H';
    msg.buff[1] = 'e';
    msg.buff[2] = 'l';
    msg.buff[3] = 'l';
    msg.buff[4] = 'o';
    msg.buff[5] = 0;
    CAN_SEND(&can0, &msg);
    
    ASYNC(&tone, mute, 0);
}

int main() {
    INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
	INSTALL(&can0, can_interrupt, CAN_IRQ0);
    TINYTIMBER(&app, startApp, 0);
    return 0;
}
