#include <LiquidCrystal.h>
#include <Servo.h>
#include <math.h>

//Variables de tiempo
int diaDelAno = 297;
double latitud = 42.350975;
double longitud = -3.688766;
int timezone = 2;
int hora = 16;
int minuto = 30;
int segundo = 34;
double pi = 3.1415926535898;


//LDRs
const int LDR_TOP_LEFT = A0;
const int LDR_TOP_RIGHT = A1;
const int LDR_BOTTOM_LEFT = A2;
const int LDR_BOTTOM_RIGHT = A3;
const int LIGHT_THRESHOLD = 10;

//Servos
Servo servo_horizontal;
Servo servo_vertical;
int pos_sh = 90;
int pos_sv = 90;

const int UPPER_LIMIT_POS = 160; 
const int LOWER_LIMIT_POS = 20;  




//Definimos variables de las posiciones solares actuales
int diaDelAno_0;
int hora_0;
int minuto_0;
int segundo_0;
double azimut_0;
double elevation_0;
int pos_sv_0 = 90;
int pos_sh_0 = 90;



//Funciones

void solar_ec(int diaDelAno, double latitud, double longitud, int timezone, int hora, int minuto, int segundo, double pi, double& azimut, double& elevation);

void moveSolarTracker();



void setup() {

  Serial.begin(9600);
  
  bool pos_solar = false; //Determina se se encuentra perpendicular al sol o no
  
  //Determinamos posiciones iniciales y puertos de los servos
  servo_vertical.attach(5);
  servo_horizontal.attach(6);
  servo_vertical.write(90);
  servo_horizontal.write(90);
  pos_sv = servo_vertical.read();
  pos_sh = servo_horizontal.read();

  //Movemos el panel a una posición perpendicular al sol
  moveSolarTracker();

  //Calculamos las posiciones solares actuales
  diaDelAno_0 = diaDelAno;
  hora_0 = hora;
  minuto_0 = minuto;
  segundo_0 = segundo;
  azimut_0 = 0;
  elevation_0 = 0;
  pos_sv_0 = pos_sv;
  pos_sh_0 = pos_sh;

  //Calculamos azimut y elevación inicial
 solar_ec(diaDelAno_0, latitud, longitud, timezone, hora_0, minuto_0, segundo_0, pi, azimut_0, elevation_0);
  Serial.println (elevation_0);
}

void loop() {

  
  double azimut;
  double elevation;

  //Calculamos azimut y elevacion actuales
  solar_ec(diaDelAno, latitud, longitud, timezone, hora, minuto, segundo, pi, azimut, elevation);                                                                         //Alberto Cuervo, Jorge Sáinz

  double inc_azimut = azimut - azimut_0;
  double inc_elevation = elevation - elevation_0;
  
  pos_sv = pos_sv_0 + (inc_elevation*180/pi);
  pos_sh = pos_sh_0 + (inc_azimut*180/pi);  

   
  servo_horizontal.write(pos_sh);
  servo_vertical.write(pos_sv);

  Serial.print(pos_sv);
  Serial.print("|");
  Serial.println(pos_sh);
  delay(200);
}



void moveSolarTracker() {

  bool pos_solar = false;

   while (not pos_solar){
   //Leemos los 4 LDRs
  int ldr_tl_value = analogRead(LDR_TOP_LEFT);
  int ldr_tr_value = analogRead(LDR_TOP_RIGHT);
  int ldr_bl_value = analogRead(LDR_BOTTOM_LEFT);
  int ldr_br_value = analogRead(LDR_BOTTOM_RIGHT);

  int average_top = (ldr_tl_value + ldr_tr_value) / 2; //Media de los 2 LDR de arriba
  int average_bottom = (ldr_bl_value + ldr_br_value) / 2; //Media de los 2 LDR de abajo
  int average_left = (ldr_tl_value + ldr_bl_value) / 2; //Media de los 2 LDR de la izquierda
  int average_right = (ldr_tr_value + ldr_br_value) / 2; //Media de los 2 LDR de la derecha
  
  bool pos_solar_v = false;
  bool pos_solar_h = false;

  //movemos el panel en busca de igualar la recepción solar vertical y horizontal
  
  if ((average_top - average_bottom) > LIGHT_THRESHOLD && pos_sv < UPPER_LIMIT_POS) {
    pos_sv++;
    servo_vertical.write(pos_sv);
    pos_solar_v = false;
  }
  else if ((average_bottom - average_top) > LIGHT_THRESHOLD && pos_sv > LOWER_LIMIT_POS) {
    pos_sv--;
    servo_vertical.write(pos_sv);
    pos_solar_v = false;
  } else{
    pos_solar_v = true;
    
    }

  
  if ((average_left - average_right) > LIGHT_THRESHOLD && pos_sh < UPPER_LIMIT_POS) {
    pos_sh++;
    servo_horizontal.write(pos_sh);
    pos_solar_h = false;
  }
  else if ((average_right - average_left) > LIGHT_THRESHOLD && pos_sh > LOWER_LIMIT_POS) {
    pos_sh--;
    servo_horizontal.write(pos_sh);
    pos_solar_h = false;
  }else{
    pos_solar_h = true;
    }
  if (pos_solar_v && pos_solar_h){
    pos_solar = true;
    }
    delay(50);
    Serial.println("yo");
  } 
  
}


void solar_ec(int diaDelAno, double latitud, double longitud, int timezone, int hora, int minuto, int segundo, double pi, double& azimut, double& elevation){
  
  double gamma = ((2 * pi / 365) * (diaDelAno - 1));
  double decl = 0.006918 - 0.399912 * cos(gamma) + 0.070257 * sin (gamma) - 0.006758 * cos (2 *gamma) + 0.000907 * sin (2 * gamma) - 0.002697 * cos (3 * gamma) + 0.00148 * sin ( 3 * gamma);
  double eqtime = 229.18 * (0.000075 + 0.001868 * cos (gamma)-0.032077 * sin (gamma) - 0.014615 * cos (2 * gamma) - 0.040849 * sin (2 * gamma));
  double timeOffset = eqtime + 4 * longitud - 60 * timezone;
  double tst = hora * 60 + minuto + segundo/60 + timeOffset;
  double hourAngle = (tst / 4) - 180;
  double zenitAngle = acos(sin(latitud*pi/180) * sin (decl) + cos(latitud*pi/180) * cos (decl) * cos (hourAngle*pi/180));
  elevation = pi/2 - zenitAngle;

  if (hourAngle > 0){       

  azimut = 360-RAD_TO_DEG*acos(-((sin(DEG_TO_RAD*latitud)*cos(DEG_TO_RAD*zenitAngle)-sin(DEG_TO_RAD*decl))/(cos(DEG_TO_RAD*latitud)*sin(DEG_TO_RAD*zenitAngle))));       

  }

  else{       
  azimut = RAD_TO_DEG*acos(-((sin(DEG_TO_RAD*latitud)*cos(DEG_TO_RAD*zenitAngle)-sin(DEG_TO_RAD*decl))/(cos(DEG_TO_RAD*latitud)*sin(DEG_TO_RAD*zenitAngle))));        }   
  azimut = acos(-((sin(latitud*pi/180) * cos (zenitAngle) - sin(decl)) / (cos(latitud*pi/180) * sin (zenitAngle))));

   }
