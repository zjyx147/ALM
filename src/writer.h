/*
 writer.h

 Copyright (c) 2014, 2015, 2016 Terumasa Tadano

 This file is distributed under the terms of the MIT license.
 Please see the file 'LICENCE.txt' in the root directory 
 or http://opensource.org/licenses/mit-license.php for information.
*/

#pragma once

#include <string>
#include <fstream>
#include <vector>
#include "alm.h"

namespace ALM_NS
{
    class AtomProperty
    {
    public:
        double x, y, z;
        int kind;
        int atom, tran;

        AtomProperty()
        {
        };

        AtomProperty(const AtomProperty &other)
            : x(other.x), y(other.y), z(other.z),
              kind(other.kind), atom(other.atom), tran(other.tran)
        {
        };

        AtomProperty(const double *pos,
                     const int kind_in,
                     const int atom_in,
                     const int tran_in)
        {
            x = pos[0];
            y = pos[1];
            z = pos[2];
            kind = kind_in;
            atom = atom_in;
            tran = tran_in;
        }
    };

    class SystemInfo
    {
    public:
        double lattice_vector[3][3];
        std::vector<AtomProperty> atoms;
        int nat, natmin, ntran;
        int nspecies;

        SystemInfo()
        {
        };
    };

    class Writer
    {
    public:
        Writer();
        ~Writer();

        void writeall(ALM *);
        void write_input_vars(ALM *);
        void write_displacement_pattern(ALM *);

    private:
        void write_force_constants(ALM *);
        void write_misc_xml(ALM *);
        void write_hessian(ALM *);
        void write_in_QEformat(ALMCore *);

        std::string double2string(const double, const int nprec = 15);
    };
}
