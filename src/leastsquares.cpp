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

#include "leastsquares.h"

#include <algorithm>
#include <cmath>
#include <limits>

// constructor with sizes
float_mat::float_mat(const std::size_t rows, const std::size_t cols, const double defval) :
    std::vector<float_vect>(rows)
{
    for (std::size_t i = 0; i < rows; ++i) {
        (*this)[i].resize(cols, defval);
    }
}

// copy constructor for matrix: the base vector's element-wise copy does it all
float_mat::float_mat(const float_mat &m) = default;

// constructor from a vector: a matrix with that vector as its single row
float_mat::float_mat(const float_vect &v) : std::vector<float_vect>(1, v) {}

//////////////////////
// Helper functions //
//////////////////////

namespace {

//! permute() orders the rows of A to match the integers in the index array.
void permute(float_mat &A, int_vect &idx)
{
    int_vect i(idx.size());

    for (std::size_t j = 0; j < A.nr_rows(); ++j) {
        i[j] = j;
    }

    // loop over permuted indices
    for (std::size_t j = 0; j < A.nr_rows(); ++j) {
        if (i[j] != idx[j]) {

            // search only the remaining indices
            for (std::size_t k = j + 1; k < A.nr_rows(); ++k) {
                if (i[k] == idx[j]) {
                    std::swap(A[j], A[k]); // swap the rows and
                    i[k] = i[j];           // the elements of
                    i[j] = idx[j];         // the ordered index.
                    break;                 // next j
                }
            }
        }
    }
}

/*! \brief Implicit partial pivoting.
 *
 * The function looks for pivot element only in rows below the current
 * element, A[idx[row]][column], then swaps that row with the current one in
 * the index map. The algorithm is for implicit pivoting (i.e., the pivot is
 * chosen as if the max coefficient in each row is set to 1) based on the
 * scaling information in the vector scale. The map of swapped indices is
 * recorded in idx. The return value is +1 or -1 depending on whether the
 * number of row swaps was even or odd respectively. */
int partial_pivot(float_mat &A, const std::size_t row, const std::size_t col, float_vect &scale,
                  int_vect &idx)
{
    int swapNum = 1;

    // default pivot is the current position, [row,col]
    std::size_t pivot = row;
    double piv_elem   = fabs(A[idx[row]][col]) * scale[idx[row]];

    // loop over possible pivots below current
    for (std::size_t j = row + 1; j < A.nr_rows(); ++j) {

        const double tmp = fabs(A[idx[j]][col]) * scale[idx[j]];

        // if this elem is larger, then it becomes the pivot
        if (tmp > piv_elem) {
            pivot    = j;
            piv_elem = tmp;
        }
    }

    if (pivot > row) {         // bring the pivot to the diagonal
        int j      = idx[row]; // reorder swap array
        idx[row]   = idx[pivot];
        idx[pivot] = j;
        swapNum    = -swapNum; // keeping track of odd or even swap
    }
    return swapNum;
}

/*! \brief Perform backward substitution.
 *
 * Solves the system of equations A*b=a, ASSUMING that A is upper
 * triangular. If diag==1, then the diagonal elements are additionally
 * assumed to be 1.  Note that the lower triangular elements are never
 * checked, so this function is valid to use after a LU-decomposition in
 * place.  A is not modified, and the solution, b, is returned in a. */
void lu_backsubst(float_mat &A, float_mat &a, bool diag = false)
{
    for (int r = (A.nr_rows() - 1); r >= 0; --r) {
        for (int c = (A.nr_cols() - 1); c > r; --c) {
            for (std::size_t k = 0; k < a.nr_cols(); ++k) {
                a[r][k] -= A[r][c] * a[c][k];
            }
        }
        if (!diag) {
            for (std::size_t k = 0; k < a.nr_cols(); ++k) {
                a[r][k] /= A[r][r];
            }
        }
    }
}

/*! \brief Perform forward substitution.
 *
 * Solves the system of equations A*b=a, ASSUMING that A is lower
 * triangular. If diag==1, then the diagonal elements are additionally
 * assumed to be 1.  Note that the upper triangular elements are never
 * checked, so this function is valid to use after a LU-decomposition in
 * place.  A is not modified, and the solution, b, is returned in a. */
void lu_forwsubst(float_mat &A, float_mat &a, bool diag = true)
{
    for (int r = 0; r < static_cast<int>(A.nr_rows()); ++r) {
        for (int c = 0; c < r; ++c) {
            for (std::size_t k = 0; k < a.nr_cols(); ++k) {
                a[r][k] -= A[r][c] * a[c][k];
            }
        }
        if (!diag) {
            for (std::size_t k = 0; k < a.nr_cols(); ++k) {
                a[r][k] /= A[r][r];
            }
        }
    }
}

/*! \brief Performs LU factorization in place.
 *
 * This is Crout's algorithm (cf., Num. Rec. in C, Section 2.3).  The map of
 * swapped indices is recorded in idx. The return value is +1 or -1
 * depending on whether the number of row swaps was even or odd
 * respectively.  idx must be preinitialized to a valid set of indices
 * (i.e., {0, 1, ..., A.nr_rows()-1}). */
int lu_factorize(float_mat &A, int_vect &idx)
{
    float_vect scale(A.nr_rows()); // implicit pivot scaling
    for (std::size_t i = 0; i < A.nr_rows(); ++i) {
        double maxval = 0.0;
        for (std::size_t j = 0; j < A.nr_cols(); ++j) {
            maxval = std::max(fabs(A[i][j]), maxval);
        }
        if (maxval == 0.0) {
            return 0;
        }
        scale[i] = 1.0 / maxval;
    }

    int swapNum = 1;
    for (std::size_t c = 0; c < A.nr_cols(); ++c) {     // loop over columns
        swapNum *= partial_pivot(A, c, c, scale, idx);  // bring pivot to diagonal
        for (std::size_t r = 0; r < A.nr_rows(); ++r) { //  loop over rows
            std::size_t lim = (r < c) ? r : c;
            for (std::size_t j = 0; j < lim; ++j) {
                A[idx[r]][c] -= A[idx[r]][j] * A[idx[j]][c];
            }
            if (r > c) A[idx[r]][c] /= A[idx[c]][c];
        }
    }
    permute(A, idx);
    return swapNum;
}

//! calculate savitzky golay coefficients.
float_vect sg_coeff(const float_vect &b, const std::size_t deg)
{
    const std::size_t rows(b.size());
    const std::size_t cols(deg + 1);
    float_mat A(rows, cols);
    float_vect res(rows);

    // generate input matrix for least squares fit
    for (std::size_t i = 0; i < rows; ++i) {
        for (std::size_t j = 0; j < cols; ++j) {
            A[i][j] = pow(double(i), double(j));
        }
    }

    const float_mat At(transpose(A)); // computed once, used twice
    float_mat c(invert(At * A) * (At * transpose(b)));

    for (std::size_t i = 0; i < b.size(); ++i) {
        res[i] = c[0][0];
        for (std::size_t j = 1; j <= deg; ++j) {
            res[i] += c[j][0] * pow(double(i), double(j));
        }
    }
    return res;
}

} // namespace

/*! \brief Solve a system of linear equations.
 * Solves the inhomogeneous matrix problem with lu-decomposition. Note that
 * inversion may be accomplished by setting a to the identity_matrix. */
float_mat lin_solve(const float_mat &A, const float_mat &a)
{
    float_mat B(A);
    float_mat b(a);
    int_vect idx(B.nr_rows());

    for (std::size_t j = 0; j < B.nr_rows(); ++j) {
        idx[j] = j; // init row swap label array
    }
    // a singular matrix cannot be solved; return NaNs explicitly instead of
    // dividing by zero pivots below (callers already guard against non-finite
    // results, e.g. the Levenberg-Marquardt step check)
    if (lu_factorize(B, idx) == 0) {
        for (auto &row : b)
            std::fill(row.begin(), row.end(), std::numeric_limits<double>::quiet_NaN());
        return b;
    }
    permute(b, idx);    // sort the inhomogeneity to match the lu-decomp
    lu_forwsubst(B, b); // solve the forward problem
    lu_backsubst(B, b); // solve the backward problem
    return b;
}

//! Returns the inverse of a matrix using LU-decomposition.
float_mat invert(const float_mat &A)
{
    const std::size_t n = A.size();
    float_mat E(n, n, 0.0);
    const float_mat &B(A);

    for (std::size_t i = 0; i < n; ++i) {
        E[i][i] = 1.0;
    }

    return lin_solve(B, E);
}

//! returns the transposed matrix.
float_mat transpose(const float_mat &a)
{
    float_mat res(a.nr_cols(), a.nr_rows());

    for (std::size_t i = 0; i < a.nr_rows(); ++i) {
        for (std::size_t j = 0; j < a.nr_cols(); ++j) {
            res[j][i] = a[i][j];
        }
    }
    return res;
}

//! matrix multiplication.
float_mat operator*(const float_mat &a, const float_mat &b)
{
    float_mat res(a.nr_rows(), b.nr_cols());
    for (std::size_t i = 0; i < a.nr_rows(); ++i) {
        for (std::size_t j = 0; j < b.nr_cols(); ++j) {
            double sum(0.0);
            for (std::size_t k = 0; k < a.nr_cols(); ++k) {
                sum += a[i][k] * b[k][j];
            }
            res[i][j] = sum;
        }
    }
    return res;
}

/*! \brief savitzky golay smoothing.
 *
 * This method means fitting a polynomial of degree 'deg' to a sliding window
 * of width 2w+1 throughout the data.  The needed coefficients are
 * generated dynamically by doing a least squares fit on a "symmetric" unit
 * vector of size 2w+1, e.g. for w=2 b=(0,0,1,0,0). evaluating the polynomial
 * yields the sg-coefficients.  at the border non symmetric vectors b are
 * used. */
float_vect sg_smooth(const float_vect &v, const std::size_t width, const int deg)
{
    float_vect res(v.size(), 0.0);
    const std::size_t window = (2 * (std::size_t)width) + 1;
    const int endidx         = v.size() - 1;

    // guard against inputs shorter than the smoothing window: the unsigned
    // v.size() - window loop bounds below would underflow and run away
    if (v.size() < window) return res;

    // do a regular sliding window average
    if (deg == 0) {
        // handle border cases first because we need different coefficients
        for (std::size_t i = 0; i < width; ++i) {
            const double scale = 1.0 / double(i + 1);
            const float_vect c1(width, scale);
            for (std::size_t j = 0; j <= i; ++j) {
                res[i] += c1[j] * v[j];
                res[endidx - i] += c1[j] * v[endidx - j];
            }
        }

        // now loop over rest of data. reusing the "symmetric" coefficients.
        const double scale = 1.0 / double(window);
        const float_vect c2(window, scale);
        for (std::size_t i = 0; i <= (v.size() - window); ++i) {
            for (std::size_t j = 0; j < window; ++j) {
                res[i + width] += c2[j] * v[i + j];
            }
        }
        return res;
    }

    // handle border cases first because we need different coefficients
    for (std::size_t i = 0; i < width; ++i) {
        float_vect b1(window, 0.0);
        b1[i] = 1.0;

        const float_vect c1(sg_coeff(b1, deg));
        for (std::size_t j = 0; j < window; ++j) {
            res[i] += c1[j] * v[j];
            res[endidx - i] += c1[j] * v[endidx - j];
        }
    }

    // now loop over rest of data. reusing the "symmetric" coefficients.
    float_vect b2(window, 0.0);
    b2[width] = 1.0;
    const float_vect c2(sg_coeff(b2, deg));

    for (std::size_t i = 0; i <= (v.size() - window); ++i) {
        for (std::size_t j = 0; j < window; ++j) {
            res[i + width] += c2[j] * v[i + j];
        }
    }
    return res;
}

// Local Variables:
// c-basic-offset: 4
// End:
