// Copyright (C) 2022 Intel Corporation

// SPDX-License-Identifier: Apache-2.0
#ifndef UTILS_H
#define UTILS_H
#include <helib/helib.h>
#include <helib/matmul.h> // matmul is not included by default
#include <helib/Matrix.h> // for defining matrices
#include <NTL/BasicThreadPool.h>
#include <algorithm>

void printMatrix(const helib::Matrix<double>& M,
                 const size_t& rmax,
                 const size_t& cmax,
                 std::ostream& out = std::cout)
{
  for (size_t i = 0; i < rmax; i++) {
    for (size_t j = 0; j < cmax; j++)
      out << M(i, j) << ", ";

    if (cmax < M.dims(1))
      out << ". . .\n";
  }
  if (rmax < M.dims(0)) {
    for (size_t i = 0; i < 3; i++) {
      for (size_t j = 0; j < cmax; j++)
        out << ". \t";
      out << "\n";
    }
  }
}
template <typename T>

void printMatrix(const std::vector<std::vector<T>>& M,
                 const size_t& rmax,
                 const size_t& cmax,
                 std::ostream& out = std::cout)
{

  for (size_t i = 0; i < rmax; i++) {
    for (size_t j = 0; j < cmax; j++)
      out << M[i][j] << ", ";

    if (cmax < M[i].size())
      out << ". . .";
    out << "\n";
  }
  if (rmax < M.size()) {
    for (size_t i = 0; i < 3; i++) {
      for (size_t j = 0; j < cmax; j++)
        out << ". \t";
      out << "\n";
    }
  }
}

template <typename T>
void printVector(const std::vector<T>& v,
                 const size_t& cmax,
                 std::ostream& out = std::cout)
{
  for (size_t i = 0; i < cmax; i++)
    out << v[i] << ",";
  if (cmax < v.size())
    out << ". . . ";
  out << std::endl;
}
/**
 * @brief assume v is a rowpacked matrix, and print as a matrix
 *
 * @tparam T data type
 * @param v rowpacked matrix
 * @param dim0 row length
 * @param dim1 number of columns to print
 *
 * @param out the stream to print to, default to cout
 */
template <typename T>
void printVectorAsMatrix(const std::vector<T>& v,
                         const size_t& dim0,
                         const size_t& dim1,
                         std::ostream& out = std::cout)
{
  for (size_t i = 0; i < dim1; i++) {
    for (size_t j = 0; j < dim0 - 1; j++) {
      out << v[i * dim0 + j] << "\t";
    }
    out << v[(i + 1) * dim0 - 1] << std::endl;
  }
}
/**
 * @brief sample from a continuous uniform on [min,max]
 */
double sampleUniform(const double& min, const double& max)
{
  double d = rand();
  d /= RAND_MAX;
  d *= (max - min);
  d += min;
  return d;
}
/**
 * @brief sample from a continuous Gaussian with mean mu, standard deviation
 * sigma. Adapts the Box-Muller method as implemented in sample.cpp of HElib
 * @param mu
 * @param sigma
 * @return double
 */
double sampleGaussian(double mu, double sigma)
{
  // r1, r2 are "uniform in (0,1)"
  double r1 = sampleUniform(0, 1);
  double r2 = sampleUniform(0, 1);
  while (r2 == 0)
    r2 = sampleUniform(0, 1);
  double theta = 2.0 * helib::PI * r1;
  double rr = sqrt(-2.0 * log(r2));
  return sigma * rr * cos(theta) + mu;
}
/**
 * @brief create a matrix with entries sampled uniformly at random
 *
 * @param dim0 number of rows
 * @param dim1 number of columns
 * @param min lower bound of continuous uniform
 * @param max upper bound of continuous uniform
 * @return Matrix<double> dim0 x dim1 matrix with each entry sampled from
 * [min,max]
 */
helib::Matrix<double> uniformMatrix(const long& dim0,
                                    const long& dim1,
                                    const double& min,
                                    const double& max)
{

  helib::Matrix<double> M(dim0, dim1);
  for (size_t i = 0; i < M.dims(0); i++) {
    for (size_t j = 0; j < M.dims(1); j++) {
      M(i, j) = sampleUniform(min, max);
    }
  }
  return M;
}

std::vector<double> uniformVector(const long& dim,
                                  const double& min,
                                  const double& max)
{
  std::vector<double> v;
  v.reserve(dim);
  for (int i = 0; i < dim; i++)
    v.emplace_back(sampleUniform(min, max));
  return v;
}
/**
 * @brief generate a vector with coordinates sampled from a discrete uniform
 *
 * @param dim number of elements
 * @param min lower bound on uniform
 * @param max upper bound on uniform
 * @return vector<int> a vector with elements uniform in [min, min + 1,..., max
 * - 1, max]
 */
std::vector<int> discreteUniformVector(long dim, int min, int max)
{
  std::vector<int> v;
  v.reserve(dim);
  for (int i = 0; i < dim; i++)
    v.emplace_back((rand() % (max - min + 1)) + min);
  return v;
}

template <typename T>
T dotProduct(const std::vector<T>& v1, const std::vector<T>& v2)
{
  helib::assertEq<helib::LogicError>(
      v1.size(),
      v2.size(),
      "can only dot two vectors of the same size");
  double dp = 0;
  for (size_t i = 0; i < v1.size(); i++)
    dp += v1[i] * v2[i];
  return dp;
}

template <typename T>
std::vector<std::vector<T>> transpose(std::vector<std::vector<T>> M)
{
  std::vector<std::vector<T>> MT;
  MT.reserve(M[0].size());
  for (size_t i = 0; i < M[0].size(); i++) {
    std::vector<T> col;
    col.reserve(M.size());
    for (size_t j = 0; j < M.size(); j++)
      col.emplace_back(M[j][i]);
    MT.push_back(col);
  }
  return MT;
}

template <typename T>
std::vector<std::vector<T>> matrixProduct(const std::vector<std::vector<T>>& A,
                                          const std::vector<std::vector<T>>& B)
{
  helib::assertEq<helib::LogicError>(B.size(),
                                     A[0].size(),
                                     "inner matrix dimension mismatch");
  std::vector<std::vector<T>> C;
  C.reserve(A.size());
  std::vector<std::vector<T>> BT = transpose(B);
  for (size_t i = 0; i < A.size(); i++) {
    std::vector<T> row;
    row.reserve(B[0].size());
    for (size_t j = 0; j < BT.size(); j++)
      row.push_back(dotProduct(A[i], BT[j]));
    C.push_back(row);
  }
  return C;
}

void invertInPlace(std::vector<std::vector<double>>& M)
{
  // Gaussian Jordan elimination inplace
  size_t nrow = M.size();
  size_t ncol = M[0].size();
  helib::assertEq<helib::InvalidArgument>(nrow,
                                          ncol,
                                          "cannot invert nonsquare matrix");
  // augment with identity
  std::vector<double> zero(nrow, 0);
  for (size_t i = 0; i < nrow; i++) {
    M[i].insert(M[i].end(), zero.begin(), zero.end());
    M[i][nrow + i] += 1;
  }

  for (size_t i = 0; i < nrow; i++) {
    // find pivot in column i
    size_t maxrow = 0;
    double maxentry = 0;
    for (size_t j = i; j < nrow; j++) {
      if (abs(M[j][i]) > maxentry) {
        maxentry = abs(M[j][i]);
        maxrow = j;
      }
    }
    helib::assertTrue<helib::LogicError>(
        maxentry > 0,
        "no pivot found in column: matrix is not invertible");
    // swap pivot row with row i
    iter_swap(M.begin() + i, M.begin() + maxrow);
    // divide by lead entry
    maxentry = M[i][i];
    for (size_t k = 0; k < 2 * ncol; k++)
      M[i][k] /= maxentry;
    // and knock out rest of column
    for (size_t j = 0; j < nrow; j++) {
      if (j == i)
        continue;
      double rat = M[j][i];
      for (size_t k = i; k < 2 * ncol; k++) {
        M[j][k] -= rat * M[i][k];
      }
    }
  }

  // inverse is now in second half of matrix
  for (size_t i = 0; i < nrow; i++)
    M[i].erase(M[i].begin(), M[i].begin() + nrow);
}

std::vector<std::vector<double>> createZ(std::vector<std::vector<double>> X)
{
  // return (X)(X^T X)^-1
  std::vector<std::vector<double>> XT = transpose(X);
  std::vector<std::vector<double>> XTX = matrixProduct(XT, X);
  invertInPlace(XTX);
  return matrixProduct(X, XTX);
}
#endif // UTILS_H
