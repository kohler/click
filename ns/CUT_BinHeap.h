//-*-c++-*-
#ifndef _CUT_BinHeap_h_
#define _CUT_BinHeap_h_


#include <assert.h>

// #define CHECK_BinHeap_INVARIANTS

template <class Key, class Data, class Compare>
class CUT_BinHeap
{ 
public:
  typedef void *Pix;
 protected:
  ///////////////////////////////////////////////////////////////////////////
  // Beginning of Node class
  ///////////////////////////////////////////////////////////////////////////

    class Node {
  public:
      Node* next;          // used to link all used items 
      Node* prev;
      Node* parent;
      Node* kid1;      // a child
      Node* kid2;      // a child

      Key key;
      Data data;
    
      void init(Key k, Data d) {
	  key = k; data = d;
	  parent = NULL;
	  kid1 = NULL;
	  kid2 = NULL;
	  next = NULL;
	  prev = NULL;
      }
};
  
  ///////////////////////////////////////////////////////////////////////////
  // End of Node class
  ///////////////////////////////////////////////////////////////////////////

  Node* get_node(Key k, Data d) {
    //
    // Get new node from freelist
    //
    Node *n;
    if ( free_list ) {
      n = free_list;
      free_list = free_list -> next;
      if ( free_list ) {
	free_list -> prev = NULL;
      }
    } else {
      n = new Node;
    }

    n -> init(k, d);

    //
    // Link into node list
    //
    n -> next = node_list;
    n -> prev = NULL;
    if ( node_list ) {
      node_list -> prev = n;
    } 
    node_list = n;
    return n;
  }

  void put_node(Node *n) {
    //
    // Remove from node list
    //
    if ( n -> prev == NULL ) {
      assert(n == node_list);
      node_list = n -> next;
    } else {
      n -> prev -> next = n -> next;
    }
    if ( n -> next != NULL ) {
      n -> next -> prev = n -> prev;
    }
    //
    // Now, link into the free list
    //
    n -> next = free_list;
    n -> prev = NULL;
    free_list = n;
  }


public:

  //
  // Iterator functions, used in other functions. We can't let the
  // user change the key, but we can let them change the data.
  //

  const Key&  key(Pix xx) const {
    Node* x = (Node *) xx;
    return x->key;
  }

  Data&  data(Pix xx) const {
    Node* x = (Node *) xx;
    return x->data;
  }

  Data&  operator()(Pix xx) const {
    Node* x = (Node *) xx;
    return x->data;
  }

  Pix first() const {
#ifdef CHECK_BinHeap_INVARIANTS
    if ( node_list ) {
      assert(node_list -> prev == NULL);
    }
#endif
    return (Pix) node_list;
  }

  int tree_height() {
    return tree_height(root);
  }

  void invariant() {
#ifdef CHECK_BinHeap_INVARIANTS
    assert(heap_property(root) == true );
    parent_property(root);
#endif
  }

  //
  // Access next item in the iterator
  //
  void next(Pix& pp) {
      Node* p = (Node *) pp;
      assert(p != NULL);
      pp = (Pix) p -> next;
  }

  //
  // Clean out the entire BinHeap
  //
  void clear() {
    //
    // Clear out BinHeap..
    //
    for (Node *FF = node_list; FF != NULL; ) {
      Node *NN = FF -> next;
      delete FF;
      FF = NN;
    }
    //
    // Clear out free list
    //
    for (Node *GG = free_list; GG != NULL; ) {
      Node *NN = GG -> next;
      delete GG;
      GG = NN;
    }

    node_count = 0;
    node_list  = NULL;
    free_list = NULL;
    root = NULL;
  }

  int  size() {
    return node_count;
  }

 /////////////////////////////////////////////////////////////////////////////
 // Constructors/Destructors
 /////////////////////////////////////////////////////////////////////////////

  CUT_BinHeap() {
      node_count = 0;
      root = NULL;
      node_list = NULL;
      free_list = NULL;
  }

  ~CUT_BinHeap()  {
    clear();
  }

  /////////////////////////////////////////////////////////////////////////////
  // Insertion/deletion
  /////////////////////////////////////////////////////////////////////////////
  Pix insert(Key k, Data d) {
    invariant();
    Node *n = get_node(k,d);
    node_count++;

    if ( root == NULL ) {
      n -> parent = NULL;
      root = n;
    } else {
      if (compare(root -> key, n -> key)) {
	// root < n
	hang_under(root,n);
      } else {
	// n < root
	hang_under(n,root);
	root = n;
	n -> parent = NULL;
      }
    }
    invariant();
    return ( (Pix) n );
  }

  //
  // Remove an arbitrary node in the heap
  //
  void erase(Pix ref) {
    invariant();

    Node *n = (Node*) ref;
    if ( n == root ) {
      root = remove_node(root);
    } else {
      Node *p = n -> parent;
      assert(p != NULL);
      assert(n == p -> kid1 || n == p -> kid2);
      //
      // Determine which kid we were..
      //
      if ( n == p -> kid1 ) {
	p -> kid1 = remove_node(p -> kid1);
      } else {
	p -> kid2 = remove_node(p -> kid2);
      }
#ifdef CHECK_BinHeap_INVARIANTS
      assert(p -> kid1 == NULL || p -> kid1 -> parent == p );
      assert(p -> kid2 == NULL || p -> kid2 -> parent == p );
#endif
    }
    //
    // Recover storage
    //
    put_node(n);
    node_count--;

    invariant();
  }

  void deq() {
    erase(root);
  }

  Pix find_top() const {
    return((Pix) root);
  }

  Data& top() const {
#ifdef CHECK_BinHeap_INVARIANTS
      assert( root != NULL );
#endif
      return root -> data;
  }

  bool  empty() const {
    if ( node_count == 0 ) {
#ifdef CHECK_BinHeap_INVARIANTS
      assert( root == NULL );
#endif
      return true;
    } else {
#ifdef CHECK_BinHeap_INVARIANTS
      assert( root != NULL );
#endif
      return false;
    }
  }
  //
  // Internal functions
  //
  protected:

  //
  // Does the node have the heap property?
  //
  bool heap_property(Node *n) {
    if (n == NULL) {
      return true;
    } else {
      //
      // Check parent connectivity
      //
      if (n -> kid1 == NULL && n -> kid2 == NULL) {
	return true;
      } else {
	bool cmp1 = true;
	bool cmp2 = true;
	if ( n -> kid1 != NULL ) {
	  cmp1 = !compare(n -> kid1 -> key, n -> key);
	}
	if ( n -> kid2 != NULL ) {
	  cmp2 = !compare(n -> kid2 -> key, n -> key);
	}
	return cmp1 & cmp2;
      }
    }
  }

  //
  // Assert that the parent links are maintained
  //
  void parent_property(Node *n) {
    if (n == NULL) {
      return;
    } else {
      //
      // Check parent connectivity
      //
      if ( n == root ) {
	assert(n -> parent == NULL);
      } else {
	assert(n -> parent != NULL);
      }
      parent_property(n -> kid1);
      parent_property(n -> kid2);
    }
  }

  //
  // Returns the height of the heap
  //
  int tree_height(Node* n) {
    if ( n == NULL ) {
      return 0;
    } else {
      int cnt1 = tree_height(n -> kid1);
      int cnt2 = tree_height(n -> kid2);
      if (cnt1 < cnt2) {
	return cnt2+1;
      } else {
	return cnt1+1;
      }
    }
  }

  //
  // Remove a Node and return the replacement Node for this. This is
  // either used to dequeue the root not *or* to remove a Node in the
  // middle of the heap.
  //
  Node* remove_node(Node* here) {
#ifdef CHECK_BinHeap_INVARIANTS
    assert(here!= NULL);
#endif

    Node *kid1 = here -> kid1;
    Node *kid2 = here -> kid2;
    
    if ( kid1 == NULL & kid2 == NULL ) {
      return NULL;
    }else if ( kid1 == NULL & kid2 != NULL ) {
      kid2 -> parent = here -> parent;
      return kid2;
    } else if (kid1 != NULL && kid2 == NULL) {
      kid1 -> parent = here -> parent;
      return kid1;
    } else {
      //
      // Both kids are non-null. Hang the greater
      // over the lessor
      //
      if (compare(kid1->key, kid2->key)) {
	// kid1 < kid2, hang kid2 under kid1
	hang_under(kid1, kid2);
	kid1 -> parent = here -> parent;
	return kid1;
      } else {
	hang_under(kid2, kid1);
	kid2 -> parent = here -> parent;
	return kid2;
      }
    }
  }

  //
  // Hang Node y under Node x
  //
  void hang_under(Node *x, Node* y) {
    for(;;) {
#ifdef CHECK_BinHeap_INVARIANTS
      assert(x != NULL);
      assert(y != NULL);
      assert( !compare(y -> key, x -> key) );

      assert(heap_property(x));
      assert(heap_property(y));
#endif

      Node *kid1 = x -> kid1;
      Node *kid2 = x -> kid2;

      if ( kid1 == NULL & kid2 == NULL ) {
	//
	// Both null. Hang based on tree size
	//
	if (node_count & 0x1) {
	  x -> kid1 = y;
	  y -> parent = x;
	} else {
	  x -> kid2 = y;
	  y -> parent = x;
	}
	return;
      } else if (kid1 == NULL) {
	x -> kid1 = y;
	y -> parent = x;
	return;
      } else if (x -> kid2 == NULL ){
	x -> kid2 = y;
	y -> parent = x;
	return;
      } else {
	//
	// Neither is null
	//
	bool cmp_kid1 = compare(kid1 -> key, y -> key);
	bool cmp_kid2 = compare(kid2 -> key, y -> key);

	if( cmp_kid1 & cmp_kid2) {
	  //
	  // The new node could be placed under either
	  // kid. Pick on based on the number of nodes
	  // in the tree.
	  //
	  if (node_count & 0x1) {
	    //
	    // Set up for un-wound tail-recursive call.
	    //
	    x = kid1;
	    y = y;
	  } else {
	    //
	    // Recursive call
	    //
	    x = kid2;
	    y = y;
	  }
	} else if ( cmp_kid1 ){
	  // kid1 < y
	  //
	  // Recursive call
	  //
	  x = kid1;
	  y = y;
	} else if (cmp_kid2){
	  // kid2 < y
	  //
	  // Recursive call
	  //
	  x = kid2;
	  y = y;
	} else {
	  //
	  // The new node is smaller than either
	  // node. We could hang either one under
	  // the new node. Choose one based on the
	  // number of nodes in the tree
	  //
	  if (node_count & 0x1) {
	    // y < kid1 - swap, and hang
	    x -> kid1 = y;
	    y -> parent = x;
	    kid1 -> parent = NULL;
	    //
	    // Recursive call
	    //
	    x = y;
	    y = kid1;
	  } else {
	    // y < kid2 - swap, and hang
	    x -> kid2 = y;
	    y -> parent = x;
	    kid2 -> parent = NULL;
	    //
	    // Recursive call
	    //
	    x = y;
	    y = kid2;
	  }
	}
      }
    }
  }

private:
  Compare compare;
  int node_count;  // number of Nodes

  // Root of binary tree
  Node *root;
  Node* node_list; // List of all Nodes (for iteration)
  Node* free_list; // free list of Nodes;
  
};

#endif
