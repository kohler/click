#ifndef KERNELERROR_HH
#define KERNELERROR_HH
#include "error.hh"

class KernelErrorHandler : public ErrorHandler {

  int _nwarnings;
  int _nerrors;

 public:
  
  KernelErrorHandler()			{ reset_counts(); }
  
  int nwarnings() const			{ return _nwarnings; }
  int nerrors() const			{ return _nerrors; }
  void reset_counts()			{ _nwarnings = _nerrors = 0; }
  
  void vmessage(Seriousness, const String &);
  
};

#endif
