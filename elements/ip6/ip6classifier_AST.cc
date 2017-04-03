#include "ip6classifier_AST.hh"

CLICK_DECLS

namespace ip6classification {

/*
* @brief destructor for the ASTNode class
* Makes free the left child and right child pointers if they were set. If they were NULL nothing happens.
*/
ASTNode::~ASTNode() {
    if (left_child != NULL) {
        delete left_child;          // free the memory the pointer took
    }
    if (right_child != NULL) {
        delete right_child;         // free the memory the pointer took
    }
}

};

CLICK_ENDDECLS

ELEMENT_PROVIDES(IP6ClassifierAST)
