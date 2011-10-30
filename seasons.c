#include <stdio.h>
#include <math.h>

#define DEBUG_MESSAGES_ON (0)
#define dprintf if (DEBUG_MESSAGES_ON) printf

void tJDToDate(double tJD, int *year, int *month, int *day);
void analemma(char *dataDir, double tJD, double *EoT, double *dec, double *EoE, double *eclipticLong);

/*
  Estimate deltaT, = TT - UT
  The formulae below are from the NASA Eclipse web site
  "Polynomial Expressions for Delta T (deltaT)"
  http://eclipse.gsfc.nasa.gov/SEcat5/deltatpoly.html
  (as of Jan. 14, 2011)
 */
double deltaT(double tJD)
{
  int day, month, iYear;
  double dT, u, year;

  tJDToDate(tJD, &iYear, &month, &day);
  year = ((double)iYear) + ((double)(month-1))/12.0 + ((double)(day-1))/365.0;
  dprintf("%f = %02d/%02d/%04d (%f) ", tJD, day, month, iYear, year);
  if (year > 2005.0) {
    u = year - 2000.0;
    dT = 62.92
       +  0.32217  * u
       +  0.005589 * u*u; 
  } else if (year < -500.0) {
    u = (year-1820.0)/100.0;
    dT = -20.0 + 32.0*u*u;
  } else if (year < 500.0) {
    u = year/100.0;
    dT = 10583.6
       -  1014.41         * u
       +    33.78311      * u*u 
       -     5.952053     * u*u*u
       -     0.1798452    * u*u*u*u
       +     0.022174192  * u*u*u*u*u
       +     0.0090316521 * u*u*u*u*u*u;
  } else if (year < 1600.0) {
    u = (year-1000)/100.0;
    dT = 1574.2
       -  556.01         * u
       +   71.23472      * u*u
       +    0.319781     * u*u*u
       -    0.8503463    * u*u*u*u
       -    0.005050998  * u*u*u*u*u
       +    0.0083572073 * u*u*u*u*u*u;
  } else if (year < 1700.0) {
    u = year - 1600.0;
    dT = 120.0
       -   0.9808     * u
       -   0.01532    * u*u
      + (1.0/7129.0) * u*u*u;
  } else if (year < 1800.0) {
    u = year - 1700.0;
    dT = 8.83
       + 0.1603          * u
       - 0.0059285       * u*u
       + 0.00013336      * u*u*u
       - (1.0/1174000.0) * u*u*u*u;
  } else if (year < 1860.0) {
    u = year - 1800.0;
    dT = 13.72
       -  0.332447       * u
       +  0.0068612      * u*u
       +  0.0041116      * u*u*u
       -  0.00037436     * u*u*u*u 
       +  0.0000121272   * u*u*u*u*u
       -  0.0000001699   * u*u*u*u*u*u
       +  0.000000000875 * u*u*u*u*u*u*u;
  } else if (year < 1900.0) {
    u = year - 1860.0;
    dT = 7.62
       + 0.5737          * u
       - 0.251754        * u*u
       + 0.01680668      * u*u*u
       - 0.0004473624    * u*u*u*u
       + (1.0/ 233174.0) * u*u*u*u*u;

  } else if (year < 1920.0) {
    u = year - 1900.0;
    dT = -2.79
       +  1.494119  * u
       -  0.0598939 * u*u
       +  0.0061966 * u*u*u
       -  0.000197  * u*u*u*u;
  } else if (year < 1941.0) {
    u = year - 1920.0;
    dT = 21.20
       +  0.84493   * u
       -  0.076100  * u*u
       +  0.0020936 * u*u*u;
  } else if (year < 1961.0) {
    u = year - 1950.0;
    dT = 29.07
       +  0.407        * u
       - (1.0/233.0)   * u*u
       +  (1.0/2547.0) * u*u*u;
  } else if (year < 1986.0) {
    u = year - 1975.0;
    dT = 45.45
       +  1.067       * u
       -  (1.0/260.0) * u*u
       -  (1.0/718.0) * u*u*u;
  } else {
    u = year - 2000.0;
    dT = 63.86
       +  0.3345        * u
       -  0.060374      * u*u
       +  0.0017275     * u*u*u
       +  0.000651814   * u*u*u*u 
       +  0.00002373599 * u*u*u*u*u;
  }
  dprintf("returning %5.1f\n", dT);
  return(dT);
}

#define DEGREES_TO_RADIANS (M_PI/180.0)

double seasonCoef[24][3] = {{485.0, 324.96,   1934.136},
			    {203.0, 337.23,  32964.467},
			    {199.0, 342.08,     20.186},
			    {182.0,  27.85, 445267.112},
			    {156.0,  73.14,  45036.886},
			    {136.0, 171.52,  22518.443},
			    { 77.0, 222.54,  65928.934},
			    { 74.0, 296.72,   3034.906},
			    { 70.0, 243.58,   9037.513},
			    { 58.0, 119.81,  33718.147},
			    { 52.0, 297.17,    150.678},
			    { 50.0,  21.02,   2281.226},
			    { 45.0, 247.54,  29929.562},
			    { 44.0, 325.15,  31555.956},
			    { 29.0,  60.93,   4443.417},
			    { 18.0, 155.12,  67555.328},
			    { 17.0, 288.79,   4562.452},
			    { 16.0, 198.04,  62894.029},
			    { 14.0, 199.76,  31436.921},
			    { 12.0,  95.39,  14577.848},
			    { 12.0, 287.11,  31931.756},
			    { 12.0, 320.81,  34777.259},
			    {  9.0, 227.73,   1222.114},
			    {  8.0,  15.45,  16859.074}};

/*
  Return the approximate time of the begining of each of the four seasons, in JD.
*/
void seasons(char *dataDir, int year,
	     double *spring, double *summer, double *fall, double *winter)
{
  int i, j;
  double Y, meanSeason[4];

  dprintf("seasons(%d, ...)\n", year);
  if (year < 1000) {
    Y = ((double)year)/1000.0;
    dprintf("Old path Y = %f\n", Y);
    meanSeason[0] = 1721139.29189
                  +  365242.13740 * Y
                  +       0.06134 * Y*Y
                  +       0.00111 * Y*Y*Y
                  -       0.00071 * Y*Y*Y*Y;
    meanSeason[1] = 1721233.25401
                  +  365241.72562 * Y
                  -       0.05323 * Y*Y
                  +       0.00907 * Y*Y*Y
                  +       0.00025 * Y*Y*Y*Y;
    meanSeason[2] = 1721325.70455
                  +  365242.49558 * Y
                  -       0.11677 * Y*Y
                  -       0.00297 * Y*Y*Y
                  +       0.00074 * Y*Y*Y*Y;
    meanSeason[3] = 1721414.39987
                  +  365242.88257 * Y
                  -       0.00769 * Y*Y
                  -       0.00933 * Y*Y*Y
                  -       0.00006 * Y*Y*Y*Y;
  } else {
    Y = ((double)(year-2000))/1000.0;
    dprintf("New path Y = %f\n", Y);
    meanSeason[0] = 2451623.80984
                  +  365242.37404 * Y
                  +       0.05169 * Y*Y
                  -       0.00411 * Y*Y*Y
                  -       0.00057 * Y*Y*Y*Y;
    meanSeason[1] = 2451716.56767
                  +  365241.62603 * Y
                  +       0.00325 * Y*Y
                  +       0.00888 * Y*Y*Y
                  -       0.00030 * Y*Y*Y*Y;
    meanSeason[2] = 2451810.21715
                  +  365242.01767 * Y
                  -       0.11575 * Y*Y
                  +       0.00337 * Y*Y*Y
                  +       0.00078 * Y*Y*Y*Y;
    meanSeason[3] = 2451900.05952
                  +  365242.74049 * Y
                  -       0.06223 * Y*Y
                  -       0.00823 * Y*Y*Y
                  +       0.00032 * Y*Y*Y*Y;
  }
  for (i = 0; i < 4; i++) {
    int iter;
    double S, T, W, deltaLambda, correction, eclipticLong, dummy;

    dprintf("JDE0[%d] = %f\n", i, meanSeason[i]);
    T           = (meanSeason[i] - 2451545.0) / 36525.0;
    W           = 35999.373*T - 2.47;
    deltaLambda = 1.0 + 0.0334*cos(W*DEGREES_TO_RADIANS) + 0.0007*cos(2.0*W*DEGREES_TO_RADIANS);
    S           = 0.0;
    for (j = 0; j < 24; j++) {
      double arg;

      arg = seasonCoef[j][1] + seasonCoef[j][2]*T;
      S += seasonCoef[j][0]*cos(arg*DEGREES_TO_RADIANS);
    }
    dprintf("\tT: %f W: %f dL: %f S: %f\n", T, W, deltaLambda, S);
    meanSeason[i] += 0.00001*S/deltaLambda;
    iter = 0;
    correction = 1000.0;
    while ((iter < 10) && (fabs(correction) > (1.0/864000.0))) {
      analemma(dataDir, meanSeason[i], &dummy, &dummy, NULL, &eclipticLong);
      correction = 58.0 * sin((((double)i)*90.0 - eclipticLong)*DEGREES_TO_RADIANS);
      dprintf("\t\t%d: long = %f, Correction = %e\n", iter, eclipticLong, correction);
      meanSeason[i] += correction;
      iter++;
    }
  }

  /* Convert from Terrestrial Time to UT */
  for (i = 0; i < 4; i++)
    meanSeason[i] -= deltaT(meanSeason[i])/86400.0;

  *spring = meanSeason[0];
  *summer = meanSeason[1];
  *fall   = meanSeason[2];
  *winter = meanSeason[3];
}
