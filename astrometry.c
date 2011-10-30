/*
  This module contains the functions needed to convert catalog
  coordinates into apparent coordinates for a particular moment.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define DEGREES_TO_RADIANS (M_PI/180.0)
#define HOURS_TO_RADIANS   (M_PI/12.0)
#define J2000_TJD          (2451545.0)
#define KAPPA              (20.49552)

double deltaT(double tJD);
void nutation(double T, double *deltaPhi, double *deltaEps, double *eps);
double sinD(double degrees);
double cosD(double degrees);
void doubleNormalize0to360(double *a);

double tanD(double degrees)
{
  return (tan(degrees*DEGREES_TO_RADIANS));
}

void calculateApparentPosition(double tJD,
			       double rAJ2000, double decJ2000,
			       float parallax, float muRA, float muDec,
			       double *rANow,  double *decNow)
{
  double rA0, dec0, t, pMRA, pMDec, zeta, z, theta, A, B, C,
    rA, dec, jDE, T, deltaPhi, deltaEps, eps, L0, M, e, sunLong,
    deltaRA1, deltaDec1, pi, deltaRA2, deltaDec2;

  rA0 = rAJ2000; dec0 = decJ2000;
  t = (tJD - J2000_TJD)/36525.0;

  pMRA  = muRA *100.0*t;
  pMDec = muDec*100.0*t;
  rA0  += (pMRA /3600.0)*DEGREES_TO_RADIANS;
  dec0 += (pMDec/3600.0)*DEGREES_TO_RADIANS;

  zeta   = 2306.2181*t + 0.30188*t*t + 0.017998*t*t*t;
  z      = 2306.2181*t + 1.09468*t*t + 0.018203*t*t*t;
  theta  = 2004.3109*t - 0.42665*t*t - 0.041833*t*t*t;
  zeta  *= DEGREES_TO_RADIANS/3600.0;
  z     *= DEGREES_TO_RADIANS/3600.0;
  theta *= DEGREES_TO_RADIANS/3600.0;
  A = cos(dec0)*sin(rA0+zeta);
  B = cos(theta)*cos(dec0)*cos(rA0+zeta) - sin(theta)*sin(dec0);
  C = sin(theta)*cos(dec0)*cos(rA0+zeta) + cos(theta)*sin(dec0);

  dec = asin(C);
  rA  = atan2(A, B) + z;

  jDE = tJD + deltaT(tJD)/86400.0;
  T   = (jDE - J2000_TJD)/36525.0;
  nutation(T, &deltaPhi, &deltaEps, &eps);
  deltaRA1  = (cosD(eps) + sinD(eps)*sin(rA)*tan(dec))*deltaPhi - cos(rA)*tan(dec)*deltaEps;
  deltaDec1 = sinD(eps)*cos(rA)*deltaPhi + sin(rA)*deltaEps;

  L0 = 280.46646 + 36000.76983*T + 0.0003032*T*T;
  M  = 357.529110000 + 35999.050290000*T - 0.0001537000*T*T;
  e  =   0.016708634 -     0.000042037*T - 0.0000001267*T*T;
  C  = (1.914602 - 0.004817*T - 0.000014*T*T)*sinD(M)
    + (0.019993 - 0.000101*T)*sinD(2.0*M)
    + 0.000289*sinD(3.0*M);
  sunLong = L0 + C;
  doubleNormalize0to360(&sunLong);
  pi = 102.93735 + 1.71946*T + 0.00046*T*T;
  deltaRA2  = (-KAPPA*(cos(rA)*cosD(sunLong)*cosD(eps) + sin(rA)*sinD(sunLong))
	       + e*KAPPA*(cos(rA)*cosD(pi)*cosD(eps) + sin(rA)*sinD(pi)))/cos(dec);
  deltaDec2 = -KAPPA*(cosD(sunLong)*cosD(eps)*(tanD(eps)*cos(dec) - sin(rA)*sin(dec))
		      + cos(rA)*sin(dec)*sinD(sunLong))
    + e*KAPPA*(cosD(pi)*cosD(eps)*(tanD(eps)*cos(dec) - sin(rA)*sin(dec)) +
	       cos(rA)*sin(dec)*sinD(pi));
  rA  += DEGREES_TO_RADIANS*(( deltaRA1 + deltaRA2 )/3600.0);
  dec += DEGREES_TO_RADIANS*((deltaDec1 + deltaDec2)/3600.0);

  *rANow = rA; *decNow = dec;
}
