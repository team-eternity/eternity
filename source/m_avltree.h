//
// The Eternity Engine
// Copyright (C) 2025 James Haley, Max Waine, et al.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//------------------------------------------------------------------------------
//
// Purpose: Generalised AVL tree routines.
// Authors: Max Waine
//

#ifndef __M_AVLTREE__
#define __M_AVLTREE__

#include "z_zone.h"

#include "m_compare.h"

#include <type_traits>

//
// This class serves as a generic way of defining
// self-balancing AVL trees
//
template<typename T, typename U>
class AVLTree
{
    static_assert(std::is_standard_layout_v<T>, "Node key's type must be standard layout");
    static_assert(std::is_standard_layout_v<U>, "Node object's type must be standard layout");

public:
    //
    // The generic node structure, and root node for the class
    //
    struct avlnode_t
    {
        T          key;
        U         *object;
        avlnode_t *left, *right, *next;
    } *root;

    // Default constructor
    AVLTree() : root(nullptr) {}

    // Parameterised constructor
    AVLTree(const T key, U *object) : root(estructalloc(avlnode_t, 1))
    {
        root->key    = key;
        root->object = object;
    }

    // Destructor. Delete the tree if there is one
    virtual ~AVLTree() { deleteTree(root, deleteobjects); }

    //
    // Insert a new node with a provided key and object into the tree
    //
    avlnode_t *insert(const T key, U *object)
    {
        avlnode_t *toinsert, *next, *prev;
        prev             = nullptr;
        toinsert         = estructalloc(avlnode_t, 1);
        toinsert->key    = key;
        toinsert->object = object;

        if(root == nullptr)
            root = toinsert;
        else
        {
            for(next = root; next != nullptr;)
            {
                prev = next;
                if(key < next->key)
                    next = next->left;
                else if(key > next->key)
                    next = next->right;
                else
                    return handleCollision(next, toinsert);
            }
            if(key > prev->key)
                prev->right = toinsert;
            if(key < prev->key)
                prev->left = toinsert;
        }
        balance(root);

        return toinsert;
    }

    //
    // Delete the node with the provided key, then rebalance the tree
    //
    bool deleteNode(const T key, U *object)
    {
        avlnode_t *prev = nullptr, *node = root;
        bool       found = false, onleft = false, removedslot = true;

        while(!found)
        {
            if(key < node->key)
            {
                // Key is smaller than node's key, let's go left
                prev = node;
                if(!(node = node->left))
                    return false; // Not found
                onleft = true;
            }
            else if(key > node->key)
            {
                // Key is bigger than node's key, let's go right
                prev = node;
                if(!(node = node->right))
                    return false; // Not found
                onleft = false;
            }
            else if(key == node->key)
            {
                // We found the root node
                found       = true;
                removedslot = node->next == nullptr;

                if(!removedslot)
                {
                    if(node->object == object)
                    {
                        if(onleft)
                            prev->left = node->next;
                        else
                            prev->right = node->next;
                    }
                    else
                    {
                        avlnode_t *previnslot = node, *curr = node->next;
                        while(curr != nullptr && curr->object != object)
                        {
                            previnslot = curr;
                            curr       = curr->next;
                        }
                        previnslot->next = curr->next;

                        efree(curr);
                    }
                }
                else if((node->left == nullptr) || (node->right == nullptr))
                {
                    // Either the found node with key has no child or one child.
                    // Both share the same logic, just using different values.
                    avlnode_t *toset;
                    if(!node->left && !node->right)
                        toset = nullptr; // No children
                    else
                        toset = node->left != nullptr ? node->left : node->right; // One child

                    if(prev)
                    {
                        if(onleft)
                            prev->left = toset;
                        else
                            prev->right = toset;
                    }
                    else
                        root = toset;

                    efree(node);
                }
                else
                {
                    avlnode_t *minnode, *minparent;

                    // Two child nodes
                    minnode = minimumNode(node, &minparent);
                    if(!prev)
                        root = minnode;
                    else if(onleft)
                        prev->left = minnode;
                    else
                        prev->right = minnode;

                    // TODO: Is this correct?
                    if(minparent != nullptr)
                        minparent->left = nullptr;

                    minnode->left  = node->left;
                    minnode->right = node->right;

                    efree(node);
                }
            }
        }

        if(removedslot)
            balance(root);
        return true;
    }

    //
    // Return a node with a given key, or nullptr if not found
    //
    avlnode_t *find(const T key) const
    {
        for(avlnode_t *node = root; node != nullptr;)
        {
            if(key == node->key)
                return node;
            else if(key < node->key)
                node = node->left;
            else if(key > node->key)
                node = node->right;
        }
        return nullptr; // Not found
    }

    //
    // Get the number of nodes in the tree
    //
    int numNodes(const avlnode_t *node = nullptr) const
    {
        if(node == nullptr)
        {
            if(root == nullptr)
                return 0;
            else
                node = root;
        }

        int ret = 1;
        if(node->left != nullptr)
            ret += numNodes(node->left);
        if(node->next != nullptr)
            ret += numNodes(node->next);
        if(node->right != nullptr)
            ret += numNodes(node->right);

        return ret;
    }

protected:
    bool deleteobjects = false;

    //
    // Handle a collision (entering a new node with a key of a node already in the tree)
    //
    virtual avlnode_t *handleCollision(avlnode_t *listroot, avlnode_t *toinsert)
    {
        efree(toinsert);
        return nullptr; // Ahh bugger
    }

    //
    // Get the node to replace the deleted node, as well as its parent
    //
    static avlnode_t *minimumNode(avlnode_t *node, avlnode_t **parent)
    {
        avlnode_t *curr = node;
        *parent         = nullptr;

        while(curr->left)
        {
            if(parent != nullptr)
                *parent = curr;
            curr = curr->left;
        }

        return curr;
    }

    //
    // Get the height of a given tree/sub-tree
    //
    static int treeHeight(const avlnode_t *node)
    {
        int lheight, rheight;

        lheight = node->left ? treeHeight(node->left) : 0;
        rheight = node->right ? treeHeight(node->right) : 0;

        return emax(lheight, rheight) + 1;
    }

    //
    // Get the balance factor of a given tree/sub-tree
    //
    inline static int balanceFactor(const avlnode_t *node)
    {
        int bf = 0;

        if(node->left)
            bf += treeHeight(node->left);
        if(node->right)
            bf -= treeHeight(node->right);

        return bf;
    }

    inline static void rotateNodeRight(avlnode_t *&node)
    {
        avlnode_t *a = node;
        avlnode_t *b = a->left;

        //     a          b
        //    / \        / \
        //   b   3  ->  1    a
        //  / \    GOTO     / \
        // 1   2           2   3
        a->left  = b->right;
        b->right = a;

        node = b;
    }

    inline static void rotateNodeLeftRight(avlnode_t *&node)
    {
        avlnode_t *a = node;
        avlnode_t *b = a->left;
        avlnode_t *c = b->right;

        // Remember, these are variables, so c->sortorder < a->sortorder
        //     a             c
        //    / \           / \
        //   b   d  ->    b     a
        //  / \    GOTO  / \   / \
        // 1   c        1   2 3   d
        //    / \
        //   2   3
        a->left  = c->right;
        b->right = c->left;
        c->left  = b;
        c->right = a;

        node = c;
    }

    inline static void rotateNodeRightLeft(avlnode_t *&node)
    {
        avlnode_t *a = node;
        avlnode_t *b = a->right;
        avlnode_t *c = b->left;

        // This is the opposite of rotateNodeLeftRight
        a->right = c->left;
        b->left  = c->right;
        c->right = b;
        c->left  = a;

        node = c;
    }

    inline static void rotateNodeLeft(avlnode_t *&node)
    {
        avlnode_t *a = node;
        avlnode_t *b = a->right;

        // This is the opposite of rotateNodeRight
        a->right = b->left;
        b->left  = a;

        node = b;
    }

    //
    // Balance a given tree/sub-tree
    //
    static void balance(avlnode_t *&root)
    {
        if(root == nullptr)
            return;

        // Balance existent children
        if(root->left)
            balance(root->left);
        if(root->right)
            balance(root->right);

        int bf = balanceFactor(root);
        if(bf > 1)
        {
            // Left is too heavy
            if(balanceFactor(root->left) <= -1)
                rotateNodeLeftRight(root);
            else
                rotateNodeRight(root);
        }
        else if(bf < -1)
        {
            // Right is too heavy
            if(balanceFactor(root->right) >= 1)
                rotateNodeRightLeft(root);
            else
                rotateNodeLeft(root);
        }
    }

private:
    //
    // Hack down an AVLTree by doing post-order deletion,
    // deleting objects if needed
    //
    static void deleteTree(avlnode_t *node, bool deleteobjs)
    {
        if(node)
        {
            if(node->left)
                deleteTree(node->left, deleteobjs);
            if(node->right)
                deleteTree(node->right, deleteobjs);
            if(deleteobjs)
                efree(node->object);
            efree(node);
        }
    }
};

#endif

// EOF

