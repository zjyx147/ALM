/*
 fitting.cpp

 Copyright (c) 2014, 2015, 2016 Terumasa Tadano

 This file is distributed under the terms of the MIT license.
 Please see the file 'LICENCE.txt' in the root directory 
 or http://opensource.org/licenses/mit-license.php for information.
*/

#include <iostream>
#include <iomanip>
#include <cmath>
#include <string>
#include <vector>
#include <boost/lexical_cast.hpp>
#include "fitting.h"
#include "files.h"
#include "error.h"
#include "memory.h"
#include "symmetry.h"
#include "system.h"
#include "fcs.h"
#include "interaction.h"
#include "timer.h"
#include "constants.h"
#include "constraint.h"
#include "mathfunctions.h"

#ifdef _USE_EIGEN
#include <Eigen/Dense>
#endif

#include <time.h>

#ifdef _VSL
#include "mkl_vsl.h"

#else
#include <cstdlib>
#endif

using namespace ALM_NS;


Fitting::Fitting(ALMCore *alm): Pointers(alm)
{
    set_default_variables();
}

Fitting::~Fitting()
{
    deallocate_variables();
}

void Fitting::fitmain()
{
    int i;
    int nat = system->nat;
    int natmin = symmetry->natmin;
    int ntran = symmetry->ntran;
    int nstart = system->nstart;
    int nend = system->nend;
    int N, M, N_new;
    int maxorder = interaction->maxorder;
    int P = constraint->P;
    int ndata_used = nend - nstart + 1;

    double **amat, *fsum;
    double *fsum_orig;
    double *param_tmp;

    int multiply_data = symmetry->multiply_data;
    int nmulti = get_number_for_multiplier(multiply_data);

    double **u;
    double **f;

    std::cout << " FITTING" << std::endl;
    std::cout << " =======" << std::endl << std::endl;

    std::cout << "  Reference files" << std::endl;
    std::cout << "   Displacement: " << files->file_disp << std::endl;
    std::cout << "   Force       : " << files->file_force << std::endl;
    std::cout << std::endl;

    std::cout << "  NSTART = " << nstart << "; NEND = " << nend << std::endl;
    std::cout << "  " << ndata_used << " entries will be used for fitting."
        << std::endl << std::endl;


    if (nmulti > 0) {
        memory->allocate(u, ndata_used * nmulti, 3 * nat);
        memory->allocate(f, ndata_used * nmulti, 3 * nat);
    } else {
        error->exit("fitmain", "nmulti has to be larger than 0.");
    }
    data_multiplier(u, f, nat, ndata_used, nmulti, multiply_data);

    N = 0;
    for (i = 0; i < maxorder; ++i) {
        N += fcs->ndup[i].size();
    }
    std::cout << "  Total Number of Parameters : "
        << N << std::endl << std::endl;

    // Calculate matrix elements for fitting

    M = 3 * natmin * ndata_used * nmulti;

    if (constraint->constraint_algebraic) {
        N_new = 0;
        for (i = 0; i < maxorder; ++i) {
            N_new += constraint->index_bimap[i].size();
        }
        std::cout << "  Total Number of Free Parameters : "
            << N_new << std::endl << std::endl;

        memory->allocate(amat, M, N_new);
        memory->allocate(fsum, M);
        memory->allocate(fsum_orig, M);

        calc_matrix_elements_algebraic_constraint(M, N, N_new, nat, natmin, ndata_used,
                                                  nmulti, maxorder, u, f, amat, fsum,
                                                  fsum_orig);
    } else {
        memory->allocate(amat, M, N);
        memory->allocate(fsum, M);

        calc_matrix_elements(M, N, nat, natmin, ndata_used,
                             nmulti, maxorder, u, f, amat, fsum);
    }

    memory->deallocate(u);
    memory->deallocate(f);

    // Execute fitting

    memory->allocate(param_tmp, N);


    // Fitting with singular value decomposition or QR-Decomposition

    if (constraint->constraint_algebraic) {
        fit_algebraic_constraints(N_new, M, amat, fsum, param_tmp,
                                  fsum_orig, maxorder);

    } else if (constraint->exist_constraint) {
        fit_with_constraints(N, M, P, amat, fsum, param_tmp,
                             constraint->const_mat,
                             constraint->const_rhs);
    } else {
        fit_without_constraints(N, M, amat, fsum, param_tmp);
    }


    // Copy force constants to public variable "params"
    if (params) {
        memory->deallocate(params);
    }
    memory->allocate(params, N);

    if (constraint->constraint_algebraic) {

        for (i = 0; i < N; ++i) {
            params[i] = param_tmp[i];
        }
        memory->deallocate(fsum_orig);

    } else {

        for (i = 0; i < N; ++i) params[i] = param_tmp[i];

    }

    memory->deallocate(amat);
    memory->deallocate(fsum);
    memory->deallocate(param_tmp);

    std::cout << std::endl;
    timer->print_elapsed();
    std::cout << " -------------------------------------------------------------------" << std::endl;
    std::cout << std::endl;
}

void Fitting::set_displacement_and_force(const double * const *disp_in,
                                         const double * const *force_in,
                                         const int nat,
                                         const int ndata_used)
{
    if (u_in) {
        memory->deallocate(u_in);
    }
    memory->allocate(u_in, ndata_used, 3 * nat);

    if (f_in) {
        memory->deallocate(f_in);
    }
    memory->allocate(f_in, ndata_used, 3 * nat);

    for (int i = 0; i < ndata_used; i++) {
        for (int j = 0; j < 3 * nat; j++) {
            u_in[i][j] = disp_in[i][j];
            f_in[i][j] = force_in[i][j];
        }
    }
}

void Fitting::fit_without_constraints(int N,
                                      int M,
                                      double **amat,
                                      double *bvec,
                                      double *param_out)
{
    int i, j;
    unsigned long k;
    int nrhs = 1, nrank, INFO, LWORK;
    int LMIN, LMAX;
    double rcond = -1.0;
    double f_square = 0.0;
    double *WORK, *S, *amat_mod, *fsum2;

    std::cout << "  Entering fitting routine: SVD without constraints" << std::endl;

    LMIN = std::min<int>(M, N);
    LMAX = std::max<int>(M, N);

    LWORK = 3 * LMIN + std::max<int>(2 * LMIN, LMAX);
    LWORK = 2 * LWORK;

    memory->allocate(WORK, LWORK);
    memory->allocate(S, LMIN);

    // transpose matrix A
    memory->allocate(amat_mod, M * N);
    memory->allocate(fsum2, LMAX);

    k = 0;
    for (j = 0; j < N; ++j) {
        for (i = 0; i < M; ++i) {
            amat_mod[k++] = amat[i][j];
        }
    }
    for (i = 0; i < M; ++i) {
        fsum2[i] = bvec[i];
        f_square += std::pow(bvec[i], 2);
    }
    for (i = M; i < LMAX; ++i) fsum2[i] = 0.0;

    std::cout << "  SVD has started ... ";

    // Fitting with singular value decomposition
    dgelss_(&M, &N, &nrhs, amat_mod, &M, fsum2, &LMAX,
            S, &rcond, &nrank, WORK, &LWORK, &INFO);

    std::cout << "finished !" << std::endl << std::endl;

    std::cout << "  RANK of the matrix = " << nrank << std::endl;
    if (nrank < N)
        error->warn("fit_without_constraints",
                    "Matrix is rank-deficient. Force constants could not be determined uniquely :(");

    if (nrank == N) {
        double f_residual = 0.0;
        for (i = N; i < M; ++i) {
            f_residual += std::pow(fsum2[i], 2);
        }
        std::cout << std::endl << "  Residual sum of squares for the solution: "
            << sqrt(f_residual) << std::endl;
        std::cout << "  Fitting error (%) : "
            << sqrt(f_residual / f_square) * 100.0 << std::endl;
    }

    for (i = 0; i < N; ++i) {
        param_out[i] = fsum2[i];
    }

    memory->deallocate(WORK);
    memory->deallocate(S);
    memory->deallocate(fsum2);
    memory->deallocate(amat_mod);
}

void Fitting::fit_with_constraints(int N,
                                   int M,
                                   int P,
                                   double **amat,
                                   double *bvec,
                                   double *param_out,
                                   double **cmat,
                                   double *dvec)
{
    int i, j;
    unsigned long k;
    int nrank;
    double f_square, f_residual;
    double *fsum2;
    double *mat_tmp;

    std::cout << "  Entering fitting routine: QRD with constraints" << std::endl;

    memory->allocate(fsum2, M);
    memory->allocate(mat_tmp, (M + P) * N);

    k = 0;

    for (j = 0; j < N; ++j) {
        for (i = 0; i < M; ++i) {
            mat_tmp[k++] = amat[i][j];
        }
        for (i = 0; i < P; ++i) {
            mat_tmp[k++] = cmat[i][j];
        }
    }

    nrank = rankQRD((M + P), N, mat_tmp, eps12);
    memory->deallocate(mat_tmp);

    if (nrank != N) {
        std::cout << std::endl;
        std::cout << " **************************************************************************" << std::endl;
        std::cout << "  WARNING : rank deficient.                                                " << std::endl;
        std::cout << "  rank ( (A) ) ! = N            A: Fitting matrix     B: Constraint matrix " << std::endl;
        std::cout << "       ( (B) )                  N: The number of parameters                " << std::endl;
        std::cout << "  rank = " << nrank << " N = " << N << std::endl << std::endl;
        std::cout << "  This can cause a difficulty in solving the fitting problem properly      " << std::endl;
        std::cout << "  with DGGLSE, especially when the difference is large. Please check if    " << std::endl;
        std::cout << "  you obtain reliable force constants in the .fcs file.                    " << std::endl << std::endl;
        std::cout << "  This issue may be resolved by setting MULTDAT = 2 in the &fitting field. " << std::endl;
        std::cout << "  If not, you may need to reduce the cutoff radii and/or increase NDATA    " << std::endl;
        std::cout << "  by giving linearly-independent displacement patterns.                    " << std::endl;
        std::cout << " **************************************************************************" << std::endl;
        std::cout << std::endl;
    }

    f_square = 0.0;
    for (i = 0; i < M; ++i) {
        fsum2[i] = bvec[i];
        f_square += std::pow(bvec[i], 2);
    }
    std::cout << "  QR-Decomposition has started ...";

    double *amat_mod, *cmat_mod;
    memory->allocate(amat_mod, M * N);
    memory->allocate(cmat_mod, P * N);

    // transpose matrix A and C
    k = 0;
    for (j = 0; j < N; ++j) {
        for (i = 0; i < M; ++i) {
            amat_mod[k++] = amat[i][j];
        }
    }
    k = 0;
    for (j = 0; j < N; ++j) {
        for (i = 0; i < P; ++i) {
            cmat_mod[k++] = cmat[i][j];
        }
    }

    // Fitting

    int LWORK = P + std::min<int>(M, N) + 10 * std::max<int>(M, N);
    int INFO;
    double *WORK, *x;
    memory->allocate(WORK, LWORK);
    memory->allocate(x, N);

    dgglse_(&M, &N, &P, amat_mod, &M, cmat_mod, &P,
            fsum2, dvec, x, WORK, &LWORK, &INFO);

    std::cout << " finished. " << std::endl;

    f_residual = 0.0;
    for (i = N - P; i < M; ++i) {
        f_residual += std::pow(fsum2[i], 2);
    }
    std::cout << std::endl << "  Residual sum of squares for the solution: "
        << sqrt(f_residual) << std::endl;
    std::cout << "  Fitting error (%) : "
        << std::sqrt(f_residual / f_square) * 100.0 << std::endl;

    // copy fcs to bvec

    for (i = 0; i < N; ++i) {
        param_out[i] = x[i];
    }

    memory->deallocate(amat_mod);
    memory->deallocate(cmat_mod);
    memory->deallocate(WORK);
    memory->deallocate(x);
    memory->deallocate(fsum2);
}

void Fitting::fit_algebraic_constraints(int N,
                                        int M,
                                        double **amat,
                                        double *bvec,
                                        double *param_out,
                                        double *bvec_orig,
                                        const int maxorder)
{
    int i, j;
    unsigned long k;
    int nrhs = 1, nrank, INFO, LWORK;
    int LMIN, LMAX;
    double rcond = -1.0;
    double f_square = 0.0;
    double *WORK, *S, *amat_mod, *fsum2;

    std::cout << "  Entering fitting routine: SVD with constraints considered algebraically." << std::endl;

    LMIN = std::min<int>(M, N);
    LMAX = std::max<int>(M, N);

    LWORK = 3 * LMIN + std::max<int>(2 * LMIN, LMAX);
    LWORK = 2 * LWORK;

    memory->allocate(WORK, LWORK);
    memory->allocate(S, LMIN);

    // transpose matrix A
    memory->allocate(amat_mod, M * N);
    memory->allocate(fsum2, LMAX);

    k = 0;
    for (j = 0; j < N; ++j) {
        for (i = 0; i < M; ++i) {
            amat_mod[k++] = amat[i][j];
        }
    }
    for (i = 0; i < M; ++i) {
        fsum2[i] = bvec[i];
        f_square += std::pow(bvec_orig[i], 2);
    }
    for (i = M; i < LMAX; ++i) fsum2[i] = 0.0;

    std::cout << "  SVD has started ... ";

    // Fitting with singular value decomposition
    dgelss_(&M, &N, &nrhs, amat_mod, &M, fsum2, &LMAX,
            S, &rcond, &nrank, WORK, &LWORK, &INFO);

    std::cout << "finished !" << std::endl << std::endl;

    std::cout << "  RANK of the matrix = " << nrank << std::endl;
    if (nrank < N)
        error->warn("fit_without_constraints",
                    "Matrix is rank-deficient. Force constants could not be determined uniquely :(");

    if (nrank == N) {
        double f_residual = 0.0;
        for (i = N; i < M; ++i) {
            f_residual += std::pow(fsum2[i], 2);
        }
        std::cout << std::endl;
        std::cout << "  Residual sum of squares for the solution: "
            << sqrt(f_residual) << std::endl;
        std::cout << "  Fitting error (%) : "
            << sqrt(f_residual / f_square) * 100.0 << std::endl;
    }

    int ishift = 0;
    int iparam = 0;
    double tmp;
    int inew, iold;

    for (i = 0; i < maxorder; ++i) {
        for (j = 0; j < constraint->const_fix[i].size(); ++j) {
            param_out[constraint->const_fix[i][j].p_index_target + ishift]
                = constraint->const_fix[i][j].val_to_fix;
        }

        for (boost::bimap<int, int>::const_iterator it = constraint->index_bimap[i].begin();
             it != constraint->index_bimap[i].end(); ++it) {
            inew = (*it).left + iparam;
            iold = (*it).right + ishift;

            param_out[iold] = fsum2[inew];
        }

        for (j = 0; j < constraint->const_relate[i].size(); ++j) {
            tmp = 0.0;

            for (k = 0; k < constraint->const_relate[i][j].alpha.size(); ++k) {
                tmp += constraint->const_relate[i][j].alpha[k]
                    * param_out[constraint->const_relate[i][j].p_index_orig[k] + ishift];
            }
            param_out[constraint->const_relate[i][j].p_index_target + ishift] = -tmp;
        }

        ishift += fcs->ndup[i].size();
        iparam += constraint->index_bimap[i].size();
    }

    memory->deallocate(WORK);
    memory->deallocate(S);
    memory->deallocate(fsum2);
    memory->deallocate(amat_mod);
}


void Fitting::calc_matrix_elements(const int M,
                                   const int N,
                                   const int nat,
                                   const int natmin,
                                   const int ndata_fit,
                                   const int nmulti,
                                   const int maxorder,
                                   double **u,
                                   double **f,
                                   double **amat,
                                   double *bvec)
{
    int i, j;
    int irow;
    int ncycle;

    std::cout << "  Calculation of matrix elements for direct fitting started ... ";
    for (i = 0; i < M; ++i) {
        for (j = 0; j < N; ++j) {
            amat[i][j] = 0.0;
        }
        bvec[i] = 0.0;
    }

    ncycle = ndata_fit * nmulti;

#ifdef _OPENMP
#pragma omp parallel private(irow, i, j)
#endif
    {
        int *ind;
        int mm, order, iat, k;
        int im, idata, iparam;
        double amat_tmp;

        memory->allocate(ind, maxorder + 1);

#ifdef _OPENMP
#pragma omp for schedule(guided)
#endif
        for (irow = 0; irow < ncycle; ++irow) {

            // generate r.h.s vector B
            for (i = 0; i < natmin; ++i) {
                iat = symmetry->map_p2s[i][0];
                for (j = 0; j < 3; ++j) {
                    im = 3 * i + j + 3 * natmin * irow;
                    bvec[im] = f[irow][3 * iat + j];
                }
            }

            // generate l.h.s. matrix A

            idata = 3 * natmin * irow;
            iparam = 0;

            for (order = 0; order < maxorder; ++order) {

                mm = 0;

                for (std::vector<int>::iterator iter = fcs->ndup[order].begin();
                     iter != fcs->ndup[order].end(); ++iter) {
                    for (i = 0; i < *iter; ++i) {
                        ind[0] = fcs->fc_table[order][mm].elems[0];
                        k = idata + inprim_index(fcs->fc_table[order][mm].elems[0]);
                        amat_tmp = 1.0;
                        for (j = 1; j < order + 2; ++j) {
                            ind[j] = fcs->fc_table[order][mm].elems[j];
                            amat_tmp *= u[irow][fcs->fc_table[order][mm].elems[j]];
                        }
                        amat[k][iparam] -= gamma(order + 2, ind) * fcs->fc_table[order][mm].coef * amat_tmp;
                        ++mm;
                    }
                    ++iparam;
                }
            }
        }

        memory->deallocate(ind);

    }

    std::cout << "done!" << std::endl << std::endl;
}


void Fitting::calc_matrix_elements_algebraic_constraint(const int M,
                                                        const int N,
                                                        const int N_new,
                                                        const int nat,
                                                        const int natmin,
                                                        const int ndata_fit,
                                                        const int nmulti,
                                                        const int maxorder,
                                                        double **u,
                                                        double **f,
                                                        double **amat,
                                                        double *bvec,
                                                        double *bvec_orig)
{
    int i, j;
    int irow;
    int ncycle;

    std::cout << "  Calculation of matrix elements for direct fitting started ... ";

    ncycle = ndata_fit * nmulti;


#ifdef _OPENMP
#pragma omp parallel for private(j)
#endif
    for (i = 0; i < M; ++i) {
        for (j = 0; j < N_new; ++j) {
            amat[i][j] = 0.0;
        }
        bvec[i] = 0.0;
        bvec_orig[i] = 0.0;
    }

#ifdef _OPENMP
#pragma omp parallel private(irow, i, j)
#endif
    {
        int *ind;
        int mm, order, iat, k;
        int im, idata, iparam;
        int ishift;
        int iold, inew;
        double amat_tmp;
        double **amat_orig;
        double **amat_mod;

        memory->allocate(ind, maxorder + 1);
        memory->allocate(amat_orig, 3 * natmin, N);
        memory->allocate(amat_mod, 3 * natmin, N_new);

#ifdef _OPENMP
#pragma omp for schedule(guided)
#endif
        for (irow = 0; irow < ncycle; ++irow) {

            // generate r.h.s vector B
            for (i = 0; i < natmin; ++i) {
                iat = symmetry->map_p2s[i][0];
                for (j = 0; j < 3; ++j) {
                    im = 3 * i + j + 3 * natmin * irow;
                    bvec[im] = f[irow][3 * iat + j];
                    bvec_orig[im] = f[irow][3 * iat + j];
                }
            }

            for (i = 0; i < 3 * natmin; ++i) {
                for (j = 0; j < N; ++j) {
                    amat_orig[i][j] = 0.0;
                }
                for (j = 0; j < N_new; ++j) {
                    amat_mod[i][j] = 0.0;
                }
            }

            // generate l.h.s. matrix A

            idata = 3 * natmin * irow;
            iparam = 0;

            for (order = 0; order < maxorder; ++order) {

                mm = 0;

                for (std::vector<int>::iterator iter = fcs->ndup[order].begin();
                     iter != fcs->ndup[order].end(); ++iter) {
                    for (i = 0; i < *iter; ++i) {
                        ind[0] = fcs->fc_table[order][mm].elems[0];
                        k = inprim_index(ind[0]);

                        amat_tmp = 1.0;
                        for (j = 1; j < order + 2; ++j) {
                            ind[j] = fcs->fc_table[order][mm].elems[j];
                            amat_tmp *= u[irow][fcs->fc_table[order][mm].elems[j]];
                        }
                        amat_orig[k][iparam] -= gamma(order + 2, ind) * fcs->fc_table[order][mm].coef * amat_tmp;
                        ++mm;
                    }
                    ++iparam;
                }
            }

            ishift = 0;
            iparam = 0;

            for (order = 0; order < maxorder; ++order) {

                for (i = 0; i < constraint->const_fix[order].size(); ++i) {

                    for (j = 0; j < 3 * natmin; ++j) {
                        bvec[j + idata] -= constraint->const_fix[order][i].val_to_fix
                            * amat_orig[j][ishift + constraint->const_fix[order][i].p_index_target];
                    }
                }

                for (boost::bimap<int, int>::const_iterator it = constraint->index_bimap[order].begin();
                     it != constraint->index_bimap[order].end(); ++it) {
                    inew = (*it).left + iparam;
                    iold = (*it).right + ishift;

                    for (j = 0; j < 3 * natmin; ++j) {
                        amat_mod[j][inew] = amat_orig[j][iold];
                    }
                }

                for (i = 0; i < constraint->const_relate[order].size(); ++i) {

                    iold = constraint->const_relate[order][i].p_index_target + ishift;

                    for (j = 0; j < constraint->const_relate[order][i].alpha.size(); ++j) {

                        inew = constraint->index_bimap[order].right.at(
                                                                 constraint->const_relate[order][i].p_index_orig[j])
                            + iparam;
                        for (k = 0; k < 3 * natmin; ++k) {
                            amat_mod[k][inew] -= amat_orig[k][iold] * constraint->const_relate[order][i].alpha[j];
                        }
                    }
                }

                ishift += fcs->ndup[order].size();
                iparam += constraint->index_bimap[order].size();
            }

            for (i = 0; i < 3 * natmin; ++i) {
                for (j = 0; j < N_new; ++j) {
                    amat[i + idata][j] = amat_mod[i][j];
                }
            }

        }

        memory->deallocate(ind);
        memory->deallocate(amat_orig);
        memory->deallocate(amat_mod);
    }

    std::cout << "done!" << std::endl << std::endl;
}

void Fitting::set_default_variables()
{
    seed = static_cast<unsigned int>(time(nullptr));
#ifdef _VSL
    brng = VSL_BRNG_MT19937;
    vslNewStream(&stream, brng, seed);
#else
    std::srand(seed);
#endif

    params = nullptr;
    u_in = nullptr;
    f_in = nullptr;
}

void Fitting::deallocate_variables()
{
    if (params) {
        memory->deallocate(params);
    }
    if (u_in) {
        memory->deallocate(u_in);
    }
    if (f_in) {
        memory->deallocate(f_in);
    }
}

void Fitting::data_multiplier(double **u,
                              double **f,
                              const int nat,
                              const int ndata_used,
                              const int nmulti,
                              const int multiply_data)
{
    int i, j, k;
    int idata, itran, isym;
    int n_mapped;
    double u_rot[3], f_rot[3];

    // Multiply data
    if (multiply_data == 0) {
        std::cout << " MULTDAT = 0: Given displacement-force data sets will be used as is."
            << std::endl << std::endl;
        idata = 0;
        for (i = 0; i < ndata_used; ++i) {
            for (j = 0; j < nat; ++j) {
                for (k = 0; k < 3; ++k) {
                    u[i][3 * j + k] = u_in[i][3 * j + k];
                    f[i][3 * j + k] = f_in[i][3 * j + k];
                }
            }
        }
    } else if (multiply_data == 1) {
        std::cout << "  MULTDAT = 1: Generate symmetrically equivalent displacement-force " << std::endl;
        std::cout << "               data sets by using pure translational operations only." << std::endl << std::endl;
        idata = 0;
        for (i = 0; i < ndata_used; ++i) {
            for (itran = 0; itran < symmetry->ntran; ++itran) {
                for (j = 0; j < nat; ++j) {
                    n_mapped = symmetry->map_sym[j][symmetry->symnum_tran[itran]];
                    for (k = 0; k < 3; ++k) {
                        u[idata][3 * n_mapped + k] = u_in[i][3 * j + k];
                        f[idata][3 * n_mapped + k] = f_in[i][3 * j + k];
                    }
                }
                ++idata;
            }
        }
    } else if (multiply_data == 2) {
        std::cout << "  MULTDAT = 2: Generate symmetrically equivalent displacement-force" << std::endl;
        std::cout << "                data sets. (including rotational part) " << std::endl << std::endl;
        idata = 0;
        for (i = 0; i < ndata_used; ++i) {
#pragma omp parallel for private(j, n_mapped, k, u_rot, f_rot)
            for (isym = 0; isym < symmetry->nsym; ++isym) {
                for (j = 0; j < nat; ++j) {
                    n_mapped = symmetry->map_sym[j][isym];
                    for (k = 0; k < 3; ++k) {
                        u_rot[k] = u_in[i][3 * j + k];
                        f_rot[k] = f_in[i][3 * j + k];
                    }
                    rotvec(u_rot, u_rot, symmetry->symrel[isym]);
                    rotvec(f_rot, f_rot, symmetry->symrel[isym]);
                    for (k = 0; k < 3; ++k) {
                        u[nmulti * idata + isym][3 * n_mapped + k] = u_rot[k];
                        f[nmulti * idata + isym][3 * n_mapped + k] = f_rot[k];
                    }
                }
            }
            ++idata;
        }
    } else {
        error->exit("data_multiplier", "Unsupported MULTDAT");
    }
}

int Fitting::get_number_for_multiplier(const int multiply_data)
{
    int nmulti = 0;
    if (multiply_data == 0) {
        nmulti = 1;
    } else if (multiply_data == 1) {
        nmulti = symmetry->ntran;
    } else if (multiply_data == 2) {
        nmulti = symmetry->nsym;
    }
    return nmulti;
}

int Fitting::inprim_index(const int n)
{
    int in;
    int atmn = n / 3;
    int crdn = n % 3;

    for (int i = 0; i < symmetry->natmin; ++i) {
        if (symmetry->map_p2s[i][0] == atmn) {
            in = 3 * i + crdn;
            break;
        }
    }
    return in;
}

double Fitting::gamma(const int n, const int *arr)
{
    int *arr_tmp, *nsame;
    int i;
    int ind_front, nsame_to_front;

    memory->allocate(arr_tmp, n);
    memory->allocate(nsame, n);

    for (i = 0; i < n; ++i) {
        arr_tmp[i] = arr[i];
        nsame[i] = 0;
    }

    ind_front = arr[0];
    nsame_to_front = 1;

    interaction->insort(n, arr_tmp);

    int nuniq = 1;
    int iuniq = 0;

    nsame[0] = 1;

    for (i = 1; i < n; ++i) {
        if (arr_tmp[i] == arr_tmp[i - 1]) {
            ++nsame[iuniq];
        } else {
            ++nsame[++iuniq];
            ++nuniq;
        }

        if (arr[i] == ind_front) ++nsame_to_front;
    }

    int denom = 1;

    for (i = 0; i < nuniq; ++i) {
        denom *= factorial(nsame[i]);
    }

    memory->deallocate(arr_tmp);
    memory->deallocate(nsame);

    return static_cast<double>(nsame_to_front) / static_cast<double>(denom);
}

int Fitting::factorial(const int n)
{
    if (n == 1 || n == 0) {
        return 1;
    } else {
        return n * factorial(n - 1);
    }
}

#ifdef _USE_EIGEN_DISABLED
int Fitting::getRankEigen(const int m, const int n, double **mat)
{
    using namespace Eigen;

    MatrixXd mat_tmp(m, n);

    int i, j;

    for (i = 0; i < m; ++i) {
        for (j = 0; j < n; ++j) {
            mat_tmp(i,j) = mat[i][j];
        }
    }
    ColPivHouseholderQR<MatrixXd> qr(mat_tmp);
    return qr.rank();
}
#endif

int Fitting::rankQRD(const int m,
                     const int n,
                     double *mat,
                     const double tolerance)
{
    // Return the rank of matrix mat revealed by the column pivoting QR decomposition
    // The matrix mat is destroyed.

    int m_ = m;
    int n_ = n;

    int LDA = m_;

    int LWORK = 10 * n_;
    int INFO;
    int *JPVT;
    double *WORK, *TAU;

    int nmin = std::min<int>(m_, n_);

    memory->allocate(JPVT, n_);
    memory->allocate(WORK, LWORK);
    memory->allocate(TAU, nmin);

    for (int i = 0; i < n_; ++i) JPVT[i] = 0;

    dgeqp3_(&m_, &n_, mat, &LDA, JPVT, TAU, WORK, &LWORK, &INFO);

    memory->deallocate(JPVT);
    memory->deallocate(WORK);
    memory->deallocate(TAU);

    if (std::abs(mat[0]) < eps) return 0;

    double **mat_tmp;
    memory->allocate(mat_tmp, m_, n_);

    unsigned long k = 0;

    for (int j = 0; j < n_; ++j) {
        for (int i = 0; i < m_; ++i) {
            mat_tmp[i][j] = mat[k++];
        }
    }

    int nrank = 0;
    for (int i = 0; i < nmin; ++i) {
        if (std::abs(mat_tmp[i][i]) > tolerance * std::abs(mat[0])) ++nrank;
    }

    memory->deallocate(mat_tmp);

    return nrank;
}

int Fitting::rankSVD(const int m,
                     const int n,
                     double *mat,
                     const double tolerance)
{
    int i;
    int m_ = m;
    int n_ = n;

    int LWORK = 10 * m;
    int INFO;
    int *IWORK;
    int ldu = 1, ldvt = 1;
    double *s, *WORK;
    double u[1], vt[1];

    int nmin = std::min<int>(m, n);

    memory->allocate(IWORK, 8 * nmin);
    memory->allocate(WORK, LWORK);
    memory->allocate(s, nmin);

    char mode[] = "N";

    dgesdd_(mode, &m_, &n_, mat, &m_, s, u, &ldu, vt, &ldvt,
            WORK, &LWORK, IWORK, &INFO);

    int rank = 0;
    for (i = 0; i < nmin; ++i) {
        if (s[i] > s[0] * tolerance) ++rank;
    }

    memory->deallocate(WORK);
    memory->deallocate(IWORK);
    memory->deallocate(s);

    return rank;
}

int Fitting::rankSVD2(const int m_in,
                      const int n_in,
                      double **mat,
                      const double tolerance)
{
    // Reveal the rank of matrix mat without destroying the matrix elements

    int i, j, k;
    double *arr;

    int m = m_in;
    int n = n_in;

    memory->allocate(arr, m * n);

    k = 0;

    for (j = 0; j < n; ++j) {
        for (i = 0; i < m; ++i) {
            arr[k++] = mat[i][j];
        }
    }

    int LWORK = 10 * m;
    int INFO;
    int *IWORK;
    int ldu = 1, ldvt = 1;
    double *s, *WORK;
    double u[1], vt[1];

    int nmin = std::min<int>(m, n);

    memory->allocate(IWORK, 8 * nmin);
    memory->allocate(WORK, LWORK);
    memory->allocate(s, nmin);

    char mode[] = "N";

    dgesdd_(mode, &m, &n, arr, &m, s, u, &ldu, vt, &ldvt,
            WORK, &LWORK, IWORK, &INFO);

    int rank = 0;
    for (i = 0; i < nmin; ++i) {
        if (s[i] > s[0] * tolerance) ++rank;
    }

    memory->deallocate(IWORK);
    memory->deallocate(WORK);
    memory->deallocate(s);
    memory->deallocate(arr);

    return rank;
}
