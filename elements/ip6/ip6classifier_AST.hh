#ifndef CLICK_IP6CLASSIFIER_AST_HH
#define CLICK_IP6CLASSIFIER_AST_HH

#include <click/config.h>
#include <click/packet.hh>
CLICK_DECLS


namespace ip6classification {

/*
 * @brief A specific Node in the AST.
 * This class functions as a bass class the Primitive and Combiner classes.
 */
class ASTNode {
public:
    virtual ~ASTNode(); // The destructor is defined in "ip6filterAST.cc"

    /*
     * @brief Normally we print first the more specialised node itself, and then we do the super call to print here the node->left_child and node->right_child.
     */
    virtual void print() {
        click_chatter("node->left_child = ");
        if (left_child != NULL) {
            left_child->print();
        }
        click_chatter("node->right_child = ");
        if (right_child != NULL) {
            right_child->print();
        }
    }
    virtual bool check_whether_packet_matches(Packet* packet) = 0;
    ASTNode* left_child = NULL;     // Initialize the children as NULL pointers
    ASTNode* right_child = NULL;    // Initialize the children as NULL pointers
};


/*
 * @brief A class representing an AST. This tree contains all tokens and the connections between them.
 */
class AST {
public:
    AST() { }
    virtual ~AST() { }

    ASTNode* root;

    /*
     * @brief Print the entire abstract syntax tree in a humable readable format
     */
    virtual void print() {
        root->print();    
    }
    /*
     * @brief This function checks whether the previously given filter string matches the given packet.
     * The packet is passed at run time, whenever IP6Filter's push method is called.
     * @return true when the packet matches the previously given filter string, false otherwise.
     */
    bool check_whether_packet_matches(Packet* packet) {
        return root->check_whether_packet_matches(packet);
    }
};

};
CLICK_ENDDECLS
#endif /* CLICK_IP6FILTER_AST_HH */
