/** \file */


#ifndef __ACLH_TREE_H__
#define __ACLH_TREE_H__

//! tree node definition
typedef struct node {
  void *key;              //!< node key
  struct node *left;      //!< left branch
  struct node *right;     //!< right branch
} node_t;


extern void *_tree_search(void *, node_t **, int (*)(const void *, const void *), int);

//! frontend to _tree_search(): find a key or install it if not found
#define tree_search(key, root, cmpf) _tree_search((key), (root), cmpf, 1)

//! frontend to _tree_search(): find a key and return NULL if not found
#define tree_find(key, root, cmpf) _tree_search((key), (root), cmpf, 0)


#endif // __ACLH_TREE_H__


