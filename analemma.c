/*
  This modual contains the code to calculate the analemma function,
  with returns the Equation of Time, and the declination of the
  sun, both in radians, the Equation of the Equinoxes in seconds,
  and the solar ecliptic longitude, in degrees.
  It must be passed the Terrestial Time.
  The calculation closely follows the procedure given in "Astronomical
  Algorithms" by Jean Meeus.   Unlike that book, however, this function
  uses all terms of the VSOP87 model, for greater accuracy.

  The constants for the VSOP87 module are stored in a binary file
  called earthVSOPData.bin, which were derived from the original
  VSOP87 table named VSOP87D.ear (from the VSOP87 FTP site).

 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>

#define TRUE  (1)
#define FALSE (0)

#define DEGREES_TO_RADIANS (M_PI/180.0)
#define HOURS_TO_RADIANS   (M_PI/12.0)

#define DEBUG_MESSAGES_ON (0)
#define dprintf if (DEBUG_MESSAGES_ON) printf

void doubleNormalize0to360(double *angle);

#define N_NUT_TERMS (63)
const
float nutSinCoef[N_NUT_TERMS][2] = {{-171996.0, -174.2},
                                    { -13187.0,   -1.6},
                                    {  -2274.0,   -0.2},
                                    {   2062.0,    0.2},
                                    {   1426.0,   -3.4},
                                    {    712.0,    0.1},
                                    {   -517.0,    1.2},
                                    {   -386.0,   -0.4},
                                    {   -301.0,    0.0},
                                    {    217.0,   -0.5},
                                    {   -158.0,    0.0},
                                    {    129.0,    0.1},
                                    {    123.0,    0.0},
                                    {     63.0,    0.0},
                                    {     63.0,    0.1},
                                    {    -59.0,    0.0},
                                    {    -58,     -0.1},
                                    {    -51.0,    0.0},
                                    {     48.0,    0.0},
                                    {     46.0,    0.0},
                                    {    -38.0,    0.0},
                                    {    -31.0,    0.0},
                                    {     29.0,    0.0},
                                    {     29.0,    0.0},
                                    {     26.0,    0.0},
                                    {    -22.0,    0.0},
                                    {     21.0,    0.0},
                                    {     17.0,   -0.1},
                                    {     16.0,    0.0},
                                    {    -16.0,    0.1},
                                    {    -15.0,    0.0},
                                    {    -13.0,    0.0},
                                    {    -12.0,    0.0},
                                    {     11.0,    0.0},
                                    {    -10.0,    0.0},
                                    {     -8.0,    0.0},
                                    {      7.0,    0.0},
                                    {     -7.0,    0.0},
                                    {     -7.0,    0.0},
                                    {     -7.0,    0.0},
                                    {      6.0,    0.0},
                                    {      6.0,    0.0},
                                    {      6.0,    0.0},
                                    {     -6.0,    0.0},
                                    {     -6.0,    0.0},
                                    {      5.0,    0.0},
                                    {     -5.0,    0.0},
                                    {     -5.0,    0.0},
                                    {     -5.0,    0.0},
                                    {      4.0,    0.0},
                                    {      4.0,    0.0},
                                    {      4.0,    0.0},
                                    {     -4.0,    0.0},
                                    {     -4.0,    0.0},
                                    {     -4.0,    0.0},
                                    {      3.0,    0.0},
                                    {     -3.0,    0.0},
                                    {     -3.0,    0.0},
                                    {     -3.0,    0.0},
                                    {     -3.0,    0.0},
                                    {     -3.0,    0.0},
                                    {     -3.0,    0.0},
                                    {     -3.0,    0.0}};
const
float nutCosCoef[N_NUT_TERMS][2] = {{  92025.0,    8.9},
                                    {   5736.0,   -3.1},
                                    {    977.0,   -0.5},
                                    {   -895.0,    0.5},
                                    {     54.0,   -0.1},
                                    {     -7.0,    0.0},
                                    {    224.0,   -0.6},
                                    {    200.0,    0.0},
                                    {    129.0,   -0.1},
                                    {    -95.0,    0.3},
                                    {      0.0,    0.0},
                                    {    -70.0,    0.0},
                                    {    -53.0,    0.0},
                                    {      0.0,    0.0},
                                    {    -33.0,    0.0},
                                    {     26.0,    0.0},
                                    {     32.0,    0.0},
                                    {     27.0,    0.0},
                                    {      0.0,    0.0},
                                    {    -24.0,    0.0},
                                    {     16.0,    0.0},
                                    {     13.0,    0.0},
                                    {      0.0,    0.0},
                                    {    -12.0,    0.0},
                                    {      0.0,    0.0},
                                    {      0.0,    0.0},
                                    {    -10.0,    0.0},
                                    {      0.0,    0.0},
                                    {     -8.0,    0.0},
                                    {      7.0,    0.0},
                                    {      9.0,    0.0},
                                    {      7.0,    0.0},
                                    {      6.0,    0.0},
                                    {      0.0,    0.0},
                                    {      5.0,    0.0},
                                    {      3.0,    0.0},
                                    {     -3.0,    0.0},
                                    {      0.0,    0.0},
                                    {      3.0,    0.0},
                                    {      3.0,    0.0},
                                    {      0.0,    0.0},
                                    {     -3.0,    0.0},
                                    {     -3.0,    0.0},
                                    {      3.0,    0.0},
                                    {      3.0,    0.0},
                                    {      0.0,    0.0},
                                    {      3.0,    0.0},
                                    {      3.0,    0.0},
                                    {      3.0,    0.0},
                                    {      0.0,    0.0},
                                    {      0.0,    0.0},
                                    {      0.0,    0.0},
                                    {      0.0,    0.0},
                                    {      0.0,    0.0},
                                    {      0.0,    0.0},
                                    {      0.0,    0.0},
                                    {      0.0,    0.0},
                                    {      0.0,    0.0},
                                    {      0.0,    0.0},
                                    {      0.0,    0.0},
                                    {      0.0,    0.0},
                                    {      0.0,    0.0},
                                    {      0.0,    0.0}};
const signed
char nutMults[N_NUT_TERMS][5] = {{ 0, 0, 0, 0, 1},
                                 {-2, 0, 0, 2, 2},
                                 { 0, 0, 0, 2, 2},
                                 { 0, 0, 0, 0, 2},
                                 { 0, 1, 0, 0, 0},
                                 { 0, 0, 1, 0, 0},
                                 {-2, 1, 0, 2, 2},
                                 { 0, 0, 0, 2, 1},
                                 { 0, 0, 1, 2, 2},
                                 {-2,-1, 0, 2, 2},
                                 {-2, 0, 1, 0, 0},
                                 {-2, 0, 0, 2, 1},
                                 { 0, 0,-1, 2, 2},
                                 { 2, 0, 0, 0, 0},
                                 { 0, 0, 1, 0, 1},
                                 { 2, 0,-1, 2, 2},
                                 { 0, 0,-1, 0, 1},
                                 { 0, 0, 1, 2, 1},
                                 {-2, 0, 2, 0, 0},
                                 { 0, 0,-2, 2, 1},
                                 { 2, 0, 0, 2, 2},
                                 { 0, 0, 2, 2, 2},
                                 { 0, 0, 2, 0, 0},
                                 {-2, 0, 1, 2, 2},
                                 { 0, 0, 0, 2, 0},
                                 {-2, 0, 0, 2, 0},
                                 { 0, 0,-1, 2, 1},
                                 { 0, 2, 0, 0, 0},
                                 { 2, 0,-1, 0, 1},
                                 {-2, 2, 0, 2, 2},
                                 { 0, 1, 0, 0, 1},
                                 {-2, 0, 1, 0, 1},
                                 { 0,-1, 0, 0, 1},
                                 { 0, 0, 2,-2, 0},
                                 { 2, 0,-1, 2, 1},
                                 { 2, 0, 1, 2, 2},
                                 { 0, 1, 0, 2, 2},
                                 {-2, 1, 1, 0, 0},
                                 { 0,-1, 0, 2, 2},
                                 { 2, 0, 0, 2, 1},
                                 { 2, 0, 1, 0, 0},
                                 {-2, 0, 2, 2, 2},
                                 {-2, 0, 1, 2, 1},
                                 { 2, 0,-2, 0, 1},
                                 { 2, 0, 0, 0, 1},
                                 { 0,-1, 1, 0, 0},
                                 {-2,-1, 0, 2, 1},
                                 {-2, 0, 0, 0, 1},
                                 { 0, 0, 2, 2, 1},
                                 {-2, 0, 2, 0, 1},
                                 {-2, 1, 0, 2, 1},
                                 { 0, 0, 1,-2, 0},
                                 {-1, 0, 1, 0, 0},
                                 {-2, 1, 0, 0, 0},
                                 { 1, 0, 0, 0, 0},
                                 { 0, 0, 1, 2, 0},
                                 { 0, 0,-2, 2, 2},
                                 {-1,-1, 1, 0, 0},
                                 { 0, 1, 1, 0, 0},
                                 { 0,-1, 1, 2, 2},
                                 { 2,-1,-1, 2, 2},
                                 { 0, 0, 3, 2, 2},
                                 { 2,-1, 0, 2, 2}};

void nutation(double T, double *deltaPhi, double *deltaEps, double *eps)
{
  int i;
  double D, M, Mprime, F, omega, dP, dE, U;

  D      = 297.85036 + 445267.111480*T - 0.0019142*T*T + T*T*T/189474.0;
  doubleNormalize0to360(&D);
  M      = 357.52772 +  35999.050340*T - 0.0001603*T*T - T*T*T/300000.0;
  doubleNormalize0to360(&M);
  Mprime = 134.96298 + 477198.867398*T + 0.0086972*T*T + T*T*T/ 56250.0;
  doubleNormalize0to360(&Mprime);
  F      =  93.27191 + 483202.017538*T - 0.0036825*T*T + T*T*T/327270.0;
  doubleNormalize0to360(&F);
  omega  = 125.04452 -   1934.136261*T + 0.0020708*T*T + T*T*T/450000.0;
  doubleNormalize0to360(&omega);
  dprintf("D = %f\nM = %f\nMprime = %f\nF = %f\nomega = %f\n",
     D, M, Mprime, F, omega);
  dP = dE = 0.0;
  for (i = 0; i < N_NUT_TERMS; i++) {
    dP += (nutSinCoef[i][0] + nutSinCoef[i][1]*T)
      * sin((((double)nutMults[i][0])*D
           + ((double)nutMults[i][1])*M
           + ((double)nutMults[i][2])*Mprime
           + ((double)nutMults[i][3])*F
           + ((double)nutMults[i][4])*omega) * DEGREES_TO_RADIANS);
    dE += (nutCosCoef[i][0] + nutCosCoef[i][1]*T)
      * cos((((double)nutMults[i][0])*D
           + ((double)nutMults[i][1])*M
           + ((double)nutMults[i][2])*Mprime
           + ((double)nutMults[i][3])*F
           + ((double)nutMults[i][4])*omega) * DEGREES_TO_RADIANS);
  }
  *deltaPhi = dP * 0.0001;
  *deltaEps = dE * 0.0001;
  U = T/100.0;
  *eps = - (4680.93/3600.0) * U
         - (   1.55/3600.0) * U*U
         + (1999.25/3600.0) * U*U*U
         - (  51.38/3600.0) * U*U*U*U
         - ( 249.67/3600.0) * U*U*U*U*U
         - (  39.05/3600.0) * U*U*U*U*U*U
         + (   7.12/3600.0) * U*U*U*U*U*U*U
         + (  27.87/3600.0) * U*U*U*U*U*U*U*U
         + (   5.79/3600.0) * U*U*U*U*U*U*U*U*U
         + (   2.45/3600.0) * U*U*U*U*U*U*U*U*U*U;
  *eps += 23.0 + (26.0/60.0) + (21.448/3600.0) + (*deltaEps)/3600.0;
}

void analemma(char *dataDir, double tJD, double *EoT, double *dec,
	      double *EoE, double *eclipticLong)
{
  int i, j, fD;
  int coef = 0;
  int power = 0;
  int done = FALSE;
  static int haveData = FALSE;
  static int nVSOPCoef[3][6];
  double E = 0.0;
  double t, L[6], Ltotal, B[6], Btotal, L0, T, lambdaPrime, deltaL, deltaB;
  double R[6], Rtotal, aberation;
  double deltaPhi, deltaEps, eps, lambda, alpha;
  double epsRad, cosEps, sinEps, lambdaRad, cosLambda, sinLambda;
  static double *earthVSOPCoef[3][6][3];

  dprintf("analemma(%f)\n", tJD);
  if (!haveData) {
    char fileName[100];

    sprintf(fileName, "%s/earthVSOPData.bin", dataDir);
    fD = open(fileName, O_RDONLY);
    if (fD < 0) {
      perror(fileName);
      exit(-1);
    }
    while (!done) {
      read(fD, &nVSOPCoef[coef][power], 4);
      for (i = 0; i < 3; i++) {
        earthVSOPCoef[coef][power][i] = (double *)malloc(sizeof(double)*nVSOPCoef[coef][power]);
        if (earthVSOPCoef[coef][power][i] == NULL) {
          perror("malloc of earthVSOPCoef[coef][power]");
          fprintf(stderr, "Wanted %d doubles\n", nVSOPCoef[coef][power]);
          exit(-1);
        }
      }
      for (i = 0; i < nVSOPCoef[coef][power]; i++)
        for (j = 0; j < 3; j++)
          read(fD, &earthVSOPCoef[coef][power][j][i], 8);
      power++;
      if (power > 5) {
        coef++;
        if (coef > 2)
          done = TRUE;
        else
          power = 0;
      }
    }
    haveData = TRUE;
  }
  t = (tJD - 2451545.0) / 365250.0;
  T = 10.0*t;
  dprintf("t = %20.16f\tT= %f\n", t, T);
  nutation(T, &deltaPhi, &deltaEps, &eps);
  dprintf("T = %f, dPhi = %f, dE = %f, eps = %f\n",
     T, deltaPhi, deltaEps, eps);
  L0 =   280.4664567
    + 360007.6982779  * t
    +      0.03032028 * t*t
    +                   t*t*t     /   49931.0
    -                   t*t*t*t   /   15300.0
    +                   t*t*t*t*t / 2000000.0;
  doubleNormalize0to360(&L0);
  dprintf("L0 = %f\n", L0);

  /* Calculate Sun's ecliptic longitude: */
  for (i = 0; i <= 5; i++) {
    L[i] = 0.0;
    for (j = 0; j < nVSOPCoef[0][i]; j++) {
      L[i] += earthVSOPCoef[0][i][0][j]*cos(earthVSOPCoef[0][i][1][j] +
                                            earthVSOPCoef[0][i][2][j]*t);
    }
  }
  Ltotal = L[0]
         + L[1] * t
         + L[2] * t*t
         + L[3] * t*t*t
         + L[4] * t*t*t*t
         + L[5] * t*t*t*t*t;
  Ltotal += M_PI;
  Ltotal *= 180.0/M_PI;
  doubleNormalize0to360(&Ltotal);
  dprintf("Ltotal = %f\n", Ltotal);

  /* Calculate Sun's ecliptic Latitude */
  for (i = 0; i <= 5; i++) {
    B[i] = 0.0;
    for (j = 0; j < nVSOPCoef[1][i]; j++) {
      B[i] += earthVSOPCoef[1][i][0][j]*cos(earthVSOPCoef[1][i][1][j] +
                                            earthVSOPCoef[1][i][2][j]*t);
    }
  }
  Btotal = B[0]
         + B[1] * t
         + B[2] * t*t
         + B[3] * t*t*t
         + B[4] * t*t*t*t
         + B[5] * t*t*t*t*t;
  Btotal *= -180.0/M_PI;
  dprintf("Btotal = %f\n", Btotal);

  /* Calculate Sun's distance */
  for (i = 0; i <= 5; i++) {
    R[i] = 0.0;
    for (j = 0; j < nVSOPCoef[2][i]; j++) {
      R[i] += earthVSOPCoef[2][i][0][j]*cos(earthVSOPCoef[2][i][1][j] +
                                            earthVSOPCoef[2][i][2][j]*t);
    }
  }
  Rtotal = R[0]
         + R[1] * t
         + R[2] * t*t
         + R[3] * t*t*t
         + R[4] * t*t*t*t
         + R[5] * t*t*t*t*t;
  dprintf("Rtotal = %f\n", Rtotal);

  aberation = -20.4898/Rtotal;
  dprintf("aberation = %f\n", aberation);
  lambdaPrime = Ltotal - 1.397*T - 0.00031*T*T;
  deltaL = -0.09033/3600.0;
  deltaB = 0.03916*(cos(lambdaPrime*DEGREES_TO_RADIANS) -
                    sin(lambdaPrime*DEGREES_TO_RADIANS)) / 3600.0;
  dprintf("lambdaPrime %f, deltaL %f, deltaB %f\n",
     lambdaPrime, deltaL*3600.0, deltaB*3600.0);
  Ltotal += deltaL;
  Btotal += deltaB;
  dprintf("FK5 L = %f, B = %f\n", Ltotal, Btotal*3600.0);
  lambda = Ltotal + deltaPhi/3600.0 + aberation/3600.0;
  dprintf("final lambda = %f\n", lambda);
  epsRad = eps * DEGREES_TO_RADIANS;
  cosEps = cos(epsRad);
  sinEps = sin(epsRad);
  lambdaRad = lambda * DEGREES_TO_RADIANS;
  cosLambda = cos(lambdaRad);
  sinLambda = sin(lambdaRad);
  if (cosLambda != 0.0) {
    alpha = atan2((sinLambda*cosEps
                   - tan((Btotal/3600.0)*DEGREES_TO_RADIANS)*sinEps),
                  cosLambda)/DEGREES_TO_RADIANS;
    doubleNormalize0to360(&alpha);
  } else
    alpha = 90.0;
  *dec = asin(sin((Btotal/3600.0)*DEGREES_TO_RADIANS)*cosEps
	      + cos((Btotal/3600.0)*DEGREES_TO_RADIANS)*sinEps
	      * sinLambda)/DEGREES_TO_RADIANS;
  dprintf("alpha = %f\n", alpha);
  E = L0 - 0.0057183 - alpha + (deltaPhi/3600.0)*cosEps;
  *EoT = E*DEGREES_TO_RADIANS;
  while (*EoT > M_PI)
    *EoT -= 2.0*M_PI;
  while (*EoT < -M_PI)
    *EoT += 2.0*M_PI;
  if (EoE != NULL)
    *EoE = deltaPhi*cosEps/15.0;
  if (eclipticLong != NULL)
    *eclipticLong = lambda;
}
