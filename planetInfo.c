/*
  planetInfo.c

  This file contains the functions related to planet orbital elements used by
the orrery program.

  Copyright (C) (2007, 2008) Ken Young orrery.moko@gmail.com

  This program is free software; you can redistribute it and/or 
  modify it under the terms of the GNU General Public License 
  as published by the Free Software Foundation; either 
  version 2 of the License, or (at your option) any later 
  version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <math.h>
#include "orrery.h"

#define TRUE  (1)
#define ERROR_EXIT (-1)

/*
  References for Solar System object positions:
  1) "Keplerian Elements for Approximate Positions of the Major Planets", E. M. Standish, JPL/Caltech
  2) "Practical Astronomy with your Calculator", 3rd Ed. Peter Duffett-Smith
*/

typedef struct elements {
  double a[2], dadt[2];
  double e[2], dedt[2];
  double I[2], dIdt[2];
  double L[2], dLdt[2];
  double o[2], dodt[2];
  double Om[2], dOmdt[2];
  double b, c, s, f;
} elements;

#define N_PLANETS (9)
#define DEGREES_TO_RADIANS (M_PI/180.0)
#define HOURS_TO_RADIANS (M_PI/12.0)
#define TWO_PI (2.0*M_PI)
#define EPS (23.43928 * DEGREES_TO_RADIANS)
#define TJD_PRE_1800  (2378496.500000)
#define TJD_POST_2050 (2470172.500000)

static int debugMessagesOn = FALSE;

#define dprintf if (debugMessagesOn) printf

static elements element[N_PLANETS];

static char *planetNames[N_PLANETS] = {"mercury", "venus",   "earthMoonBary",
         			"mars",    "jupiter", "saturn",
		        	"uranus",  "neptune", "pluto"};
static char *monthAbrs[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
			      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
static float V0[N_SOLAR_SYSTEM_OBJECTS] =
  {-26.0, 0.68, -4.40, 0.0, 0.0, -1.52, -9.40, -8.88, -7.19, -6.87, -1.00};

cometEphem *cometRoot = NULL;
ephemEntry *ephemRoot = NULL;
int cometDataReadIn = FALSE; /* Don't bother reading in all the comet data until it's needed */

void exit(int status);
int getLine(int fD, char *buffer, int *eOF);
double buildTJD(int year, int month, int day, int hour, int minute, int second, int nsec);

double *vector(nl, nh)
     int nl, nh;
{
  double *v;
  
  v=(double *)malloc((unsigned) (nh-nl+1)*sizeof(double));
  if (!v) {
    perror("allocation failure in dvector()");
    exit(ERROR_EXIT);
  }
  return v-nl;
}

void free_vector(v, nl, nh)
double *v;
int nl, nh;
{
  free((char*) (v+nl));
}

void spline(x, y, n, y2)
double x[], y[], y2[];
int n;
{
  int i, k;
  double p, qn, sig, un, *u, *vector();
  void free_vector();
  
  u = vector(0, n-2);
  y2[0] = u[0] = 0.0;
  for (i = 1; i <= n-2; i++) {
    sig = (x[i] - x[i-1])/(x[i+1] - x[i-1]);
    p = sig*y2[i-1] + 2.0;
    y2[i] = (sig - 1.0)/p;
    u[i] = (y[i+1]-y[i])/(x[i+1]-x[i]) - (y[i]-y[i-1])/(x[i]-x[i-1]);
    u[i] = (6.0*u[i]/(x[i+1]-x[i-1])-sig*u[i-1])/p;
  }
  qn = un = 0.0;
  y2[n-1] = (un-qn*u[n-2])/(qn*y2[n-2]+1.0);
  for (k = n-2; k >= 0; k--)
    y2[k] = y2[k]*y2[k+1]+u[k];
  free_vector(u, 0, n-2);
}

double splint(xa, ya, y2a, n, x)
     double xa[], ya[], y2a[], x;
     int n;
{
  int klo, khi, k;
  double h, b, a;
  void nrerror();
  
  klo = 0;
  khi = n-1;
  while (khi-klo > 1) {
    k = (khi+klo) >> 1;
    if (xa[k] > x)
      khi=k;
    else
      klo=k;
  }
  h = xa[khi] - xa[klo];
  if (h == 0.0) {
    perror("Bad XA input to routine SPLINT");
    exit(ERROR_EXIT);
  }
  a = (xa[khi] - x)/h;
  b = (x - xa[klo])/h;
  return(a*ya[klo]+b*ya[khi]+((a*a*a-a)*y2a[klo]+(b*b*b-b)*y2a[khi])*(h*h)/6.0);
}

float phase(float sunLambda, float earthLambda)
{
  float d;

  d = earthLambda - sunLambda;
  return (0.5*(1.0 + cos(d)));
}

/*
  The following function solves Kepler's Equation
  M = E - e*sin(E)
*/
double kepler(double e, double M)
{
  int n = 0;
  int iM;
  double dM, dE, eD, En;

  iM = (int)(M / 360.0);
  M -= (double)(iM*360);
  while (M < -180.0)
    M += 360.0;
  while (M > 180.0)
    M -= 360.0;
  eD = e/DEGREES_TO_RADIANS;
  En = M + eD*sin(M*DEGREES_TO_RADIANS);
  do {
    dM = M - (En - eD*sin(En*DEGREES_TO_RADIANS));
    dE = dM/(1.0 - e*cos(En*DEGREES_TO_RADIANS));
    En += dE;
    n++;
  } while (fabs(dE) > 1.0e-7);
  dprintf("Kepler iterated %d times E = %f, M = %f, eD = %f\n",
	 n, En, M, eD);
  return(En);
}

void eclipticToJ2000(double beta, double lambda, double *rA, double *dec)
{
  *rA = atan2(sin(lambda)*cos(EPS) - tan(beta)*sin(EPS), cos(lambda));
  *dec = asin(sin(beta)*cos(EPS) + cos(beta)*sin(EPS)*sin(lambda));
}

void j2000ToEcliptic(double rA, double dec, double *beta, double *lambda)
{
  *lambda = atan2(sin(rA)*cos(EPS) + tan(dec)*sin(EPS), cos(rA));
  *beta   = asin(sin(dec)*cos(EPS) - cos(dec)*sin(EPS)*sin(rA));
}

/*
  Put an angle in degrees within the range of 0 -> 360 by.
*/
void norm(double *value)
{
  int iRaw;
  double raw;

  raw = *value;
  iRaw = (int)(raw/360.0);
  raw -= (double)(iRaw*360);
  while (raw < 0.0)
    raw += 360.0;
  while (raw > 360.0)
    raw -= 360.0;
  *value = raw;
  return;
}

/*
  From Reference 2
 */
void getMoonPosition(double tJD, double sunELong, double *rA, double *dec, double *eLong, double *eLat, float *F)
{
  double V, lDoublePrime, NPrime, rat, moonELong, moonELat;
  double D, M, l, Mm, N, Enu, Ae, A3, A4, MmPrime, Ec, lPrime, x, y;

  D = tJD - 2447891.5;
  M          = (360.0/365.242191)*D + 279.403303 - 282.768422;
  norm(&M);
  l = 13.1763966*D + 318.351648;
  norm(&l);
  Mm = l - 0.1114041*D - 36.340410;
  norm(&Mm);
  N = 318.510107 - 0.0529539*D;
  norm(&N);
  Enu = 1.2739 * sin(2.0*(l*DEGREES_TO_RADIANS - sunELong) - Mm*DEGREES_TO_RADIANS);
  Ae = 0.1858 * sin(M*DEGREES_TO_RADIANS);
  A3 = 0.37 * sin(M*DEGREES_TO_RADIANS);
  MmPrime = Mm + Enu - Ae - A3;
  norm(&MmPrime);
  Ec = 6.2886 * sin(MmPrime*DEGREES_TO_RADIANS);
  A4 = 0.214 *  sin(2.0*MmPrime*DEGREES_TO_RADIANS);
  lPrime = l + Enu + Ec - Ae - A4;
  V = 0.6583 * sin(2.0*(lPrime*DEGREES_TO_RADIANS - sunELong));
  lDoublePrime = lPrime + V;
  NPrime = N - 0.16*sin(sunELong);
  y = sin((lDoublePrime - NPrime)*DEGREES_TO_RADIANS) * 0.995970320973;
  x = cos((lDoublePrime - NPrime)*DEGREES_TO_RADIANS);
  rat = atan2(y, x)/DEGREES_TO_RADIANS;
  norm(&rat);
  *eLong = moonELong = (rat + NPrime)*DEGREES_TO_RADIANS;
  *eLat = moonELat = asin(sin((lDoublePrime - NPrime)*DEGREES_TO_RADIANS)*8.96834418471e-2);
  eclipticToJ2000(moonELat, moonELong, rA, dec);
  *F = 0.5*(1.0 - cos(lDoublePrime*DEGREES_TO_RADIANS - sunELong));
}

void calculateCometSplineData(void)
/*
  Use the arrays of comet ephemeris data to calculate the arrays needed for cubic
  spline interpolation of the sampled values.
*/
{
  cometEphem *comet;

  comet = cometRoot;
  while (comet != NULL) {
    if (comet->valid) {
      int nEntries = comet->nEntries;

      spline(comet->tJD, comet->rA,     nEntries, &(comet->rA[nEntries])    );
      spline(comet->tJD, comet->dec,    nEntries, &(comet->dec[nEntries])   );
      spline(comet->tJD, comet->eLong,  nEntries, &(comet->eLong[nEntries]) );
      spline(comet->tJD, comet->eLat,   nEntries, &(comet->eLat[nEntries])  );
      spline(comet->tJD, comet->radius, nEntries, &(comet->radius[nEntries]));
      spline(comet->tJD, comet->mag,    nEntries, &(comet->mag[nEntries])   );
    }
    comet = comet->next;
  }
}

void readInCometEphemerides(char *dataDir)
/*
  Read in text ephemeris files produced by JPL Horizons program.   The files from
  Horizons are edited as follows:

  1) Comment lines are removed.
  2) The JPL Horizons name of the comet is added as the first line in the file,
     along with a more common name (optional) separated by a space.
  3) The individual lines of the horizon output are edited a bit to make the
     file more compact.   The fields are as follows:
     Character Range      Description
      0 ->  3           Year (2012, etc)
      5 ->  7           Month ("Jan", "Feb" etc)
      9 -> 10           Day of month
     12 -> 13           Hour (HH)
     15 -> 16           Minute (MM)
     18 -> 28           RA (HH MM SS.SS)
     30 -> 40           Dec (+DD MM SS.S)
     43 -> 47           Magnitude (MM.MM)
     49 -> 56           Ecliptic longitude DDD.DDDD
     58 -> 65           Ecliptic latitude  +DD.DDDD
     67 -> 79           radius (AU)
 */
{
  int eOT, theFile, nEntries;
  char *dirName, *fullFileName, inLine[133];
  ephemEntry *newEphemEntry, *lastEphemEntry = NULL;
  cometEphem *newCometEphem, *lastCometEphem = NULL;
  DIR *dp;
  struct dirent *ep;

  dirName = malloc(strlen(dataDir)+strlen("/comets/")+1);
  if (dirName == NULL) {
    perror("comet directory name");
    return;
  }
  sprintf(dirName, "%s/comets/", dataDir);
  dp = opendir(dirName);
  if (dp == NULL) {
    perror("Opening comets directory");
    return;
  }
  while ((ep = readdir(dp)) != NULL) {
    if (strstr(ep->d_name, ".comet")) {
      fullFileName = malloc(strlen(dirName)+strlen(ep->d_name)+1);
      if (fullFileName != NULL) {
	sprintf(fullFileName,"%s%s", dirName, ep->d_name);
	theFile = open(fullFileName, O_RDONLY);
	free(fullFileName);
	if (theFile >= 0) {
	  eOT = FALSE;
	  getLine(theFile, &inLine[0], &eOT);
	  if ((!eOT) && (strlen(inLine) > 0)) {
	    char *token;

	    newCometEphem = (cometEphem *)malloc(sizeof(cometEphem));
	    if (newCometEphem == NULL) {
	      perror("newCometEphem");
	      exit(ERROR_EXIT);
	    }
	    token = strtok(inLine, " ");
	    newCometEphem->name = malloc(strlen(token)+1);
	    if (newCometEphem->name == NULL) {
	      perror("newCometEphem->name");
	      exit(ERROR_EXIT);
	    }
	    strcpy(newCometEphem->name, token);
	    token = strtok(NULL, " ");
	    if (token) {
	      newCometEphem->nickName = malloc(strlen(token)+1);
	      if (newCometEphem->nickName == NULL) {
		perror("newCometEphem->nickName");
		exit(ERROR_EXIT);
	      }
	      strcpy(newCometEphem->nickName, token);
	    } else
	      newCometEphem->nickName = NULL;
	    newCometEphem->valid = TRUE;
	    newCometEphem->firstTJD = newCometEphem->firstTJD = 0.0;
	    newCometEphem->next = NULL;
	    nEntries = 0;
	    while ((!eOT) && (newCometEphem->valid)) {
	      getLine(theFile, &inLine[0], &eOT);
	      if (!eOT) {
		if (strlen(inLine) >= 79) {
		  int year, month, day, nRead, hH, mM;
		  double mag, rA = 0.0, dec = 0.0, eLong, eLat, radius, tJD, sS;
		  char token[20];

		  strncpy(token, inLine, 4);
		  token[4] = (char)0;
		  nRead = sscanf(token, "%d", &year);
		  if (nRead != 1) {
		    fprintf(stderr, "(1) nRead != 1 (%d)\n", nRead);
		    newCometEphem->valid = FALSE;
		  }
		  strncpy(token, &inLine[5], 3);
		  token[3] = (char)0;
		  month = 0;
		  while (strcmp(token, monthAbrs[month++]));
		  strncpy(token, &inLine[9], 2);
		  token[2] = (char)0;
		  nRead = sscanf(token, "%d", &day);
		  if (nRead != 1) {
		    fprintf(stderr, "(2) nRead != 1 (%d)\n", nRead);
		    newCometEphem->valid = FALSE;
		  }
		  strncpy(token, &inLine[12], 2);
		  token[2] = (char)0;
		  nRead = sscanf(token, "%d", &hH);
		  if (nRead != 1) {
		    fprintf(stderr, "(3) nRead != 1 (%d)\n", nRead);
		    newCometEphem->valid = FALSE;
		  }
		  strncpy(token, &inLine[15], 2);
		  token[2] = (char)0;
		  nRead = sscanf(token, "%d", &mM);
		  if (nRead != 1) {
		    fprintf(stderr, "(4) nRead != 1 (%d)\n", nRead);
		    newCometEphem->valid = FALSE;
		  }
		  tJD = buildTJD(year-1900, month-1, day, hH, mM, 0, 0);
		  nRead = sscanf(&inLine[18], "%d %d %lf", &hH, &mM, &sS);
		  if (nRead == 3)
		    rA = ((double)hH + ((double)mM)/60.0 + sS/3600)*HOURS_TO_RADIANS;
		  else {
		    fprintf(stderr, "(5) nRead != 1 (%d)\n", nRead);
		    newCometEphem->valid = FALSE;
		  }
		  nRead = sscanf(&inLine[30], "%d %d %lf", &hH, &mM, &sS);
		  if (nRead == 3) {
		    if (inLine[34] == '-') {
		      mM *= -1;
		      sS *= -1.0;
		    }
		    dec = ((double)hH + ((double)mM)/60.0 + sS/3600)*DEGREES_TO_RADIANS;
		  } else {
		    fprintf(stderr, "(6) nRead != 1 (%d)\n", nRead);
		    newCometEphem->valid = FALSE;
		  }
		  nRead = sscanf(&inLine[43], "%lf", &mag);
		  if (nRead != 1) {
		    fprintf(stderr, "(7) nRead != 1 (%d)\n", nRead);
		    newCometEphem->valid = FALSE;
		  }
		  nRead = sscanf(&inLine[49], "%lf", &eLong);
		  if (nRead != 1) {
		    fprintf(stderr, "(8) nRead != 1 (%d)\n", nRead);
		    newCometEphem->valid = FALSE;
		  }
		  nRead = sscanf(&inLine[58], "%lf", &eLat);
		  if (nRead != 1) {
		    fprintf(stderr, "(9) nRead != 1 (%d)\n", nRead);
		    newCometEphem->valid = FALSE;
		  }
		  nRead = sscanf(&inLine[67], "%lf", &radius);
		  if (nRead != 1) {
		    fprintf(stderr, "(10) nRead != 1 (%d)\n", nRead);
		    newCometEphem->valid = FALSE;
		  }
		  if (newCometEphem->valid) {
		    newEphemEntry = (ephemEntry *)malloc(sizeof(ephemEntry));
		    if (newEphemEntry == NULL) {
		      perror("newEphemEntry");
		      newCometEphem->valid = FALSE;
		    } else {
		      newEphemEntry->tJD    = tJD;
		      newEphemEntry->rA     = rA;
		      newEphemEntry->dec    = dec;
		      newEphemEntry->eLong  = eLong;
		      newEphemEntry->eLat   = eLat;
		      newEphemEntry->radius = radius;
		      newEphemEntry->mag    = mag;
		      newEphemEntry->next   = NULL;
		      if (ephemRoot == NULL) {
			newCometEphem->firstTJD = tJD;
			ephemRoot = newEphemEntry;
		      } else
			lastEphemEntry->next = newEphemEntry;
		      newCometEphem->lastTJD = tJD;
		      lastEphemEntry = newEphemEntry;
		      nEntries++;
		    }
		  }
		} else {
		  fprintf(stderr, "Comet string too short (%d)\n", strlen(inLine));
		  newCometEphem->valid = FALSE;
		}
	      }
	    }
	    if (newCometEphem->valid) {
	      /*
		We've built a linked list pointed to by ephemRoot which has all the
		epehmeris entries in a linked list.   Now read all the entries in that
		list, and transfer the data to arrays, while freeing the linked list
		memory.
	      */
	      int firstEntry;
	      int element = 0;
	      double unwrap, lastELong;
	      ephemEntry *current, *next;

	      newCometEphem->nEntries = nEntries; /* Keep track of the size of the arrays */
	      newCometEphem->tJD = (double *)malloc(2*nEntries*sizeof(double));
	      if (newCometEphem->tJD == NULL) {
		perror("newCometEphem->tJD");
		exit(ERROR_EXIT);
	      }
	      newCometEphem->rA = (double *)malloc(2*nEntries*sizeof(double));
	      if (newCometEphem->rA == NULL) {
		perror("newCometEphem->rA");
		exit(ERROR_EXIT);
	      }
	      newCometEphem->dec = (double *)malloc(2*nEntries*sizeof(double));
	      if (newCometEphem->dec == NULL) {
		perror("newCometEphem->dec");
		exit(ERROR_EXIT);
	      }
	      newCometEphem->eLong = (double *)malloc(2*nEntries*sizeof(double));
	      if (newCometEphem->eLong == NULL) {
		perror("newCometEphem->eLong");
		exit(ERROR_EXIT);
	      }
	      newCometEphem->eLat = (double *)malloc(2*nEntries*sizeof(double));
	      if (newCometEphem->eLat == NULL) {
		perror("newCometEphem->eLat");
		exit(ERROR_EXIT);
	      }
	      newCometEphem->radius = (double *)malloc(2*nEntries*sizeof(double));
	      if (newCometEphem->radius == NULL) {
		perror("newCometEphem->radius");
		exit(ERROR_EXIT);
	      }
	      newCometEphem->mag = (double *)malloc(2*nEntries*sizeof(double));
	      if (newCometEphem->mag == NULL) {
		perror("newCometEphem->mag");
		exit(ERROR_EXIT);
	      }
	      current = ephemRoot;
	      unwrap = 0.0;
	      firstEntry = TRUE;
	      while (current != NULL) {
		newCometEphem->tJD[element]    = current->tJD;
		newCometEphem->rA[element]     = current->rA;
		newCometEphem->dec[element]    = current->dec;
		if (!firstEntry) {
		  /*
		    Handle unwrapping - the Heliocentric longitude may abruptly go from a number like
		    359 degrees to 2 degrees on the next entry.   Since these values will be interpolated,
		    the interpolation would fail if that happened.   So in a transition like that, we
		    use the variable "unwrap" to make the transition 359->362 instead, which interpolates
		    smoothly.   Since the eliocentric longitude is only used as a argument to Trig.
		    functions, extra 360 degree offsets won't matter.
		   */
		  if ((lastELong - current->eLong) > 180.0)
		    unwrap += 360.0;
		  else if ((lastELong - current->eLong) < -180.0)
		    unwrap -= 360.0;
		}
		newCometEphem->eLong[element]  = current->eLong + unwrap;
		lastELong = current->eLong;
		newCometEphem->eLat[element]   = current->eLat;
		newCometEphem->radius[element] = current->radius;
		newCometEphem->mag[element++]  = current->mag;
		next = current->next;
		free(current);
		firstEntry = FALSE;
		current = next;
	      }
	      ephemRoot = NULL;
	    }
	    if (cometRoot == NULL)
	      cometRoot = newCometEphem;
	    else
	      lastCometEphem->next = newCometEphem;
	    lastCometEphem = newCometEphem;
	  }
	  close(theFile);
	}
      }
    }
  }
  free(dirName);
  closedir(dp);
  calculateCometSplineData();
  cometDataReadIn = TRUE;
}

/*
  Return the coordinates of the comet "name" for time TJD, if possible.   If successful, return TRUE,
  otherwise return FALSE.
 */
int getCometRADec(char *dataDir, char *name, double tJD, int equatorial,
		  double *coord1, double *coord2, double *coord3, double *mag)
{
  cometEphem *comet;

  if (!cometDataReadIn) {
    readInCometEphemerides(dataDir);
  }
  comet = cometRoot;
  while (comet != NULL) {
    if (!strcmp(name, comet->name) && comet->valid
	&& (comet->firstTJD <= tJD) && (comet->lastTJD >= tJD)) {
      int nEntries = comet->nEntries;

      if (equatorial) {
	*coord1 = splint(comet->tJD, comet->rA,  &(comet->rA[nEntries]),  nEntries, tJD);
	*coord2 = splint(comet->tJD, comet->dec, &(comet->dec[nEntries]), nEntries, tJD);
      } else {
	*coord1 = splint(comet->tJD, comet->eLong,  &(comet->eLong[nEntries]),  nEntries, tJD);
	*coord2 = splint(comet->tJD, comet->eLat,   &(comet->eLat[nEntries]),   nEntries, tJD);
	*coord3 = splint(comet->tJD, comet->radius, &(comet->radius[nEntries]), nEntries, tJD);
      }
      if (mag != NULL)
	*mag = splint(comet->tJD, comet->mag, &(comet->mag[nEntries]), nEntries, tJD);
      return(TRUE);
    }
    comet = comet->next;
  }
  return(FALSE);
}

/*
  Read in the Keplerian Elements for Solar System Objects from files.
*/
void readInElements(char *dataDir)
{
  int nRead, i;
  char fileName[100];
  FILE *fileHandle;

  for (i = 0; i < N_PLANETS; i++) {
    sprintf(fileName, "%s/orbitalElements/%sElements", dataDir, planetNames[i]);
    dprintf("Reading elements from \"%s\"\n", fileName);
    fileHandle = fopen(fileName, "r");
    if (fileName == NULL) {
      perror(fileName);
      exit(ERROR_EXIT);
    }
    nRead = fscanf(fileHandle, "%lf %lf %lf %lf %lf %lf",
		   &(element[i].a[0]), &(element[i].e[0]), &(element[i].I[0]),
		   &(element[i].L[0]), &(element[i].o[0]), &(element[i].Om[0]));
    if (nRead != 6) {
      fprintf(stderr, "Wrong number (%d) of numbers found on line 1 of %s\n",
	      nRead, fileName);
      exit(ERROR_EXIT);
    }
    nRead = fscanf(fileHandle, "%lf %lf %lf %lf %lf %lf",
		   &(element[i].dadt[0]), &(element[i].dedt[0]), &(element[i].dIdt[0]),
		   &(element[i].dLdt[0]), &(element[i].dodt[0]), &(element[i].dOmdt[0]));
    if (nRead != 6) {
      fprintf(stderr, "Wrong number (%d) of numbers found on line 2 of %s\n",
	      nRead, fileName);
      exit(ERROR_EXIT);
    }
    nRead = fscanf(fileHandle, "%lf %lf %lf %lf %lf %lf",
		   &(element[i].a[1]), &(element[i].e[1]), &(element[i].I[1]),
		   &(element[i].L[1]), &(element[i].o[1]), &(element[i].Om[1]));
    if (nRead != 6) {
      fprintf(stderr, "Wrong number (%d) of numbers found on line 3 of %s\n",
	      nRead, fileName);
      exit(ERROR_EXIT);
    }
    nRead = fscanf(fileHandle, "%lf %lf %lf %lf %lf %lf",
		   &(element[i].dadt[1]), &(element[i].dedt[1]), &(element[i].dIdt[1]),
		   &(element[i].dLdt[1]), &(element[i].dodt[1]), &(element[i].dOmdt[1]));
    if (nRead != 6) {
      fprintf(stderr, "Wrong number (%d) of numbers found on line 4 of %s\n",
	      nRead, fileName);
      exit(ERROR_EXIT);
    }
    nRead = fscanf(fileHandle, "%lf %lf %lf %lf",
		   &(element[i].b), &(element[i].c), &(element[i]).s, &(element[i].f));
    if (nRead != 4) {
      fprintf(stderr, "Wrong number (%d) of numbers found on line 5 of %s\n",
	      nRead, fileName);
      exit(ERROR_EXIT);
    }
    fclose(fileHandle);
  }
}

double getTJD(int year, int month, int day, double hour)
{
  int y, m, a, b, c, dd;
  double d;
  double tJD;

  y = year;
  m = month;
  d = day + hour/24.0;
  if (m < 3) {
    y -= 1;
    m += 12;
  }
  a = (int)(y/100);
  b = 2 - a + a/4;
  c = (int)(365.25*(double)y);
  dd = (int)(30.6001*(double)(m+1));
  tJD = (double)(b+c+dd) + d + 1720994.5;
  return(tJD);
}

/*
  Calculate a planet's position using the method described in reference 1.
*/
void calculatePlanetPosition(int planet, double tJD,
			     double *eX, double *eY, double *eZ)
{
  int iM, indx, farTime, planetIndex;
  double T, a, e, I, L, o, Om, omega, M, E;
  double xp, yp;

  if (planet < MARS)
    planetIndex = planet-1;
  else
    planetIndex = planet-2;
  if ((tJD < TJD_PRE_1800) || (tJD > TJD_POST_2050)) {
    indx = 1;
    farTime = TRUE;
  } else {
    indx = 0;
    farTime = FALSE;
  }
  T  = (tJD - 2451545.0)/36525.0;
  a  = element[planetIndex].a[indx]  + element[planetIndex].dadt[indx]*T;
  e  = element[planetIndex].e[indx]  + element[planetIndex].dedt[indx]*T;
  I  = element[planetIndex].I[indx]  + element[planetIndex].dIdt[indx]*T;
  L  = element[planetIndex].L[indx]  + element[planetIndex].dLdt[indx]*T;
  o  = element[planetIndex].o[indx]  + element[planetIndex].dodt[indx]*T;
  Om = element[planetIndex].Om[indx] + element[planetIndex].dOmdt[indx]*T;
  omega = o - Om;
  if (farTime) {
    M = L - o + element[planetIndex].b*T*T +
      element[planetIndex].c*cos(element[planetIndex].f*T) +
      element[planetIndex].s*sin(element[planetIndex].f*T);
  } else
    M = L - o;
  dprintf("Pre M = %f\t", M);
  iM = (int)(M/360.0);
  M -= (double)(iM*360);
  while (M > 180.0)
    M -= 360.0;
  while (M < -180.0)
    M += 360.0;
  dprintf("post M = %f\n", M);
  E = kepler(e, M);
  xp = a*(cos(E*DEGREES_TO_RADIANS) - e);
  yp = a*sqrt(1.0 - e*e)*sin(E*DEGREES_TO_RADIANS);
  dprintf("M = %f, E = %f, xp = %f, yp = %f\n", M, E, xp, yp);
  *eX = (cos(omega*DEGREES_TO_RADIANS)*cos(Om*DEGREES_TO_RADIANS) -
	       sin(omega*DEGREES_TO_RADIANS)*sin(Om*DEGREES_TO_RADIANS)*cos(I*DEGREES_TO_RADIANS))*xp
    + (-sin(omega*DEGREES_TO_RADIANS)*cos(Om*DEGREES_TO_RADIANS)
       - cos(omega*DEGREES_TO_RADIANS)*sin(Om*DEGREES_TO_RADIANS)*cos(I*DEGREES_TO_RADIANS))*yp;
  *eY = (cos(omega*DEGREES_TO_RADIANS)*sin(Om*DEGREES_TO_RADIANS) +
		sin(omega*DEGREES_TO_RADIANS)*cos(Om*DEGREES_TO_RADIANS)*cos(I*DEGREES_TO_RADIANS))*xp
    + (-sin(omega*DEGREES_TO_RADIANS)*sin(Om*DEGREES_TO_RADIANS)
       + cos(omega*DEGREES_TO_RADIANS)*cos(Om*DEGREES_TO_RADIANS)*cos(I*DEGREES_TO_RADIANS))*yp;
  *eZ = sin(omega*DEGREES_TO_RADIANS)*sin(I*DEGREES_TO_RADIANS)*xp
    + cos(omega*DEGREES_TO_RADIANS)*sin(I*DEGREES_TO_RADIANS)*yp;
  dprintf("xecl = %f. yecl = %f, zecl = %f\n", *eX, *eY, *eZ);
}

void planetInfo(char *dataDir, int planetNumber, double tJD, double *rA, double *dec, float *F, float *mag)
{
  static int firstCall = TRUE;
  static double earthEx, earthEy, earthEz, sunELong;
  double ex, ey, ez, r, eLong, eLat, planetEx, planetEy, planetEz, rPlanet, dDummy;

  if (firstCall) {
    readInElements(dataDir);
    firstCall = FALSE;
  }

  switch (planetNumber) {
  case MERCURY:
  case VENUS:
  case MARS:
  case JUPITER:
  case SATURN:
  case URANUS:
  case NEPTUNE:
    calculatePlanetPosition(planetNumber, tJD, &planetEx, &planetEy, &planetEz);
    rPlanet = sqrt(planetEx*planetEx + planetEy*planetEy +planetEz*planetEz);
    ex = planetEx-earthEx;
    ey = planetEy-earthEy;
    ez = planetEz-earthEz;
    r = sqrt(ex*ex + ey*ey + ez*ez);
    *F = phase(atan2(planetEy, planetEx), atan2(ey, ex));
    *mag = 5.0*log10(r*rPlanet/sqrt(*F)) + V0[planetNumber];
    if (r > 0.0) {
      ex /= r;
      ey /= r;
      ez /= r;
    }
    eLat = asin(ez);
    eLong = atan2(ey, ex);
    dprintf("Distance = %f AU, Ecl Lat = %f, long = %f\n", r, eLat/DEGREES_TO_RADIANS,
	    eLong/DEGREES_TO_RADIANS);
    eclipticToJ2000(eLat, eLong, rA, dec);
    if (*rA < 0.0)
      *rA += TWO_PI;
    break;

  case EARTH:
    calculatePlanetPosition(EARTH, tJD, &earthEx, &earthEy, &earthEz);
    ex = -earthEx;
    ey = -earthEy;
    ez = -earthEz;
    r = sqrt(ex*ex + ey*ey + ez*ez);
    if (r > 0.0) {
      ex /= r;
      ey /= r;
      ez /= r;
    }
    eLat = asin(ez);
    sunELong = atan2(ey, ex);
    eclipticToJ2000(eLat, sunELong, rA, dec);
    if (sunELong < 0.0)
      sunELong += TWO_PI;
    if (*rA < 0.0)
      *rA += TWO_PI;
    *F = 1.0;
    *mag = -26.8;
    break;

  case MOON:
    getMoonPosition(tJD, sunELong, rA, dec, &dDummy, &dDummy, F);
    if (*rA < 0.0)
      *rA += TWO_PI;
    *mag = -12.0;
    break;
  default:
    fprintf(stderr, "Illegal planet number (%d) passed to planetInfo\n",
	    planetNumber);
    exit(ERROR_EXIT);
  }
}

void getCurrentOrbitalElements(int planet,         /* Planet we need elements for                  */
			       double tJD,         /* Time for which elements should be calculated */
			       double *a,          /* Semimajor axis                               */
			       double *e,          /* Eccentricity                                 */
			       double *I,          /* Inclination                                  */
			       double *L,          /* Mean longitude                               */
			       double *smallOmega, /* Longitude of perihelion                      */
			       double *bigOmega    /* Longitude of ascending node                  */
			       )
{
  int planetIndex, indx;
  double T;

  if (planet < MARS)
    planetIndex = planet-1;
  else
    planetIndex = planet-2;
  if ((tJD < TJD_PRE_1800) || (tJD > TJD_POST_2050))
    indx = 1;
    else
    indx = 0;
  T            = (tJD - 2451545.0)/36525.0;
  *a           = element[planetIndex].a[indx]  + element[planetIndex].dadt[indx]*T;
  *e           = element[planetIndex].e[indx]  + element[planetIndex].dedt[indx]*T;
  *I           = element[planetIndex].I[indx]  + element[planetIndex].dIdt[indx]*T;
  *L           = element[planetIndex].L[indx]  + element[planetIndex].dLdt[indx]*T;
  *smallOmega  = element[planetIndex].o[indx]  + element[planetIndex].dodt[indx]*T;
  *bigOmega    = element[planetIndex].Om[indx] + element[planetIndex].dOmdt[indx]*T;
}
