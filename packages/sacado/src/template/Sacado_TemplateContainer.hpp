// @HEADER
// ***********************************************************************
//
//                           Sacado Package
//                 Copyright (2006) Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// This library is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 2.1 of the
// License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301
// USA
// Questions? Contact David M. Gay (dmgay@sandia.gov) or Eric T. Phipps
// (etphipp@sandia.gov).
//
// ***********************************************************************
// @HEADER

#ifndef SACADO_TEMPLATE_CONTAINER_HPP
#define SACADO_TEMPLATE_CONTAINER_HPP

#include "Sacado_ConfigDefs.h"
#ifdef HAVE_SACADO_CXX11

#include <tuple>

#include "Sacado_mpl_size.hpp"
#include "Sacado_mpl_find.hpp"
#include "Sacado_mpl_for_each.hpp"
#include "Sacado_mpl_apply.hpp"
#include "Sacado_mpl_begin.hpp"
#include "Sacado_mpl_end.hpp"
#include "Sacado_mpl_deref.hpp"
#include "Sacado_mpl_next.hpp"

namespace Sacado {

  namespace Impl {

    // Forward declaration
    template <class Seq,
              class ObjectT,
              class Iter1 = typename mpl::begin<Seq>::type,
              class Iter2 = typename mpl::end<Seq>::type>
    struct MakeTupleType;

  } // namespace Impl


  //! Container class to manager template instantiations of a template class
  /*!
   * This class provides a generic container class for managing multiple
   * instantiations of a class ObjectT<T> with the values of T specified
   * through a type sequence.  Objects of type ObjectT<T> must have value
   * semantics, be default constructable and copyable/assignable (for objects
   * that don't satisfy these requirements, one would typically wrap them in
   * a (smart) pointer.  Methods are provided to access the object of type
   * ObjectT<T> for a given type T, as well as initialize them.  One would
   * typically operate on the contained objects using mpl::for_each(), e.g.,
   *
   *    template <class T> class MyClass;
   *    typedef mpl::vector< double, Fad::DFad<double> > MyTypes;
   *    typedef TemplateContainer< MyTypes, MyClass<_> > MyObjects;
   *    MyObjects myObjects;
   *    mpl::for_each<MyObjects>([](auto x) {
   *       typedef decltype(x) T;
   *       myObjects.get<T>() = T;
   *    });
   *
   * (Note:  This requires generalized lambda's introduced in C++14.  For
   * C++11, provide a functor that implements the lambda body).
   */
  template <typename TypeSeq, typename ObjectT>
  class TemplateContainer {

    template <typename TupleT, typename BuilderOpT>
    struct BuildObject {
      TupleT& objects;
      const BuilderOpT& builder;
      BuildObject(TupleT& objects_, const BuilderOpT& builder_) :
        objects(objects_), builder(builder_) {}
      template <typename T> void operator()(T x) const {
        std::get< mpl::find<TypeSeq,T>::value >(objects) = builder(x);
      }
    };

  public:

    //! Type sequence containing template types
    typedef TypeSeq types;

    //! The default builder class for building objects for each ScalarT
    struct DefaultBuilderOp {

      //! Returns a new object of type ObjectT<ScalarT>
      template<class T>
      typename Sacado::mpl::apply<ObjectT,T>::type operator() (T) const {
        return typename Sacado::mpl::apply<ObjectT,T>::type();
      }

    };

    //! Default constructor
    TemplateContainer() {}

    //! Destructor
    ~TemplateContainer() {}

    //! Get object corrensponding ObjectT<T>
    template<typename T>
    typename Sacado::mpl::apply<ObjectT,T>::type& get() {
      return std::get< Sacado::mpl::find<TypeSeq,T>::value >(objects);
    }

    //! Get object corrensponding ObjectT<T>
    template<typename T>
    const typename Sacado::mpl::apply<ObjectT,T>::type& get() const  {
      return std::get< Sacado::mpl::find<TypeSeq,T>::value >(objects);
    }

    //! Build objects for each type T
    template <typename BuilderOpT = DefaultBuilderOp>
    void build(const BuilderOpT& builder) {
      Sacado::mpl::for_each_no_kokkos<TypeSeq>(BuildObject<tuple_type,BuilderOpT>(objects,builder));
    }

  private:

    //! Our container for storing each object
    typedef typename Impl::MakeTupleType<TypeSeq, ObjectT>::type tuple_type;

    //! Stores type of objects of each type
    tuple_type objects;

  };

  // Wrap call to mpl::for_each so you don't have to specify the container
  // or type sequence
  template <typename TypeSeq, typename ObjectT, typename FunctorT>
  void container_for_each(TemplateContainer<TypeSeq,ObjectT>& container,
                          const FunctorT& op) {
    typedef TemplateContainer<TypeSeq,ObjectT> Container;
    Sacado::mpl::for_each<Container> f(op);
  }

  // Wrap call to mpl::for_each so you don't have to specify the container
  // or type sequence
  template <typename TypeSeq, typename ObjectT, typename FunctorT>
  void container_for_each_no_kokkos(TemplateContainer<TypeSeq,ObjectT>& container,
                                    const FunctorT& op) {
    typedef TemplateContainer<TypeSeq,ObjectT> Container;
    Sacado::mpl::for_each_no_kokkos<Container> f(op);
  }

  namespace mpl {

    // Give TemplateContainer begin<> and end<> iterators for for_each

    template <typename TypeSeq, typename ObjectT>
    struct begin< TemplateContainer<TypeSeq,ObjectT> > {
      typedef typename begin<TypeSeq>::type type;
    };

    template <typename TypeSeq, typename ObjectT>
    struct end< TemplateContainer<TypeSeq,ObjectT> > {
      typedef typename end<TypeSeq>::type type;
    };

  }

  namespace Impl {

    template <class TupleType,
              class ObjectT,
              class Iter1,
              class Iter2>
    struct MakeTupleTypeImpl{};

    template <class ... TupleArgs,
              class ObjectT,
              class Iter1,
              class Iter2>
    struct MakeTupleTypeImpl< std::tuple<TupleArgs...>,
                              ObjectT,
                              Iter1,
                              Iter2 >{
      typedef std::tuple<
        TupleArgs...,
        typename mpl::apply<ObjectT, typename mpl::deref<Iter1>::type >::type >
      tuple_type;

      typedef typename MakeTupleTypeImpl< tuple_type,
                                          ObjectT,
                                          typename mpl::next<Iter1>::type,
                                          Iter2 >::type type;
    };

    template <class ... TupleArgs,
              class ObjectT,
              class Iter1>
    struct MakeTupleTypeImpl< std::tuple<TupleArgs...>,
                              ObjectT,
                              Iter1,
                              Iter1 >{
      typedef std::tuple< TupleArgs... > type;
    };

    template <class Seq,
              class ObjectT,
              class Iter1,
              class Iter2>
    struct MakeTupleType{
      typedef typename MakeTupleTypeImpl< std::tuple<>,
                                          ObjectT,
                                          Iter1,
                                          Iter2 >::type type;
    };

  } // namespace Impl

}

#endif
#endif
