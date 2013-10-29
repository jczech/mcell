/***********************************************************************************
 *                                                                                 *
 * Copyright (C) 2006-2013 by                                                      *
 * The Salk Institute for Biological Studies and                                   *
 * Pittsburgh Supercomputing Center, Carnegie Mellon University                    *
 *                                                                                 *
 * This program is free software; you can redistribute it and/or                   *
 * modify it under the terms of the GNU General Public License                     *
 * as published by the Free Software Foundation; either version 2                  *
 * of the License, or (at your option) any later version.                          *
 *                                                                                 *
 * This program is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of                  *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                   *
 * GNU General Public License for more details.                                    *
 *                                                                                 *
 * You should have received a copy of the GNU General Public License               *
 * along with this program; if not, write to the Free Software                     *
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA. *
 *                                                                                 *
 ***********************************************************************************/

#ifndef LIBMCELL_H 
#define LIBMCELL_H


#include "config.h"

#include "mcell_engine.h"
#include "mcell_structs.h"


typedef struct volume MCELL_STATE;
typedef int MCELL_STATUS;

/****************************************************************
 * setup routines 
 ****************************************************************/
MCELL_STATE* mcell_create();

MCELL_STATUS mcell_init_state();

MCELL_STATUS mcell_parse_mdl(MCELL_STATE *state);

MCELL_STATUS mcell_init_simulation(MCELL_STATE *state);

MCELL_STATUS mcell_read_checkpoint(MCELL_STATE *state);

MCELL_STATUS mcell_init_output(MCELL_STATE *state);


/****************************************************************
 * routines for running simulations 
 ****************************************************************/

/* this function runs the whole simulations */
MCELL_STATUS mcell_run_simulation(MCELL_STATE *state);

/* returns the recommended output frequence either based on 
 * a user request in the MDL or via some heuristics */
long long mcell_determine_output_frequency(MCELL_STATE *state);

/* this function runs a single iteration of simulations */
MCELL_STATUS mcell_run_iteration(MCELL_STATE *state, 
    long long output_frequency, int *restarted_from_checkpoint);

/* flush all output buffers to disk to disk after the simulation
 * run is complete */
MCELL_STATUS mcell_flush_data(MCELL_STATE *state);

/* print any warnings that were gererated during the simulation
 * run */
MCELL_STATUS mcell_print_final_warnings(MCELL_STATE *state);

/* print the final simulation statistics */
MCELL_STATUS mcell_print_final_statistics(MCELL_STATE *state);



/*****************************************************************
 * helper functions 
 *****************************************************************/
void mcell_print_version();
void mcell_print_usage(const char *executable_name);
void mcell_print_stats();
int mcell_argparse(int argc, char **argv, MCELL_STATE *state);

// XXX this is a temporary hack to be able to print in mcell.c
// since mcell disables regular printf
void mcell_print(const char *message);

#endif