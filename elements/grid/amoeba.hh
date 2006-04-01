#ifndef CLICK_amoeba_hh
#define CLICK_amoeba_hh
CLICK_DECLS

/*
 * A function minimizer.
 *
 * If you override fn(a[n]) with a function that takes n
 * arguments, then call minimize(pt[n]), you'll get a
 * point that minimizes the function value in pt[].
 *
 * From Numerical Recipes in C (1st edition), Chapter 10,
 * page 307, Downhill Simplex Method in Multimensions
 * for minimization.
 */

class Amoeba {
public:
  Amoeba(int dimensions);
    virtual ~Amoeba();

  virtual double fn(double a[]) = 0; // Override this. a[] is 0-origin.

  void minimize(double pt[]); // Call this, yields pt[dimensions]. 0-origin.

  int dimensions() { return(_dimensions); }

private:
  int _dimensions;

  double *vector(int nl, int nh);
  void free_vector(double *, int, int);
  void nrerror(const char *);
  void amoeba1(double **p, double y[], double ftol, int *nfunk);
  double amotry(double **p, double y[], double psum[],
                int ihi, double fac);
};

CLICK_ENDDECLS
#endif
