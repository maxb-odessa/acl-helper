/** \file */


#include "acl-helper.h"
#include "tree.h"

//! tree search/find function
//! if new key is to be installed (add != 0) or no key found (add == 0)
//! then 'errno' also will be set to ENOENT
//! \param key key to find/insert
//! \param root tree root node
//! \param compar comparison func
//! \param add missed node insertion flag
//! \return pointer to found key or null
void *_tree_search(void *key, node_t **root, int (*compar)(const void *, const void *), int add) {

  // reset errno
  errno = 0;

  // start from root node
  node_t *p = *root;
  int cmp;

  // empty tree? init its root
  if (! *root && add) {
    *root = calloc(1, sizeof(node_t));
    assert(*root);
    (*root)->key = key;
    errno = ENOENT;
    return (*root)->key;
  }


  // walk the tree
  while (p) {

    // find needed key
    cmp = compar(p->key, key);

    // not found, go right brnach
    if (cmp > 0) {

      // last node in branch: add new node if needed
      if (p->right == NULL && add) {
        p->right = calloc(1, sizeof(node_t));
        assert(p->right);
        p = p->right;
        p->key = key;
        errno = ENOENT;
        break;
      }
      p = p->right;

    // not found, go left branch
    } else if (cmp < 0) {

      // last node in branch: add new node if needed
      if (p->left == NULL && add) {
        p->left = calloc(1, sizeof(node_t));
        assert(p->left);
        p = p->left;
        p->key = key;
        errno = ENOENT;
        break;
      }
      p = p->left;

    // match!
    } else
      break;

  } //while(..)

  // return found key or NULL
  return p ? p->key : (errno = ENOENT, NULL);
}

