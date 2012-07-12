// @HEADER
// ***********************************************************************
//            copyright
//
// ***********************************************************************
// @HEADER

/*! \file Zoltan2_IdentifierTraits.hpp
   \brief Defines basic traits for user global identifiers.
*/

#ifndef _ZOLTAN2_IDENTIFIERTRAITS
#define _ZOLTAN2_IDENTIFIERTRAITS

#include <Zoltan2_Standards.hpp>
#include <Zoltan2_AlltoAll.hpp>

#include <Teuchos_SerializationTraits.hpp>
#include <Teuchos_HashUtils.hpp>
#include <Teuchos_ReductionOp.hpp>

#include <utility>
#include <iostream>
#include <sstream>
#include <string>
#include <limits>
#include <cstdlib>

using Teuchos::SerializationTraits;

namespace Zoltan2
{
/*! \brief helper function to find min and max of array of user Ids
 */
template <typename T>
  std::pair<T, T> z2LocalMinMax(const T *val, size_t n)
{
  T max = numeric_limits<T>::min();
  T min = numeric_limits<T>::max();

  if (n < 1) return std::pair<T,T>(min, max);

  for (size_t i=0; i < n; i++){
    if (val[i] < min) min = val[i];
    else if (val[i] > max) max = val[i];
  }
  return std::pair<T,T>(min,max);
}

/*! \brief helper to hash values larger than int to an int.
 *  Hash values do not need to be unique, but there should be
 *  as few overlaps as possible.
 */
int getHashCode(const unsigned char *a, size_t len)
{
  int total=0;
  unsigned char *to = reinterpret_cast<unsigned char *>(&total);
  int c=0;
  for (size_t i=0; i < len; i++){
    to[c++] += a[i];
    if (c == sizeof(int))
      c = 0;
  }
  if (total < 0)
    total *= -1;
  return total;
}

/*! \brief Teuchos reduction operation
 *
 *  Because T may not be a valid Packet type, we cast it to
 *  char when doing the reduction operation.
 */
template <typename T> class Zoltan2_MinMaxOperation :
  public Teuchos::ValueTypeReductionOp<int, char>
{
public:
  void reduce( 
    const int count, const char inBuffer[], char inoutBuffer[]) const
  {
    const T *in = reinterpret_cast<const T*>(inBuffer);
    T *inout = reinterpret_cast<T*>(inoutBuffer);

    if (in[0] < inout[0]) inout[0] = in[0];
    if (in[1] > inout[1]) inout[1] = in[1];
  }
};

/*! \brief helper function to find global min and max of array of user Ids
 */
template <typename T>
  void z2GlobalMinMax(const Comm<int> &comm, 
    const T &localMin, const T &localMax, T &globalMin, T &globalMax)
{
  if (comm.getSize() == 1){
    globalMin = localMin;
    globalMax = localMax;
    return;
  }

  Zoltan2_MinMaxOperation<T> reductionOp;

  T localValues[2] = {localMin, localMax};
  T globalValues[2];

  char *lv = reinterpret_cast<char *>(&localValues[0]);
  char *gv = reinterpret_cast<char *>(&globalValues[0]);

  Teuchos::reduceAll<int, char>(comm, reductionOp, 2*sizeof(T), lv, gv);

  globalMin = globalValues[0];
  globalMax = globalValues[1];
}

/*! \brief helper function to determine if list of user Ids are consecutive
 */
template <typename T>
  bool z2AreConsecutive(const T *val, size_t n)
{
  if (n == 0) return true;

  if (val[n-1] - val[0] + 1 != T(n))
    return false;

  T nextval = val[0]+1;
  for (size_t i=1; i < n-1; i++){
    if (val[i] != nextval)
      return false;
    nextval++;
  }
  return true;
}

/*! \brief helper function write a user ID to a string
 */
template <typename T>
  std::string stringifyOrdinal(T ordinal)
{
  std::ostringstream oss;
  oss << ordinal;
  return oss.str();
}

/*!  \brief Structure to catch invalid user ID types.
 */
template<typename T>
struct UndefIdTraits
{
static inline void noop() { 
  T::UsingInvalidGlobalIdentifierDataType(); 
}
static inline T invalid() { 
  return T::UsingInvalidGlobalIdentifierDataType(); 
}
};


/*!  \brief The functions to be defined for users' global ID types.

  The template parameter is the user's global ID data type.

  The data types permitted for global identifiers for Zoltan2 callers 
  may include those that are not represented in Teuchos::OrdinalTraits.  
  A common case is when a matrix nonzero is represented as an (i,j) pair.

  In such a case, Zoltan2 will map them to a list of new IDs that 
  \em are Teuchos Ordinals.  All computation will be in the space of 
  the new global numbers.  When the Solution object
  is written, the internal global numbers are mapped back to the user's
  global IDs.

  During this process of determining if the user's IDs can be used by
  Zoltan2, and mapping them to new global numbers if they can not,
  the Traits defined here are used to manipulate the user's IDs.

  Traits are defined for the following types:
    \li char
    \li unsigned char
    \li short
    \li unsigned short
    \li int
    \li unsigned int
    \li long
    \li unsigned long
    \li long long
    \li unsigned long long
    \li std::pair<T1, T2>
    \li string

  The <tt> long long </tt>  and <tt> unsigned long long </tt> traits are only 
  defined if Trilinos was configured with the TEUCHOS_ENABLE_LONG_LONG_INT
  option.

  If the user's global ID type does not appear in the above list, it
  can be added by the user in his or her application code.  See the example
  in tobeWritten.cpp.

  \todo write an example where user's global ID is a C-struct containing
        \c i and \c j indices.

  \todo fix this note regarding gid_t
  Developer note: By convention we use \c gid_t as the users global ID
  data type and \c gno_t as the data type used internally by Zoltan2.

  Each data type which is not defined in Teuchos::DirectSerializationTraits
  must have a Zoltan2::AlltoAllv specialization in Zoltan2_AlltoAll.hpp.
*/

template<typename T>
struct IdentifierTraits {

  /*! \brief Compute an integer hash code for the user's global ID.

      \param id  the user's global id
      \result the integer code, which need not be unique for each id
   */
  static int hashCode(const T id) {
   return UndefIdTraits<int>::invalid();
  }

  /*! \brief Determine if the data type supports unique hash keys.
      \result true if unique hash keys (of data type double) can be generated
         given a value, with no knowledge about the global space of values.
   */
  static inline bool hasUniqueKey(){
   return UndefIdTraits<bool>::invalid();}

  /*! \brief Return a double which is a unique key for the value.

      \result the integer code, which need not be unique for each id

      if \c hasUniqueKey() is false, throw a logic_error.
   */

  static inline double key(const T c){return static_cast<double>(c); }

  /*! \brief The name of the identifier data type.

      \result The name
   */
  static inline std::string name() {
    return UndefIdTraits<std::string>::invalid();
  }
		
  /*! \brief A string displaying the value.

      \param  val  the value to represent as a string
      \result The string
   */
  static std::string stringify(T val) {
    return UndefIdTraits<std::string>::invalid();
  }

  /*! \brief Determine whether the data type can be a Teuchos Ordinal

      \result true if it can be a Teuchos Ordinal

     Data types are those with a definition in Teuchos::OrdinalTraits.
   */
  static inline bool isGlobalOrdinal() {
    return UndefIdTraits<bool>::invalid();
  }

  /*! \brief Compute b - a, if possible

      \param a  The \em a of b-a
      \param b  The \em b of b-a
      \result the value b-a 

       A \c std::logic_error is throw at runtime if the operation
                 is not valid for T.
   */
  static inline T difference(const T a, const T b) {
    return UndefIdTraits<bool>::invalid();
  }

  /*! \brief Determine if the data type is one for which IdentifierTraits are 
                   defined

      \result true if data type has definition
   */
  static inline bool is_valid_id_type() { return false; }

  /*! \brief Return the minimum and maximum of a list of values.
      \param values   a pointer to \c numValues values
      \param numValues   the number of values to consider
      \param min on return the minimum value in the list
      \param max on return the maximum value in the list

       A \c std::logic_error is throw at runtime if T is a type
            that can not be ordered.
   */
  static void minMax(const T *values, size_t numValues, T &min, T&max) {
    UndefIdTraits<T>::noop();
  }

  /*! \brief Find global minimum and maximum

      \param comm  Communicator for global operation
      \param localMin  The local minimum
      \param localMax  The local maximum
      \param globalMin  On return, the global minimum
      \param globalMax  On return, the global maximum

       A \c std::logic_error is throw at runtime if T is a type
            that can not be ordered.
   */
  static void globalMinMax(const Comm<int> &comm,
      const T &localMin, const T &localMax, T &globalMin, T &globalMax){
    UndefIdTraits<T>::noop();
  }

  /*! \brief Determine if the values are locally increasing consecutive
      
      \param val  a pointer to \c n values
      \param n  the number of values in the list
      \result true if the values are increasing consecutive,
            false if otherwise

       A \c std::logic_error is throw at runtime if T is a type
            that can not be ordered.
   */
  static bool areConsecutive(const T *val, size_t n){
    return UndefIdTraits<bool>::invalid();
  }

};

#ifndef DOXYGEN_SHOULD_SKIP_THIS

template<>
struct IdentifierTraits<char> {
  typedef char T;
  static inline int hashCode(const T c) {return static_cast<int>(c);}
  static inline bool hasUniqueKey() { return true;}
  static inline double key(const T c){return static_cast<double>(c); }
  static inline std::string name()     {return("char");}
  static std::string stringify(T val) {return stringifyOrdinal(val);}
  static inline bool isGlobalOrdinal() {return true; }

  static inline T difference(const T a, const T b) { return (b-a); }
  static inline bool is_valid_id_type() {return true; }
  static void minMax(const T *values, size_t n, T &min, T &max) {
    std::pair<T, T> ext = z2LocalMinMax(values, n);
    min = ext.first;
    max = ext.second;
  }
  static void globalMinMax(const Comm<int> &comm,
      const T &localMin, const T &localMax, T &globalMin, T &globalMax){
    z2GlobalMinMax(comm, localMin, localMax, globalMin, globalMax);}
  static bool areConsecutive(const T *val, size_t n){ 
   return z2AreConsecutive(val, n); }
};

template<>
struct IdentifierTraits<unsigned char> {
  typedef unsigned char T;
  static inline int hashCode(const T c) {return static_cast<int>(c);}
  static inline bool hasUniqueKey() { return true;}
  static inline double key(const T c){return static_cast<double>(c); }
  static inline std::string name()     {return("unsigned char");}
  static std::string stringify(T val) {return stringifyOrdinal(val);}
  static inline bool isGlobalOrdinal() {return true; }

  static inline T difference(const T a, const T b) { return (b-a); }
  static inline bool is_valid_id_type() {return true; }
  static void minMax(const T *values, size_t n, T &min, T &max) {
    std::pair<T, T> ext = z2LocalMinMax(values, n);
    min = ext.first;
    max = ext.second;
  }
  static void globalMinMax(const Comm<int> &comm,
      const T &localMin, const T &localMax, T &globalMin, T &globalMax){
    z2GlobalMinMax(comm, localMin, localMax, globalMin, globalMax);}
  static bool areConsecutive(const T *val, size_t n){ 
   return z2AreConsecutive(val, n); }
};

template<>
struct IdentifierTraits<short> {
  typedef short T;
  static inline int hashCode(const T  a) {return static_cast<int>(a);}
  static inline bool hasUniqueKey() { return true;}
  static inline double key(const T a){return static_cast<double>(a); }
  static inline std::string name()   {return("short");}
  static std::string stringify(T val) {return stringifyOrdinal(val);}
  static inline bool isGlobalOrdinal() {return true; }

  static inline T difference(const T a, const T b) { return (b-a); }
  static inline bool is_valid_id_type() {return true; }
  static void minMax(const T *values, size_t n, T &min, T &max) {
    std::pair<T, T> ext = z2LocalMinMax(values, n);
    min = ext.first;
    max = ext.second;
  }
  static void globalMinMax(const Comm<int> &comm,
      const T &localMin, const T &localMax, T &globalMin, T &globalMax){
    z2GlobalMinMax(comm, localMin, localMax, globalMin, globalMax);}
  static bool areConsecutive(const T *val, size_t n){ 
  return z2AreConsecutive(val, n); }
};

template<>
struct IdentifierTraits<unsigned short> {
  typedef unsigned short T;
  static inline int hashCode(const T  a) {return static_cast<int>(a);}
  static inline bool hasUniqueKey() { return true;}
  static inline double key(const T a){return static_cast<double>(a); }
  static inline std::string name()   {return("unsigned short");}
  static std::string stringify(T val) {return stringifyOrdinal(val);}
  static inline bool isGlobalOrdinal() {return true; }

  static inline T difference(const T a, const T b) { return (b-a); }
  static inline bool is_valid_id_type() {return true; }
  static void minMax(const T *values, size_t n, T &min, T &max) {
    std::pair<T, T> ext = z2LocalMinMax(values, n);
    min = ext.first;
    max = ext.second;
  }
  static void globalMinMax(const Comm<int> &comm,
      const T &localMin, const T &localMax, T &globalMin, T &globalMax){
    z2GlobalMinMax(comm, localMin, localMax, globalMin, globalMax);}
  static bool areConsecutive(const T *val, size_t n){ 
  return z2AreConsecutive(val, n); }
};

template<>
struct IdentifierTraits<int> {
  typedef int T;
  static inline int hashCode(const T  a) {return static_cast<int>(a);}
  static inline bool hasUniqueKey() { return true;}
  static inline double key(const T a){return static_cast<double>(a); }
  static inline std::string name()   {return("int");}
  static std::string stringify(T val) {return stringifyOrdinal(val);}
  static inline bool isGlobalOrdinal() {return true; }

  static inline T difference(const T a, const T b) { return (b-a); }
  static inline bool is_valid_id_type() {return true; }
  static void minMax(const T *values, size_t n, T &min, T &max) {
    std::pair<T, T> ext = z2LocalMinMax(values, n);
    min = ext.first;
    max = ext.second;
  }
  static void globalMinMax(const Comm<int> &comm,
      const T &localMin, const T &localMax, T &globalMin, T &globalMax){
    z2GlobalMinMax(comm, localMin, localMax, globalMin, globalMax);}
  static bool areConsecutive(const T *val, size_t n){ 
  return z2AreConsecutive(val, n); }
};

template<>
struct IdentifierTraits<unsigned int> {
  typedef unsigned int T;
  static inline int hashCode(const T  a) {return static_cast<int>(a);}
  static inline bool hasUniqueKey() { return true;}
  static inline double key(const T a){return static_cast<double>(a); }
  static inline std::string name()   {return("unsigned int");}
  static std::string stringify(T val) {return stringifyOrdinal(val);}
  static inline bool isGlobalOrdinal() {return true; }

  static inline T difference(const T a, const T b) { return (b-a); }
  static inline bool is_valid_id_type() {return true; }
  static void minMax(const T *values, size_t n, T &min, T &max) {
    std::pair<T, T> ext = z2LocalMinMax(values, n);
    min = ext.first;
    max = ext.second;
  }
  static void globalMinMax(const Comm<int> &comm,
      const T &localMin, const T &localMax, T &globalMin, T &globalMax){
    z2GlobalMinMax(comm, localMin, localMax, globalMin, globalMax);}
  static bool areConsecutive(const T *val, size_t n){ 
  return z2AreConsecutive(val, n); }
};


template<>
struct IdentifierTraits<long> {
  typedef long T;
  static inline int hashCode(const T a) {
    return getHashCode(
      reinterpret_cast<const unsigned char *>(&a), sizeof(T));}
  static inline bool hasUniqueKey() { return true;}
  static inline double key(const T a){return static_cast<double>(a); }
  static inline std::string name()    {return("long");}
  static std::string stringify(T val) {return stringifyOrdinal(val);}
  static inline bool isGlobalOrdinal() {return true; }
  static inline T difference(const T a, const T b) { return (b-a); }
  static inline bool is_valid_id_type() {return true; }
  static void minMax(const T *values, size_t n, T &min, T &max) {
    std::pair<T, T> ext = z2LocalMinMax(values, n);
    min = ext.first;
    max = ext.second;
  }
  static void globalMinMax(const Comm<int> &comm,
      const T &localMin, const T &localMax, T &globalMin, T &globalMax){
    z2GlobalMinMax(comm, localMin, localMax, globalMin, globalMax);}
  static bool areConsecutive(const T *val, size_t n){
    return z2AreConsecutive(val, n); }
};

template<>
struct IdentifierTraits<unsigned long> {
  typedef unsigned long T;
  static inline int hashCode(const T a) { 
    return getHashCode(
      reinterpret_cast<const unsigned char *>(&a), sizeof(T));}
  static inline bool hasUniqueKey() { return true;}
  static inline double key(const T a){return static_cast<double>(a);}
  static inline std::string name()   {return("unsigned long");}
  static std::string stringify(T val) {return stringifyOrdinal(val);}
  static inline bool isGlobalOrdinal() {return true; }
  static inline T difference(const T a, const T b) { return (b-a); }
  static inline bool is_valid_id_type() {return true; }
  static void minMax(const T *values, size_t n, T &min, T &max) {
    std::pair<T, T> ext = z2LocalMinMax(values, n);
    min = ext.first;
    max = ext.second;
  }
  static void globalMinMax(const Comm<int> &comm,
      const T &localMin, const T &localMax, T &globalMin, T &globalMax){
    z2GlobalMinMax(comm, localMin, localMax, globalMin, globalMax);}
  static bool areConsecutive(const T *val, size_t n){
    return z2AreConsecutive(val, n); }
};

#ifdef HAVE_ZOLTAN2_LONG_LONG

template<>
struct IdentifierTraits<long long> {
  typedef long long T;
  static inline int hashCode(const T a) {
    return getHashCode(
      reinterpret_cast<const unsigned char *>(&a), sizeof(T));}
  static inline bool hasUniqueKey() { return true;}
  static inline double key(const T a){return static_cast<double>(a); }
  static inline std::string name()    {return("long long");}
  static std::string stringify(T val) {return stringifyOrdinal(val);}
  static inline bool isGlobalOrdinal() {return true; }
  static inline T difference(const T a, const T b) { return (b-a); }
  static inline bool is_valid_id_type() {return true; }
  static void minMax(const T *values, size_t n, T &min, T &max) {
    std::pair<T, T> ext = z2LocalMinMax(values, n);
    min = ext.first;
    max = ext.second;
  }
  static void globalMinMax(const Comm<int> &comm,
      const T &localMin, const T &localMax, T &globalMin, T &globalMax){
    z2GlobalMinMax(comm, localMin, localMax, globalMin, globalMax);}
  static bool areConsecutive(const T *val, size_t n){ 
    return z2AreConsecutive(val, n); }
};

template<>
struct IdentifierTraits<unsigned long long> {
  typedef unsigned long long T;
  static inline int hashCode(const T a) {
    return getHashCode(
      reinterpret_cast<const unsigned char *>(&a), sizeof(T));}
  static inline bool hasUniqueKey() { return true;}
  static inline double key(const T a){return static_cast<double>(a);}
  static inline std::string name()   {return("unsigned long long");}
  static std::string stringify(T val) {return stringifyOrdinal(val);}
  static inline bool isGlobalOrdinal() {return true; }
  static inline T difference(const T a, const T b) { return (b-a); }
  static inline bool is_valid_id_type() {return true; }
  static void minMax(const T *values, size_t n, T &min, T &max) {
    std::pair<T, T> ext = z2LocalMinMax(values, n);
    min = ext.first;
    max = ext.second;
  }
  static void globalMinMax(const Comm<int> &comm,
      const T &localMin, const T &localMax, T &globalMin, T &globalMax){
    z2GlobalMinMax(comm, localMin, localMax, globalMin, globalMax);}
  static bool areConsecutive(const T *val, size_t n){ 
    return z2AreConsecutive(val, n); }
};

#endif

template<>
struct IdentifierTraits<string> {
  typedef string T;
  static inline int hashCode(const T a) {
    return getHashCode(
      reinterpret_cast<const unsigned char *>(a.c_str()), a.size());}
  static inline bool hasUniqueKey() { return false;}
  static inline double key(const T a){ throw std::logic_error("invalid call");}
  static inline std::string name()   {return("string");}
  static std::string stringify(T val) {return val;}
  static inline bool isGlobalOrdinal() {return false; }
  static inline T difference(const T a, const T b) { 
    throw std::logic_error("invalid call");}
  static inline bool is_valid_id_type() {return true; }
  static void minMax(const T *values, size_t n, T &min, T &max) {
    throw std::logic_error("invalid call");}
  static void globalMinMax(const Comm<int> &comm,
      const T &localMin, const T &localMax, T &globalMin, T &globalMax){
    throw std::logic_error("invalid call");}
  static bool areConsecutive(const T *val, size_t n){ 
    throw std::logic_error("invalid call");}
};

template<typename T1, typename T2>
struct IdentifierTraits<std::pair<T1, T2> > {
  typedef std::pair<T1, T2> pair_t;
  typedef typename std::pair<pair_t, pair_t> pairPair_t;

  static inline int hashCode(const pair_t p)  {
    return IdentifierTraits<T1>::hashCode(p.first) +
      IdentifierTraits<T2>::hashCode(p.second);
  }

  static inline bool hasUniqueKey() { 
    if ((sizeof(T1)*2 <= sizeof(double))&&(sizeof(T2)*2 <= sizeof(double)))
      return true;
    else
      return false;
  }

  static inline double key(const pair_t p)  {
    size_t nbytes = sizeof(double) / 2;
    if ((sizeof(T1) > nbytes)||(sizeof(T2) > nbytes))
      throw std::logic_error("invalid call");
    else{
      double keyVal=0;
      char *cx = reinterpret_cast<char *>(&keyVal);
      char *cy = cx + nbytes;
      T1 *xpos = reinterpret_cast<T1 *>(cx + nbytes-sizeof(T1));
      T2 *ypos = reinterpret_cast<T2 *>(cy + nbytes-sizeof(T2));
      *xpos = p.first;
      *ypos = p.second;
      return keyVal;
    }
  }

  static inline std::string name() {
     std::string s("std::pair<");
     s += IdentifierTraits<T1>::name();
     s += std::string(", ");
     s += IdentifierTraits<T2>::name();
     s += std::string(">");
     return s;
  }

  static std::string stringify(pair_t val) {
    std::ostringstream oss;
    oss << "pair<" << IdentifierTraits<T1>::name();
    oss << "," << IdentifierTraits<T2>::name();
    oss << ">(" << val.first << "," << val.second << ")";
    return oss.str();
  }

  static inline bool isGlobalOrdinal() {return false; }

  static inline pair_t difference( const pair_t a, const pair_t b) {
      throw std::logic_error("invalid call");
      return pair_t();}

  static inline bool is_valid_id_type() {
    return (sizeof(T1)+sizeof(T2) <= sizeof(double)); }

  static void minMax(const pair_t *values, size_t n, pair_t &min, pair_t &max) {
      throw std::logic_error("invalid call");}

  static void globalMinMax(const Comm<int> &comm,
      const pair_t &localMin, const pair_t &localMax, 
      pair_t &globalMin, pair_t &globalMax){
      throw std::logic_error("invalid call");}

  static bool areConsecutive(const pair_t *val, size_t n){ 
      throw std::logic_error("invalid call");
      return false; }
};

//////////////////////////////////////////////////////////////
//  A helper function.  If T is an ordinal, are the values
//    globally consecutive?  If so return true, otherwise
//    return false.
//
//  On return, globalLen is set to the sum of the local lengths.
//
//  If T is an ordinal, but the list is not globally consecutive, 
//    on return dist[0] is set to the global minimum of
//    the values and dist[1] to the global maximum.
//    
//  If T is an ordinal and the list is globally consecutive,
//    on return dist[p] is set to val[0] on process p.  dist[nprocs]
//    is set to one past the global maximum value.
//////////////////////////////////////////////////////////////

template <typename T>
  bool globallyConsecutiveOrdinals(
    const Comm<int> &comm, const Environment &env, 
    const T* val, size_t len,
    ArrayRCP<T> &dist, size_t &globalLen)
{
  try{
    reduceAll<int, size_t>(comm, Teuchos::REDUCE_SUM, 1, &len, &globalLen);
  }
  Z2_THROW_OUTSIDE_ERROR(env);

  if (!IdentifierTraits<T>::isGlobalOrdinal()){
    return false;
  }

  // Get global minimum and maximum

  T gMin, gMax;
  T v0 = val[0];
  T v1 = val[len-1];

  try{
    IdentifierTraits<T>::globalMinMax(comm, v0, v1, gMin, gMax);
  }
  Z2_FORWARD_EXCEPTIONS; 

  T *minMax = new T [2];
  minMax[0] = gMin;
  minMax[1] = gMax;
  dist = arcp<T>(minMax, 0, 2);

  size_t g0 = Teuchos::as<size_t>(gMin);
  size_t g1 = Teuchos::as<size_t>(gMax);
  bool globallyConsecutive = false;

  if (g1 - g0 + 1 == globalLen){

    size_t sentinel = g1 + 1;
    int nprocs = comm.getSize();
    bool locallyConsecutive = IdentifierTraits<T>::areConsecutive(val, len);

    if (locallyConsecutive && nprocs==1){
      dist[nprocs] = Teuchos::as<T>(sentinel);
      return true;
    }

    int lFlag = (locallyConsecutive ? 1 : 0);
    int gFlag = 0;

    try{
      reduceAll<int, int>(comm, Teuchos::REDUCE_MIN, 1, &lFlag, &gFlag);
    }
    Z2_THROW_OUTSIDE_ERROR(env);

    if (gFlag == 0)  // not all processes have consecutive values
      return false;

    Array<size_t> sendBuf(nprocs);
    ArrayRCP<size_t> recvBuf;

    for (int i=0; i < nprocs; i++)
      sendBuf[i] = Teuchos::as<size_t>(v0);

    try{
      AlltoAll<size_t>(comm, env, sendBuf, 1, recvBuf);
    }
    Z2_FORWARD_EXCEPTIONS;

    globallyConsecutive = true;
    for (int i=1; globallyConsecutive && i < nprocs; i++)
      if (recvBuf[i] < recvBuf[i-1]) globallyConsecutive = false;

    if (globallyConsecutive){
      T *idDist = new T [nprocs+1];
      for (int i=0; i < nprocs; i++)
        idDist[i] = Teuchos::as<T>(recvBuf[i]);
      idDist[nprocs] = Teuchos::as<T>(sentinel);
      dist = arcp(idDist, 0, nprocs+1);
    }
  }

  return globallyConsecutive;
}

#endif  // DOXYGEN_SHOULD_SKIP_THIS

} // namespace Z2

#endif // _ZOLTAN2_IDENTIFIERTRAITS
