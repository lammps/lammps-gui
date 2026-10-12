// -*- c++ -*- /////////////////////////////////////////////////////////////////////////
// LAMMPS-GUI - A Graphical Tool to Learn and Explore the LAMMPS MD Simulation Software
//
// Copyright (c) 2023, 2024, 2025, 2026  Axel Kohlmeyer
//
// Documentation: https://lammps-gui.lammps.org/
// Contact: akohlmey@gmail.com
//
// This software is distributed under the GNU General Public License version 2 or later.
////////////////////////////////////////////////////////////////////////////////////////

#ifndef LEASTSQUARES_H
#define LEASTSQUARES_H

// Small, self-contained (Qt-free) linear-algebra and least-squares toolkit.
// Originally the file-local Savitzky-Golay support inside chartviewer.cpp; it
// is factored out here so the dense LU solver can be reused for polynomial and
// equation-of-state fits and exercised directly by the unit tests.

#include <cstddef>
#include <vector>

/** Dense vector of doubles used by the least-squares routines */
using float_vect = std::vector<double>;

/** Dense vector of integers used for LU pivot bookkeeping */
using int_vect = std::vector<int>;

/**
 * @brief Simple dense matrix of doubles backed by a vector of rows
 *
 * Elements are indexed [row][column] with 0-based indices (C style). Because
 * the storage is row-major, iterating over rows is cheaper than over columns.
 * Used as the working type for the LU solver, matrix inverse, and the
 * Savitzky-Golay coefficient generation.
 */
class float_mat : public std::vector<float_vect> {

public:
    // disable selected default constructors and assignment operators
    float_mat()                             = delete;
    float_mat(float_mat &&)                 = default; ///< Move constructor
    ~float_mat()                            = default;
    float_mat &operator=(const float_mat &) = delete;
    float_mat &operator=(float_mat &&)      = delete;

    /**
     * @brief Construct a rows x cols matrix filled with a default value
     * @param rows Number of rows
     * @param cols Number of columns
     * @param def  Initial value for every element (default 0.0)
     */
    float_mat(std::size_t rows, std::size_t cols, double def = 0.0);
    /** @brief Copy constructor */
    float_mat(const float_mat &m);
    /**
     * @brief Construct a single-row matrix from a vector
     * @param v Row contents
     */
    float_mat(const float_vect &v);

    /** @brief Number of rows */
    std::size_t nr_rows() const { return size(); };
    /** @brief Number of columns (size of the first row) */
    std::size_t nr_cols() const { return front().size(); };
};

/**
 * @brief Return the transpose of a matrix
 * @param a Input matrix
 * @return Transposed matrix
 */
float_mat transpose(const float_mat &a);

/**
 * @brief Matrix-matrix multiplication
 * @param a Left operand
 * @param b Right operand (must have as many rows as @p a has columns)
 * @return Product matrix
 */
float_mat operator*(const float_mat &a, const float_mat &b);

/**
 * @brief Solve the linear system A*X = a via in-place LU decomposition
 * @param A Coefficient matrix
 * @param a Right-hand side(s); each column is an independent system
 * @return Solution matrix X with the same shape as @p a
 *
 * Setting @p a to the identity matrix yields the inverse of @p A.
 */
float_mat lin_solve(const float_mat &A, const float_mat &a);

/**
 * @brief Invert a square matrix using LU decomposition
 * @param A Square matrix to invert
 * @return Inverse of @p A
 */
float_mat invert(const float_mat &A);

/**
 * @brief Smooth a data vector with a Savitzky-Golay filter
 * @param v     Input samples (assumed equally spaced)
 * @param width Half-window size; the filter window spans 2*width+1 points
 * @param deg   Polynomial degree fitted in each window (0 = moving average)
 * @return Smoothed vector with the same length as @p v
 *
 * Fits a polynomial of degree @p deg to a sliding window of width 2*width+1
 * by least squares; non-symmetric windows are used near the borders.
 */
float_vect sg_smooth(const float_vect &v, std::size_t width, int deg);

#endif

// Local Variables:
// c-basic-offset: 4
// End:
