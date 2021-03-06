/*
 alm_cui.cpp

 Copyright (c) 2014, 2015, 2016 Terumasa Tadano

 This file is distributed under the terms of the MIT license.
 Please see the file 'LICENCE.txt' in the root directory
 or http://opensource.org/licenses/mit-license.php for information.
*/

#include <iostream>
#include <iomanip>
#include "alm.h"
#include "alm_core.h"
#include "alm_cui.h"
#include "input_parser.h"
#include "writer.h"
#include "version.h"
#include "timer.h"

#ifdef _OPENMP
#include <omp.h>
#endif

using namespace ALM_NS;

ALMCUI::ALMCUI()
{
}

void ALMCUI::run(int narg, char **arg)
{
    std::cout << " +-----------------------------------------------------------------+" << std::endl;
    std::cout << " +                         Program ALM                             +" << std::endl;
    std::cout << " +                             Ver.";
    std::cout << std::setw(7) << ALAMODE_VERSION;
    std::cout << "                         +" << std::endl;
    std::cout << " +-----------------------------------------------------------------+" << std::endl;
    std::cout << std::endl;

    ALM *alm = new ALM();

#ifdef _OPENMP
    std::cout << " Number of OpenMP threads = "
        << omp_get_max_threads() << std::endl << std::endl;
#endif

    ALMCore *alm_core = alm->get_alm_core();
    std::cout << " Job started at " << alm_core->timer->DateAndTime() << std::endl;
    // alm_core->mode is set herein.
    InputParser *input_parser = new InputParser();
    input_parser->run(alm_core, narg, arg);

    Writer *writer = new Writer();
    writer->write_input_vars(alm);


    if (alm_core->mode == "fitting") {
        input_parser->parse_displacement_and_force(alm_core);
    }
    delete input_parser;

    alm->run();

    if (alm_core->mode == "fitting") {
        writer->writeall(alm);
    } else if (alm_core->mode == "suggest") {
        writer->write_displacement_pattern(alm);
    }
    delete writer;

    std::cout << std::endl << " Job finished at "
        << alm_core->timer->DateAndTime() << std::endl;

    /*
        std::cout << "FCS: " << alm_core->timer->get_walltime("fcs") << " " << alm_core->timer->get_cputime("fcs") << std::endl;
        std::cout << "CONSTRAINT: " << alm_core->timer->get_walltime("constraint") << " " << alm_core->timer->get_cputime("constraint") << std::endl;
        std::cout << "FITTING: " << alm_core->timer->get_walltime("fitting") << " " << alm_core->timer->get_cputime("fitting") << std::endl;
        std::cout << "SYSTEM: " << alm_core->timer->get_walltime("system") << std::endl;
    */

    delete alm;
}

ALMCUI::~ALMCUI()
{
}
