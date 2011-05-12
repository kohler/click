#ifndef CLICKY_TMAIN_HH
#define CLICKY_TMAIN_HH 1
#include "crouter.hh"
namespace clicky {

class tmain : public crouter { public:

    tmain(dcss_set *ccss);
    ~tmain();

    // not really public
    void on_config_changed(bool replace, LexerTInfo *linfo);
    void on_error(bool replace, const String &dialog);

};

}
#endif
