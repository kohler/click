
#ifndef ROUTERTHREAD_HH
#define ROUTERTHREAD_HH

class Router;

class RouterThread {
  Router *_router;
  static const unsigned int _max_driver_count = 10000;
  
  bool _please_stop_driver;
  void please_stop_driver()		{ _please_stop_driver = 1; }
  
public:
  RouterThread(Router*);
  ~RouterThread();
  Router *router() const		{ return _router; }
 
  void driver();
  void driver_once();
  void wait();
  
  friend class Router;
};

#endif

