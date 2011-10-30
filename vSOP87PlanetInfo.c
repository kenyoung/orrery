/*
  This module contains function calls to calculate the positions of
  the sun and planets.   It follows the method given in Chapter 32
  of Jean Meeus's book Astronomical Algorythms.  However, unlike that
  book, this code uses *all* of the terms in the VSOP87 model, so the
  result is more accurate than what one gets using Meeus's truncated
  tables.   There are few comments in this code, because it pretty
  closely follows the steps given in Meeus's book, and uses similar
  variable names, so the book is the documentation for the code.

  The full VSOP87 model, for all planets in the Solar System, contains
  nearly 100,000 coefficients.   These coefficients are available via
  FTP, as ASCII tables.   A standalone program was written to read
  those files and write out the coefficients as binary files (mostly
  double precision floats, with a few integers specifying table sizes).
  The code here reads those files into heap storage, the first time the
  function is called.   Obviously the file read only occurs once -  the
  heap storage is never freed.
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>

#define TRUE  (1)
#define FALSE (0)

#define DEGREES_TO_RADIANS (M_PI/180.0)
#define ARCSEC_TO_RADIANS  (M_PI/(180.0*3600.0))
#define N_PLANETS (8)
#define SUN (2)

static double *vSOPCoef[N_PLANETS][3][6][3];
static unsigned short nVSOPCoef[N_PLANETS][3][6];
static char *planetName[N_PLANETS] = {"mercury", "venus",   "earth",
				      "mars",    "jupiter", "saturn",
				      "uranus",  "neptune"};
void nutation(double T, double *deltaPhi, double *deltaEps, double *eps);

void doubleNormalize0to2pi(double *angle);

void heliocentricEclipticCoordinates(char *dataDir, double tJD, int planet,
		    double *lHelio, double *bHelio, double *rHelio)
{
  static int dataNeeded = TRUE;
  int i, j;
  double tau, L[6], B[6], R[6], Ltotal, Btotal, Rtotal;

  if (dataNeeded) {
    /*
      Read the binary files containing the VSOP87 coefficients
    */
    int p, fD, done, power, coef, nRead;
    int nDoubles = 0;
    char fileName[100];

    /*
      We aren't going to use this function for Mercury, Uranus and Neptune
      right away, so don't bother reading the data for them into
      RAM.
    */
    for (p = 1; p < N_PLANETS-2; p++) {
      sprintf(fileName, "%s/%sVSOPData.bin", dataDir, planetName[p]);
      fD = open(fileName, O_RDONLY);
      if (fD < 0) {
	perror(fileName);
	exit(-1);
      }
      done = FALSE;
      power = coef = 0;
      while (!done) {
	nRead = read(fD, &nVSOPCoef[p][coef][power], 4);
	if (nRead != 4) {
	  fprintf(stderr, "rRead = %d for read of nVSOPCoef[%d][%d][%d]\n",
		  nRead, p, coef, power);
	  exit(-1);
	}
	for (i = 0; i < 3; i++) {
	  vSOPCoef[p][coef][power][i] =
	    (double *)malloc(sizeof(double)*nVSOPCoef[p][coef][power]);
	  nDoubles += nVSOPCoef[p][coef][power];
	  if (vSOPCoef[p][coef][power][i] == NULL) {
	    perror("malloc of vSOPCoef[p][coef][power]");
 	    fprintf(stderr, "Wanted %d doubles\n", nVSOPCoef[p][coef][power]);
	    fprintf(stderr, "p = %d, coef = %d, power = %d\n",
		    p, coef, power);
	    exit(-1);
	  }
	}
	for (i = 0; i < nVSOPCoef[p][coef][power]; i++)
	  for (j = 0; j < 3; j++)
	    read(fD, &vSOPCoef[p][coef][power][j][i], 8);
	power++;
	if (power > 5) {
	  coef++;
	  if (coef > 2)
	    done = TRUE;
	  else
	    power = 0;
	}
      }
    }
    dataNeeded = FALSE;
  }
  tau = (tJD - 2451545.0) / 365250.0;
  for (i = 0; i <= 5; i++) {
    L[i] = 0.0;
    for (j = 0; j < nVSOPCoef[planet][0][i]; j++) {
      L[i] += vSOPCoef[planet][0][i][0][j]*cos(vSOPCoef[planet][0][i][1][j] +
					       vSOPCoef[planet][0][i][2][j]*tau);
    }
  }
  Ltotal = L[0]
         + L[1] * tau
         + L[2] * tau*tau
         + L[3] * tau*tau*tau
         + L[4] * tau*tau*tau*tau
         + L[5] * tau*tau*tau*tau*tau;
  doubleNormalize0to2pi(&Ltotal);

  for (i = 0; i <= 5; i++) {
    B[i] = 0.0;
    for (j = 0; j < nVSOPCoef[planet][1][i]; j++) {
      B[i] += vSOPCoef[planet][1][i][0][j]*cos(vSOPCoef[planet][1][i][1][j] +
					       vSOPCoef[planet][1][i][2][j]*tau);
    }
  }
  Btotal = B[0]
         + B[1] * tau
         + B[2] * tau*tau
         + B[3] * tau*tau*tau
         + B[4] * tau*tau*tau*tau
         + B[5] * tau*tau*tau*tau*tau;
  doubleNormalize0to2pi(&Btotal);
  for (i = 0; i <= 5; i++) {
    R[i] = 0.0;
    for (j = 0; j < nVSOPCoef[planet][2][i]; j++) {
      R[i] += vSOPCoef[planet][2][i][0][j]*cos(vSOPCoef[planet][2][i][1][j] +
					       vSOPCoef[planet][2][i][2][j]*tau);
    }
  }
  Rtotal = R[0]
         + R[1] * tau
         + R[2] * tau*tau
         + R[3] * tau*tau*tau
         + R[4] * tau*tau*tau*tau
         + R[5] * tau*tau*tau*tau*tau;

  *lHelio = Ltotal;
  *bHelio = Btotal;
  *rHelio = Rtotal;
}

void vSOPPlanetInfo(char *dataDir, double tJD, int planet,
		    double *rA, double *dec, double *distance)
{
  double L, B, R, L0, B0, R0, x, y, z, lambda, beta, delta, tau,
    T, Lprime, deltaL, deltaB, deltaPhi, deltaEps, eps, tY, tX;

  heliocentricEclipticCoordinates(dataDir, tJD, 2, &L0, &B0, &R0);
  if (planet != SUN)
    heliocentricEclipticCoordinates(dataDir, tJD, planet, &L, &B, &R);
  else
    L = B = R = 0.0;
  x = R*cos(B)*cos(L) - R0*cos(B0)*cos(L0);
  y = R*cos(B)*sin(L) - R0*cos(B0)*sin(L0);
  z = R*sin(B)        - R0*sin(B0);
  delta = sqrt(x*x + y*y + z*z);
  tau = 0.0057755183*delta;
  heliocentricEclipticCoordinates(dataDir, tJD-tau, 2, &L0, &B0, &R0);
  if (planet != SUN)
    heliocentricEclipticCoordinates(dataDir, tJD-tau, planet, &L, &B, &R);
  else
    L = B = R = 0.0;
  x = R*cos(B)*cos(L) - R0*cos(B0)*cos(L0);
  y = R*cos(B)*sin(L) - R0*cos(B0)*sin(L0);
  z = R*sin(B)        - R0*sin(B0);
  lambda = atan2(y, x);
  doubleNormalize0to2pi(&lambda);
  beta   = atan2(z, sqrt(x*x + y*y));

  T = (tJD - 2451545.0) / 36525.0;
  Lprime = lambda - 1.397*DEGREES_TO_RADIANS*T - 0.00031*DEGREES_TO_RADIANS*T*T;
  deltaL = -0.09033*ARCSEC_TO_RADIANS
    + 0.03916*ARCSEC_TO_RADIANS*(cos(Lprime) + sin(Lprime))*tan(beta);
  deltaB = 0.03916*ARCSEC_TO_RADIANS*(cos(Lprime) - sin(Lprime));
  lambda += deltaL;
  beta   += deltaB;

  nutation(T, &deltaPhi, &deltaEps, &eps);
  eps *= DEGREES_TO_RADIANS;
  lambda += deltaPhi*ARCSEC_TO_RADIANS;
  tY = sin(lambda)*cos(eps) - tan(beta)*sin(eps) + 1.0;
  tX = cos(lambda) + 1.0;
  *rA  = atan2(sin(lambda)*cos(eps) - tan(beta)*sin(eps), cos(lambda));
  doubleNormalize0to2pi(rA);
  *dec = asin(sin(beta)*cos(eps) + cos(beta)*sin(eps)*sin(lambda));
  *distance = sqrt(x*x + y*y + z*z);
}
