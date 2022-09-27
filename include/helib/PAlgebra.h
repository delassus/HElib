/* Copyright (C) 2012-2020 IBM Corp.
 * This program is Licensed under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *   http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. See accompanying LICENSE file.
 */
#ifndef HELIB_PALGEBRA_H
#define HELIB_PALGEBRA_H
/**
 * @file PAlgebra.h
 * @brief Declarations of the classes PAlgebra
 */
#include <exception>
#include <utility>
#include <vector>
#include <complex>

#include <helib/NumbTh.h>
#include <helib/zzX.h>
#include <helib/hypercube.h>
#include <helib/PGFFT.h>
#include <helib/ClonedPtr.h>
#include <helib/apiAttributes.h>

namespace helib {

struct half_FFT
{
  PGFFT fft;
  std::vector<std::complex<double>> pow;

  half_FFT(long m);
};

struct quarter_FFT
{
  PGFFT fft;
  std::vector<std::complex<double>> pow1, pow2;

  quarter_FFT(long m);
};

/**
 * @class PAlgebra
 * @brief The structure of (Z/mZ)* /(p)
 *
 * A PAlgebra object is determined by an integer m and a prime p, where p does
 * not divide m. It holds information describing the structure of (Z/mZ)^*,
 * which is isomorphic to the Galois group over A = Z[X]/Phi_m(X)).
 *
 * We represent (Z/mZ)^* as (Z/mZ)^* = (p) x (g1,g2,...) x (h1,h2,...)
 * where the group generated by g1,g2,... consists of the elements that
 * have the same order in (Z/mZ)^* as in (Z/mZ)^* /(p,g_1,...,g_{i-1}), and
 * h1,h2,... generate the remaining quotient group (Z/mZ)^* /(p,g1,g2,...).
 *
 * We let T subset (Z/mZ)^* be a set of representatives for the quotient
 * group (Z/mZ)^* /(p), defined as T={ prod_i gi^{ei} * prod_j hj^{ej} }
 * where the ei's range over 0,1,...,ord(gi)-1 and the ej's range over
 * 0,1,...ord(hj)-1 (these last orders are in (Z/mZ)^* /(p,g1,g2,...)).
 *
 * Phi_m(X) is factored as Phi_m(X)= prod_{t in T} F_t(X) mod p,
 * where the F_t's are irreducible modulo p. An arbitrary factor
 * is chosen as F_1, then for each t in T we associate with the index t the
 * factor F_t(X) = GCD(F_1(X^t), Phi_m(X)).
 *
 * Note that fixing a representation of the field R=(Z/pZ)[X]/F_1(X)
 * and letting z be a root of F_1 in R (which
 * is a primitive m-th root of unity in R), we get that F_t is the minimal
 * polynomial of z^{1/t}.
 */
class PAlgebra
{
  long m; // the integer m defines (Z/mZ)^*, Phi_m(X), etc.
  long p; // the prime base of the plaintext space

  long phiM;      // phi(m)
  long ordP;      // the order of p in (Z/mZ)^*
  long nfactors;  // number of distinct prime factors of m
  long radm;      // rad(m) = prod of distinct primes dividing m
  double normBnd; // max-norm-on-pwfl-basis <= normBnd * max-norm-canon-embed
  double polyNormBnd; // max-norm-on-poly-basis <= polyNormBnd *
                      // max-norm-canon-embed

  long pow2; // if m = 2^k, then pow2 == k; otherwise, pow2 == 0

  std::vector<long> gens; // Our generators for (Z/mZ)^* (other than p)

  //  native[i] is true iff gens[i] has the same order in the quotient
  //  group as its order in Zm*.
  NTL::Vec<bool> native;

  // frob_perturb[i] = j if gens[i] raised to its order equals p^j,
  // otherwise -1
  NTL::Vec<long> frob_perturb;

  CubeSignature cube; // the hypercube structure of Zm* /(p)

  NTL::ZZX PhimX; // Holds the integer polynomial Phi_m(X)

  double cM; // the "ring constant" c_m for Z[X]/Phi_m(X)
  // NOTE: cM is related to the ratio between the l_infinity norm of
  // a "random" ring element in different bases. For example, think of
  // choosing the power-basis coefficients of x uniformly at random in
  // [+-a/2] (for some parameter a), then the powerful basis norm of x
  // should be bounded whp by cM*a.
  //
  // More precisely, for an element x whose coefficients are chosen
  // uniformly in [+-a/2] (in either the powerful or the power basis)
  // we have a high-probability bound |x|_canonical < A*a for some
  // A = O(sqrt(phi(m)). Also for "random enough" x we have some bound
  //       |x|_powerful < |x|_canonical * B
  // where we "hope" that B = O(1/sqrt(phi(m)). The cM value is
  // supposed to be cM=A*B.
  //
  // The value cM is only used for bootstrapping, see more comments
  // for the method RecryptData::setAE in recryption.cpp. Also see
  // Appendix A of https://ia.cr/2014/873 (updated version from 2019)

  std::vector<long> T;    // The representatives for the quotient group Zm* /(p)
  std::vector<long> Tidx; // i=Tidx[t] is the index i s.t. T[i]=t.
                          // Tidx[t]==-1 if t notin T

  std::vector<long> zmsIdx; // if t is the i'th element in Zm* then zmsIdx[t]=i
                            // zmsIdx[t]==-1 if t notin Zm*

  std::vector<long> zmsRep; // inverse of zmsIdx

  std::shared_ptr<PGFFT> fftInfo; // info for computing m-point complex FFT's
                                  // shard_ptr allows delayed initialization
                                  // and lightweight copying

  std::shared_ptr<half_FFT> half_fftInfo;
  // an optimization for FFT's with even m

  std::shared_ptr<quarter_FFT> quarter_fftInfo;
  // an optimization for FFT's with m = 0 (mod 4)

public:
  PAlgebra& operator=(const PAlgebra&) = delete;

  PAlgebra(long mm,
           long pp = 2,
           const std::vector<long>& _gens = std::vector<long>(),
           const std::vector<long>& _ords = std::vector<long>()); // constructor

  bool operator==(const PAlgebra& other) const;
  bool operator!=(const PAlgebra& other) const { return !(*this == other); }
  // comparison

  /* I/O methods */

  //! Prints the structure in a readable form
  void printout(std::ostream& out = std::cout) const;
  void printAll(std::ostream& out = std::cout) const; // print even more

  /* Access methods */

  //! Returns m
  long getM() const { return m; }

  //! Returns p
  long getP() const { return p; }

  //! Returns phi(m)
  long getPhiM() const { return phiM; }

  //! The order of p in (Z/mZ)^*
  long getOrdP() const { return ordP; }

  //! The number of distinct prime factors of m
  long getNFactors() const { return nfactors; }

  //! getRadM() = prod of distinct prime factors of m
  long getRadM() const { return radm; }

  //! max-norm-on-pwfl-basis <= normBnd * max-norm-canon-embed
  double getNormBnd() const { return normBnd; }

  //! max-norm-on-pwfl-basis <= polyNormBnd * max-norm-canon-embed
  double getPolyNormBnd() const { return polyNormBnd; }

  //! The number of plaintext slots = phi(m)/ord(p)
  long getNSlots() const { return cube.getSize(); }

  //! if m = 2^k, then pow2 == k; otherwise, pow2 == 0
  long getPow2() const { return pow2; }

  //! The cyclotomix polynomial Phi_m(X)
  const NTL::ZZX& getPhimX() const { return PhimX; }

  //! The "ring constant" cM
  double get_cM() const { return cM; }

  //! The prime-power factorization of m
  //  const std::vector<long> getMfactors() const { return mFactors; }

  //! The number of generators in (Z/mZ)^* /(p)
  long numOfGens() const { return gens.size(); }

  //! the i'th generator in (Z/mZ)^* /(p) (if any)
  long ZmStarGen(long i) const { return (i < long(gens.size())) ? gens[i] : 0; }

  //! the i'th generator to the power j mod m
  // VJS: I'm moving away from all of this unsigned stuff...
  // Also, note that j really may be negative
  // NOTE: i == -1 means Frobenius
  long genToPow(long i, long j) const;

  // p to the power j mod m
  long frobeniusPow(long j) const;

  //! The order of i'th generator (if any)
  long OrderOf(long i) const { return cube.getDim(i); }

  //! The product prod_{j=i}^{n-1} OrderOf(i)
  long ProdOrdsFrom(long i) const { return cube.getProd(i); }

  //! Is ord(i'th generator) the same as its order in (Z/mZ)^*?
  bool SameOrd(long i) const { return native[i]; }

  // FrobPerturb[i] = j if gens[i] raised to its order equals p^j,
  // where j in [0..ordP), otherwise -1
  long FrobPerturb(long i) const { return frob_perturb[i]; }

  //! @name Translation between index, representatives, and exponents

  //! Returns the i'th element in T
  long ith_rep(long i) const { return (i < getNSlots()) ? T[i] : 0; }

  //! Returns the index of t in T
  long indexOfRep(long t) const { return (t > 0 && t < m) ? Tidx[t] : -1; }

  //! Is t in T?
  bool isRep(long t) const { return (t > 0 && t < m && Tidx[t] > -1); }

  //! Returns the index of t in (Z/mZ)*
  long indexInZmstar(long t) const { return (t > 0 && t < m) ? zmsIdx[t] : -1; }

  //! Returns the index of t in (Z/mZ)* -- no range checking
  long indexInZmstar_unchecked(long t) const { return zmsIdx[t]; }

  //! Returns rep whose index is i
  long repInZmstar_unchecked(long idx) const { return zmsRep[idx]; }

  bool inZmStar(long t) const { return (t > 0 && t < m && zmsIdx[t] > -1); }

  //! @brief Returns prod_i gi^{exps[i]} mod m. If onlySameOrd=true,
  //! use only generators that have the same order as in (Z/mZ)^*.
  long exponentiate(const std::vector<long>& exps,
                    bool onlySameOrd = false) const;

  //! @brief Returns coordinate of index k along the i'th dimension.
  long coordinate(long i, long k) const { return cube.getCoord(k, i); }

  //! Break an index into the hypercube to index of the dimension-dim
  //! subcube and index inside that subcube.
  std::pair<long, long> breakIndexByDim(long idx, long dim) const
  {
    return cube.breakIndexByDim(idx, dim);
  }

  //! The inverse of breakIndexByDim
  long assembleIndexByDim(std::pair<long, long> idx, long dim) const
  {
    return cube.assembleIndexByDim(idx, dim);
  }

  //! @brief adds offset to index k in the i'th dimension
  long addCoord(long i, long k, long offset) const
  {
    return cube.addCoord(k, i, offset);
  }

  /* Miscellaneous */

  //! exps is an array of exponents (the dLog of some t in T), this function
  //! increment exps lexicographic order, return false if it cannot be
  //! incremented (because it is at its maximum value)
  bool nextExpVector(std::vector<long>& exps) const
  {
    return cube.incrementCoords(exps);
  }

  //! The largest FFT we need to handle degree-m polynomials
  long fftSizeNeeded() const { return NTL::NextPowerOfTwo(getM()) + 1; }
  // TODO: should have a special case when m is power of two

  const PGFFT& getFFTInfo() const { return *fftInfo; }
  const half_FFT& getHalfFFTInfo() const { return *half_fftInfo; }
  const quarter_FFT& getQuarterFFTInfo() const { return *quarter_fftInfo; }
};

enum PA_tag
{
  PA_GF2_tag,
  PA_zz_p_tag,
  PA_cx_tag
};

/**
@class: PAlgebraMod
@brief The structure of Z[X]/(Phi_m(X), p)

An object of type PAlgebraMod stores information about a PAlgebra object
zMStar, and an integer r. It also provides support for encoding and decoding
plaintext slots.

the PAlgebra object zMStar defines (Z/mZ)^* /(0), and the PAlgebraMod object
stores various tables related to the polynomial ring Z/(p^r)[X].  To do this
most efficiently, if p == 2 and r == 1, then these polynomials are represented
as GF2X's, and otherwise as zz_pX's. Thus, the types of these objects are not
determined until run time. As such, we need to use a class hierarchy, as
follows.

\li PAlgebraModBase is a virtual class

\li PAlgebraModDerived<type> is a derived template class, where
  type is either PA_GF2 or PA_zz_p.

\li The class PAlgebraMod is a simple wrapper around a smart pointer to a
  PAlgebraModBase object: copying a PAlgebra object results is a "deep copy" of
  the underlying object of the derived class. It provides dDirect access to the
  virtual methods of PAlgebraModBase, along with a "downcast" operator to get a
  reference to the object as a derived type, and also == and != operators.
**/

//! \cond FALSE (make doxygen ignore these classes)
class DummyBak
{
  // placeholder class used in GF2X impl

public:
  void save() {}
  void restore() const {}
};

class DummyContext
{
  // placeholder class used in GF2X impl

public:
  void save() {}
  void restore() const {}
  DummyContext() {}
  DummyContext(long) {}
};

class DummyModulus
{};
// placeholder class for CKKS

// some stuff to help with template code
template <typename R>
struct GenericModulus
{
};

template <>
struct GenericModulus<NTL::zz_p>
{
  static void init(long p) { NTL::zz_p::init(p); }
};

template <>
struct GenericModulus<NTL::GF2>
{
  static void init(long p)
  {
    assertEq<InvalidArgument>(p, 2l, "Cannot init NTL::GF2 with p not 2");
  }
};

class PA_GF2
{
  // typedefs for algebraic structures built up from GF2

public:
  static const PA_tag tag = PA_GF2_tag;
  typedef NTL::GF2 R;
  typedef NTL::GF2X RX;
  typedef NTL::vec_GF2X vec_RX;
  typedef NTL::GF2XModulus RXModulus;
  typedef DummyBak RBak;
  typedef DummyContext RContext;
  typedef NTL::GF2E RE;
  typedef NTL::vec_GF2E vec_RE;
  typedef NTL::mat_GF2E mat_RE;
  typedef NTL::GF2EX REX;
  typedef NTL::GF2EBak REBak;
  typedef NTL::vec_GF2EX vec_REX;
  typedef NTL::GF2EContext REContext;
  typedef NTL::mat_GF2 mat_R;
  typedef NTL::vec_GF2 vec_R;
};

class PA_zz_p
{
  // typedefs for algebraic structures built up from zz_p

public:
  static const PA_tag tag = PA_zz_p_tag;
  typedef NTL::zz_p R;
  typedef NTL::zz_pX RX;
  typedef NTL::vec_zz_pX vec_RX;
  typedef NTL::zz_pXModulus RXModulus;
  typedef NTL::zz_pBak RBak;
  typedef NTL::zz_pContext RContext;
  typedef NTL::zz_pE RE;
  typedef NTL::vec_zz_pE vec_RE;
  typedef NTL::mat_zz_pE mat_RE;
  typedef NTL::zz_pEX REX;
  typedef NTL::zz_pEBak REBak;
  typedef NTL::vec_zz_pEX vec_REX;
  typedef NTL::zz_pEContext REContext;
  typedef NTL::mat_zz_p mat_R;
  typedef NTL::vec_zz_p vec_R;
};

class PA_cx
{
  // typedefs for algebraic structures built up from complex<double>

public:
  static const PA_tag tag = PA_cx_tag;
  typedef double R;
  typedef std::complex<double> RX;
  typedef NTL::Vec<RX> vec_RX;
  typedef DummyModulus RXModulus;
  typedef DummyBak RBak;
  typedef DummyContext RContext;

  // the other typedef's should not ever be used...they
  // are all defined as void, so that PA_INJECT still works
  typedef void RE;
  typedef void vec_RE;
  typedef void mat_RE;
  typedef void REX;
  typedef void REBak;
  typedef void vec_REX;
  typedef void REContext;
  typedef void mat_R;
  typedef void vec_R;
};

//! \endcond

//! Virtual base class for PAlgebraMod
class PAlgebraModBase
{

public:
  virtual ~PAlgebraModBase() {}
  virtual PAlgebraModBase* clone() const = 0;

  //! Returns the type tag: PA_GF2_tag or PA_zz_p_tag
  virtual PA_tag getTag() const = 0;

  //! Returns reference to underlying PAlgebra object
  virtual const PAlgebra& getZMStar() const = 0;

  //! Returns reference to the factorization of Phi_m(X) mod p^r, but as ZZX's
  virtual const std::vector<NTL::ZZX>& getFactorsOverZZ() const = 0;

  //! The value r
  virtual long getR() const = 0;

  //! The value p^r
  virtual long getPPowR() const = 0;

  //! Restores the NTL context for p^r
  virtual void restoreContext() const = 0;

  virtual zzX getMask_zzX(long i, long j) const = 0;
};

#ifndef DOXYGEN_IGNORE
#define PA_INJECT(typ)                                                         \
  static const PA_tag tag = typ::tag;                                          \
  typedef typename typ::R R;                                                   \
  typedef typename typ::RX RX;                                                 \
  typedef typename typ::vec_RX vec_RX;                                         \
  typedef typename typ::RXModulus RXModulus;                                   \
  typedef typename typ::RBak RBak;                                             \
  typedef typename typ::RContext RContext;                                     \
  typedef typename typ::RE RE;                                                 \
  typedef typename typ::vec_RE vec_RE;                                         \
  typedef typename typ::mat_RE mat_RE;                                         \
  typedef typename typ::REX REX;                                               \
  typedef typename typ::REBak REBak;                                           \
  typedef typename typ::vec_REX vec_REX;                                       \
  typedef typename typ::REContext REContext;                                   \
  typedef typename typ::mat_R mat_R;                                           \
  typedef typename typ::vec_R vec_R;

#endif

template <typename type>
class PAlgebraModDerived;
// forward declaration

//! Auxiliary structure to support encoding/decoding slots.
template <typename type>
class MappingData
{

public:
  PA_INJECT(type)

  friend class PAlgebraModDerived<type>;

private:
  RX G;      // the polynomial defining the field extension
  long degG; // the degree of the polynomial

  REContext contextForG;

  /* the remaining fields are visible only to PAlgebraModDerived */

  std::vector<RX> maps;
  std::vector<mat_R> matrix_maps;
  std::vector<REX> rmaps;

public:
  const RX& getG() const { return G; }
  long getDegG() const { return degG; }
  void restoreContextForG() const { contextForG.restore(); }

  // copy and assignment
};

//! \cond FALSE (make doxygen ignore these classes)
template <typename T>
class TNode
{
public:
  std::shared_ptr<TNode<T>> left, right;
  T data;

  TNode(std::shared_ptr<TNode<T>> _left,
        std::shared_ptr<TNode<T>> _right,
        const T& _data) :
      left(_left), right(_right), data(_data)
  {
  }
};

template <typename T>
std::shared_ptr<TNode<T>> buildTNode(std::shared_ptr<TNode<T>> left,
                                     std::shared_ptr<TNode<T>> right,
                                     const T& data)
{
  return std::shared_ptr<TNode<T>>(new TNode<T>(left, right, data));
}

template <typename T>
std::shared_ptr<TNode<T>> nullTNode()
{
  return std::shared_ptr<TNode<T>>();
}
//! \endcond

//! A concrete instantiation of the virtual class
template <typename type>
class PAlgebraModDerived : public PAlgebraModBase
{
public:
  PA_INJECT(type)

private:
  const PAlgebra& zMStar;
  long r;
  long pPowR;
  RContext pPowRContext;

  RXModulus PhimXMod;

  vec_RX factors;
  std::vector<NTL::ZZX> factorsOverZZ;
  vec_RX crtCoeffs;
  std::vector<std::vector<RX>> maskTable;
  std::vector<RX> crtTable;
  std::shared_ptr<TNode<RX>> crtTree;

  void genMaskTable();
  void genCrtTable();

public:
  PAlgebraModDerived& operator=(const PAlgebraModDerived&) = delete;

  PAlgebraModDerived(const PAlgebra& zMStar, long r);

  PAlgebraModDerived(const PAlgebraModDerived& other) // copy constructor
      :
      zMStar(other.zMStar),
      r(other.r),
      pPowR(other.pPowR),
      pPowRContext(other.pPowRContext)
  {
    RBak bak;
    bak.save();
    restoreContext();
    PhimXMod = other.PhimXMod;
    factors = other.factors;
    maskTable = other.maskTable;
    crtTable = other.crtTable;
    crtTree = other.crtTree;
  }

  //! Returns a pointer to a "clone"
  virtual PAlgebraModBase* clone() const override
  {
    return new PAlgebraModDerived(*this);
  }

  //! Returns the type tag: PA_GF2_tag or PA_zz_p_tag
  virtual PA_tag getTag() const override { return tag; }

  //! Returns reference to underlying PAlgebra object
  virtual const PAlgebra& getZMStar() const override { return zMStar; }

  //! Returns reference to the factorization of Phi_m(X) mod p^r, but as ZZX's
  virtual const std::vector<NTL::ZZX>& getFactorsOverZZ() const override
  {
    return factorsOverZZ;
  }

  //! The value r
  virtual long getR() const override { return r; }

  //! The value p^r
  virtual long getPPowR() const override { return pPowR; }

  //! Restores the NTL context for p^r
  virtual void restoreContext() const override { pPowRContext.restore(); }

  /* In all of the following functions, it is expected that the caller
     has already restored the relevant modulus (p^r), which
     can be done by invoking the method restoreContext()
   */

  //! Returns reference to an RXModulus representing Phi_m(X) (mod p^r)
  const RXModulus& getPhimXMod() const { return PhimXMod; }

  //! Returns reference to the factors of Phim_m(X) modulo p^r
  const vec_RX& getFactors() const { return factors; }

  //! @brief Returns the CRT coefficients:
  //! element i contains (prod_{j!=i} F_j)^{-1} mod F_i,
  //! where F_0 F_1 ... is the factorization of Phi_m(X) mod p^r
  const vec_RX& getCrtCoeffs() const { return crtCoeffs; }

  /**
     @brief Returns ref to maskTable, which is used to implement rotations
     (in the EncryptedArray module).

     `maskTable[i][j]` is a polynomial representation of a mask that is 1 in
     all slots whose i'th coordinate is at least j, and 0 elsewhere. We have:
     \verbatim
       maskTable.size() == zMStar.numOfGens()     // # of generators
       for i = 0..maskTable.size()-1:
         maskTable[i].size() == zMStar.OrderOf(i)+1 // order of generator i
     \endverbatim
  **/
  // logically, but not really, const
  const std::vector<std::vector<RX>>& getMaskTable() const { return maskTable; }

  zzX getMask_zzX(long i, long j) const override
  {
    RBak bak;
    bak.save();
    restoreContext();
    return balanced_zzX(maskTable.at(i).at(j));
  }

  ///@{
  //! @name Embedding in the plaintext slots and decoding back
  //! In all the functions below, G must be irreducible mod p,
  //! and the order of G must divide the order of p modulo m
  //! (as returned by zMStar.getOrdP()).
  //! In addition, when r > 1, G must be the monomial X (RX(1, 1))

  //! @brief Returns a std::vector crt[] such that crt[i] = H mod Ft (with t =
  //! T[i])
  void CRT_decompose(std::vector<RX>& crt, const RX& H) const;

  //! @brief Returns H in R[X]/Phi_m(X) s.t. for every i<nSlots and t=T[i],
  //! we have H == crt[i] (mod Ft)
  void CRT_reconstruct(RX& H, std::vector<RX>& crt) const;

  //! @brief Compute the maps for all the slots.
  //! In the current implementation, we if r > 1, then
  //! we must have either deg(G) == 1 or G == factors[0]
  void mapToSlots(MappingData<type>& mappingData, const RX& G) const;

  //! @brief Returns H in R[X]/Phi_m(X) s.t. for every t in T, the element
  //! Ht = (H mod Ft) in R[X]/Ft(X) represents the same element as alpha
  //! in R[X]/G(X).
  //!
  //! Must have deg(alpha)<deg(G). The mappingData argument should contain
  //! the output of mapToSlots(G).
  void embedInAllSlots(RX& H,
                       const RX& alpha,
                       const MappingData<type>& mappingData) const;

  //! @brief Returns H in R[X]/Phi_m(X) s.t. for every t in T, the element
  //! Ht = (H mod Ft) in R[X]/Ft(X) represents the same element as alphas[i]
  //! in R[X]/G(X).
  //!
  //! Must have deg(alpha[i])<deg(G). The mappingData argument should contain
  //! the output of mapToSlots(G).
  void embedInSlots(RX& H,
                    const std::vector<RX>& alphas,
                    const MappingData<type>& mappingData) const;

  //! @brief Return an array such that alphas[i] in R[X]/G(X) represent the
  //! same element as rt = (H mod Ft) in R[X]/Ft(X) where t=T[i].
  //!
  //! The mappingData argument should contain the output of mapToSlots(G).
  void decodePlaintext(std::vector<RX>& alphas,
                       const RX& ptxt,
                       const MappingData<type>& mappingData) const;

  //! @brief Returns a coefficient std::vector C for the linearized polynomial
  //! representing M.
  //!
  //! For h in Z/(p^r)[X] of degree < d,
  //! \f[ M(h(X) mod G) = sum_{i=0}^{d-1} (C[j] mod G) * (h(X^{p^j}) mod G).\f]
  //! G is assumed to be defined in mappingData, with d = deg(G).
  //! L describes a linear map M by describing its action on the standard
  //! power basis: M(x^j mod G) = (L[j] mod G), for j = 0..d-1.
  void buildLinPolyCoeffs(std::vector<RX>& C,
                          const std::vector<RX>& L,
                          const MappingData<type>& mappingData) const;
  ///@}
private:
  /* internal functions, not for public consumption */

  static void SetModulus(long p)
  {
    RContext context(p);
    context.restore();
  }

  //! w in R[X]/F1(X) represents the same as X in R[X]/G(X)
  void mapToF1(RX& w, const RX& G) const { mapToFt(w, G, 1); }

  //! Same as above, but embeds relative to Ft rather than F1. The
  //! optional rF1 contains the output of mapToF1, to speed this operation.
  void mapToFt(RX& w, const RX& G, long t, const RX* rF1 = nullptr) const;

  void buildTree(std::shared_ptr<TNode<RX>>& res,
                 long offset,
                 long extent) const;

  void evalTree(RX& res,
                std::shared_ptr<TNode<RX>> tree,
                const std::vector<RX>& crt1,
                long offset,
                long extent) const;
};

//! A different derived class to be used for the approximate-numbers scheme
//! This is mostly a dummy class, but needed since the context always has a
//! PAlgebraMod data member.
template <>
class PAlgebraModDerived<PA_cx> : public PAlgebraModBase
{
  const PAlgebra& zMStar;
  long r; // counts bits of precision

public:
  PAlgebraModDerived(const PAlgebra& palg, long _r);

  PAlgebraModBase* clone() const override
  {
    return new PAlgebraModDerived(*this);
  }

  PA_tag getTag() const override { return PA_cx_tag; }

  const PAlgebra& getZMStar() const override { return zMStar; }
  long getR() const override { return r; }
  long getPPowR() const override { return 1L << r; }
  void restoreContext() const override {}

  // These function make no sense for PAlgebraModCx
  const std::vector<NTL::ZZX>& getFactorsOverZZ() const override
  {
    throw LogicError("PAlgebraModCx::getFactorsOverZZ undefined");
  }

  zzX getMask_zzX(UNUSED long i, UNUSED long j) const override
  {
    throw LogicError("PAlgebraModCx::getMask_zzX undefined");
  }
};

typedef PAlgebraModDerived<PA_cx> PAlgebraModCx;

//! Builds a table, of type PA_GF2 if p == 2 and r == 1, and PA_zz_p otherwise
PAlgebraModBase* buildPAlgebraMod(const PAlgebra& zMStar, long r);

// A simple wrapper for a pointer to an object of type PAlgebraModBase.
//
// Direct access to the virtual methods of PAlgebraModBase is provided,
// along with a "downcast" operator to get a reference to the object
// as a derived type, and == and != operators.
class PAlgebraMod
{

private:
  ClonedPtr<PAlgebraModBase> rep;

public:
  // copy constructor: default
  // assignment: deleted
  // destructor: default
  // NOTE: the use of ClonedPtr ensures that the default copy constructor
  // and destructor will work correctly.

  PAlgebraMod& operator=(const PAlgebraMod&) = delete;

  explicit PAlgebraMod(const PAlgebra& zMStar, long r) :
      rep(buildPAlgebraMod(zMStar, r))
  {
  }
  // constructor

  //! Downcast operator
  //! example: const PAlgebraModDerived<PA_GF2>& rep =
  //! alMod.getDerived(PA_GF2());
  template <typename type>
  const PAlgebraModDerived<type>& getDerived(type) const
  {
    return dynamic_cast<const PAlgebraModDerived<type>&>(*rep);
  }
  const PAlgebraModCx& getCx() const
  {
    return dynamic_cast<const PAlgebraModCx&>(*rep);
  }

  bool operator==(const PAlgebraMod& other) const
  {
    return getZMStar() == other.getZMStar() && getR() == other.getR();
  }
  // comparison

  bool operator!=(const PAlgebraMod& other) const { return !(*this == other); }
  // comparison

  /* direct access to the PAlgebraModBase methods */

  //! Returns the type tag: PA_GF2_tag or PA_zz_p_tag
  PA_tag getTag() const { return rep->getTag(); }
  //! Returns reference to underlying PAlgebra object
  const PAlgebra& getZMStar() const { return rep->getZMStar(); }
  //! Returns reference to the factorization of Phi_m(X) mod p^r, but as ZZX's
  const std::vector<NTL::ZZX>& getFactorsOverZZ() const
  {
    return rep->getFactorsOverZZ();
  }
  //! The value r
  long getR() const { return rep->getR(); }
  //! The value p^r
  long getPPowR() const { return rep->getPPowR(); }
  //! Restores the NTL context for p^r
  void restoreContext() const { rep->restoreContext(); }

  zzX getMask_zzX(long i, long j) const { return rep->getMask_zzX(i, j); }
};

//! returns true if the palg parameters match the rest, false otherwise
bool comparePAlgebra(const PAlgebra& palg,
                     unsigned long m,
                     unsigned long p,
                     unsigned long r,
                     const std::vector<long>& gens,
                     const std::vector<long>& ords);

// for internal consumption only
double calcPolyNormBnd(long m);

} // namespace helib

#endif // #ifndef HELIB_PALGEBRA_H
