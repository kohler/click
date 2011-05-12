#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <click/config.h>
#include "tmain.hh"
#include "gathererror.hh"
namespace clicky {

tmain::tmain(dcss_set *ccss)
    : crouter(ccss)
{
}

tmain::~tmain()
{
    // only call from GtkWidget destruction
    clear(false);
}

void tmain::on_config_changed(bool, LexerTInfo *)
{
    GatherErrorHandler *gerrh = error_handler();
    if (gerrh->nerrors() || gerrh->nwarnings())
	on_error(true, String());
}

void tmain::on_error(bool, const String &)
{
    ErrorHandler *errh = ErrorHandler::default_handler();
    GatherErrorHandler *gerrh = error_handler();
    for (GatherErrorHandler::iterator gi = gerrh->begin();
	 gi != gerrh->end(); ++gi)
	errh->xmessage("{}" + gi->message);
    gerrh->clear();
}

}
