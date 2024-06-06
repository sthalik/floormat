#pragma once
#include "compat/assert.hpp"
#include "RTree.h"

RTREE_TEMPLATE
template<typename F>
int RTREE_QUAL::Search(const ELEMTYPE a_min[NUMDIMS], const ELEMTYPE a_max[NUMDIMS], F&& callback) const
{
#ifndef FM_NO_DEBUG
  for(int index=0; index<NUMDIMS; ++index)
  {
    fm_assert(a_min[index] <= a_max[index]);
  }
#endif

  Rect rect;

  for(int axis=0; axis<NUMDIMS; ++axis)
  {
    rect.m_min[axis] = a_min[axis];
    rect.m_max[axis] = a_max[axis];
  }

  // NOTE: May want to return search result another way, perhaps returning the number of found elements here.

  int foundCount = 0;
  Search(m_root, &rect, foundCount, callback);

  return foundCount;
}

// Search in an index tree or subtree for all data retangles that overlap the argument rectangle.
RTREE_TEMPLATE
template<typename F>
bool RTREE_QUAL::Search(Node* a_node, Rect* a_rect, int& a_foundCount, F&& callback) const
{
  fm_assert(a_node);
  fm_assert(a_node->m_level >= 0);
  fm_assert(a_rect);

  if(a_node->IsInternalNode())
  {
    // This is an internal node in the tree
    for(int index=0; index < a_node->m_count; ++index)
    {
      if(Overlap(a_rect, &a_node->m_branch[index].m_rect))
      {
        if(!Search(a_node->m_branch[index].m_child, a_rect, a_foundCount, callback))
        {
          // The callback indicated to stop searching
          return false;
        }
      }
    }
  }
  else
  {
    // This is a leaf node
    for(int index=0; index < a_node->m_count; ++index)
    {
      if(Overlap(a_rect, &a_node->m_branch[index].m_rect))
      {
        ++a_foundCount;
        const Rect& r = a_node->m_branch[index].m_rect;
        if(!callback(a_node->m_branch[index].m_data, r))
          return false; // Don't continue searching
      }
    }
  }

  return true; // Continue searching
}
