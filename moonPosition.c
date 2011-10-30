/*
  This module contains the code to calculate the moon's position.   It closely
  follows the method presented in Chapter 47 of Jean Meeus' book
  Astronomical Algorythms, second edition.   The code is un commented because
  the book describes the calculations, and the names of the variables
  I use match those i the book.
 */
#include <stdio.h>
#include <math.h>

#define M_2PI (2.0*M_PI)
#define DEGREES_TO_RADIANS (M_PI/180)

char argMultlr[60][4] = {{ 0, 0, 1, 0},
			 { 2, 0,-1, 0},
			 { 2, 0, 0, 0},
			 { 0, 0, 2, 0},
			 { 0, 1, 0, 0},
			 { 0, 0, 0, 2},
			 { 2, 0,-2, 0},
			 { 2,-1,-1, 0},
			 { 2, 0, 1, 0},
			 { 2,-1, 0, 0},
			 { 0, 1,-1, 0},
			 { 1, 0, 0, 0},
			 { 0, 1, 1, 0},
			 { 2, 0, 0,-2},
			 { 0, 0, 1, 2},
			 { 0, 0, 1,-2},
			 { 4, 0,-1, 0},
			 { 0, 0, 3, 0},
			 { 4, 0,-2, 0},
			 { 2, 1,-1, 0},
			 { 2, 1, 0, 0},
			 { 1, 0,-1, 0},
			 { 1, 1, 0, 0},
			 { 2,-1, 1, 0},
			 { 2, 0, 2, 0},
			 { 4, 0, 0, 0},
			 { 2, 0,-3, 0},
			 { 0, 1,-2, 0},
			 { 2, 0,-1, 2},
			 { 2,-1,-2, 0},
			 { 1, 0, 1, 0},
			 { 2,-2, 0, 0},
			 { 0, 1, 2, 0},
			 { 0, 2, 0, 0},
			 { 2,-2,-1, 0},
			 { 2, 0, 1,-2},
			 { 2, 0, 0, 2},
			 { 4,-1,-1, 0},
			 { 0, 0, 2, 2},
			 { 3, 0,-1, 0},
			 { 2, 1, 1, 0},
			 { 4,-1,-2, 0},
			 { 0, 2,-1, 0},
			 { 2, 2,-1, 0},
			 { 2, 1,-2, 0},
			 { 2,-1, 0,-2},
			 { 4, 0, 1, 0},
			 { 0, 0, 4, 0},
			 { 4,-1, 0, 0},
			 { 1, 0,-2, 0},
			 { 2, 1, 0,-2},
			 { 0, 0, 2,-2},
			 { 1, 1, 1, 0},
			 { 3, 0,-2, 0},
			 { 4, 0,-3, 0},
			 { 2,-1, 2, 0},
			 { 0, 2, 1, 0},
			 { 1, 1,-1, 0},
			 { 2, 0, 3, 0},
                         { 2, 0,-1,-2}};
int SigmalCoef[60] = {6288774,
		      1274027,
		       658314,
		       213618,
		      -185116,
		      -114332,
		        58793,
		        57066,
		        53322,
		        45758,
		       -40923,
		       -34720,
		       -30383,
		        15327,
		       -12528,
		        10980,
		        10675,
		        10034,
		         8548,
		        -7888,
		        -6766,
		        -5163,
		         4987,
		         4036,
		         3994,
		         3861,
		         3665,
		        -2689,
		        -2602,
		         2390,
		        -2348,
		         2236,
		        -2120,
		        -2069,
		         2048,
		        -1773,
		        -1595,
		         1215,
		        -1110,
		         -892,
		         -810,
		          759,
		         -713,
		         -700,
		          691,
		          596,
		          549,
		          537,
		          520,
		         -487,
		         -399,
		         -381,
		          351,
		         -340,
		          330,
		          327,
		         -323,
		          299,
		          294,
		           0};
int SigmarCoef[60] = {-20905355,
		       -3699111,
		       -2955968,
		        -569925,
		          48888,
		          -3149,
		         246158,
		        -152138,
		        -170733,
		        -204586,
		        -129620,
		         108743,
		         104755,
		          10321,
		              0,
		          79661,
		         -34782,
		         -23210,
		         -21636,
		          24208,
		          30824,
		          -8379,
		         -16675,
		         -12831,
		         -10445,
		         -11650,
		          14403,
		          -7003,
		              0,
		          10056,
		           6322,
		          -9884,
		           5751,
		              0,
		          -4950,
		           4130,
		              0,
		          -3958,
		              0,
		           3258,
		           2616,
		          -1897,
		          -2117,
		           2354,
		              0,
		              0,
		          -1423,
		          -1117,
		          -1571,
		          -1739,
		              0,
		          -4421,
		              0,
		              0,
		              0,
		              0,
		           1165,
		              0,
		              0,
		           8752};
char argMultb[60][4] = {{ 0, 0, 0, 1},
			{ 0, 0, 1, 1},
			{ 0, 0, 1,-1},
			{ 2, 0, 0,-1},
			{ 2, 0,-1, 1},
			{ 2, 0,-1,-1},
			{ 2, 0, 0, 1},
			{ 0, 0, 2, 1},
			{ 2, 0, 1,-1},
			{ 0, 0, 2,-1},
			{ 2,-1, 0,-1},
			{ 2, 0,-2,-1},
			{ 2, 0, 1, 1},
			{ 2, 1, 0,-1},
			{ 2,-1,-1, 1},
			{ 2,-1, 0, 1},
			{ 2,-1,-1,-1},
			{ 0, 1,-1,-1},
			{ 4, 0,-1,-1},
			{ 0, 1, 0, 1},
			{ 0, 0, 0, 3},
			{ 0, 1,-1, 1},
			{ 1, 0, 0, 1},
			{ 0, 1, 1, 1},
			{ 0, 1, 1,-1},
			{ 0, 1, 0,-1},
			{ 1, 0, 0,-1},
			{ 0, 0, 3, 1},
			{ 4, 0, 0,-1},
			{ 4, 0,-1, 1},
			{ 0, 0, 1,-3},
			{ 4, 0,-2, 1},
			{ 2, 0, 0,-3},
			{ 2, 0, 2,-1},
			{ 2,-1, 1,-1},
			{ 2, 0,-2, 1},
			{ 0, 0, 3,-1},
			{ 2, 0, 2, 1},
			{ 2, 0,-3,-1},
			{ 2, 1,-1, 1},
			{ 2, 1, 0, 1},
			{ 4, 0, 0, 1},
			{ 2,-1, 1, 1},
			{ 2,-2, 0,-1},
			{ 0, 0, 1, 3},
			{ 2, 1, 1,-1},
			{ 1, 1, 0,-1},
			{ 1, 1, 0, 1},
			{ 0, 1,-2,-1},
			{ 2, 1,-1,-1},
			{ 1, 0, 1, 1},
			{ 2,-1,-2,-1},
			{ 0, 1, 2, 1},
			{ 4, 0,-2,-1},
			{ 4,-1,-1,-1},
			{ 1, 0, 1,-1},
			{ 4, 0, 1,-1},
			{ 1, 0,-1,-1},
			{ 4,-1, 0,-1},
			{ 2,-2, 0, 1}};
int SigmabCoef[60] = {5128122,
		       280602,
		       277693,
		       173237,
		        55413,
		        46271,
		        32573,
		        17198,
		         9266,
		         8822,
		         8216,
		         4324,
		         4200,
		        -3359,
		         2463,
		         2211,
		         2065,
		        -1870,
		         1828,
		        -1794,
		        -1749,
		        -1565,
		        -1491,
		        -1475,
		        -1410,
		        -1344,
		        -1335,
		         1107,
		         1021,
		          833,
		          777,
		          671,
		          607,
		          596,
		          491,
		         -451,
		          439,
		          422,
		          421,
		         -366,
		         -351,
		          331,
		          315,
		          302,
		         -283,
		         -229,
		          223,
		          223,
		         -220,
		         -220,
		         -185,
		          181,
		         -177,
		          176,
		          166,
		         -164,
		          132,
		         -119,
		          115,
		          107};

void doubleNormalize0to360(double *a);
void nutation(double T, double *deltaPhi, double *deltaEps, double *eps);

void moonPosition(double jDE, double sunELong, double *rA, double *dec,
		     double *eLong, double *eLat, double *distance, float *Fr)
{
  int i;
  double T, Lprime, D, M, Mprime, F, A1, A2, A3, Sigmal, Sigmar, Sigmab,
    E, alpha, dD, dM, dMprime, dF, lambda, beta, delta, deltaPhi, deltaEps,
    eps, pi;

  T = (jDE - 2451545.0)/36525.0;
  nutation(T, &deltaPhi, &deltaEps, &eps);
  Lprime = 218.3164477
    +   481267.88123421*T
    -        0.00157860*T*T
    +                   T*T*T/538841.0
    -                   T*T*T*T/65194000.0;
  doubleNormalize0to360(&Lprime);
  D =    297.8501921
    + 445267.1114034*T
    -      0.0018819*T*T
    +                T*T*T/545868.0
    -                T*T*T*T/113065000.0;
  doubleNormalize0to360(&D);
  M =   357.5291092
    + 35999.0502909*T
    -     0.0001536*T*T
    +               T*T*T/24490000.0;

  doubleNormalize0to360(&M);
  Mprime = 134.9633964
    +   477198.8675055*T
    +        0.0087414*T*T
    +                  T*T*T/69699.0
    -                  T*T*T*T/14712000.0;
  doubleNormalize0to360(&Mprime);
  F =     93.2720950
    + 483202.0175233*T
    -      0.0036539*T*T
    -                T*T*T/3526000.0
    -                T*T*T*T/863310000.0;
  doubleNormalize0to360(&F);
  A1 = 119.75 +    131.849*T;
  doubleNormalize0to360(&A1);
  A2 =  53.09 + 479264.290*T;
  doubleNormalize0to360(&A2);
  A3 = 313.45 + 481266.484*T;
  doubleNormalize0to360(&A3);
  Sigmal = Sigmar = Sigmab = 0.0;
  E = 1.0 - 0.002516*T - 0.0000074*T*T;
  dD = D*DEGREES_TO_RADIANS;
  dM = M*DEGREES_TO_RADIANS;
  dMprime = Mprime*DEGREES_TO_RADIANS;
  dF = F*DEGREES_TO_RADIANS;
  for (i = 0; i < 60; i++) {
    if ((argMultlr[i][1] == 1) || (argMultlr[i][1] == -1))
      alpha = E;
    else if ((argMultlr[i][1] == 2) || (argMultlr[i][1] == -2))
      alpha = E*E;
    else
      alpha = 1;
    Sigmal += alpha*SigmalCoef[i]*sin(argMultlr[i][0]*dD
				    + argMultlr[i][1]*dM
				    + argMultlr[i][2]*dMprime
				    + argMultlr[i][3]*dF);
    Sigmar += alpha*SigmarCoef[i]*cos(argMultlr[i][0]*dD
				    + argMultlr[i][1]*dM
				    + argMultlr[i][2]*dMprime
				    + argMultlr[i][3]*dF);
    if ((argMultb[i][1] == 1) || (argMultb[i][1] == -1))
      alpha = E;
    else if ((argMultb[i][1] == 2) || (argMultb[i][1] == -2))
      alpha = E*E;
    else
      alpha = 1;
    Sigmab += alpha*SigmabCoef[i]*sin(argMultb[i][0]*dD
				    + argMultb[i][1]*dM
				    + argMultb[i][2]*dMprime
				    + argMultb[i][3]*dF);
  }
  Sigmal += 3958.0*sin(A1*DEGREES_TO_RADIANS)
          + 1962.0*sin((Lprime-F)*DEGREES_TO_RADIANS)
          +  318.0*sin(A2*DEGREES_TO_RADIANS);
  Sigmab += -2235.0*sin(Lprime*DEGREES_TO_RADIANS)
    + 382.0*sin(A3*DEGREES_TO_RADIANS)
    + 175.0*sin((A1-F)*DEGREES_TO_RADIANS)
    + 175.0*sin((A1+F)*DEGREES_TO_RADIANS)
    + 127.0*sin((Lprime-Mprime)*DEGREES_TO_RADIANS)
    - 115.0*sin((Lprime+Mprime)*DEGREES_TO_RADIANS);
  lambda = Lprime + Sigmal*1.0e-6 + deltaPhi/3600.0;
  doubleNormalize0to360(&lambda);
  beta   = Sigmab*1.0e-6;
  doubleNormalize0to360(&beta);
  delta = 385000.56 + Sigmar*1.0e-3;
  pi = asin(6378.14/delta)/DEGREES_TO_RADIANS;
  eps    *= DEGREES_TO_RADIANS;
  lambda *= DEGREES_TO_RADIANS;
  beta   *= DEGREES_TO_RADIANS;
  *eLong = lambda;
  *eLat = beta;
  *rA  = atan2(sin(lambda)*cos(eps) - tan(beta)*sin(eps), cos(lambda));
  doubleNormalize0to2pi(rA);
  *dec = asin(sin(beta)*cos(eps) + cos(beta)*sin(eps)*sin(lambda));
  *distance = delta;
}
