//-----------------------------------------------------------------------------
//
// Copyright (C) 2015 David Hill
//
// See COPYING for license information.
//
//-----------------------------------------------------------------------------
//
// Vector class.
//
//-----------------------------------------------------------------------------

#ifndef ACSVM__Vector_H__
#define ACSVM__Vector_H__

#include "Types.hpp"

#include <new>
#include <utility>


//----------------------------------------------------------------------------|
// Types                                                                      |
//

namespace ACSVM
{
   //
   // Vector
   //
   // Runtime sized array.
   //
   template<typename T>
   class Vector
   {
   public:
      using const_iterator = T const *;
      using iterator       = T *;
      using size_type      = std::size_t;


      Vector() : dataV{nullptr}, dataC{0} {}
      Vector(Vector<T> const &) = delete;
      Vector(Vector<T> &&v) : dataV{v.dataV}, dataC{v.dataC}
         {v.dataV = nullptr; v.dataC = 0;}
      Vector(size_type count) : dataV{nullptr}, dataC{0} {alloc(count);}

      Vector(T const *v, size_type c)
      {
         dataC = c;
         dataV = static_cast<T *>(::operator new(sizeof(T) * dataC));

         for(T *itr = dataV, *end = itr + dataC; itr != end; ++itr)
            new(itr) T{*v++};
      }

      ~Vector() {free();}

      T &operator [] (size_type i) {return dataV[i];}

      Vector<T> &operator = (Vector<T> &&v) {swap(v); return *this;}

      //
      // alloc
      //
      template<typename... Args>
      void alloc(size_type count, Args const &...args)
      {
         if(dataV) free();

         dataC = count;
         dataV = static_cast<T *>(::operator new(sizeof(T) * dataC));

         for(T *itr = dataV, *end = itr + dataC; itr != end; ++itr)
            new(itr) T{args...};
      }

      // begin
            iterator begin()       {return dataV;}
      const_iterator begin() const {return dataV;}

      // data
      T *data() {return dataV;}

      // end
            iterator end()       {return dataV + dataC;}
      const_iterator end() const {return dataV + dataC;}

      //
      // free
      //
      void free()
      {
         if(!dataV) return;

         for(T *itr = dataV + dataC; itr != dataV;)
            (--itr)->~T();

         ::operator delete(dataV);
         dataV = nullptr;
         dataC = 0;
      }

      // size
      size_type size() const {return dataC;}

      // swap
      void swap(Vector<T> &v)
         {std::swap(dataV, v.dataV); std::swap(dataC, v.dataC);}

   private:
      T        *dataV;
      size_type dataC;
   };
}

#endif//ACSVM__Vector_H__

