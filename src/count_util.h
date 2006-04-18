#ifndef MCELL_COUNT_UTIL
#define MCELL_COUNT_UTIL

#include "mcell_structs.h"

void update_collision_count(struct species *sp,struct region_list *rl,int direction,int crossed,double factor);
int region_listed(struct region_list *rl,struct region *r);
void count_me_by_region(struct abstract_molecule *me,int n,struct rxn_pathname *rxp);
int place_waypoints();
int check_region_counters();

#endif
