#ifndef CLICK_HANDLERPROXY_HH
#define CLICK_HANDLERPROXY_HH
#include <click/element.hh>
CLICK_DECLS

class HandlerProxy : public Element { public:

    typedef ErrorHandler* (*ErrorReceiverHook)(const String&, void*);

    HandlerProxy() CLICK_COLD;
    ~HandlerProxy() CLICK_COLD;

    virtual int add_error_receiver(ErrorReceiverHook, void*);
    virtual int remove_error_receiver(ErrorReceiverHook, void*);

    virtual int check_handler(const String&, bool write, ErrorHandler*);

    enum {
	CSERR_OK		= 200,
	CSERR_SYNTAX		= 500,
	CSERR_NO_SUCH_ELEMENT	= 510,
	CSERR_NO_SUCH_HANDLER	= 511,
	CSERR_HANDLER_ERROR	= 520,
	CSERR_PERMISSION	= 530,
	CSERR_NO_ROUTER		= 540,
	CSERR_UNSPECIFIED	= 590
    };

  protected:

    struct ErrorReceiver {
	ErrorReceiverHook hook;
	void* thunk;
    };

    ErrorReceiver* _err_rcvs;
    int _nerr_rcvs;

};

CLICK_ENDDECLS
#endif
