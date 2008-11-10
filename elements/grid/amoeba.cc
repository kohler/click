/*
 * From Numerical Recipes in C (1st edition), Chapter 10,
 * page 307, Downhill Simplex Method in Multimensions
 * for minimization.
 *
 * The Numerical Recipes copyright forbids this code from
 * being distributed.
 */

#include <click/config.h>
#include "amoeba.hh"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
CLICK_DECLS

Amoeba::Amoeba(int dimensions)
{
  _dimensions = dimensions;
}

Amoeba::~Amoeba()
{
}

#define TINY 1.0e-10
#define NMAX 5000
#define GET_PSUM \
  for (j=1;j<=ndim;j++) {\
  for (sum=0.0,i=1;i<=mpts;i++) sum += p[i][j];\
  psum[j]=sum;}
#define SWAP(a,b) {swap=(a);(a)=(b);(b)=swap;}

void
Amoeba::amoeba1(double **p, double y[], double ftol, int *nfunk)
{
  int ndim = dimensions();
  int i,ihi,ilo,inhi,j,mpts=ndim+1;
  double rtol,sum,swap,ysave,ytry,*psum;

  psum=vector(1,ndim);
  *nfunk=0;
  GET_PSUM;
  for (;;) {
    ilo=1;
    ihi = y[1]>y[2] ? (inhi=2,1) : (inhi=1,2);
    for (i=1;i<=mpts;i++) {
      if (y[i] <= y[ilo])
        ilo=i;
      if (y[i] > y[ihi]) {
        inhi=ihi;
        ihi=i;
      } else if (y[i] > y[inhi] && i != ihi)
        inhi=i;
    }
    rtol=2.0*fabs(y[ihi]-y[ilo])/(fabs(y[ihi])+fabs(y[ilo])+TINY);

    if (rtol < ftol) {
      SWAP(y[1],y[ilo]);
      for (i=1;i<=ndim;i++){
        SWAP(p[1][i],p[ilo][i]);
      }
      break;
    }

    if (*nfunk >= NMAX)
      nrerror("NMAX exceeded");
    *nfunk += 2;

    ytry=amotry(p,y,psum,ihi,-1.0);
    if (ytry <= y[ilo])
      ytry=amotry(p,y,psum,ihi,2.0);
    else if (ytry >= y[inhi]) {
      ysave=y[ihi];
      ytry=amotry(p,y,psum,ihi,0.5);
      if (ytry >= ysave) {
        for (i=1;i<=mpts;i++) {
          if (i != ilo) {
            for (j=1;j<=ndim;j++)
              p[i][j]=psum[j]=0.5*(p[i][j]+p[ilo][j]);
            y[i]=fn(psum + 1);
          }
        }
        *nfunk += ndim;
        GET_PSUM;
      }
    } else {
      --(*nfunk);
    }
  }
  free_vector(psum,1,ndim);
}

double
Amoeba::amotry(double **p, double y[], double psum[],
               int ihi, double fac)
{
  int ndim = dimensions();
  int j;
  double fac1,fac2,ytry,*ptry;
  ptry=vector(1,ndim);
  fac1=(1.0-fac)/ndim;
  fac2=fac1-fac;
  for (j=1;j<=ndim;j++)
    ptry[j]=psum[j]*fac1-p[ihi][j]*fac2;
  ytry = fn(ptry + 1);
  if (ytry < y[ihi]) {
    y[ihi]=ytry;
    for (j=1;j<=ndim;j++) {
      psum[j] += ptry[j]-p[ihi][j];
      p[ihi][j]=ptry[j];
    }
  }
  free_vector(ptry,1,ndim);
  return ytry;
}

void
Amoeba::nrerror(const char *error_text)
{
  fprintf(stderr, "Oops %s\n", error_text);
  exit(1);
}

double *
Amoeba::vector(int nl, int nh)
{
  double *v;

  v = (double *) malloc((nh-nl+1)*sizeof(double));
  return v - nl;
}

void
Amoeba::free_vector(double *v, int nl, int)
{
  free((char *) (v + nl));
}

void
Amoeba::minimize(double res[])
{
  double ftmp[150];
  double *p[10];  /* vertices of starting simplex */
  double y[10];   /* funk() on each p[] */
  double ftol;    /* tolerance of funk output */
  int nfunk = 0;  /* number of times funk called */
  int i;

  assert(dimensions() < 9);

  p[0] = 0;
  for(i = 1; i <= dimensions()+1; i++)
    p[i] = ftmp + i*(dimensions()+1);

  for(i = 1; i <= dimensions()+1; i++){
    int j;
    for(j = 1; j <= dimensions(); j++){
      p[i][j] = 0.0;
    }
  }

  /* start simplex points at 0,0,0 1,0,0 0,1,0 0,0,1 */
  for(i = 2; i <= dimensions()+1; i++){
    p[i][i-1] = 1 + i*0.01;
  }

  /* evaluate at each simplex point */
  for(i = 1; i <= dimensions()+1; i++)
    y[i] = fn(p[i] + 1);

  ftol = 0.00001;

  amoeba1(p, y, ftol, &nfunk);

  for(i = 1; i <= dimensions(); i++)
    res[i-1] = p[1][i];
}

class AmoebaTest : public Amoeba {
public:
  AmoebaTest();
  virtual double fn(double a[]);
  void doit();
};

AmoebaTest::AmoebaTest() : Amoeba(2)
{
}

struct xxx {
  double x;
  double y;
  double d;
};
static struct xxx da[] = {
  { 1000, 1000, 1 },
  { 1001, 1001, 1 },
  { 1000.5, 1001, .5 }
};

double
AmoebaTest::fn(double a[])
{
  double x = a[0];
  double y = a[1];
  double d = 0;
  int i;

  printf("xfunc(%f,%f) ", x, y);

  for(i = 0; i < (int)(sizeof(da) / sizeof(da[0])); i++){
    double dx = x - da[i].x;
    double dy = y - da[i].y;
    double dd = da[i].d - sqrt(dx*dx + dy*dy);
    dd = dd * dd;
    d += dd;
    printf("%f ", dd);
  }

  printf("= %f\n", d);

  return(d);
}

void
AmoebaTest::doit()
{
  double pt[2];

  minimize(pt);

  printf("%f %f\n", pt[0], pt[1]);

  if(pt[0] > 999.999 && pt[0] < 1000.001 &&
     pt[1] > 1000.999 && pt[1] < 1001.001){
    printf("AmoebaTest ok\n");
  } else {
    printf("AmoebaTest failed\n");
  }
}

#if 0
main()
{
  AmoebaTest at;
  at.doit();
}
#endif

CLICK_ENDDECLS
ELEMENT_PROVIDES(Amoeba)
ELEMENT_REQUIRES(userlevel)
