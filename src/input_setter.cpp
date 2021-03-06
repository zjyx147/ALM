/*
 input_setter.cpp

 Copyright (c) 2014, 2015, 2016 Terumasa Tadano

 This file is distributed under the terms of the MIT license.
 Please see the file 'LICENCE.txt' in the root directory 
 or http://opensource.org/licenses/mit-license.php for information.
*/

#include <string>
#include "input_setter.h"
#include "alm_core.h"
#include "memory.h"
#include "files.h"
#include "interaction.h"
#include "system.h"
#include "symmetry.h"
#include "fitting.h"
#include "constraint.h"
#include "patterndisp.h"

using namespace ALM_NS;

InputSetter::InputSetter()
{
}

InputSetter::~InputSetter()
{
}

void InputSetter::deallocator(ALMCore *alm_core)
{
    if (alm_core->system->kdname) {
        deallocate(alm_core->system->kdname);
    }
    alm_core->system->kdname = nullptr;
    if (alm_core->system->xcoord) {
        deallocate(alm_core->system->xcoord);
    }
    alm_core->system->xcoord = nullptr;
    if (alm_core->system->kd) {
        deallocate(alm_core->system->kd);
    }
    alm_core->system->kd = nullptr;
    if (alm_core->system->magmom) {
        deallocate(alm_core->system->magmom);
    }
    alm_core->system->magmom = nullptr;
    if (alm_core->interaction->nbody_include) {
        deallocate(alm_core->interaction->nbody_include);
    }
    alm_core->interaction->nbody_include = nullptr;
    if (alm_core->interaction->rcs) {
        deallocate(alm_core->interaction->rcs);
    }
    alm_core->interaction->rcs = nullptr;
}

void InputSetter::set_general_vars(ALMCore *alm_core,
                                   const std::string prefix,
                                   const std::string mode,
                                   const std::string str_disp_basis,
                                   const std::string str_magmom,
                                   const int nat,
                                   const int nkd,
                                   const int nsym,
                                   const int printsymmetry,
                                   const int is_periodic[3],
                                   const bool trim_dispsign_for_evenfunc,
                                   const bool lspin,
                                   const bool print_hessian,
                                   const int noncollinear,
                                   const int trevsym,
                                   const std::string *kdname,
                                   const double * const *magmom,
                                   const double tolerance,
                                   const double tolerance_constraint)
{
    int i, j;

    alm_core->files->job_title = prefix;
    alm_core->mode = mode;
    alm_core->system->nat = nat;
    alm_core->system->nkd = nkd;
    alm_core->system->str_magmom = str_magmom;
    alm_core->symmetry->nsym = nsym;
    alm_core->symmetry->printsymmetry = printsymmetry;
    alm_core->symmetry->tolerance = tolerance;
    allocate(alm_core->system->kdname, nkd);
    allocate(alm_core->system->magmom, nat, 3);

    for (i = 0; i < nkd; ++i) {
        alm_core->system->kdname[i] = kdname[i];
    }
    for (i = 0; i < 3; ++i) {
        alm_core->interaction->is_periodic[i] = is_periodic[i];
    }
    for (i = 0; i < nat; ++i) {
        for (j = 0; j < 3; ++j) {
            alm_core->system->magmom[i][j] = magmom[i][j];
        }
    }
    alm_core->system->lspin = lspin;
    alm_core->system->noncollinear = noncollinear;
    alm_core->symmetry->trev_sym_mag = trevsym;
    alm_core->files->print_hessian = print_hessian;
    alm_core->constraint->tolerance_constraint = tolerance_constraint;

    if (mode == "suggest") {
        alm_core->displace->disp_basis = str_disp_basis;
        alm_core->displace->trim_dispsign_for_evenfunc = trim_dispsign_for_evenfunc;
    }
}

void InputSetter::set_cell_parameter(ALMCore *alm_core,
                                     const double a,
                                     const double lavec_tmp[3][3])
{
    int i, j;

    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 3; ++j) {
            alm_core->system->lavec[i][j] = a * lavec_tmp[i][j];
        }
    }
}

void InputSetter::set_interaction_vars(ALMCore *alm_core,
                                       const int maxorder,
                                       const int *nbody_include)
{
    int i;

    alm_core->interaction->maxorder = maxorder;
    allocate(alm_core->interaction->nbody_include, maxorder);

    for (i = 0; i < maxorder; ++i) {
        alm_core->interaction->nbody_include[i] = nbody_include[i];
    }
}

void InputSetter::set_cutoff_radii(ALMCore *alm_core,
                                   const int maxorder,
                                   const int nkd,
                                   const double * const * const *rcs)
{
    int i, j, k;

    allocate(alm_core->interaction->rcs, maxorder, nkd, nkd);

    for (i = 0; i < maxorder; ++i) {
        for (j = 0; j < nkd; ++j) {
            for (k = 0; k < nkd; ++k) {
                alm_core->interaction->rcs[i][j][k] = rcs[i][j][k];
            }
        }
    }
}

void InputSetter::set_fitting_vars(ALMCore *alm_core,
                                   const int ndata,
                                   const int nstart,
                                   const int nend,
                                   const std::string dfile,
                                   const std::string ffile,
                                   const int constraint_flag,
                                   const std::string rotation_axis,
                                   const std::string fc2_file,
                                   const std::string fc3_file,
                                   const bool fix_harmonic,
                                   const bool fix_cubic)
{
    alm_core->system->ndata = ndata;
    alm_core->system->nstart = nstart;
    alm_core->system->nend = nend;

    alm_core->files->file_disp = dfile;
    alm_core->files->file_force = ffile;
    alm_core->constraint->constraint_mode = constraint_flag;
    alm_core->constraint->rotation_axis = rotation_axis;
    alm_core->constraint->fc2_file = fc2_file;
    alm_core->constraint->fix_harmonic = fix_harmonic;
    alm_core->constraint->fc3_file = fc3_file;
    alm_core->constraint->fix_cubic = fix_cubic;
}

void InputSetter::set_atomic_positions(ALMCore *alm_core,
                                       const int nat,
                                       const int *kd,
                                       const double * const *xeq)
{
    int i, j;

    allocate(alm_core->system->xcoord, nat, 3);
    allocate(alm_core->system->kd, nat);

    for (i = 0; i < nat; ++i) {

        alm_core->system->kd[i] = kd[i];

        for (j = 0; j < 3; ++j) {
            alm_core->system->xcoord[i][j] = xeq[i][j];
        }
    }
}
