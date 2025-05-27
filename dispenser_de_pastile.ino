#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <Keypad.h>
#include <RTClib.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo myServo;
RTC_DS3231 rtc;

#define BUZZER_BIT PC2  // A2 - pin pentru buzzer activ
#define BUTTON_BIT PC3  // A3 - pin pentru buton
#define LED_BIT PC1     // A1 - pin pentru LED
#define SERVOPIN 10     // pin PWM pentru servomotor

const byte ROWS = 4;
const byte COLS = 4;

//Harta tastelor fizice
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

// Pini legati la tastatura
byte rowPins[ROWS] = {2, 3, 4, 5};
byte colPins[COLS] = {6, 7, 8, 9};

// Initializarea tastaturii
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

//setari utlizator
int targetHour = 12;
int targetMinute = 0;
bool isRecurring = true;

// Structura care retine o programare
struct Programare {
  int ora;
  int minut;
  bool recurent;
};

Programare compartimente[3];
int compartimentCurent = 0;

enum Stare {ASTEPTARE, INTRO_HHMM, INTRO_RECURENTA};
Stare stareCurenta = ASTEPTARE;

String inputHHMM = "";
bool afisat = false;

volatile bool buzzerOn = false;
volatile unsigned long buzzerTimer = 0;

volatile bool servoOpen = false;
volatile unsigned long servoTimer = 0;
volatile int compartimentDeschis = -1;

bool oraValida(int h, int m) {
  return (h >= 0 && h <= 23 && m >= 0 && m <= 59);
}

void setupTimer2() {
  cli();
  TCCR2A = (1 << WGM21);
  TCCR2B = (1 << CS22) | (1 << CS21);
  OCR2A = 249;
  TIMSK2 |= (1 << OCIE2A);
  sei();
}

ISR(TIMER2_COMPA_vect) {
  if (buzzerOn) {
    buzzerTimer++;
    if (buzzerTimer >= 3000) {
      PORTC &= ~(1 << BUZZER_BIT);
      buzzerOn = false;
    }
  }

  if (servoOpen) {
    servoTimer++;
    if (servoTimer >= 10000) {
      myServo.write(0);
      PORTC &= ~(1 << LED_BIT);

      if (compartimentDeschis >= 0 && !compartimente[compartimentDeschis].recurent) {
        compartimente[compartimentDeschis].ora = -1;
        compartimente[compartimentDeschis].minut = -1;
      }

      compartimentDeschis -= 1;
      servoOpen = false;
    }
  }
}

void setup() {
  Serial.begin(9600);
  // Initializare ecran LCD I2C
  lcd.init();
  // Activare iluminare de fundal a LCD-ului
  lcd.backlight();

  // Seteaza LED-ul ca OUTPUT
  DDRC |= (1 << LED_BIT);
  // Seteaza buzzerul ca OUTPUT
  DDRC |= (1 << BUZZER_BIT);
  // Seteaza butonul ca INPUT
  DDRC &= ~(1 << BUTTON_BIT);

  // Ataseaza servomotorul la pinul specificat
  myServo.attach(SERVOPIN);
  // Pozitioneaza servomotorul la 0 grade
  myServo.write(0);

  // Initializare modul RTC
  if (!rtc.begin()) {
    lcd.setCursor(0, 0);
    lcd.print("RTC ERROR");
    while (1);
  }

  // Setare ora RTC o singura data
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // Mesaj de intampinare
  lcd.setCursor(0, 0);
  lcd.print("Bun venit!");
  delay(5000);
  lcd.clear();
  lcd.print("Apasa butonul");

  // Initializeaza compartimentele ca neprogramate
  for (int i = 0; i < 3; i++) {
    compartimente[i].ora = -1;
    compartimente[i].minut = -1;
    compartimente[i].recurent = false;
  }

  setupTimer2();
}

void loop() {
  // Citeste ora curenta
  DateTime now = rtc.now();

  if (stareCurenta == ASTEPTARE) {
    char key = keypad.getKey();

    if (key == 'A') {
      //Afiseaza cate compartimente mai sunt libere
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Libere: ");
      lcd.print(3 - compartimentCurent);
      delay(5000);
      lcd.clear();
      lcd.print("Apasa butonul");
    } else if (key == 'B') {
      // Afiseaza ce compartimente sunt ocupate
      lcd.clear();
      lcd.print("Ocupate:");
      delay(5000);

      for (int i = 0; i < 3; i++) {
        if (compartimente[i].ora >= 0 && compartimente[i].minut >= 0) {
          lcd.setCursor(0, 0);
          lcd.print("C");
          lcd.print(i + 1);
          lcd.print(": ");
          if (compartimente[i].ora < 10) lcd.print("0");
          lcd.print(compartimente[i].ora);
          lcd.print(":");
          if (compartimente[i].minut < 10) lcd.print("0");
          lcd.print(compartimente[i].minut);
          delay(5000);
          lcd.clear();
        }
      }

      lcd.clear();
      lcd.print("Apasa butonul");
    } else if ((PINC & (1 << BUTTON_BIT)) && compartimentCurent < 3) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Foloseste");
      lcd.setCursor(0, 1);
      lcd.print("tastatura");
      // stare de introducere ora
      stareCurenta = INTRO_HHMM;
      inputHHMM = "";
      delay(500);
    } else if (key == 'C') {
      // Afisare ora curenta
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Ora curenta:");

      lcd.setCursor(0, 1);
      if (now.hour() < 10) lcd.print("0");
      lcd.print(now.hour());
      lcd.print(":");
      if (now.minute() < 10) lcd.print("0");
      lcd.print(now.minute());
      lcd.print(":");
      if(now.second() < 10) lcd.print("0");
      lcd.print(now.second());

      delay(5000);
      lcd.clear();
      lcd.print("Apasa butonul");
    }
  } else if (stareCurenta == INTRO_HHMM) {
    char key = keypad.getKey();
    if (key >= '0' && key <= '9' && inputHHMM.length() < 4) {
      // Construieste stringul HHMM
      inputHHMM += key;

      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("ORA INTRODUSA");
      lcd.setCursor(0, 1);

      int hh = inputHHMM.substring(0, 2).toInt();
      int mm = inputHHMM.substring(2, 4).toInt();

      lcd.print(hh);
      lcd.print(" : ");
      lcd.print(mm);
    }

    if (key == '#') {
      if (inputHHMM.length() == 4) {
        // Trecere la intrebarea de recurenta
        stareCurenta = INTRO_RECURENTA;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Recurent? 1/0");
      } else {
        lcd.setCursor(0, 1);
        lcd.print("Format: HHMM");
      }
    }
  } else if (stareCurenta == INTRO_RECURENTA) {
    char key = keypad.getKey();
    if (key == '1' || key == '0') {
      int hh = inputHHMM.substring(0, 2).toInt();
      int mm = inputHHMM.substring(2, 4).toInt();

      if (hh >= 0 && hh <= 23 && mm >= 0 && mm <= 59) {
        compartimente[compartimentCurent].ora = hh;
        compartimente[compartimentCurent].minut = mm;
        compartimente[compartimentCurent].recurent = (key == '1');

        int unghi = 60 * (compartimentCurent + 1); // 60, 120, 180
        //Deschidere compartiment
        myServo.write(unghi);
        delay(9000);
        // Revenire la pozitia initiala
        myServo.write(0);
        delay(2000);

        compartimentCurent++;

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Salvat!");
        lcd.setCursor(0, 1);
        lcd.print("Ramase: ");
        lcd.print(3 - compartimentCurent);
        delay(2000);
      } else {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Ora invalida!");
        delay(2000);
      }
      // Revine la starea initiala
      lcd.clear();
      if (compartimentCurent < 3) {
        lcd.setCursor(0, 0);
        lcd.print("Apasa butonul");
        stareCurenta = ASTEPTARE;
      } else {
        lcd.setCursor(0, 0);
        lcd.print("Toate setate!");
      }
    }
  }

  static int lastCheckedMinute = -1;

  if (rtc.now().minute() != lastCheckedMinute) {
    // Executa doar o data pe minut
    lastCheckedMinute = rtc.now().minute();

    for (int i = 0; i < 3; i++) {
      if (compartimente[i].ora == rtc.now().hour() && compartimente[i].minut == rtc.now().minute()) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Deschidere:");
        lcd.setCursor(0, 1);
        lcd.print("Compartiment ");
        lcd.print(i + 1);

         // compartiment 1 -> 60, 2 -> 120, 3 -> 180
        int unghi = 60 * (i + 1);
        // Deschide compartiment
        myServo.write(unghi);
        // Aprinde LED
        PORTC |= (1 << LED_BIT);
        //Porneste buzzer
        PORTC |= (1 << BUZZER_BIT);

        buzzerOn = true;
        buzzerTimer = 0;
        servoOpen = true;
        servoTimer = 0;
        compartimentDeschis +=1 ;

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Apasa buton:");
      

        break;
      }
    }
  }
}
