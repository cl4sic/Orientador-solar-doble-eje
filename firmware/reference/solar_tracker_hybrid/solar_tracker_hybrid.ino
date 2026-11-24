/*
 * Orientador Solar de Doble Eje - Firmware de Referencia (Híbrido: LDR + Ecuaciones Solares)
 *
 * Proyecto: Bachillerato de Investigación y Excelencia (Curso 2022/23)
 *           IES Félix Rodríguez de la Fuente & Universidad de Burgos (Grupo SWIFT)
 * Autores originales: Jorge Sáinz Vidal, Alberto Cuervo Prieto
 * Tutor: Gregorio Gómez Pérez
 *
 * Descripción:
 *   Versión de referencia limpia y corregida a partir del sketch original
 *   (`firmware/original/control_ldr_con_solar_ec__Felix`).
 *
 *   Funcionamiento híbrido:
 *   1. En setup(), las 4 fotorresistencias (LDR) orientan automáticamente el panel
 *      hasta igualar la iluminación superior/inferior e izquierda/derecha (autocalibración inicial).
 *   2. Se toma esa postura como referencia angular (pos_sv_0, pos_sh_0) junto con la posición
 *      solar inicial (azimut_0, elevation_0) calculada mediante las ecuaciones de la NOAA.
 *   3. En loop(), se recalcula la posición solar y se desplazan los servos según el incremento
 *      angular respecto a la calibración inicial, manteniendo límites mecánicos seguros.
 *
 *   Nota: La memoria del proyecto describe la integración de un reloj RTC (I2C) y una
 *   pantalla LCD 16x2 (I2C). En este sketch se mantiene la base sin dependencias externas
 *   para compilar de forma directa con las librerías estándar de Arduino, dejando el reloj
 *   configurable manualmente o ampliable mediante RTClib / LiquidCrystal_I2C.
 */

#include <Servo.h>
#include <math.h>

// -----------------------------------------------------------------------------
// Configuración geográfica y temporal (Burgos, España por defecto)
// -----------------------------------------------------------------------------
const double LATITUD_DEG  = 42.350975;
const double LONGITUD_DEG = -3.688766;
const int    TIMEZONE_UTC = 2; // UTC+2 (horario de verano peninsular)

// Fecha y hora iniciales (sustituibles por lectura de módulo RTC DS1307/DS3231)
int diaDelAno = 297;
int hora      = 16;
int minuto    = 30;
int segundo   = 34;

// -----------------------------------------------------------------------------
// Pines de sensores LDR (divisor de tensión en entradas analógicas)
// -----------------------------------------------------------------------------
const int LDR_TOP_LEFT     = A0;
const int LDR_TOP_RIGHT    = A1;
const int LDR_BOTTOM_LEFT  = A2;
const int LDR_BOTTOM_RIGHT = A3;
const int LIGHT_THRESHOLD  = 10;

// -----------------------------------------------------------------------------
// Servomotores y límites mecánicos de seguridad
// -----------------------------------------------------------------------------
const int PIN_SERVO_VERTICAL   = 5; // Eje de elevación
const int PIN_SERVO_HORIZONTAL = 6; // Eje de azimut

const int UPPER_LIMIT_POS = 160;
const int LOWER_LIMIT_POS = 20;

Servo servo_horizontal;
Servo servo_vertical;

int pos_sh = 90;
int pos_sv = 90;

// Variables de referencia tras la autocalibración inicial
double azimut_0    = 0.0;
double elevation_0 = 0.0;
int pos_sv_0       = 90;
int pos_sh_0       = 90;

// -----------------------------------------------------------------------------
// Prototipos
// -----------------------------------------------------------------------------
void computeSolarPosition(int dayOfYear, double latDeg, double lonDeg, int tz,
                          int hr, int min, int sec,
                          double &azimutRad, double &elevationRad);
void calibrateWithLDR();

// -----------------------------------------------------------------------------
// Inicialización
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(9600);

  servo_vertical.attach(PIN_SERVO_VERTICAL);
  servo_horizontal.attach(PIN_SERVO_HORIZONTAL);

  pos_sv = 90;
  pos_sh = 90;
  servo_vertical.write(pos_sv);
  servo_horizontal.write(pos_sh);
  delay(500);

  // Fase 1: Autocalibración inicial mediante el sensor cuadrante de 4 LDR
  calibrateWithLDR();

  // Fase 2: Registro de la referencia mecánica y solar inicial
  pos_sv_0 = pos_sv;
  pos_sh_0 = pos_sh;

  computeSolarPosition(diaDelAno, LATITUD_DEG, LONGITUD_DEG, TIMEZONE_UTC,
                       hora, minuto, segundo, azimut_0, elevation_0);

  Serial.print(F("Calibracion inicial -> Elevacion (deg): "));
  Serial.print(elevation_0 * RAD_TO_DEG);
  Serial.print(F(" | Azimut (deg): "));
  Serial.println(azimut_0 * RAD_TO_DEG);
}

// -----------------------------------------------------------------------------
// Bucle principal de seguimiento astronómico
// -----------------------------------------------------------------------------
void loop() {
  double azimutRad    = 0.0;
  double elevationRad = 0.0;

  computeSolarPosition(diaDelAno, LATITUD_DEG, LONGITUD_DEG, TIMEZONE_UTC,
                       hora, minuto, segundo, azimutRad, elevationRad);

  double incAzimutDeg    = (azimutRad - azimut_0) * RAD_TO_DEG;
  double incElevationDeg = (elevationRad - elevation_0) * RAD_TO_DEG;

  pos_sv = constrain(pos_sv_0 + (int)round(incElevationDeg), LOWER_LIMIT_POS, UPPER_LIMIT_POS);
  pos_sh = constrain(pos_sh_0 + (int)round(incAzimutDeg), LOWER_LIMIT_POS, UPPER_LIMIT_POS);

  servo_horizontal.write(pos_sh);
  servo_vertical.write(pos_sv);

  Serial.print(F("SV: "));
  Serial.print(pos_sv);
  Serial.print(F(" | SH: "));
  Serial.println(pos_sh);

  delay(200);
}

// -----------------------------------------------------------------------------
// Calibración inicial por equilibrio lumínico en los 4 LDR
// -----------------------------------------------------------------------------
void calibrateWithLDR() {
  bool calibrated = false;

  while (!calibrated) {
    int tl = analogRead(LDR_TOP_LEFT);
    int tr = analogRead(LDR_TOP_RIGHT);
    int bl = analogRead(LDR_BOTTOM_LEFT);
    int br = analogRead(LDR_BOTTOM_RIGHT);

    int avgTop    = (tl + tr) / 2;
    int avgBottom = (bl + br) / 2;
    int avgLeft   = (tl + bl) / 2;
    int avgRight  = (tr + br) / 2;

    bool alignedV = false;
    bool alignedH = false;

    if ((avgTop - avgBottom) > LIGHT_THRESHOLD && pos_sv < UPPER_LIMIT_POS) {
      pos_sv++;
      servo_vertical.write(pos_sv);
    } else if ((avgBottom - avgTop) > LIGHT_THRESHOLD && pos_sv > LOWER_LIMIT_POS) {
      pos_sv--;
      servo_vertical.write(pos_sv);
    } else {
      alignedV = true;
    }

    if ((avgLeft - avgRight) > LIGHT_THRESHOLD && pos_sh < UPPER_LIMIT_POS) {
      pos_sh++;
      servo_horizontal.write(pos_sh);
    } else if ((avgRight - avgLeft) > LIGHT_THRESHOLD && pos_sh > LOWER_LIMIT_POS) {
      pos_sh--;
      servo_horizontal.write(pos_sh);
    } else {
      alignedH = true;
    }

    calibrated = (alignedV && alignedH);
    delay(50);
  }
}

// -----------------------------------------------------------------------------
// Cálculo de azimut y elevación solar según ecuaciones de la NOAA (en radianes)
// -----------------------------------------------------------------------------
void computeSolarPosition(int dayOfYear, double latDeg, double lonDeg, int tz,
                          int hr, int min, int sec,
                          double &azimutRad, double &elevationRad) {
  // Ángulo fraccional del año (radianes)
  double gamma = (2.0 * M_PI / 365.0) * (dayOfYear - 1 + (hr - 12.0) / 24.0);

  // Declinación solar (radianes)
  double decl = 0.006918
                - 0.399912 * cos(gamma) + 0.070257 * sin(gamma)
                - 0.006758 * cos(2.0 * gamma) + 0.000907 * sin(2.0 * gamma)
                - 0.002697 * cos(3.0 * gamma) + 0.001480 * sin(3.0 * gamma);

  // Ecuación del tiempo (minutos)
  double eqTime = 229.18 * (0.000075
                            + 0.001868 * cos(gamma) - 0.032077 * sin(gamma)
                            - 0.014615 * cos(2.0 * gamma) - 0.040849 * sin(2.0 * gamma));

  // Hora solar verdadera (minutos) y ángulo horario (grados)
  double timeOffset = eqTime + 4.0 * lonDeg - 60.0 * tz;
  double tst        = hr * 60.0 + min + (sec / 60.0) + timeOffset;
  double hourAngleDeg = (tst / 4.0) - 180.0;

  double latRad       = latDeg * DEG_TO_RAD;
  double hourAngleRad = hourAngleDeg * DEG_TO_RAD;

  // Ángulo cenital y elevación solar (radianes)
  double cosZenith = constrain(
    sin(latRad) * sin(decl) + cos(latRad) * cos(decl) * cos(hourAngleRad),
    -1.0, 1.0
  );
  double zenithRad = acos(cosZenith);
  elevationRad     = (M_PI / 2.0) - zenithRad;

  // Azimut solar medido desde el Norte en sentido horario (radianes)
  double denom = cos(latRad) * sin(zenithRad);
  if (fabs(denom) < 1e-6) {
    azimutRad = 0.0;
    return;
  }

  double cosAz = constrain(
    -(sin(latRad) * cos(zenithRad) - sin(decl)) / denom,
    -1.0, 1.0
  );
  double baseAzRad = acos(cosAz);

  azimutRad = (hourAngleDeg > 0.0) ? (2.0 * M_PI - baseAzRad) : baseAzRad;
}
