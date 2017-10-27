//https://www.drive2.ru/l/474186105906790427/
//https://www.drive2.ru/c/476276827267007358/
//версия для включения вебасто
#include <SoftwareSerial.h>
SoftwareSerial m590(4, 5); // RX, TX
#include <DallasTemperature.h> // подключаем библиотеку чтения датчиков температуры
#define ONE_WIRE_BUS 12 // и настраиваем  пин 12 как шину подключения датчиков DS18B20
OneWire oneWire(ONE_WIRE_BUS); 
DallasTemperature sensors(&oneWire);

#define STARTER_Pin 8      // на реле стартера, через транзистор с 8-го пина ардуино
#define ON_Pin 9           // на реле зажигания, через транзистор с 9-го пина ардуино
#define FIRST_P_Pin 11     // на для дополнительного реле первого положения ключа зажигания
#define ACTIV_Pin 13       // на светодиод c 13-го пина для индикации активности 
#define BAT_Pin A0         // на батарею, через делитель напряжения 39кОм / 11 кОм
#define Feedback_Pin A3    // на силовой провод замка зажигания
#define STOP_Pin A2        // на концевик педали тормоза для отключения режима прогрева или датчик нейтральной передачи
#define DDM_Pin A1         // на датчик давления масла
#define boot_Pin 3         // на 19-ю ногу модема для его пробуждения
#define ring_Pin 2         // на 10-ю ногу модема для пробуждения ардуино


float TempDS0;     // переменная хранения температуры с датчика двигателя
float TempDS1;     // переменная хранения температуры с датчика на улице

int k = 0;
int led = 13;
int interval = 2;        // интервал отправки данных на народмон 20 сек после старта
String at = "";
String SMS_phone = "+375000000000"; // куда шлем СМС
String call_phone = "375000000000"; // телефон хозяина без плюса
unsigned long Time1 = 0;
int WarmUpTimer = 0;     // переменная времени прогрева двигателя по умолчанию
int modem =0;            // переменная состояние модема
bool start = false;      // переменная разовой команды запуска
bool heating = false;    // переменная состояния режим прогрева двигателя
bool SMS_send = false;   // флаг разовой отправки СМС
bool SMS_report = false; // флаг разовой отправки СМС
bool n_send = true;      // флаг отправки данных на народмон 
float Vbat;              // переменная хранящая напряжение бортовой сети
float Vstart = 12.50;    // поорог распознавания момента запуска по напряжению
float m = 69.80;         // делитель для перевода АЦП в вольты для резистров 39/11kOm



void setup() {
  pinMode(ring_Pin, INPUT);
  pinMode(STARTER_Pin, OUTPUT);
  pinMode(ON_Pin, OUTPUT);
  pinMode(FIRST_P_Pin, OUTPUT);
  pinMode(boot_Pin, OUTPUT);
  digitalWrite(boot_Pin, LOW);
  Serial.begin(9600);  //скорость порта
  m590.begin(38400);
  delay(1000);
  if (digitalRead(STOP_Pin) == HIGH) SMS_report = true;  // включаем народмон при нажатой педали тормоза при подаче питания 
  Serial.print("Starting M590 27.10.2017, SMS_report =  "), Serial.println(SMS_report);
 // m590.println("AT+IPR=38400");  // настройка скорости M590 если не завелся на 9600 но завелся на 38400
 //delay (1000);
 // m590.begin(38400);
 
}

void loop() {
  if (m590.available()) { // если что-то пришло от модема 
    while (m590.available()) k = m590.read(), at += char(k),delay(1);
    
    if (at.indexOf("RING") > -1) {
      m590.println("AT+CLIP=1"), Serial.print(" R I N G > ");  //включаем АОН
      if (at.indexOf(call_phone) > -1) delay(50), m590.println("ATH0");
      Serial.println("Incoming call is cleared"), WarmUpTimer = 120, start = true;
                          
    } else if (at.indexOf("\"SM\",") > -1) {Serial.println("in SMS"); // если пришло SMS
           m590.println("AT+CMGF=1"), delay(50); // устанавливаем режим кодировки СМС
           m590.println("AT+CSCS=\"gsm\""), delay(50);  // кодировки GSM
           m590.println("AT+CMGR=1"), delay(20), m590.println("AT+CMGD=1,4"), delay(20);  

    } else if (at.indexOf("+PBREADY") > -1) { // если модем стартанул  
           Serial.println(" P B R E A D Y > ATE0 / AT+CMGD=1,4 / AT+CNMI / AT+CMGF/ AT+CSCS");
           m590.println("ATE0"),              delay(100); // отключаем режим ЭХА 
           m590.println("AT+CMGD=1,4"),       delay(100); // стираем все СМС
           m590.println("AT+CNMI=2,1,0,0,0"), delay(100); // включем оповещения при поступлении СМС
           m590.println("AT+CMGF=1"),         delay(100); // 
           m590.println("AT+CSCS=\"gsm\""),   delay(100); // кодировка смс

    } else if (at.indexOf("123Webasto10") > -1 ) { WarmUpTimer = 60, start = true; // команда запуска на 10 мин.
           
    } else if (at.indexOf("123Webasto20") > -1 ) { WarmUpTimer = 120, start = true; // команда запуска на 20 мин.

    } else if (at.indexOf("123Webasto30") > -1 ) { WarmUpTimer = 180, start = true; // команда запуска на 30 мин.
           
    } else if (at.indexOf("123stop") > -1 ) {WarmUpTimer = 0;  // команда остановки остановки
          
    } else if (at.indexOf("+TCPRECV:0,") > -1 ) {  // если сервер что-то ответил - закрываем соединение
           Serial.println("T C P R E C V --> T C P C L O S E "), m590.println("AT+TCPCLOSE=0");      

    } else if (at.indexOf("+TCPCLOSE:0,OK") > -1 ) {  // если соеденение закрылось меняем статус модема
           Serial.println("T C P C L O S E  OK  --> Modem =0 "), modem = 0; 
           
  //  } else if (at.indexOf("+TCPSETUP:0,OK") > -1 && modem == 2 ) { // если конект к народмону  успешен 
    } else if (at.indexOf("+TCPSETUP:") > -1 && modem == 2 ) { // если конект к народмону  успешен 
                Serial.print(at), Serial.println("  --> T C P S E N D = 0,75 , modem = 3" );
                m590.println("AT+TCPSEND=0,75"), delay(200), modem = 3; // меняем статус модема

    } else if (at.indexOf(">") > -1 && modem == 3 ) {  // "набиваем" пакет данными и шлем на сервер 
         m590.print("#59-01-AA-77-88-33#M590+Sensor"); // индивидуальный номер для народмона XX-XX-XX заменяем на свое придуманное !!! 
         if (TempDS0 > -40 && TempDS0 < 54) m590.print("\n#Temp1#"), m590.print(TempDS0);  // значение первого датчиака для народмона
         if (TempDS1 > -40 && TempDS1 < 54) m590.print("\n#Temp2#"), m590.print(TempDS1);  // значение второго датчиака для народмона
         m590.print("\n#Vbat#"), m590.print(Vbat);  // напряжение АКБ для отображения на народмоне
         m590.println("\n##");      // обязательный параметр окончания пакета данных

       Serial.print("#59-01-AA-77-88-33#M590+Sensor"); // индивидуальный номер для народмона XX-XX-XX заменяем на свое придуманное !!! 
       Serial.print("\n#Temp1#"),  Serial.print(TempDS0);
       Serial.print("\n#Temp2#"),  Serial.print(TempDS1);
       Serial.print("\n#Vbat#"),   Serial.print(Vbat);
       Serial.println("\n##");
       delay (50), m590.println("AT+TCPCLOSE=0"), modem = 4;

     } else  Serial.println(at);    // если пришло что-то другое выводим в серийный порт
     at = "";  // очищаем переменную
}

if (millis()> Time1 + 10000) detection(), Time1 = millis(); // выполняем функцию detection () каждые 10 сек 
if (heating == true) {  // если во время прогрева нажали стоп, отправляем смс.
                     if (digitalRead(STOP_Pin) == HIGH) heatingstop();
                     }  
}

void detection(){ // условия проверяемые каждые 10 сек  
    sensors.requestTemperatures();   // читаем температуру с трех датчиков
    TempDS0 = sensors.getTempCByIndex(0);
    TempDS1 = sensors.getTempCByIndex(1);
    
  Vbat = analogRead(BAT_Pin);  // замеряем напряжение на батарее
  Vbat = Vbat / m ; // переводим попугаи в вольты
  Serial.print("Vbat= "),Serial.print(Vbat), Serial.print (" V.");  
  Serial.print(" || Temp : "), Serial.print(TempDS0);  
  Serial.print(" || Interval : "), Serial.print(interval);  
  Serial.print(" || WarmUpTimer ="), Serial.println (WarmUpTimer);


    if (modem == 1) {  // шлем данные на сервер
      m590.println("AT+XISP=0"), delay (50);
      m590.println("AT+CGDCONT=1,\"IP\",\"internet.life.com.by\""), delay (50);
      m590.println("AT+XGAUTH=1,1,\"life\",\"life\""), delay (50);
      m590.println("AT+XIIC=1"), delay (200);
      m590.println("AT+TCPSETUP=0,94.142.140.101,8283"), modem = 2;
                    }

        
    if (modem == 0 && SMS_send == true) {  // если фаг SMS_send равен 1 высылаем отчет по СМС
        delay(3000); 
        m590.println("AT+CMGF=1"),delay(100); // устанавливаем режим кодировки СМС
        m590.println("AT+CSCS=\"gsm\""), delay(100);  // кодировки GSM
        Serial.print("SMS send start...");
        m590.println("AT+CMGS=\"" + SMS_phone + "\""),delay(100);
        m590.print("Status Webasto 1.0 ");
        m590.print("\n Temp.Dvig: "), m590.print(TempDS0);
        m590.print("\n Temp.Salon: "), m590.print(TempDS1);
        m590.print("\n Vbat: "), m590.print(Vbat);
        m590.print((char)26),delay(1000), SMS_send = false;
              }
             
    
    if (WarmUpTimer > 0 && start == true) start = false, enginestart(); 
    if (WarmUpTimer > 0 ) WarmUpTimer--;    // если таймер больше ноля  
    if (heating == true && WarmUpTimer <1) Serial.println("End timer"),SMS_send = false,  heatingstop(); 
       
    interval--;       //  если интернет плюшки не нужны поставьте в начале строки "//"
    if (interval <1 ) interval = 30, modem = 1;
    if (modem != 0 && interval == 28) Serial.println(" modem != 0 && interval == 28 > T C P C L O S E "), m590.println("AT+TCPCLOSE=0"), modem = 0;  
   
}             

void enginestart() {                                      // программа запуска двигателя
// Serial.println ("enginestart() > , count = 3 || StarterTime = 1000");
int count = 3;                                            // переменная хранящая число оставшихся потыток зауска
int StarterTime = 1400;                                   // переменная хранения времени работы стартера (1 сек. для первой попытки)  
if (TempDS0 < 15 && TempDS0 != -127)  StarterTime = 1200, count = 2;   // при 15 градусах крутим 1.2 сек 2 попытки 
if (TempDS0 < 5  && TempDS0 != -127)  StarterTime = 1800, count = 2;   // при 5  градусах крутим 1.8 сек 2 попытки 
if (TempDS0 < -5 && TempDS0 != -127)  StarterTime = 2200, count = 3;   // при -5 градусах крутим 2.2 сек 3 попытки 
if (TempDS0 <-10 && TempDS0 != -127)  StarterTime = 3300, count = 4;   //при -10 градусах крутим 3.3 сек 4 попытки 
if (TempDS0 <-15 && TempDS0 != -127)  StarterTime = 6000, count = 5;   //при -15 градусах крутим 6.0 сек 5 попыток 
if (TempDS0 <-20 && TempDS0 != -127)  StarterTime = 1,count = 0, SMS_send = true;   //при -20 отправляем СМС 

 while (Vbat > 10.00 && digitalRead(Feedback_Pin) == LOW && count > 0) 
    { // если напряжение АКБ больше 10 вольт, зажигание выключено, счетчик числа попыток не равен 0 то...
   
    Serial.print ("count = "), Serial.println (count), Serial.print (" ||  StarterTime = "), Serial.println (StarterTime);

    digitalWrite(ON_Pin, LOW),       delay (2000);        // выключаем зажигание на 2 сек. на всякий случай   
    digitalWrite(FIRST_P_Pin, HIGH), delay (500);         // включаем реле первого положения замка зажигания 
    digitalWrite(ON_Pin, HIGH),      delay (4000);        // включаем зажигание  и ждем 4 сек.

    if (TempDS0 < -5 &&  TempDS0 != -127) digitalWrite(ON_Pin, LOW),  delay (2000), digitalWrite(ON_Pin, HIGH), delay (8000);
    if (TempDS0 < -15 && TempDS0 != -127) digitalWrite(ON_Pin, LOW), delay (10000), digitalWrite(ON_Pin, HIGH), delay (8000);

    if (digitalRead(STOP_Pin) == LOW) digitalWrite(STARTER_Pin, HIGH); // включаем реле стартера
    
    delay (StarterTime);                                  // выдерживаем время StarterTime
    digitalWrite(STARTER_Pin, LOW),  delay (6000);        // отключаем реле, выжидаем 6 сек.
    
    Vbat =        analogRead(BAT_Pin), delay (300);       // замеряем напряжение АКБ 1 раз
    Vbat = Vbat + analogRead(BAT_Pin), delay (300);       // через 0.3 сек.  2-й раз 
    Vbat = Vbat + analogRead(BAT_Pin), delay (300);       // через 0.3 сек.  3-й раз
    Vbat = Vbat / m / 3 ;                                 // переводим попугаи в вольты и плучаем срееднне 3-х замеров
   
     // if (digitalRead(DDM_Pin) != LOW)                  // если детектировать по датчику давления масла /-А-/
     // if (digitalRead(DDM_Pin) != HIGH)                 // если детектировать по датчику давления масла /-В-/
     
     if (Vbat > Vstart) {                                 // если детектировать по напряжению зарядки     /-С-/
        Serial.print ("heating = true, break,  Vbat > Vstart = "), Serial.println(Vbat); 
        heating = true, digitalWrite(ACTIV_Pin, HIGH);
        break; // считаем старт успешным, выхдим из цикла запуска двигателя
                        }
                        
        else { // если статра нет вертимся в цикле пока 
        Serial.print ("heating = true, break,  Vbat < Vstart = "), Serial.println(Vbat); 
        StarterTime = StarterTime + 200;                      // увеличиваем время следующего старта на 0.2 сек.
        count--;                                              // уменьшаем на еденицу число оставшихся потыток запуска
        heatingstop();
             }
      }
  Serial.println (" out >>>>> enginestart()");
 // if (digitalRead(Feedback_Pin) == LOW) SMS_send = true;  // отправляем смс только в случае незапуска
  
 }

void heatingstop() {  // программа остановки прогрева двигателя
    digitalWrite(ON_Pin, LOW),      delay (200);
    digitalWrite(ACTIV_Pin, LOW),   delay (200);
    digitalWrite(FIRST_P_Pin, LOW), delay (200);
    Serial.println ("Ignition OFF"),
    heating= false,                 delay(3000); 
                   }

