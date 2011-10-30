#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define DEGREES_TO_RADIANS (M_PI/180.0)

void doubleNormalize0to360(double *a);

double sinD(double angleInDegrees)
{
  return (sin(angleInDegrees*DEGREES_TO_RADIANS));
}

double cosD(double angleInDegrees)
{
  return (cos(angleInDegrees*DEGREES_TO_RADIANS));
}

double atanD(double argument)
{
  return (atan(argument)/DEGREES_TO_RADIANS);
}

double atan2D(double y, double x)
{
  return (atan2(y, x)/DEGREES_TO_RADIANS);
}

/*
  Calculate the positions of the four Galilean satellites using the "high accuracy"
  method given in "Astronomical Algorythms" (Jean Meeus, second edition, chapter 44).
 */
void highAccuracyJovSats(double jD, double deltaAU,
			 double lambda0, double beta0,
			 double *X, double *Y, double *Z)
{
  int i;
  double tau, t, l1, l2, l3, l4, psi, Phi,
    pi1, pi2, pi3, pi4, om1, om2, om3, om4, Gamma, PhiLam, Omega,
    G, Gprime, Pi, Sigma1, Sigma2, Sigma3, Sigma4, L[5], Te,
    tanB1, tanB2, tanB3, tanB4, B[5], rP1, rP2, rP3, rP4,
    orbI, b3[6], c3[6],
    RP[5], T0, P, I, T, tX[6], tY[6], tZ[6], sinPhi, cosPhi,
    cosI, sinI, a1[6], b1[6], c1[6], a2[6], b2[6], D, dT[5],
    sinOrbI, cosOrbI, sinOmega, cosOmega, a4[6], b4[6],
    sinLambda0, cosLambda0, a5[6], b5[6], b6[6], c6[6],
    sinBeta0, cosBeta0, sX[5], sY[5], sZ[5], cosDD, sinDD
    ;

  tau    = 0.0057755183*deltaAU; /* Light travel time from Jupiter */
  t      = jD - 2443000.5 - tau;
  l1     = 106.07719 + 203.488955790*t;
  l2     = 175.73161 + 101.374724735*t;
  l3     = 120.55883 +  50.317609207*t;
  l4     =  84.44459 +  21.571071177*t;
  pi1    =  97.0881  +   0.16138586 *t;
  pi2    = 154.8663  +   0.04726307 *t;
  pi3    = 188.1840  +   0.00712734 *t;
  pi4    = 335.2868  +   0.00184000 *t;
  om1    = 312.3346  -   0.13279386 *t;
  om2    = 100.4411  -   0.03263064 *t;
  om3    = 119.1942  -   0.00717703 *t;
  om4    = 322.6186  -   0.00175934 *t;
  Gamma  = 0.33033*sinD(163.679 + 0.0010512*t) + 0.03439*sinD(34.486 - 0.0161731*t);
  PhiLam = 199.6766  +   0.17379190 *t;
  psi    = 316.5182  -   0.00000208 *t;
  G      =  30.23756 +   0.0830925701*t + Gamma;
  Gprime =  31.97853 +   0.0334597339*t;
  Pi     =  13.469942;

  Sigma1 =   0.47259*sinD(2.0*(l1-l2))               - 0.00186*sinD(G)
           - 0.03478*sinD(pi3-pi4)                   + 0.00162*sinD(pi2-pi3)
           + 0.01081*sinD(l2-2.0*l3+pi3)             + 0.00158*sinD(4.0*(l1-l2))
           + 0.00738*sinD(PhiLam)                    - 0.00155*sinD(l1-l3)
           + 0.00713*sinD(l2-2.0*l3+pi2)             - 0.00138*sinD(psi+om3-2.0*Pi-2.0*G)
           - 0.00674*sinD(pi1+pi3-2.0*Pi-2.0*G)      - 0.00115*sinD(2.0*(l1-2.0*l2+om2))
           + 0.00666*sinD(l2-2.0*l3+pi4)             + 0.00089*sinD(pi2-pi4)
           + 0.00445*sinD(l1-pi3)                    + 0.00085*sinD(l1+pi3-2.0*Pi-2.0*G)
           - 0.00354*sinD(l1-l2)                     + 0.00083*sinD(om2-om3)
           - 0.00317*sinD(2.0*psi-2.0*Pi)            + 0.00053*sinD(psi-om2)
           + 0.00265*sinD(l1-pi4);
  Sigma2 =   1.06476*sinD(2.0*(l2-l3))               - 0.00115*sinD(l1-2.0*l3+pi3)
           + 0.04256*sinD(l1-2.0*l2+pi3)             - 0.00094*sinD(2.0*(l2-om2))
           + 0.03581*sinD(l2-pi3)                    + 0.00086*sinD(2.0*(l1-2.0*l2+om2))
           + 0.02395*sinD(l1-2.0*l2+pi4)             - 0.00086*sinD(5.0*Gprime-2.0*G+52.225)
           + 0.01984*sinD(l2-pi4)                    - 0.00078*sinD(l2-l4)
           - 0.01778*sinD(PhiLam)                    - 0.00064*sinD(3.0*l3-7.0*l4+4.0*pi4)
           + 0.01654*sinD(l2-pi2)                    + 0.00064*sinD(pi1-pi4)
           + 0.01334*sinD(l2-2.0*l3+pi2)             - 0.00063*sinD(l1-2.0*l3+pi4)
           + 0.01294*sinD(pi3-pi4)                   + 0.00058*sinD(om3-om4)
           - 0.01142*sinD(l2-l3)                     + 0.00056*sinD(2.0*(psi-Pi-G))
           - 0.01057*sinD(G)                         + 0.00056*sinD(2.0*(l2-l4))
           - 0.00775*sinD(2.0*(psi-Pi))              + 0.00055*sinD(2.0*(l1-l3))
           + 0.00524*sinD(2.0*(l1-l2))               + 0.00052*sinD(3.0*l3-7.0*l4+pi3+3.0*pi4)
           - 0.00460*sinD(l1-l3)                     - 0.00043*sinD(l1-pi3)
           + 0.00316*sinD(psi-2.0*G+om3-2.0*Pi)      + 0.00041*sinD(5.0*(l2-l3))
           - 0.00203*sinD(pi1+pi3-2.0*Pi-2.0*G)      + 0.00041*sinD(pi4-Pi)
           + 0.00146*sinD(psi-om3)                   + 0.00032*sinD(om2-om3)
           - 0.00145*sinD(2.0*G)                     + 0.00032*sinD(2.0*(l3-G-Pi))
           + 0.00125*sinD(psi-om4);
  Sigma3 =   0.16490*sinD(l3-pi3)                    + 0.00091*sinD(om3-om4)
           + 0.09081*sinD(l3-pi4)                    + 0.00080*sinD(3.0*l3-7.0*l4+pi3+3.0*pi4)
           - 0.06907*sinD(l2-l3)                     - 0.00075*sinD(2.0*l2-3.0*l3+pi3)
           + 0.03784*sinD(pi3-pi4)                   + 0.00072*sinD(pi1+pi3-2.0*Pi-2.0*G)
           + 0.01846*sinD(2.0*(l3-l4))               + 0.00069*sinD(pi4-Pi)
           - 0.01340*sinD(G)                         - 0.00058*sinD(2.0*l3-3.0*l4+pi4)
           - 0.01014*sinD(2.0*(psi-Pi))              - 0.00057*sinD(l3-2.0*l4+pi4)
           + 0.00704*sinD(l2-2.0*l3+pi3)             + 0.00056*sinD(l3+pi3-2.0*Pi-2.0*G)
           - 0.00620*sinD(l2-2.0*l3+pi2)             - 0.00052*sinD(l2-2.0*l3+pi1)
           - 0.00541*sinD(l3-l4)                     - 0.00050*sinD(pi2-pi3)
           + 0.00381*sinD(l2-2.0*l3+pi4)             + 0.00048*sinD(l3-2.0*l4+pi3)
           + 0.00235*sinD(psi-om3)                   - 0.00045*sinD(2.0*l2-3.0*l3+pi4)
           + 0.00198*sinD(psi-om4)                   - 0.00041*sinD(pi2-pi4)
           + 0.00176*sinD(PhiLam)                    - 0.00038*sinD(2.0*G)
           + 0.00130*sinD(3.0*(l3-l4))               - 0.00037*sinD(pi3-pi4+om3-om4)
           + 0.00125*sinD(l1-l3)                     - 0.00032*sinD(3.0*l3-7.0*l4+2.0*pi3+2.0*pi4)
           - 0.00119*sinD(5.0*Gprime-2.0*G+52.225)   + 0.00030*sinD(4.0*(l3-l4))
           + 0.00109*sinD(l1-l2)                     + 0.00029*sinD(l3+pi4-2.0*Pi-2.0*G)
           - 0.00100*sinD(3.0*l3-7.0*l4+4.0*pi4)     - 0.00028*sinD(om3+psi-2.0*Pi-2.0*G)
           + 0.00026*sinD(l3-Pi-G)                   - 0.00021*sinD(l3-pi2)
           + 0.00024*sinD(l2-3.0*l3+2.0*l4)          + 0.00017*sinD(2.0*(l3-pi3))
           + 0.00021*sinD(2.0*(l3-Pi-G));
  Sigma4 =   0.84287*sinD(l4-pi4)                    + 0.00061*sinD(l1-l4)
           + 0.03431*sinD(pi4-pi3)                   - 0.00056*sinD(psi-om3)
           - 0.03305*sinD(2.0*(psi-Pi))              - 0.00054*sinD(l3-2.0*l4+pi3)
           - 0.03211*sinD(G)                         + 0.00051*sinD(l2-l4)
           - 0.01862*sinD(l4-pi3)                    + 0.00042*sinD(2.0*(psi-G-Pi))
           + 0.01186*sinD(psi-om4)                   + 0.00039*sinD(2.0*(pi4-om4))
           + 0.00623*sinD(l4+pi4-2.0*G-2.0*Pi)       + 0.00036*sinD(psi+Pi-pi4-om4)
           + 0.00387*sinD(2.0*(l4-pi4))              + 0.00035*sinD(2.0*Gprime-G+188.37)
           - 0.00284*sinD(5.0*Gprime-2.0*G+52.225)   - 0.00035*sinD(l4-pi4+2.0*Pi-2.0*psi)
           - 0.00234*sinD(2.0*(psi-pi4))             - 0.00032*sinD(l4+pi4-2.0*Pi-G)
           - 0.00223*sinD(l3-l4)                     + 0.00030*sinD(2.0*Gprime-2.0*G+149.15)
           - 0.00208*sinD(l4-Pi)                     + 0.00029*sinD(3.0*l3-7.0*l4+2.0*pi3+2.0*pi4)
           + 0.00178*sinD(psi+om4-2.0*pi4)           + 0.00028*sinD(l4-pi4+2.0*psi-2.0*Pi)
           + 0.00134*sinD(pi4-Pi)                    - 0.00028*sinD(2.0*(l4-om4))
           + 0.00125*sinD(2.0*(l4-G-Pi))             - 0.00027*sinD(pi3-pi4+om3-om4)
           - 0.00117*sinD(2.0*G)                     - 0.00026*sinD(5.0*Gprime-3.0*G+188.37)
           - 0.00112*sinD(2.0*(l3-l4))               + 0.00025*sinD(om4-om3)
           + 0.00107*sinD(3.0*l3-7.0*l4+4.0*pi4)     - 0.00025*sinD(l2-3.0*l3+2.0*l4)
           + 0.00102*sinD(l4-G-Pi)                   - 0.00023*sinD(3.0*(l3-l4))
           + 0.00096*sinD(2.0*l4-psi-om4)            + 0.00021*sinD(2.0*l4-2.0*Pi-3.0*G)
           + 0.00087*sinD(2.0*(psi-om4))             - 0.00021*sinD(2.0*l3-3.0*l4+pi4)
           - 0.00085*sinD(3.0*l3-7.0*l4+pi3+3.0*pi4) + 0.00019*sinD(l4-pi4-G)
           + 0.00085*sinD(l3-2.0*l4+pi4)             - 0.00019*sinD(2.0*l4-pi3-pi4)
           - 0.00081*sinD(2.0*(l4-psi))              - 0.00018*sinD(l4-pi4+G)
           + 0.00071*sinD(l4+pi4-2.0*Pi-3.0*G)       - 0.00016*sinD(l4+pi3-2.0*Pi-2.0*G);

  L[1] = l1 + Sigma1; L[2] = l2 + Sigma2; L[3] = l3 + Sigma3; L[4] = l4 + Sigma4;

  tanB1 =    0.0006393*sinD(L[1]-om1)
           + 0.0001825*sinD(L[1]-om2)
           + 0.0000329*sinD(L[1]-om3)
           - 0.0000311*sinD(L[1]-psi)
           + 0.0000093*sinD(L[1]-om4)
           + 0.0000075*sinD(3.0*L[1] - 4.0*l2 - 1.9927*Sigma1 + om2)
           + 0.0000046*sinD(L[1] + psi - 2.0*Pi - 2.0*G);
  tanB2 =    0.0081004*sinD(L[2]-om2)
           + 0.0004512*sinD(L[2]-om3)
           - 0.0003284*sinD(L[2]-psi)
           + 0.0001160*sinD(L[2]-om4)
           + 0.0000272*sinD(l1 - 2.0*l3 + 1.0146*Sigma2 + om2)
           - 0.0000144*sinD(L[2]-om1)
           + 0.0000143*sinD(L[2] + psi - 2.0*Pi - 2.0*G)
           + 0.0000035*sinD(L[2]-psi+G)
           - 0.0000028*sinD(l1 - 2.0*l3 + 1.0146*Sigma2 + om3);
  tanB3 =    0.0032402*sinD(L[3]-om3)
           - 0.0016911*sinD(L[3]-psi)
           + 0.0006847*sinD(L[3]-om4)
           - 0.0002797*sinD(L[3]-om2)
           + 0.0000321*sinD(L[3] + psi - 2.0*Pi - 2.0*G)
           + 0.0000051*sinD(L[3]-psi+G)
           - 0.0000045*sinD(L[3]-psi-G)
           - 0.0000045*sinD(L[3] + psi - 2.0*Pi)
           + 0.0000037*sinD(L[3] + psi - 2.0*Pi - 3.0*G)
           + 0.0000030*sinD(2.0*l2 - 3.0*L[3] + 4.03*Sigma3 + om2)
           - 0.0000021*sinD(2.0*l2 - 3.0*L[3] + 4.03*Sigma3 + om3);
  tanB4 =   -0.0076579*sinD(L[4]-psi)
           + 0.0044134*sinD(L[4]-om4)
           - 0.0005112*sinD(L[4]-om3)
           + 0.0000773*sinD(L[4] + psi - 2.0*Pi - 2.0*G)
           + 0.0000104*sinD(L[4]-psi+G)
           - 0.0000102*sinD(L[4]-psi-G)
           + 0.0000088*sinD(L[4] + psi - 2.0*Pi - 3.0*G)
           - 0.0000038*sinD(L[4] + psi - 2.0*Pi - G);

  B[1] = atanD(tanB1); B[2] = atanD(tanB2); B[3] = atanD(tanB3); B[4] = atanD(tanB4);

  rP1 =     -0.0041339*cosD(2.0*(l1-l2))
           - 0.0000387*cosD(l1-pi3)
           - 0.0000214*cosD(l1-pi4)
           + 0.0000170*cosD(l1-l2)
           - 0.0000131*cosD(4.0*(l1-l2))
           + 0.0000106*cosD(l1-l3)
           - 0.0000066*cosD(l1 + pi3 - 2.0*Pi - 2.0*G);
  rP2 =      0.0093848*cosD(l1-l2)
           - 0.0003116*cosD(l2-pi3)
           - 0.0001744*cosD(l2-pi4)
           - 0.0001442*cosD(l2-pi2)
           + 0.0000553*cosD(l2-l3)
           + 0.0000523*cosD(l1-l3)
           - 0.0000290*cosD(2.0*(l1-l2))
           + 0.0000164*cosD(2.0*(l2-om2))
           + 0.0000107*cosD(l1 - 2.0*l3 + pi3)
           - 0.0000102*cosD(l2-pi1)
           - 0.0000091*cosD(2.0*(l1-l3));
  rP3 =     -0.0014388*cosD(l3-pi3)
           - 0.0007919*cosD(l3-pi4)
           + 0.0006342*cosD(l2-l3)
           - 0.0001761*cosD(2.0*(l3-l4))
           + 0.0000294*cosD(l3-l4)
           - 0.0000156*cosD(3.0*(l3-l4))
           + 0.0000156*cosD(l1-l3)
           - 0.0000153*cosD(l1-l2)
           + 0.0000070*cosD(2.0*l2 - 3.0*l3 + pi3)
           - 0.0000051*cosD(l3 + pi3 - 2.0*Pi - 2.0*G);
  rP4 =     -0.0073546*cosD(l4-pi4)
           + 0.0001621*cosD(l4-pi3)
           + 0.0000974*cosD(l3-l4)
           - 0.0000543*cosD(l4 + pi4 - 2.0*Pi - 2.0*G)
           - 0.0000271*cosD(2.0*(l4-pi4))
           + 0.0000182*cosD(l4-Pi)
           + 0.0000177*cosD(2.0*(l3-l4))
           - 0.0000167*cosD(2.0*l4 - psi - om4)
           + 0.0000167*cosD(psi - om4)
           - 0.0000155*cosD(2.0*(l4-Pi-G))
           + 0.0000142*cosD(2.0*(l4-psi))
           + 0.0000105*cosD(l1-l4)
           + 0.0000092*cosD(l2-l4)
           - 0.0000089*cosD(l4-Pi-G)
           - 0.0000062*cosD(l4 + pi4 - 2.0*Pi - 3.0*G)
           + 0.0000048*cosD(2.0*(l4-om4));

  RP[1] =    5.90569 * (1.0 + rP1);
  RP[2] =    9.39657 * (1.0 + rP2);
  RP[3] =   14.98832 * (1.0 + rP3);
  RP[4] =   26.36273 * (1.0 + rP4);

  T0 =     (jD - 2433282.423)/36525.0;
  P  =       1.3966626*T0 + 0.0003088*T0*T0;
  L[1] += P; L[2] += P; L[3] += P; L[4] += P; psi += P;
  T  =       (jD - 2415020.5)/36525.0;
  I  =       3.120262 + 0.0006*T;

  for (i = 1; i <= 4; i++){
    tX[i] = RP[i]*cosD(L[i]-psi)*cosD(B[i]);
    tY[i] = RP[i]*sinD(L[i]-psi)*cosD(B[i]);
    tZ[i] = RP[i]*sinD(B[i]);
  }
  tX[5] = tY[5] = 0.0; tZ[5] = 1.0;

  sinI = sinD(I); cosI = cosD(I);
  for (i = 1; i <= 5; i++) {
    a1[i] = tX[i]; b1[i] = tY[i]*cosI - tZ[i]*sinI; c1[i] = tY[i]*sinI + tZ[i]*cosI;
  }

  Te = (jD-2451545.0)/36525.0;
  Omega = 100.464407 + 1.0209774*Te + 0.00040315*Te*Te + 0.000000404*Te*Te*Te;
  orbI  =   1.303267 - 0.0054965*Te + 0.00000466*Te*Te - 0.000000002*Te*Te*Te;
  Phi = psi - Omega;
  sinPhi = sinD(Phi); cosPhi = cosD(Phi);
  for (i = 1; i <= 5; i++) {
    a2[i] = a1[i]*cosPhi - b1[i]*sinPhi; b2[i] = a1[i]*sinPhi + b1[i]*cosPhi;
  }

  sinOrbI = sinD(orbI); cosOrbI = cosD(orbI);
  for (i = 1; i <= 5; i++) {
    b3[i] = b2[i]*cosOrbI - c1[i]*sinOrbI;
    c3[i] = b2[i]*sinOrbI + c1[i]*cosOrbI;
  }

  sinOmega = sinD(Omega); cosOmega = cosD(Omega);
  for (i = 1; i <= 5; i++) {
    a4[i] = a2[i]*cosOmega - b3[i]*sinOmega; b4[i] = a2[i]*sinOmega + b3[i]*cosOmega;
  }

  sinLambda0 = sinD(lambda0); cosLambda0 = cosD(lambda0);
  for (i = 1; i <= 5; i++) {
    a5[i] = a4[i]*sinLambda0 - b4[i]*cosLambda0; b5[i] = a4[i]*cosLambda0 + b4[i]*sinLambda0;
  }

  sinBeta0 = sinD(beta0); cosBeta0 = cosD(beta0);
  for (i = 1; i <= 5; i++) {
    b6[i] = c3[i]*sinBeta0 + b5[i]*cosBeta0; c6[i] = c3[i]*cosBeta0 - b5[i]*sinBeta0;
  }

  D = atan2D(a5[5], c6[5]);
  sinDD = sinD(D); cosDD = cosD(D);
  for (i = 1; i <= 4; i++) {
    sX[i] = a5[i]*cosDD - c6[i]*sinDD;
    sY[i] = a5[i]*sinDD + c6[i]*cosDD;
    sZ[i] = b6[i];
  }

  for (i = 1; i <= 4; i++) {
    dT[i] = fabs(sZ[i])*sqrt(1.0 - pow(sX[i]/sqrt(sX[i]*sX[i] + sY[i]*sY[i] + sZ[i]*sZ[i]), 2.0));
  }
  sX[1] += dT[1]/17295.0;
  sX[2] += dT[2]/21819.0;
  sX[3] += dT[3]/27558.0;
  sX[4] += dT[4]/36548.0;

  for (i = 1; i <= 4; i++) {
    double W;

    W = deltaAU/(deltaAU + sZ[i]/2095.0);
    sX[i] *= W;
    sY[i] *= W;
  }

  X[1] = sX[1]; Y[1] = sY[1]; Z[1] = sZ[1];
  X[2] = sX[2]; Y[2] = sY[2]; Z[2] = sZ[2];
  X[3] = sX[3]; Y[3] = sY[3]; Z[3] = sZ[3];
  X[4] = sX[4]; Y[4] = sY[4]; Z[4] = sZ[4];
}

/*
  Calculate the positions of the four Galilean satellites using the "low accuracy"
  method given in "Astronomical Algorythms" (Jean Meeus, second edition, chapter 44).
 */
void lowAccuracyJovSats(double jD, double *X, double *Y, double *uR)
{
  double d, V, M, N, J, A, B, K, R, r, delta, psi, lambda, Ds, De,
    dd, pB, u1, u2, u3, u4, G, H, r1, r2, r3, r4, u1c, u2c, u3c, u4c;
  
  d      = jD      - 2451545.0;
  V      = 172.74  + 0.00111588*d                ; doubleNormalize0to360(&V);
  M      = 357.529 + 0.9856003*d                 ; doubleNormalize0to360(&M);
  N      =  20.020 + 0.0830853*d  + 0.329*sinD(V); doubleNormalize0to360(&N);
  J      =  66.115 + 0.9025179*d  - 0.329*sinD(V); doubleNormalize0to360(&J);
  A      = 1.915*sinD(M) + 0.020*sinD(2.0*M);
  B      = 5.555*sinD(N) + 0.168*sinD(2.0*N);
  K      = J + A - B; doubleNormalize0to360(&K);
  R      = 1.00014 - 0.01671*cosD(M) - 0.00014*cosD(2.0*M);
  r      = 5.20872 - 0.25208*cosD(N) - 0.00611*cosD(2.0*N);
  delta  = sqrt(r*r + R*R - 2.0*r*R*cosD(K));
  psi    = asin(R*sinD(K)/delta)/DEGREES_TO_RADIANS;
  lambda = 34.35 + 0.083091*d + 0.329*sinD(V) + B; doubleNormalize0to360(&lambda);
  Ds     = 3.12*sinD((lambda+42.8));
  De     = Ds - 2.22*sinD(psi)*cosD(lambda+22.0)
           - 1.30*(r-delta)*sinD(lambda-100.5)/delta;
  dd     = d - delta/173.0; pB = psi - B;
  u1     = 163.8069 + 203.4058646*dd + pB; doubleNormalize0to360(&u1);
  u2     = 358.4140 + 101.2916335*dd + pB; doubleNormalize0to360(&u2);
  u3     =   5.7176 +  50.2345180*dd + pB; doubleNormalize0to360(&u3);
  u4     = 224.8092 +  21.4879800*dd + pB; doubleNormalize0to360(&u4);
  G      = 331.18 + 50.310482*dd; doubleNormalize0to360(&G);
  H      =  87.45 + 21.569231*dd; doubleNormalize0to360(&H);
  u1c    = u1 + 0.473*sinD(2.0*(u1-u2));
  u2c    = u2 + 1.065*sinD(2.0*(u2-u3));
  u3c    = u3 + 0.165*sinD(G);
  u4c    = u4 + 0.843*sinD(H);
  r1     =  5.9057 - 0.0244*cosD(2.0*(u1-u2));
  r2     =  9.3966 - 0.0882*cosD(2.0*(u2-u3));
  r3     = 14.9883 - 0.0216*cosD(G);
  r4     = 26.3627 - 0.1939*cosD(H);
  X[1]   = r1*sinD(u1c);
  X[2]   = r2*sinD(u2c);
  X[3]   = r3*sinD(u3c);
  X[4]   = r4*sinD(u4c);
  if (Y != NULL) {
    double sinDe;

    sinDe = sinD(De);
    Y[1] = -r1*cosD(u1c)*sinDe;
    Y[2] = -r2*cosD(u2c)*sinDe;
    Y[3] = -r3*cosD(u3c)*sinDe;
    Y[4] = -r4*cosD(u4c)*sinDe;
  }
  uR[1] = u1;
  uR[2] = u2;
  uR[3] = u3;
  uR[4] = u4;
}
