

#include <fnmatch.h>
#include <ftw.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "mcell_structs.h"
#include "grid_util.h"
#include "sched_util.h"
#include "viz_output.h"
#include "strfunc.h"
#include "util.h"


extern struct volume *world;
/* final iteration number in the simulation */
static long long final_iteration = 0; 
static int obj_to_show_number; /* number of viz_obj objects in the world */
static int eff_to_show_number; /* number of types of effectors */ 
static int mol_to_show_number; /* number of types of 3D mol's */

/* these arrays are used to hold values of main_index for objects */
u_int *surf_states = NULL;
u_int *surf_pos = NULL;
u_int *surf_con = NULL;
u_int *surf_field_indices = NULL;
u_int *eff_pos = NULL;
u_int *eff_orient = NULL;
u_int *eff_states = NULL;
u_int *eff_field_indices = NULL;
u_int *mol_pos = NULL;
u_int *mol_orient = NULL;
u_int *mol_states = NULL;
u_int *mol_field_indices = NULL;
/* array of viz_objects names */
char **obj_names = NULL;
/* array of volume_molecule names */
char **mol_names = NULL;
/* array of surface_molecule (effectors) names */
char **eff_names = NULL;
/* 2-dimensional array of regions names for each object. 
   First index refers to the object, second - to the regions. */
char  ***region_names = NULL; 
/* array that holds number of regions for each object */
int *obj_num_regions = NULL;
/* 2-dimensional array of regions indices for each object. 
   First index refers to the object, second - to the regions indices. */
int  **surf_region_values = NULL; 

  /* used for creating links to the files */
  static long long last_meshes_iteration = -1;
  static long long last_mols_iteration = -1;

  /* used in 'time_values' and 'iteration_numbers' arrays */
  /* total number of non-equal values of "viz_iteration"
     combined for all frames */
  static  u_int time_values_total = 0;
  u_int *time_values = NULL;

/**************************************************************************
update_frame_data_list:
	In: struct frame_data_list * fdlp
        Out: Nothing. Calls output visualization functions if necessary.
	     Updates value of the current iteration step and pointer
             to the current iteration in the linked list
**************************************************************************/
void update_frame_data_list(struct frame_data_list *fdlp)
{
  FILE *log_file = NULL;

  log_file=world->log_file;

  if(world->viz_mode == DREAMM_V3_MODE)
  {
  /* this part of the code is used for creating symbolic links in viz_output */

     struct frame_data_list *fdlp_temp = NULL;
     fdlp_temp = fdlp;
     while (fdlp_temp!=NULL) {
       if(world->it_time==fdlp_temp->viz_iterationll)
       {
     
       switch (fdlp_temp->list_type) {
         case OUTPUT_BY_ITERATION_LIST:
      
               if((fdlp_temp->type == ALL_MESH_DATA) ||
                   (fdlp_temp->type == REG_DATA) ||
                   (fdlp_temp->type == MESH_GEOMETRY))
               {

                  if(fdlp_temp->viz_iterationll > last_meshes_iteration){
                     last_meshes_iteration = fdlp_temp->viz_iterationll;
                   }
               }else if((fdlp_temp->type == ALL_MOL_DATA) ||
                   (fdlp_temp->type == MOL_POS) ||
                   (fdlp_temp->type == MOL_ORIENT))
               {
                   if(fdlp_temp->viz_iterationll > last_mols_iteration){
                     last_mols_iteration = fdlp_temp->viz_iterationll;
                   }
               }
         
            break;
         case OUTPUT_BY_TIME_LIST:
        
               if((fdlp_temp->type == ALL_MESH_DATA) ||
                   (fdlp_temp->type == REG_DATA) ||
                   (fdlp_temp->type == MESH_GEOMETRY))
               {
                  if(fdlp_temp->viz_iterationll > last_meshes_iteration){
                      last_meshes_iteration = fdlp_temp->viz_iterationll;
                   }
              }else if((fdlp_temp->type == ALL_MOL_DATA) ||
                   (fdlp_temp->type == MOL_POS) ||
                   (fdlp_temp->type == MOL_ORIENT))
               {
                  if(fdlp_temp->viz_iterationll > last_mols_iteration){
                        last_mols_iteration = fdlp_temp->viz_iterationll;
                   }
               }
	
              break;
          default:
              fprintf(world->err_file,"File '%s', Line %ld: error - wrong frame_data_list list_type %d\n", __FILE__, (long)__LINE__, fdlp->list_type);
              break;
       } /* end switch */

     } /* end if (world->it_time == fdlp_temp->viz_iterationll) */
    fdlp_temp = fdlp_temp->next;
    
   } /* end while */
  } /* if (world->viz_mode) */


  while (fdlp!=NULL) {
    if(world->it_time==fdlp->viz_iterationll)
    {
 
      switch (world->viz_mode)
      {
	case DX_MODE:
          output_dx_objects(fdlp);
	  break;
	case DREAMM_V3_MODE:
          output_dreamm_objects(fdlp);
	  break;
	case DREAMM_V3_GROUPED_MODE:
          output_dreamm_objects_grouped(fdlp);
	  break;
	case RK_MODE:
          output_rk_custom(fdlp);
	  break;
	case ASCII_MODE:
	  output_ascii_molecules(fdlp);
	  break;
	case NO_VIZ_MODE:
	default:
	  /* Do nothing for vizualization */
	  break;
      }
      fdlp->curr_viz_iteration=fdlp->curr_viz_iteration->next;
      if (fdlp->curr_viz_iteration!=NULL) {
	switch (fdlp->list_type) {
	  case OUTPUT_BY_ITERATION_LIST:
	  	  fdlp->viz_iterationll=(long long)fdlp->curr_viz_iteration->value; 
	          break;
	  case OUTPUT_BY_TIME_LIST:
	          fdlp->viz_iterationll=(long long)(fdlp->curr_viz_iteration->value/world->time_unit + ROUND_UP);
	          break;
          default:
                  fprintf(world->err_file,"File '%s', Line %ld: error - wrong frame_data_list list_type %d\n", __FILE__, (long)__LINE__, fdlp->list_type);
                  break;
	}
      }
    }

    fdlp=fdlp->next;
  }
  return;
}

/********************************************************************* 
init_frame_data:
   In: struct frame_data_list* fdlp
   Out: nothing.  Initializes  frame_data_list structure. 
   	Sets the value of the current iteration step to the start value.
   	Sets the number of iterations. 
        Initializes parameters used in 
       'output_dreamm_objects()' and 'output_dreamm_objects_grouped()'. 
***********************************************************************/
void init_frame_data_list(struct frame_data_list *fdlp)
{
  struct num_expr_list *nelp = NULL;
  int done, ii, jj;
  struct species *sp = NULL;
  int mol_orient_frame_present = 0;
  int mol_pos_frame_present = 0;
  int reg_data_frame_present = 0;
  int mesh_geometry_frame_present = 0;
  FILE *log_file = NULL;

  log_file=world->log_file;

  while (fdlp!=NULL) {
 
    fdlp->viz_iterationll=-1;
    fdlp->n_viz_iterations=0;
    nelp=fdlp->iteration_list;
    done=0;
    switch (fdlp->list_type) {
    case OUTPUT_BY_ITERATION_LIST:
      while (nelp!=NULL) {
	fdlp->n_viz_iterations++;

	if (!done) {
	  if (nelp->value>=world->start_time) {
             fdlp->viz_iterationll=(long long)nelp->value;
             fdlp->curr_viz_iteration=nelp;
             done=1;
	  }
	}
        
        if((long long)(nelp->value) > final_iteration){
             final_iteration = (long long)(nelp->value);
        }
         
	nelp=nelp->next;
      }
      break;
    case OUTPUT_BY_TIME_LIST:
      while (nelp!=NULL) {
	fdlp->n_viz_iterations++;

        if (!done) {
	  if (nelp->value>=world->current_start_real_time) {
	    fdlp->viz_iterationll=(long long)(nelp->value/world->time_unit+ROUND_UP);
	    fdlp->curr_viz_iteration=nelp;
	    done=1;
	  }
        }
        if((long long)(nelp->value/world->time_unit + ROUND_UP) > final_iteration){
             final_iteration = (long long)(nelp->value/world->time_unit + ROUND_UP);
        }
	nelp=nelp->next;
      }
      break;
     default:
         fprintf(world->err_file,"File '%s', Line %ld: error - wrong frame_data_list list_type %d\n", __FILE__, (long)__LINE__, fdlp->list_type);
         break;
    }

    if(fdlp->type == MOL_ORIENT) {
        mol_orient_frame_present = 1;
    }
    if(fdlp->type == MOL_POS) {
        mol_pos_frame_present = 1;
    }
    if(fdlp->type == MESH_GEOMETRY) {
        mesh_geometry_frame_present = 1;
    }
    if(fdlp->type == REG_DATA) {
        reg_data_frame_present = 1;
    }

    if(fdlp->n_viz_iterations > time_values_total){
        time_values_total = fdlp->n_viz_iterations;
    }

    fdlp=fdlp->next;
  } /* end while */

  if(world->iterations < time_values_total - 1){
      time_values_total = world->iterations + 1;
  }
  if(world->chkpt_flag){
    
    if((world->start_time + world->chkpt_iterations) < time_values_total -1){
        time_values_total = world->start_time + world->chkpt_iterations + 1;
    }
  }

  
  if((mol_orient_frame_present) & (!mol_pos_frame_present)){
     fprintf(log_file, "The input file contains ORIENTATIONS but not POSITIONS statement in the MOLECULES block. The molecules cannot be visualized.\n");
  }
  if((reg_data_frame_present) & (!mesh_geometry_frame_present)){
     fprintf(log_file, "The input file contains REGION_DATA but not GEOMETRY statement in the MESHES block. The meshes cannot be visualized.\n");
  }

  /* find out the number of objects to visualize. */
  struct viz_obj *vizp = world->viz_obj_head;
  while(vizp != NULL){
        struct viz_child *vcp = vizp->viz_child_head;
        while(vcp != NULL){
	   obj_to_show_number++;
 
           vcp = vcp->next;
        } /* end while (vcp) */
	vizp = vizp->next;
  } /* end while (vizp) */


  /* fill the obj_num_regions array */
  if (obj_to_show_number==0) obj_num_regions = (int*)malloc(sizeof(int));
  else obj_num_regions = (int *)malloc(obj_to_show_number * sizeof(int)); 
  if(obj_num_regions == NULL){
     fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
     exit(1);
  }
  vizp = world->viz_obj_head;
  ii = 0;
  while(vizp != NULL){
        struct viz_child *vcp = vizp->viz_child_head;
        while(vcp != NULL){
           /* find out the number of regions in the object */ 
           obj_num_regions[ii] = vcp->obj->num_regions - 1;
           ii++;
           vcp = vcp->next;
        } /* end while (vcp) */
	vizp = vizp->next;
  } /* end while (vizp) */

  
  /* declare and initialize 2-dimensional array that will hold
     indices of the "region_data" objects which are components
     of the field object.
     Here first index points to the object and the second index
     - to the array of indices for this object regions */
  if (obj_to_show_number==0) surf_region_values = (int**)malloc(sizeof(int*));
  else surf_region_values = (int **)malloc(obj_to_show_number * sizeof(int*));
  if(surf_region_values == NULL){
     fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
       exit(1);
  }
  /* initialize the array */
  for(ii = 0; ii < obj_to_show_number; ii++)
  {
     surf_region_values[ii] = NULL;

  }
  int n_regs; /* number of regions for the object */
  for(ii = 0; ii < obj_to_show_number; ii++)
  {
    n_regs = obj_num_regions[ii];
    if(n_regs > 0){
       surf_region_values[ii] = (int *)malloc(n_regs*sizeof(int));
       if(surf_region_values[ii] == NULL){
          fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
          exit(1);
       }
       for(jj = 0; jj < n_regs; jj++)
       {
          surf_region_values[ii][jj] = 0;
       }
    }
      
  }


  /* find out number of effectors and 3D molecules to visualize */
  for(ii = 0; ii < world->n_species; ii++)
  {
     sp = world->species_list[ii];
     if((sp->flags & IS_SURFACE) != 0) continue;
     if(strcmp(sp->sym->name, "GENERIC_MOLECULE") == 0) continue;
     if(((sp->flags & ON_GRID) == ON_GRID) && (sp->viz_state > 0)){ 
	  eff_to_show_number++;
     }else if(((sp->flags & NOT_FREE) == 0) && (sp->viz_state > 0)) {
          mol_to_show_number++;

     }    
  }
  return;
}

/*************************************************************************
output_dx_objects:
	In: struct frame_data_list *fdlp
	Out: 0 on success, 1 on error; output visualization files (*.dx)
             are written.
**************************************************************************/

int output_dx_objects(struct frame_data_list *fdlp)
{
  FILE *log_file = NULL;
  FILE *wall_verts_header = NULL;
  FILE *wall_states_header = NULL;
  FILE *eff_pos_header = NULL;
  FILE *eff_states_header = NULL;
  FILE *mol_pos_header = NULL;
  FILE *mol_states_header = NULL;
  struct viz_obj *vizp = NULL;
  struct viz_child *vcp = NULL;
  struct wall *w = NULL,**wp = NULL;
  struct surface_grid *sg = NULL;
  struct grid_molecule *gmol = NULL;
  struct species **species_list = NULL;
  struct object *objp = NULL;
  struct polygon_object *pop = NULL;
  struct ordered_poly *opp = NULL;
  struct element_data *edp = NULL;
  struct vector3 p0;
  struct storage_list *slp = NULL;
  struct storage *sp = NULL;
  struct schedule_helper *shp = NULL;
  struct abstract_molecule *amp = NULL;
  struct volume_molecule *molp = NULL,***viz_molp = NULL;
  float v1,v2,v3;
  u_int n_tiles,spec_id,*viz_mol_count = NULL;
  u_int mol_pos_index,mol_pos_field_index,mol_pos_group_index;
  u_int mol_states_index,mol_states_group_index;
  int ii,jj;
  int vi1,vi2,vi3;
  /*int vi4;*/
  int num;
  long long viz_iterationll;
  long long n_viz_iterations;
  /* int first_viz_iteration; */
  int pos_count,state_count,element_data_count;
  int effector_state_pos_count;
  int state;
  int viz_type;
  unsigned int index,n_eff;
  int word;
  byte *word_p = NULL;
  byte viz_eff,viz_mol,viz_surf;
  byte viz_surf_or_eff;
  byte viz_eff_pos,viz_eff_states;
  byte viz_mol_pos,viz_mol_states;
  byte viz_surf_pos,viz_surf_states;
  char file_name[1024];
  char my_byte_order[8];

  u_int n_species = world->n_species;
  
  log_file=world->log_file;
  no_printf("Viz output in DX mode...\n");

  word_p=(unsigned char *)&word;
  word=0x04030201;

  if (word_p[0]==1) {
    sprintf(my_byte_order,"lsb");
  }
  else {
    sprintf(my_byte_order,"msb");
  }

  viz_iterationll=fdlp->viz_iterationll;
  n_viz_iterations=fdlp->n_viz_iterations;
  /* first_viz_iteration=(viz_iteration==fdlp->iteration_list->value); */
  
 /* here is the check to prevent writing twice the same info 
    - at the end of one checkpoint and the beginning of the next checkpoint.
   */
  if((world->chkpt_flag) && (world->start_time > 0)){
     if (world->it_time == world->start_time){
         if(viz_iterationll % (world->chkpt_iterations) == 0){
             return 0;
         }
     }
  }

  viz_type=fdlp->type;
  viz_eff=((viz_type==ALL_FRAME_DATA) || (viz_type==EFF_POS)
               || (viz_type==EFF_STATES));
  viz_mol=((viz_type==ALL_FRAME_DATA) || (viz_type==MOL_POS)
               || (viz_type==MOL_STATES));
  viz_surf=((viz_type==ALL_FRAME_DATA) || (viz_type==SURF_POS)
               || (viz_type==SURF_STATES));
  viz_surf_or_eff=(viz_surf || viz_eff);


  viz_eff_pos=((viz_type==ALL_FRAME_DATA) || (viz_type==EFF_POS));
  viz_eff_states=((viz_type==ALL_FRAME_DATA) || (viz_type==EFF_STATES));
  viz_mol_pos=((viz_type==ALL_FRAME_DATA) || (viz_type==MOL_POS));
  viz_mol_states=((viz_type==ALL_FRAME_DATA) || (viz_type==MOL_STATES));
  viz_surf_pos=((viz_type==ALL_FRAME_DATA) || (viz_type==SURF_POS));
  viz_surf_states=((viz_type==ALL_FRAME_DATA) || (viz_type==SURF_STATES));


/* dump walls and effectors: */
  if (viz_surf_or_eff) {
  vizp = world->viz_obj_head;
  while(vizp!=NULL) {

    wall_verts_header=NULL;
    wall_states_header=NULL;
    eff_pos_header=NULL;
    eff_states_header=NULL;

    if (viz_surf_pos) {
      sprintf(file_name,"%s.mesh_elements.%lld.dx",
               vizp->name,viz_iterationll);
      if ((wall_verts_header=fopen(file_name,"wb"))==NULL) {
        fprintf(world->err_file,"File %s, Line %ld: error cannot open mesh elements file %s\n", __FILE__, (long)__LINE__, file_name);
        return(1);
      }
    }

    if (viz_surf_states) {
      sprintf(file_name,"%s.mesh_element_states.%lld.dx",
               vizp->name,viz_iterationll);
      if ((wall_states_header=fopen(file_name,"wb"))==NULL) {
        fprintf(world->err_file,"File %s, Line %ld: error cannot open mesh element states file %s\n", __FILE__, (long)__LINE__, file_name);
        return(1);
      }
    }

    if (viz_eff_pos) {
      sprintf(file_name,"%s.effector_site_positions.%lld.dx",vizp->name,viz_iterationll);
      if ((eff_pos_header=fopen(file_name,"wb"))==NULL) {
        fprintf(world->err_file,"File %s, Line %ld: error cannot open effector position file %s\n", __FILE__, (long)__LINE__, file_name);
        return(1);
      }
    }
  
    if (viz_eff_states) {
      sprintf(file_name,"%s.effector_site_states.%lld.dx",vizp->name,viz_iterationll);
      if ((eff_states_header=fopen(file_name,"wb"))==NULL) {
        fprintf(world->err_file,"File %s, Line %ld: error cannot open effector states file %s\n", __FILE__, (long)__LINE__, file_name);
        return(1);
      }
    }

    /* Traverse all visualized compartments */
    /* output mesh element positions and connections */
    /* output effector positions */
    /* output effector states */
    /* do not output effector normals header or normals data yet */
/*
    wall_verts_index = 1;
    wall_states_index = 1;
    wall_data_index = 1;
*/
/*
    poly_pos_count = 0;
*/
/*
    eff_pos_index = 1;
    eff_states_index = 1;
*/
    pos_count = 0;
    state_count = 0;
    effector_state_pos_count = 0;
    vcp = vizp->viz_child_head;
    while(vcp!=NULL) {
      objp = vcp->obj;
      pop=(struct polygon_object *)objp->contents;
      if (objp->object_type==BOX_OBJ) {
#if 0
        if (viz_surf_pos || viz_surf_states)
        {
          element_data_count=0.5*objp->n_walls_actual;
        
          if (viz_surf_pos && !viz_surf_states) {
            fprintf(wall_verts_header,
              "object \"%s.positions\" class array type float rank 1 shape 3 items %d %s binary data follows\n",
              objp->sym->name,objp->n_verts,my_byte_order);
            /* output box vertices */
            for (ii=0;ii<objp->n_verts;ii++) {
              v1 = world->length_unit*objp->verts[ii].x;
              v2 = world->length_unit*objp->verts[ii].y;
              v3 = world->length_unit*objp->verts[ii].z;
              fwrite(&v1,sizeof v1,1,wall_verts_header);
              fwrite(&v2,sizeof v2,1,wall_verts_header);
              fwrite(&v3,sizeof v3,1,wall_verts_header);
            }
            fprintf(wall_verts_header,
              "\nattribute \"dep\" string \"positions\"\n#\n");
/*
            wall_verts_index++;
*/
    
            /* output box wall connections */
            fprintf(wall_verts_header,
              "object \"%s.connections\" class array type int rank 1 shape 4 items %d %s binary data follows\n",
              objp->sym->name,element_data_count,my_byte_order);
            for (ii=0;ii<objp->n_walls;ii+=2) {
              if (pop->side_stat[ii]) {
                switch (ii) {
                  case TP:
                    vi1=3;
                    vi2=7;
                    vi3=1;
                    vi4=5;
                    fwrite(&vi1,sizeof vi1,1,wall_verts_header);
                    fwrite(&vi2,sizeof vi2,1,wall_verts_header);
                    fwrite(&vi3,sizeof vi3,1,wall_verts_header);
                    fwrite(&vi4,sizeof vi4,1,wall_verts_header);
                  break;
                  case BOT:
                    vi1=0;
                    vi2=4;
                    vi3=2;
                    vi4=6;
                    fwrite(&vi1,sizeof vi1,1,wall_verts_header);
                    fwrite(&vi2,sizeof vi2,1,wall_verts_header);
                    fwrite(&vi3,sizeof vi3,1,wall_verts_header);
                    fwrite(&vi4,sizeof vi4,1,wall_verts_header);
                  break;
                  case FRNT:
                    vi1=4;
                    vi2=0;
                    vi3=5;
                    vi4=1;
                    fwrite(&vi1,sizeof vi1,1,wall_verts_header);
                    fwrite(&vi2,sizeof vi2,1,wall_verts_header);
                    fwrite(&vi3,sizeof vi3,1,wall_verts_header);
                    fwrite(&vi4,sizeof vi4,1,wall_verts_header);
                  break;
                  case BCK:
                    vi1=2;
                    vi2=6;
                    vi3=3;
                    vi4=7;
                    fwrite(&vi1,sizeof vi1,1,wall_verts_header);
                    fwrite(&vi2,sizeof vi2,1,wall_verts_header);
                    fwrite(&vi3,sizeof vi3,1,wall_verts_header);
                    fwrite(&vi4,sizeof vi4,1,wall_verts_header);
                  break;
                  case LFT:
                    vi1=0;
                    vi2=2;
                    vi3=1;
                    vi4=3;
                    fwrite(&vi1,sizeof vi1,1,wall_verts_header);
                    fwrite(&vi2,sizeof vi2,1,wall_verts_header);
                    fwrite(&vi3,sizeof vi3,1,wall_verts_header);
                    fwrite(&vi4,sizeof vi4,1,wall_verts_header);
                  break;
                  case RT:
                    vi1=6;
                    vi2=4;
                    vi3=7;
                    vi4=5;
                    fwrite(&vi1,sizeof vi1,1,wall_verts_header);
                    fwrite(&vi2,sizeof vi2,1,wall_verts_header);
                    fwrite(&vi3,sizeof vi3,1,wall_verts_header);
                    fwrite(&vi4,sizeof vi4,1,wall_verts_header);
                  break;
                }
              }
            }
            fprintf(wall_verts_header,
              "\nattribute \"ref\" string \"positions\"\n");
            fprintf(wall_verts_header,
              "attribute \"element type\" string \"quads\"\n#\n");
/*
            wall_verts_index++;
*/
          }

          if (viz_surf_pos && viz_surf_states) {
            fprintf(wall_verts_header,
              "object \"%s.positions\" class array type float rank 1 shape 3 items %d %s binary data follows\n",
              objp->sym->name,objp->n_verts,my_byte_order);
            /* output box vertices */
            for (ii=0;ii<objp->n_verts;ii++) {
              v1 = world->length_unit*objp->verts[ii].x;
              v2 = world->length_unit*objp->verts[ii].y;
              v3 = world->length_unit*objp->verts[ii].z;
              fwrite(&v1,sizeof v1,1,wall_verts_header);
              fwrite(&v2,sizeof v2,1,wall_verts_header);
              fwrite(&v3,sizeof v3,1,wall_verts_header);
            }
            fprintf(wall_verts_header,
              "\nattribute \"dep\" string \"positions\"\n#\n");
/*
            wall_verts_index++;
*/
    
            /* output box wall connections */
            fprintf(wall_verts_header,
              "object \"%s.connections\" class array type int rank 1 shape 4 items %d %s binary data follows\n",
              objp->sym->name,element_data_count,my_byte_order);
            fprintf(wall_states_header,
              "object \"%s.states\" class array type int rank 0 items %d ascii data follows\n",
              objp->sym->name,element_data_count);
            for (ii=0;ii<objp->n_walls;ii+=2) {
              if (pop->side_stat[ii]) {
                switch (ii) {
                  case TP:
                    vi1=3;
                    vi2=7;
                    vi3=1;
                    vi4=5;
                    fwrite(&vi1,sizeof vi1,1,wall_verts_header);
                    fwrite(&vi2,sizeof vi2,1,wall_verts_header);
                    fwrite(&vi3,sizeof vi3,1,wall_verts_header);
                    fwrite(&vi4,sizeof vi4,1,wall_verts_header);
                  break;
                  case BOT:
                    vi1=0;
                    vi2=4;
                    vi3=2;
                    vi4=6;
                    fwrite(&vi1,sizeof vi1,1,wall_verts_header);
                    fwrite(&vi2,sizeof vi2,1,wall_verts_header);
                    fwrite(&vi3,sizeof vi3,1,wall_verts_header);
                    fwrite(&vi4,sizeof vi4,1,wall_verts_header);
                  break;
                  case FRNT:
                    vi1=4;
                    vi2=0;
                    vi3=5;
                    vi4=1;
                    fwrite(&vi1,sizeof vi1,1,wall_verts_header);
                    fwrite(&vi2,sizeof vi2,1,wall_verts_header);
                    fwrite(&vi3,sizeof vi3,1,wall_verts_header);
                    fwrite(&vi4,sizeof vi4,1,wall_verts_header);
                  break;
                  case BCK:
                    vi1=2;
                    vi2=6;
                    vi3=3;
                    vi4=7;
                    fwrite(&vi1,sizeof vi1,1,wall_verts_header);
                    fwrite(&vi2,sizeof vi2,1,wall_verts_header);
                    fwrite(&vi3,sizeof vi3,1,wall_verts_header);
                    fwrite(&vi4,sizeof vi4,1,wall_verts_header);
                  break;
                  case LFT:
                    vi1=0;
                    vi2=2;
                    vi3=1;
                    vi4=3;
                    fwrite(&vi1,sizeof vi1,1,wall_verts_header);
                    fwrite(&vi2,sizeof vi2,1,wall_verts_header);
                    fwrite(&vi3,sizeof vi3,1,wall_verts_header);
                    fwrite(&vi4,sizeof vi4,1,wall_verts_header);
                  break;
                  case RT:
                    vi1=6;
                    vi2=4;
                    vi3=7;
                    vi4=5;
                    fwrite(&vi1,sizeof vi1,1,wall_verts_header);
                    fwrite(&vi2,sizeof vi2,1,wall_verts_header);
                    fwrite(&vi3,sizeof vi3,1,wall_verts_header);
                    fwrite(&vi4,sizeof vi4,1,wall_verts_header);
                  break;
                }
                state=objp->viz_state[ii];
                fprintf(wall_states_header,"%d\n",state);
              }
            }
            fprintf(wall_verts_header,
              "\nattribute \"ref\" string \"positions\"\n");
            fprintf(wall_verts_header,
              "attribute \"element type\" string \"quads\"\n#\n");
/*
            wall_verts_index++;
*/
            fprintf(wall_states_header,"\nattribute \"dep\" string \"connections\"\n#\n");
/*
            wall_states_index++;
*/

          }

          if (viz_surf_states && !viz_surf_pos) {
            fprintf(wall_states_header,
              "object \"%s.states\" class array type int rank 0 items %d ascii data follows\n",
              objp->sym->name,element_data_count);
            for (ii=0;ii<objp->n_walls;ii+=2) {
              if (!get_bit(pop->side_removed,ii)) {
                state=objp->viz_state[ii];
                fprintf(wall_states_header,"%d\n",state);
              }
            }
          
          }
          fprintf(wall_states_header,"\nattribute \"dep\" string \"connections\"\n#\n");
/*
          wall_states_index++;
*/
        }
#endif
      }

      if (objp->object_type==POLY_OBJ || objp->object_type==BOX_OBJ) {
        if (viz_surf && (viz_surf_pos || viz_surf_states))
        {
          opp=(struct ordered_poly *)pop->polygon_data;
          edp=opp->element;
          element_data_count=objp->n_walls_actual;

          if (viz_surf_pos && !viz_surf_states) {

	    fprintf(wall_verts_header,
              "object \"%s.positions\" class array type float rank 1 shape 3 items %d %s binary data follows\n",
              objp->sym->name,objp->n_verts,my_byte_order);
            /* output polyhedron vertices */
            for (ii=0;ii<objp->n_verts;ii++) {
              v1 = world->length_unit*objp->verts[ii].x;
              v2 = world->length_unit*objp->verts[ii].y;
              v3 = world->length_unit*objp->verts[ii].z;
	      fwrite(&v1,sizeof v1,1,wall_verts_header);
	      fwrite(&v2,sizeof v2,1,wall_verts_header);
	      fwrite(&v3,sizeof v3,1,wall_verts_header);
            }
            fprintf(wall_verts_header,
              "\nattribute \"dep\" string \"positions\"\n#\n");
/*
	    wall_verts_index++;
*/

            /* output polygon element connections */
	    fprintf(wall_verts_header,
              "object \"%s.connections\" class array type int rank 1 shape 3 items %d %s binary data follows\n",
              objp->sym->name,element_data_count,my_byte_order);
            for (ii=0;ii<objp->n_walls;ii++) {
              if (!get_bit(pop->side_removed,ii)) {
                vi1=edp[ii].vertex_index[0];
                vi2=edp[ii].vertex_index[1];
                vi3=edp[ii].vertex_index[2];
	        fwrite(&vi1,sizeof vi1,1,wall_verts_header);
	        fwrite(&vi2,sizeof vi2,1,wall_verts_header);
	        fwrite(&vi3,sizeof vi3,1,wall_verts_header);
              }
            }
            fprintf(wall_verts_header,
              "\nattribute \"ref\" string \"positions\"\n");
            fprintf(wall_verts_header,
              "attribute \"element type\" string \"triangles\"\n#\n");
/*
	    wall_verts_index++;
*/


          }
    
          if (viz_surf_pos && viz_surf_states) {
	    fprintf(wall_verts_header,
              "object \"%s.positions\" class array type float rank 1 shape 3 items %d %s binary data follows\n",
              objp->sym->name,objp->n_verts,my_byte_order);
            /* output polyhedron vertices */
            for (ii=0;ii<objp->n_verts;ii++) {
              v1 = world->length_unit*objp->verts[ii].x;
              v2 = world->length_unit*objp->verts[ii].y;
              v3 = world->length_unit*objp->verts[ii].z;
	      fwrite(&v1,sizeof v1,1,wall_verts_header);
	      fwrite(&v2,sizeof v2,1,wall_verts_header);
	      fwrite(&v3,sizeof v3,1,wall_verts_header);
            }
            fprintf(wall_verts_header,
              "\nattribute \"dep\" string \"positions\"\n#\n");
/*
	    wall_verts_index++;
*/

            /* output polygon element connections */
	    fprintf(wall_verts_header,
              "object \"%s.connections\" class array type int rank 1 shape 3 items %d %s binary data follows\n",
              objp->sym->name,element_data_count,my_byte_order);
            fprintf(wall_states_header,
              "object \"%s.states\" class array type int rank 0 items %d ascii data follows\n",
              objp->sym->name,element_data_count);
            for (ii=0;ii<objp->n_walls;ii++) {
              if (!get_bit(pop->side_removed,ii)) {
                vi1=edp[ii].vertex_index[0];
                vi2=edp[ii].vertex_index[1];
                vi3=edp[ii].vertex_index[2];
	        fwrite(&vi1,sizeof vi1,1,wall_verts_header);
	        fwrite(&vi2,sizeof vi2,1,wall_verts_header);
	        fwrite(&vi3,sizeof vi3,1,wall_verts_header);
		state=objp->viz_state[ii];
		fprintf(wall_states_header,"%d\n",state);
              }
            }
            fprintf(wall_verts_header,
              "\nattribute \"ref\" string \"positions\"\n");
            fprintf(wall_verts_header,
              "attribute \"element type\" string \"triangles\"\n#\n");
/*
	    wall_verts_index++;
*/
            fprintf(wall_states_header,"\nattribute \"dep\" string \"connections\"\n#\n");
/*
            wall_states_index++;
*/
          }

          if (viz_surf_states && !viz_surf_pos) {
          fprintf(wall_states_header,
            "object \"%s.states\" class array type int rank 0 items %d ascii data follows\n",
            objp->sym->name,element_data_count);
          for (ii=0;ii<objp->n_walls;ii++) {
            if (!get_bit(pop->side_removed,ii)) {
              state=objp->viz_state[ii];
              fprintf(wall_states_header,"%d\n",state);
            }
          }
          fprintf(wall_states_header,"\nattribute \"dep\" string \"connections\"\n#\n");
/*
          wall_states_index++;
*/
          }
        }
      }

      wp = objp->wall_p;
      no_printf("Traversing walls in object %s\n",objp->sym->name);
      if (viz_eff) {
        n_eff=0;
        for (ii=0;ii<objp->n_walls;ii++) {
          w = wp[ii];
          if (w!=NULL) {
	    sg = w->effectors;
            if (sg!=NULL) {
              for (index=0;index<sg->n_tiles;index++) {
	        gmol=sg->mol[index];
	        if (gmol!=NULL) {
	          state=sg->mol[index]->properties->viz_state;
	        }
	        else {
	          state=EXCLUDE_OBJ;
	        }
                if (state!=EXCLUDE_OBJ) {
                  n_eff++;
                }
              }
            }
          }
        }
        no_printf("Dumping %d effectors...\n",n_eff);
        fflush(log_file);
        if (viz_eff_pos) {
          if (n_eff) {
            fprintf(eff_pos_header,
              "object \"%s.pos_and_norm\" class array type float rank 2 shape 2 3 items %d %s binary data follows\n",
              objp->sym->name,n_eff,my_byte_order);
/*
            eff_pos_index++;
*/
          }
          else {
            fprintf(eff_pos_header,
              "object \"%s.pos_and_norm\" array",objp->sym->name);
/*
            eff_pos_index++;
*/
          }
        }

        if (viz_eff_states) {
          if (n_eff) {
            fprintf(eff_states_header,
              "object \"%s.states\" class array type int rank 0 items %d ascii data follows\n",
              objp->sym->name,n_eff);
/*
            eff_states_index++;
*/
          }
          else {
            fprintf(eff_states_header,
              "object \"%s.states\" array\n",objp->sym->name);
/*
            eff_states_index++;
*/
          }
        }
      }
      wp = objp->wall_p;
      for (ii=0;ii<objp->n_walls;ii++) {
        w = wp[ii];
        if (w!=NULL) {
	  sg = w->effectors;

          /* dump the effectors */
          if (viz_eff) {
            if (sg!=NULL) {
              n_tiles=sg->n_tiles;
              for (index=0;index<n_tiles;index++) {
                grid2xyz(sg,index,&p0);
	        gmol=sg->mol[index];
	        if (gmol!=NULL) {
	          state=sg->mol[index]->properties->viz_state;
	        }
	        else {
	          state=EXCLUDE_OBJ;
	        }
      
                if (state!=EXCLUDE_OBJ) {
                  if (viz_eff_states) {
                   fprintf(eff_states_header,"%d\n",state);
                  }
                  if (viz_eff_pos) {
	           v1=world->length_unit*p0.x;
	           v2=world->length_unit*p0.y;
	           v3=world->length_unit*p0.z;
	           fwrite(&v1,sizeof v1,1,eff_pos_header);
	           fwrite(&v2,sizeof v2,1,eff_pos_header);
	           fwrite(&v3,sizeof v3,1,eff_pos_header);
                   v1=(float)((w->normal.x)*(gmol->orient));
                   v2=(float)((w->normal.y)*(gmol->orient));
                   v3=(float)((w->normal.z)*(gmol->orient));
                   fwrite(&v1,sizeof v1,1,eff_pos_header);
                   fwrite(&v2,sizeof v2,1,eff_pos_header);
                   fwrite(&v3,sizeof v3,1,eff_pos_header);
                  }
                }
              }
            }
          }
        }
      }
      if (viz_eff_pos) {
        fprintf(eff_pos_header,
          "\n#\n");
      }
      if (viz_eff_states) {
        fprintf(eff_states_header,
          "attribute \"dep\" string \"positions\"\n#\n");
      }
      vcp = vcp->next;
    }

    /* output effector states null object */
    if (viz_eff) {
      if (viz_eff_states) {
        fprintf(eff_states_header,"object \"null_object\" array\n");
        fprintf(eff_states_header,
          "  attribute \"dep\" string \"positions\"\n\n");
      }
    }

    /* output surface states null object */
    if (viz_surf_states) {
      if (wall_states_header!=NULL) {
        fprintf(wall_states_header,"object \"null_object\" array\n");
        fprintf(wall_states_header,
          "  attribute \"dep\" string \"connections\"\n\n");
      }
    }

    /* output effector and surface positions field objects */
    vcp = vizp->viz_child_head;
    while(vcp!=NULL) {
      objp = vcp->obj;

      /* effectors */
      if (viz_eff) {
        /* effector positions */
        if (viz_eff_pos) {
          fprintf(eff_pos_header,"object \"%s.field\" field\n",objp->sym->name);
/*
          fprintf(eff_pos_header,
            "  component \"positions\" \"%s.positions\"\n",objp->sym->name);
*/
          fprintf(eff_pos_header,
            "  component \"data\" \"%s.pos_and_norm\"\n\n",objp->sym->name);
        }
      }

      /* surfaces */
      if (viz_surf) {
        /* surface positions */
        if (viz_surf_pos) {
          if (wall_verts_header!=NULL) {
            fprintf(wall_verts_header,
              "object \"%s.field\" field\n",objp->sym->name);
            fprintf(wall_verts_header,
              "  component \"positions\" \"%s.positions\"\n",objp->sym->name);
            fprintf(wall_verts_header,
              "  component \"connections\" \"%s.connections\"\n\n",
              objp->sym->name);
          }
        }
      }
      vcp = vcp->next;
    }

    /* output effector positions null objects */
    if (viz_eff) {
      if (viz_eff_pos) {
        fprintf(eff_pos_header,"object \"null_pos_and_norm\" array\n\n");
/*
        fprintf(eff_pos_header,"object \"null_data\" array\n\n");
*/
        fprintf(eff_pos_header,"object \"null_object\" field\n");
/*
        fprintf(eff_pos_header,"  component \"positions\" \"null_positions\"\n");
*/
        fprintf(eff_pos_header,"  component \"data\" \"null_pos_and_norm\"\n\n");
      }
    }

    /* output surface positions null objects */
    if (viz_surf_pos) {
      if (wall_verts_header!=NULL) {
        fprintf(wall_verts_header,"object \"null_positions\" array\n\n");
        fprintf(wall_verts_header,"object \"null_connections\" array\n\n");
        fprintf(wall_verts_header,"object \"null_object\" field\n");
        fprintf(wall_verts_header,"  component \"positions\" \"null_positions\"\n");
        fprintf(wall_verts_header,"  component \"connections\" \"null_connections\"\n\n");
      }
    }

    /* output effector group objects and null members*/
    /* group object name is full MCell parent object name */
    if (viz_eff) {
      /* effector positions */
      if (viz_eff_pos) {
        fprintf(eff_pos_header,"object \"%s\" group\n",vizp->full_name);
        fprintf(eff_pos_header,
          "  member \"null_object (default)\" \"null_object\"\n");
      }
      /* effector states */
      if (viz_eff_states) {
        fprintf(eff_states_header,"object \"%s\" group\n",vizp->full_name);
        fprintf(eff_states_header,
          "  member \"null_object (default)\" \"null_object\"\n");
      }
    }

    /* output surface group objects and null members*/
    /* group object name is full MCell parent object name */
    if (viz_surf) {
      /* surface positions */
      if (viz_surf_pos) {
        if (wall_verts_header!=NULL) {
          fprintf(wall_verts_header,"object \"%s\" group\n",vizp->full_name);
          fprintf(wall_verts_header,
            "  member \"null_object (default)\" \"null_object\"\n");
        }
      }
      /* surface states */
      if (viz_surf_states) {
        if (wall_states_header!=NULL) {
          fprintf(wall_states_header,"object \"%s\" group\n",vizp->full_name);
          fprintf(wall_states_header,
            "  member \"null_object (default)\" \"null_object\"\n");
        }
      }
    }

    /* output group object members */
    /* member name is full MCell child object name */
    vcp = vizp->viz_child_head;
    while(vcp!=NULL) {
      objp = vcp->obj;

      /* effectors */
      if (viz_eff) {
        /* effector positions */
        if (viz_eff_pos) {
          fprintf(eff_pos_header,
            "  member \"%s\" \"%s.field\"\n",objp->sym->name,objp->sym->name);
        }
        /* effector states */
        if (viz_eff_states) {
          fprintf(eff_states_header,
            "  member \"%s.states\" \"%s.states\"\n",objp->sym->name,objp->sym->name);
        }
      }

      /* surfaces */
      if (viz_surf) {
        /* surface positions */
        if (viz_surf_pos) {
          if (wall_verts_header!=NULL) {
            fprintf(wall_verts_header,
              "  member \"%s\" \"%s.field\"\n",objp->sym->name,objp->sym->name);
          }
        }
        /* surface states */
        if (viz_surf_states) {
          if (wall_states_header!=NULL) {
            fprintf(wall_states_header,
              "  member \"%s.states\" \"%s.states\"\n",objp->sym->name,objp->sym->name);
          }
        }
      }
      vcp = vcp->next;
    }

    if (wall_verts_header!=NULL) {
      fclose(wall_verts_header);
    }
    if (wall_states_header!=NULL) {
      fclose(wall_states_header);
    }
    if (eff_pos_header!=NULL) {
      fclose(eff_pos_header);
    }
    if (eff_states_header!=NULL) {
      fclose(eff_states_header);
    }
    vizp = vizp->next;
  }

  } /* end viz_surf_or_eff */

/* dump diffusible molecules: */
  if (viz_mol) {
    mol_pos_header = NULL;
    mol_states_header = NULL;
    mol_pos_index=1;
    mol_states_index=1;
    pos_count=0;
    state_count=0;
    if (viz_mol_pos) {
      sprintf(file_name,"%s.molecule_positions.%lld.dx",world->molecule_prefix_name,viz_iterationll);
      if ((mol_pos_header=fopen(file_name,"wb"))==NULL) {
        fprintf(world->err_file,"File %s, Line %ld: error cannot open molecule positions header file %s\n", __FILE__, (long)__LINE__, file_name);
        return(1);
      }
    }
    if (viz_mol_states) {
      sprintf(file_name,"%s.molecule_states.%lld.dx",world->molecule_prefix_name,viz_iterationll);
      if ((mol_states_header=fopen(file_name,"wb"))==NULL) {
        fprintf(world->err_file,"File %s, Line %ld: error cannot open molecule states header file %s\n", __FILE__, (long)__LINE__, file_name);
        return(1);
      }
    }

    species_list=world->species_list;
    n_species=world->n_species;

    if ((viz_molp=(struct volume_molecule ***)malloc(n_species*sizeof(struct volume_molecule **)))==NULL) {
        fprintf(world->err_file,"File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
      return(1);
    }
    if ((viz_mol_count=(u_int *)malloc(n_species*sizeof(u_int)))==NULL) {
        fprintf(world->err_file,"File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
      return(1);
    }

    for (ii=0;ii<n_species;ii++) {

      spec_id=species_list[ii]->species_id;
      viz_molp[spec_id]=NULL;
      viz_mol_count[spec_id]=0;

      if (species_list[ii]->viz_state!=EXCLUDE_OBJ) {
        num=species_list[ii]->population;
        if (num>0) {
          if ((viz_molp[spec_id]=(struct volume_molecule **)malloc
            (num*sizeof(struct volume_molecule *)))==NULL) {
                fprintf(world->err_file,"File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                return(1);
          }
        }
      }
    }

    slp=world->storage_head;
    while (slp!=NULL) {
      sp=slp->store;
      shp=sp->timer;
      while (shp!=NULL) {

        for (ii=0;ii<shp->buf_len;ii++) {
          amp=(struct abstract_molecule *)shp->circ_buf_head[ii];
          while (amp!=NULL) {
            if ((amp->properties!=NULL) && (amp->flags&TYPE_3D)==TYPE_3D) {
              molp=(struct volume_molecule *)amp;
              if (molp->properties->viz_state!=EXCLUDE_OBJ) {
                spec_id=molp->properties->species_id;
                if (viz_mol_count[spec_id]<molp->properties->population) {
                  viz_molp[spec_id][viz_mol_count[spec_id]++]=molp;
                }
                else {
                  fprintf(log_file,"MCell: molecule count disagreement!!\n");
                  fprintf(log_file,"  Species %s  population = %d  count = %d\n",molp->properties->sym->name,molp->properties->population,viz_mol_count[spec_id]);
                }
              }
            }
            amp=amp->next;
          }
        }

        amp=(struct abstract_molecule *)shp->current;
        while (amp!=NULL) {
          if ((amp->properties!=NULL) && (amp->flags&TYPE_3D)==TYPE_3D) {
            molp=(struct volume_molecule *)amp;
            if (molp->properties->viz_state!=EXCLUDE_OBJ) {
              spec_id=molp->properties->species_id;
              if (viz_mol_count[spec_id]<molp->properties->population) {
                viz_molp[spec_id][viz_mol_count[spec_id]++]=molp;
              }
              else {
                fprintf(log_file,"MCell: molecule count disagreement!!\n");
                fprintf(log_file,"  Species %s  population = %d  count = %d\n",molp->properties->sym->name,molp->properties->population,viz_mol_count[spec_id]);
              }
            }
          }
          amp=amp->next;
        }
        
        shp=shp->next_scale;
      }

      slp=slp->next;
    }

    for (ii=0;ii<n_species;ii++) {
      spec_id=species_list[ii]->species_id;
      num=viz_mol_count[spec_id];
      state=species_list[ii]->viz_state;
      if (state!=EXCLUDE_OBJ
          && num!=species_list[ii]->population
          && ((species_list[ii]->flags & NOT_FREE)==0)) {
        fprintf(log_file,"MCell: molecule count disagreement!!\n");
        fprintf(log_file,"  Species %s  population = %d  count = %d\n",species_list[ii]->sym->name,species_list[ii]->population,num);
      }

      if (num>0 && ((species_list[ii]->flags & NOT_FREE)==0)) {
        if (viz_mol_pos) {
          fprintf(mol_pos_header,"object \"%d\" class array type float rank 1 shape 3 items %d %s binary data follows\n",mol_pos_index,num,my_byte_order);
          for (jj=0;jj<num;jj++) {
            molp=viz_molp[spec_id][jj];
	    v1=world->length_unit*molp->pos.x;
	    v2=world->length_unit*molp->pos.y;
	    v3=world->length_unit*molp->pos.z;
	    fwrite(&v1,sizeof v1,1,mol_pos_header);
	    fwrite(&v2,sizeof v2,1,mol_pos_header);
	    fwrite(&v3,sizeof v3,1,mol_pos_header);
          }
          fprintf(mol_pos_header,"\n#\n");
          mol_pos_index++;
        }
        if (viz_mol_states) {
          fprintf(mol_states_header,"object \"%d\"\n  constantarray type int items %d\n",mol_states_index,num);
          fprintf(mol_states_header,"  data follows %d\n",state);
          fprintf(mol_states_header,"  attribute \"dep\" string \"positions\"\n\n");
          mol_states_index++;
        }
      }
/* output empty arrays for zero molecule counts here */
      else if (num==0 && ((species_list[ii]->flags & NOT_FREE)==0)) {
        if (viz_mol_pos) {
          fprintf(mol_pos_header,"object \"%d\" array\n\n",mol_pos_index);
          mol_pos_index++;
        }
        if (viz_mol_states) {
          fprintf(mol_states_header,"object \"%d\" array\n",mol_states_index);
          fprintf(mol_states_header,"  attribute \"dep\" string \"positions\"\n\n");
          mol_states_index++;
        }
      }
    }

/* build fields and groups here */
    if (viz_mol_pos) {
      mol_pos_field_index=0;
      for (ii=0;ii<n_species;ii++) {
        if ((species_list[ii]->flags & NOT_FREE)==0) {
          fprintf(mol_pos_header,
            "object \"%d\" field\n",mol_pos_index+mol_pos_field_index);
          fprintf(mol_pos_header,
            "  component \"positions\" \"%d\"\n\n",1+mol_pos_field_index);
          mol_pos_field_index++;
        }
      }
      fprintf(mol_pos_header,"object \"null_positions\" array\n\n");
      fprintf(mol_pos_header,"object \"null_object\" field\n");
      fprintf(mol_pos_header,"  component \"positions\" \"null_positions\"\n\n");
      fprintf(mol_pos_header,"object \"%d\" group\n",
        2*mol_pos_index-1);
      fprintf(mol_pos_header,"  member \"null_object (default)\" \"null_object\"\n");
      mol_pos_group_index=0;
      for (ii=0;ii<n_species;ii++) {
        if ((species_list[ii]->flags & NOT_FREE)==0) {
          fprintf(mol_pos_header,"  member \"%s\" \"%d\"\n",species_list[ii]->sym->name,mol_pos_index+mol_pos_group_index);
          mol_pos_group_index++;
        }
      }
      fclose(mol_pos_header);
    }
    if (viz_mol_states) {
      fprintf(mol_states_header,"object \"null_object\" array\n");
      fprintf(mol_states_header,"  attribute \"dep\" string \"positions\"\n\n");
      fprintf(mol_states_header,"object \"%d\" group\n",mol_states_index);
      fprintf(mol_states_header,"  member \"null_object (default)\" \"null_object\"\n");
      mol_states_group_index=0;
      for (ii=0;ii<n_species;ii++) {
        if ((species_list[ii]->flags & NOT_FREE)==0) {
          fprintf(mol_states_header,"  member \"%s\" \"%d\"\n",species_list[ii]->sym->name,1+mol_states_group_index);
          mol_states_group_index++;
        }
      }
      fclose(mol_states_header);
    }

  }

  if (viz_molp != NULL) {
    for (ii=0;ii<n_species;ii++) {
      if (viz_molp[ii]!=NULL) {
        free(viz_molp[ii]);
        viz_molp[ii] = NULL;
      }
    }
    
    free(viz_molp);
    viz_molp = NULL;
  }
  if (viz_mol_count != NULL){
    free (viz_mol_count);
    viz_mol_count = NULL;
  }
  return(0);
}

/*************************************************************************
output_dreamm_objects:
	In: struct frame_data_list *fdlp
	Out: 0 on success, 1 on error; output visualization files (*.dx)
             in dreamm group format are written.
**************************************************************************/

int output_dreamm_objects(struct frame_data_list *fdlp)
{
  FILE *log_file = NULL;
  FILE *master_header = NULL;
  FILE *meshes_header = NULL; /* header file for meshes */
  FILE *mesh_pos_data = NULL;  /* data file for wall vertices */
  FILE *mesh_states_data = NULL; /* data file for wall states */
  FILE *region_data = NULL; /* data file for region's data */
  FILE *vol_mol_header = NULL; /* header file for volume_molecules */
  FILE *vol_mol_pos_data = NULL; /* data file for volume_molecule positions */
  FILE *vol_mol_states_data = NULL; /* data file for volume_molecule states */
  FILE *vol_mol_orient_data = NULL; /* data file for volume_molecule orientations */
  FILE *surf_mol_header = NULL; /* header file for volume_molecules */
  FILE *surf_mol_pos_data = NULL; /* data file for volume_molecule positions */
  FILE *surf_mol_states_data = NULL; /* data file for volume_molecule states */
  FILE *surf_mol_orient_data = NULL; /* data file for volume_molecule orientations */
  FILE *iteration_numbers_data = NULL; /* data file for iteration numbers */
  FILE *time_values_data = NULL; /* data file for time_values */
  struct viz_obj *vizp = NULL;
  struct viz_child *vcp = NULL;
  struct surface_grid *sg = NULL;
  struct wall *w = NULL, **wp = NULL;
  struct species **species_list = NULL;
  struct species *specp = NULL;
  struct object *objp = NULL;
  struct polygon_object *pop = NULL;
  struct ordered_poly *opp = NULL;
  struct element_data *edp = NULL;
  struct vector3 p0;
  struct storage_list *slp = NULL;
  struct storage *sp = NULL;
  struct schedule_helper *shp = NULL;
  struct abstract_molecule *amp = NULL;
  struct grid_molecule *gmol = NULL;
  struct volume_molecule *molp = NULL, ***viz_molp = NULL;       /* for 3D molecules */
  struct region *rp = NULL;
  struct region_list *rlp = NULL;
  float v1,v2,v3;
  u_int spec_id = 0, *viz_mol_count = NULL, *viz_grid_mol_count = NULL;
  int word;
  byte *word_p = NULL;
  byte viz_mol_pos_flag = 0, viz_mol_states_flag = 0;	/* flags */
  byte viz_mol_orient_flag = 0, viz_region_data_flag = 0;   /* flags */
  byte viz_mol_all_data_flag = 0, viz_surf_all_data_flag = 0;	/* flags */
  byte viz_surf_pos_flag = 0, viz_surf_states_flag = 0;	/* flags */
  char *filename = NULL;
  static char *viz_data_dir_name = NULL;
  static char *frame_data_dir_name = NULL; 
  char *master_header_name = NULL;
  char iteration_number[1024];
  char chkpt_seq_num[1024];
  char *iteration_number_dir = NULL;
  char *mesh_pos_file_path = NULL; /* path to meshes vertices data file */
  static char *mesh_pos_name = NULL; /* meshes vertices data file name */
  char *mesh_states_file_path = NULL; /* meshes states data file path */
  static char *mesh_states_name = NULL; /* meshes states data file name */
  char *region_viz_data_file_path = NULL; /* path to region_viz_data file */
  static char *region_viz_data_name = NULL; /* region_viz_data file name */
  char *meshes_header_file_path = NULL; /* meshes header name */
  static char *meshes_header_name = NULL; /* meshes header name */
  char *vol_mol_pos_file_path = NULL; /* path to volume molecule positions data file */
  static char *vol_mol_pos_name = NULL; /* volume molecule positions data file name */
  char *vol_mol_states_file_path = NULL; /* path to volume molecule states data file  */
  static char *vol_mol_states_name = NULL; /* volume molecule states data file name */
  char *vol_mol_orient_file_path = NULL; /* path to volume molecule orientations data file  */
  static char *vol_mol_orient_name = NULL; /* volume molecule orientations data file name */
  char *vol_mol_header_file_path = NULL; /* path to volume molecules header  */
  static char *vol_mol_header_name = NULL; /* volume molecules header name */
  char *surf_mol_pos_file_path = NULL; /* path to surface molecule positions data file */
  static char *surf_mol_pos_name = NULL; /* surface molecule positions data file name */
  char *surf_mol_states_file_path = NULL; /* surface molecule states data file */
  static char *surf_mol_states_name = NULL; /* surface molecule states data file name */
  char *surf_mol_orient_file_path = NULL; /* path to surface molecule orientations data file  */
  static char *surf_mol_orient_name = NULL; /* surface molecule orientations data file name */
  char *surf_mol_header_file_path = NULL; /* path to surface molecules header */
  static char *surf_mol_header_name = NULL; /* surface molecules header name */
  char *grid_mol_name = NULL; /* points to the name of the grid molecule */
  char path_name_1[1024]; /* used for creating file links */
  char path_name_2[1024]; /* used for creating file links */
  char buf[1024];
  char *ch_ptr = NULL; /* pointer used to extract data file name */
  char my_byte_order[8];  /* shows binary ordering ('lsb' or 'msb') */
  u_int viz_iteration;
  u_int n_viz_iterations;
  int element_data_count;
  int state;
  int viz_type;
  unsigned int index; 
  static u_int meshes_main_index = 1;
  static u_int vol_mol_main_index = 1;
  static u_int surf_mol_main_index = 1;
  /* indices used in arrays "u_int *surf_pos", etc. */
  int surf_pos_index = 0; 
  int surf_con_index = 0;
  int surf_states_index = 0;
  int surf_region_values_index = 0;
  int surf_obj_region_values_index = 0;
  int eff_states_index = 0;
  int eff_pos_index = 0;
  int eff_orient_index = 0;
  int mol_states_index = 0;
  int mol_pos_index = 0;
  int mol_orient_index = 0;
  
  int status, ii, jj;
  int vi1,vi2,vi3;
  u_int num = 0;
  int time_to_write_master_header = 0; /* flag */
  /* flag pointing to another frame with certain condition 
     used with 'time_to_write_master_header' flag*/
  int found = 0;
  /* used when checking for the existence of directory */
  struct stat f_stat;
  int errno;  /* error code of the function */


  char *dir_name_end = "_viz_data";
  char *iteration_numbers_name = NULL; /* iteration numbers data file name */ 
  char *time_values_name = NULL; /* time values data file name */

  /* points to the values of the current iteration steps
     for certain frame types. */
  static long long curr_surf_pos_iteration_step = -1;
  static long long curr_region_data_iteration_step = -1;
  static long long curr_mol_pos_iteration_step = -1;
  static long long curr_mol_orient_iteration_step = -1;
  
  /* points to the special case in iteration steps
     when both values for GEOMETRY and REGION_VIZ_VALUES
     are equal.  E.g. for the cases when GEOMETRY = [0,200], 
     REGION_VIZ_VALUES = [0,100, 200,300] 
     special_surf_iteration_step = [0,200].Same for (MOL_POS,MOL_ORIENT). 
  */
   
  static long long special_surf_iteration_step = -1;
   
  static long long special_mol_iteration_step = -1;

  /* linked lists that stores data for the 'iteration_numbers' object */
  static u_int *iteration_numbers_meshes;
  static u_int *iteration_numbers_vol_mols;
  static u_int *iteration_numbers_surf_mols;
  u_int elem; /* element of the above arrays */


  /* counts number of times header files were opened.*/
  static int count_meshes_header = 0;
  static int count_vol_mol_header = 0;
  static int count_surf_mol_header = 0;

  static u_int iteration_numbers_meshes_count = 0; /* count elements in 
                                           iteration_numbers_meshes array  */
  static u_int iteration_numbers_vol_mols_count = 0; /* count elements in 
                                           iteration_numbers_vol_mols array  */
  static u_int iteration_numbers_surf_mols_count = 0; /* count elements in 
                                           iteration_numbers_surf_mols array  */
  static u_int time_values_count = 0; /* count elements in 
                                           time_values array  */
  int iteration_numbers_count; /* count elements in the iteration_numbers
                                          array */

  static int iteration_numbers_byte_offset = 0; /* defines position of the 
                 iteration numbers data in the iteration_numbers binary file */
  static int time_values_byte_offset = 0; /* defines position of the time 
                          values data in the time_values binary file */
  int mesh_pos_byte_offset = 0;  /* defines position of the object data
                                  in the mesh positions binary data file */
  int mesh_states_byte_offset = 0; /* defines position of the object data
                                    in the mesh states binary file */
  int region_data_byte_offset = 0; /* defines position of the 
                            object data in the region_data binary file */
  int region_data_byte_offset_prev = 0; /* defines position of the 
                            object data in the region_data binary file */
  int vol_mol_pos_byte_offset = 0; /*defines position of the object data 
                     in the volume_molecules positions binary file */
  int vol_mol_orient_byte_offset = 0; /*defines position of the object 
            data in the volume molecules orientations binary file */
  int vol_mol_states_byte_offset = 0; /* defines position of the object 
               data in the volume molecules states binary file. */
  int vol_mol_pos_byte_offset_prev = 0; /* used when defining position  
               in the volume molecules positions binary file */
  int vol_mol_orient_byte_offset_prev = 0; /* used when defining position                      in the volume molecules orientations binary file */
  int vol_mol_states_byte_offset_prev = 0; /* used when defining position                      in the volume molecules states binary file. */

  int surf_mol_pos_byte_offset = 0; /*defines position of the object data                    in the surface_molecules positions binary file */
  int surf_mol_orient_byte_offset = 0; /*defines position of the object 
            data in the surface molecules orientations binary file */
  int surf_mol_states_byte_offset = 0; /* defines position of the object 
               data in the surface molecules states binary file. */
  int surf_mol_pos_byte_offset_prev = 0; /* used when defining position  
               in the surface molecules positions binary file */
  int surf_mol_orient_byte_offset_prev = 0; /* used when defining position                      in the surface molecules orientations binary file */
  int surf_mol_states_byte_offset_prev = 0; /* used when defining position                      in the surface molecules states binary file. */
  

  /* flag that signals the creation of the main data directory.*/
  static int viz_data_dir_created = 0;
  /* depth of the user specified viz_output directory structure */
  int viz_dir_depth = 0;
  /* keeps track of the last value written into 'time_values' array */
  static u_int last_time_value_written = 0;
  
  /* counts number of this function executions
     for special_surf_iteration_step/special_mol_iteration_step cases. */
  
  static int special_surf_frames_counter = 0;
  static int special_mol_frames_counter = 0;

  /* flags that signal when meshes/vol_mols/surf_mols data is written */
  int show_meshes = 0;
  int show_molecules = 0;
  int show_effectors = 0;

  struct frame_data_list * fdl_ptr; 
  u_int n_species = world->n_species;
  species_list=world->species_list;

  log_file=world->log_file;
  no_printf("Viz output in DREAMM_V3 mode...\n");



  if(world->file_prefix_name == NULL) {
   	fprintf(world->err_file, "File %s, Line %ld: Inside VIZ_OUTPUT block the required keyword FILENAME is missing.\n", __FILE__, (long)__LINE__);
   	exit(1);
  }

  word_p=(unsigned char *)&word;
  word=0x04030201;

  if (word_p[0]==1) {
    sprintf(my_byte_order,"lsb");
  }
  else {
    sprintf(my_byte_order,"msb");
  }

 
  /*initialize  arrays. */
  
  if(time_values == NULL){
  
     time_values = (u_int *)malloc(sizeof(u_int) * time_values_total);
     if(time_values == NULL){
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
      }
      
      for(ii = 0; ii < time_values_total; ii++){
        time_values[ii] = UINT_MAX;
      }
   }

  if(iteration_numbers_meshes == NULL){
  
     iteration_numbers_meshes = (u_int *)malloc(sizeof(u_int) * time_values_total);
     if(iteration_numbers_meshes == NULL){
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
      }
      
      for(ii = 0; ii < time_values_total; ii++){
        iteration_numbers_meshes[ii] = UINT_MAX;
      }
     
   }

  if(iteration_numbers_vol_mols == NULL){
  
     iteration_numbers_vol_mols = (u_int *)malloc(sizeof(u_int) * time_values_total);
     if(iteration_numbers_vol_mols == NULL){
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
      }
      
      for(ii = 0; ii < time_values_total; ii++){
        iteration_numbers_vol_mols[ii] = UINT_MAX;
      }
     
   }
  
   if(iteration_numbers_surf_mols == NULL){
  
     iteration_numbers_surf_mols = (u_int *)malloc(sizeof(u_int) * time_values_total);
     if(iteration_numbers_surf_mols == NULL){
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
      }
      
      for(ii = 0; ii < time_values_total; ii++){
        iteration_numbers_surf_mols[ii] = UINT_MAX;
      }
     
   }

  viz_iteration = (u_int)(fdlp->viz_iterationll);
  n_viz_iterations = (u_int)(fdlp->n_viz_iterations);

  viz_type=fdlp->type;

  /* here is the check to prevent writing twice the same info 
    - at the end of one checkpoint and the beginning of the next checkpoint.
   */
  if((world->chkpt_flag) && (world->start_time > 0)){
     if (world->it_time == world->start_time){
         if(viz_iteration % (world->chkpt_iterations) == 0){
             return 0;
         }
     }
  }


  /* initialize flags */
  viz_mol_pos_flag = (viz_type==MOL_POS);
  viz_mol_orient_flag = (viz_type==MOL_ORIENT);
  viz_mol_all_data_flag = (viz_type==ALL_MOL_DATA);
  if(viz_mol_all_data_flag){
       viz_mol_pos_flag = viz_mol_orient_flag = 1;
  }
  if(viz_mol_pos_flag){
     if((world->viz_output_flag & VIZ_MOLECULES_STATES) != 0){
     	viz_mol_states_flag = 1;
     }
  } 
  
  viz_surf_pos_flag = (viz_type==MESH_GEOMETRY);
  viz_region_data_flag = (viz_type==REG_DATA);
  viz_surf_all_data_flag = (viz_type==ALL_MESH_DATA);
  if(viz_surf_all_data_flag){
     viz_surf_pos_flag = viz_region_data_flag = 1;
  }
  if(viz_surf_pos_flag){
     if((world->viz_output_flag & VIZ_SURFACE_STATES) != 0){
     	viz_surf_states_flag = 1;
     }
  }


  /* these arrays are used to hold values of main_index for objects */
  if((viz_surf_pos_flag) || (viz_region_data_flag))
  {
     if(obj_to_show_number > 0)
     {
  	if((surf_states == NULL) && (viz_surf_states_flag)){ 
     		if ((surf_states=(u_int *)malloc(obj_to_show_number*sizeof(u_int)))==NULL)      {
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
     		}
        	for(ii = 0; ii < obj_to_show_number; ii++){
			surf_states[ii] = 0;
     		}
   	}
   	if((surf_pos == NULL) && (viz_surf_pos_flag)){
     		if ((surf_pos=(u_int *)malloc(obj_to_show_number*sizeof(u_int)))==NULL) {
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
     		}
     		for(ii = 0; ii < obj_to_show_number; ii++){
			surf_pos[ii] = 0;
     		}
   	}
   	if((surf_con == NULL) && (viz_surf_pos_flag)){
      		if ((surf_con=(u_int *)malloc(obj_to_show_number*sizeof(u_int)))==NULL) {
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
      		}
      		for(ii = 0; ii < obj_to_show_number; ii++){
			surf_con[ii] = 0;
      		}
        }
   	if(surf_field_indices == NULL){
      		if ((surf_field_indices=(u_int *)malloc(obj_to_show_number*sizeof(u_int)))==NULL) {
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
      		}
      		for(ii = 0; ii < obj_to_show_number; ii++){
			surf_field_indices[ii] = 0;
      		}
        }
    

   	/* initialize array of viz_objects names */
   	if(obj_names == NULL){
      		if ((obj_names = (char **)malloc(obj_to_show_number*sizeof(char *)))==NULL) {
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
      		}
      		for(ii = 0; ii < obj_to_show_number; ii++){
			obj_names[ii] = NULL;
      		}
                /* create an array of region names */
   	        if(region_names == NULL){
      	            if ((region_names = (char ***)malloc(obj_to_show_number*sizeof(char **)))==NULL) { 
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         	        return (1);
      	            }
                    /* initialize it */
                    for(ii = 0; ii < obj_to_show_number; ii++)
                    {
                       region_names[ii] = NULL;
                    }
                 }          
      
     	        ii = 0;
     	        vizp = world->viz_obj_head;
     	        while(vizp != NULL){
		   vcp = vizp->viz_child_head;
        	   while(vcp != NULL){
         	       obj_names[ii] = my_strdup(vcp->obj->sym->name);
                       if(obj_names[ii] == NULL){
                            fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		    return (1);
         	        }
                
                        int n_regs;   /* number of regions in the object */
                        /* subtract the default region ALL */
                        n_regs = vcp->obj->num_regions - 1;
                        if(n_regs > 0){
		            region_names[ii] = (char **)malloc(n_regs*sizeof(char *));
                            if(region_names[ii] == NULL){ 
                               fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		       return (1);
      		            }
                        }

                        jj = 0;
 
                        for(rlp = vcp->obj->regions; rlp != NULL; rlp = rlp->next){
                            rp = rlp->reg;
                            if(strcmp(rp->region_last_name, "ALL") == 0) continue;
                            
                            region_names[ii][jj] = my_strdup(rp->region_last_name);
                            if(region_names[ii][jj] == NULL)
                            { 
                               fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		       return (1);
      		            }
                            jj++;
                             
                        }
                        
                        ii++;
         		vcp = vcp->next;
       		    } /* end while (vcp) */
       		    vizp = vizp->next;
    	         } /* end while (vizp) */

   	}
       
       } /* end if (obj_to_show_number > 0) */
    }  /* end if (viz_surf_pos_flag  || viz_region_data_flag) */



     /* these arrays are used to hold values of main_index for effectors 
      and 3D molecules */
   if(viz_mol_pos_flag || viz_mol_orient_flag)
   {
     if(eff_to_show_number > 0)
     {
     	if((eff_states == NULL) && (viz_mol_states_flag)){ 
     		if((eff_states=(u_int *)malloc(eff_to_show_number*sizeof(u_int)))==NULL)        {
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
     		}
     		for(ii = 0; ii < eff_to_show_number; ii++){
			eff_states[ii] = 0;
     		}
     	}
     	if((eff_pos == NULL) && (viz_mol_pos_flag)){
     		if ((eff_pos=(u_int *)malloc(eff_to_show_number*sizeof(u_int)))==NULL) 	{
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
     		}
     		for(ii = 0; ii < eff_to_show_number; ii++){
			eff_pos[ii] = 0;
     		}
     	}
     	if((eff_orient == NULL) && (viz_mol_orient_flag)){
     		if ((eff_orient = (u_int *)malloc(eff_to_show_number*sizeof(u_int)))==NULL) 	{
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
     		}
     		for(ii = 0; ii < eff_to_show_number; ii++){
			eff_orient[ii] = 0;
     		}
     	}
     	if(eff_field_indices == NULL){
     		if ((eff_field_indices = (u_int *)malloc(eff_to_show_number*sizeof(u_int)))==NULL) 	{
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
     		}
     		for(ii = 0; ii < eff_to_show_number; ii++){
			eff_field_indices[ii] = 0;
     		}
     	}
   	/* initialize array of grid_mol's names */
   	if(eff_names == NULL){

      		if ((eff_names = (char **)malloc(eff_to_show_number*sizeof(char *)))==NULL) {
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
      		}
      		for(ii = 0; ii < eff_to_show_number; ii++){
			eff_names[ii] = NULL;
      		}
   	
                index = 0;
        
                for(ii = 0; ii < world->n_species; ii++)
                {
     	           specp = world->species_list[ii];
     	           if((specp->flags & IS_SURFACE) != 0) continue;
     	           if(strcmp(specp->sym->name, "GENERIC_MOLECULE") == 0) continue;
                   if(((specp->flags & ON_GRID) == ON_GRID) && (specp->viz_state > 0)){ 
                      eff_names[index] = my_strdup(specp->sym->name);
                      if(eff_names[index] == NULL){
                         fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                         return (1);
                      }
                      index++;
                    } 
                 }
        }
        index = 0;
     } /* end if (eff_to_show_number > 0) */
   }  /* end if(viz_eff_pos_flag  */
   
   if(viz_mol_pos_flag || viz_mol_orient_flag)
   {
     if(mol_to_show_number > 0)
     {
     	if((mol_states == NULL) && (viz_mol_states_flag)){ 
     		if((mol_states=(u_int *)malloc(mol_to_show_number*sizeof(u_int)))==NULL)        {
                         fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
     		}
     		for(ii = 0; ii < mol_to_show_number; ii++){
			mol_states[ii] = 0;
     		}
     	}
     	if((mol_pos == NULL) && (viz_mol_pos_flag)) {
     		if ((mol_pos=(u_int *)malloc(mol_to_show_number*sizeof(u_int)))==NULL) 	{
                         fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
     		}
     		for(ii = 0; ii < mol_to_show_number; ii++){
			mol_pos[ii] = 0;
     		}
     	}
     	if((mol_orient == NULL) && (viz_mol_orient_flag)){
     		if ((mol_orient = (u_int *)malloc(mol_to_show_number*sizeof(u_int)))==NULL) 	{
                         fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
     		}
     		for(ii = 0; ii < mol_to_show_number; ii++){
			mol_orient[ii] = 0;
     		}
     	}
     	if(mol_field_indices == NULL){
     		if ((mol_field_indices = (u_int *)malloc(mol_to_show_number*sizeof(u_int)))==NULL) 	{
                         fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
     		}
     		for(ii = 0; ii < mol_to_show_number; ii++){
			mol_field_indices[ii] = 0;
     		}
     	}
        
   	/* initialize array of mol's names */
   	if(mol_names == NULL){
      		if ((mol_names = (char **)malloc(mol_to_show_number*sizeof(char *)))==NULL) {
                         fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
      		}
      		for(ii = 0; ii < mol_to_show_number; ii++){
			mol_names[ii] = NULL;
      		}
   	
        	index = 0;
        	for(ii = 0; ii < world->n_species; ii++)
        	{
     	   	   specp = world->species_list[ii];
     	   	   if((specp->flags & IS_SURFACE) != 0) continue;
     	   	   if(strcmp(specp->sym->name, "GENERIC_MOLECULE") == 0) continue;
           	   if(((specp->flags & NOT_FREE) == 0) && (specp->viz_state > 0)){ 
                       mol_names[index] = my_strdup(specp->sym->name);
                       if(mol_names[index] == NULL){
                         fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                         return (1);
                       }
                       index++;
                   } 
                }
        }
        index = 0;

     } /* end if (mol_to_show_number > 0) */
  }  /* end if(viz_mol_pos_flag || viz_mol_orient_flag) */

  if(viz_data_dir_name == NULL)
  {
     viz_data_dir_name = (char *)malloc((strlen(world->file_prefix_name) + strlen(dir_name_end) + 1)*sizeof(char));
     if(viz_data_dir_name == NULL){
         fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         return (1);
     }
     strcpy(viz_data_dir_name, world->file_prefix_name);
     strcat(viz_data_dir_name, dir_name_end);
  }
  
   
  if(!viz_data_dir_created)
  {
     /* test whether a directory structure created by the user exists */
     /* count the number of '/' */
     ii = 0;
     while(world->file_prefix_name[ii] != '\0') 
     {
        if(world->file_prefix_name[ii] == '/'){
           viz_dir_depth++;
        }
        ii++;
     }

     if(viz_dir_depth > 1)
     {
     	ch_ptr = strrchr(world->file_prefix_name, '/');
        num = ch_ptr - world->file_prefix_name;
        filename = (char *)malloc(sizeof(char) * (num + 1));
        if(filename == NULL){
            fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
            return (1);
        }
        strncpy(filename, world->file_prefix_name, num);  
        filename[num] = '\0';

        status = stat(filename, &f_stat);
        if((status == -1) && (errno == ENOENT)){
   	     fprintf(world->err_file, "File %s, Line %ld: Error ERROENT while looking for viz_output data directory '%s'.  Please create it.\n", __FILE__, (long)__LINE__, filename);
   	     exit (1);
        }

     } 

     /* create folder for the visualization data */  
     status = mkdir(viz_data_dir_name, S_IRWXU | S_IRWXG | S_IRWXO);
     if((status != 0) && (errno != EEXIST)){
   	fprintf(world->err_file, "File %s, Line %ld: Error while creating viz_output data directory '%s'.\n", __FILE__, (long)__LINE__, viz_data_dir_name);
   	return (1);
     }

     /* create top level directory for "frame data" */
     if(frame_data_dir_name == NULL)
     {
         frame_data_dir_name = (char *)malloc((strlen(viz_data_dir_name) + strlen("/frame_data") + 1)*sizeof(char));
         if(frame_data_dir_name == NULL){
            fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
            return (1);
         }

        strcpy(frame_data_dir_name, viz_data_dir_name);
        strcat(frame_data_dir_name, "/frame_data");
     }


     status = mkdir(frame_data_dir_name, S_IRWXU | S_IRWXG | S_IRWXO);
     if((status != 0) && (errno != EEXIST)){
   	fprintf(world->err_file, "File %s, Line %ld: Error while creating frame data directory '%s'.\n", __FILE__, (long)__LINE__, frame_data_dir_name);
   	return (1);
     }

     viz_data_dir_created = 1;


  }

    /* create a subdirectory for the "iteration" data */
    sprintf(iteration_number, "%d", viz_iteration);
    iteration_number_dir = (char *)malloc((strlen(frame_data_dir_name) + strlen("/iteration_") + strlen(iteration_number) + 1));
    if(iteration_number_dir == NULL){
            fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
            return (1);
    }   
    strcpy(iteration_number_dir, frame_data_dir_name);
    strcat(iteration_number_dir, "/");
    strcat(iteration_number_dir, "iteration_");

    strcat(iteration_number_dir, iteration_number);       

    /* searh for the directory 'iteration_number_dir' */
    status = stat(iteration_number_dir, &f_stat);

    if((status == -1) && (errno != ENOENT)){
         fprintf(world->err_file, "File %s, Line %ld: error %d searching for the directory %s.\n", __FILE__, (long)__LINE__, errno, iteration_number_dir);
     }else if((status == -1) && (errno == ENOENT)){
         /* directory not found - create one */
         status = mkdir(iteration_number_dir, S_IRWXU | S_IRWXG | S_IRWXO);
         if((status != 0) && (errno != EEXIST)){
   	     fprintf(world->err_file, "File %s, Line %ld: Error %d while creating viz_output data directory '%s'.\n", __FILE__, (long)__LINE__, errno, iteration_number_dir);
   	     return (1);
         }

     }
 
    /* if MESHES keyword is absent (commented) the corresponding 
       'meshes' files should be empty */

   if ((viz_surf_pos_flag) || (obj_to_show_number == 0)) {
  	 
        mesh_pos_file_path = (char *)malloc(sizeof(char) * (strlen(iteration_number_dir) + strlen("/mesh_positions.bin") + 1));
        if(mesh_pos_file_path == NULL){
            fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
            return (1);
        }
 
        sprintf(mesh_pos_file_path,"%s/mesh_positions.bin",iteration_number_dir); 
 
        if(mesh_pos_name == NULL)
        {
           mesh_pos_name = (char *)malloc(sizeof(char) * (strlen("mesh_positions.bin") + 1));
          if(mesh_pos_name == NULL){
              fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
              return (1);
          }
     	  strcpy(mesh_pos_name, "mesh_positions.bin");
        }

        /* if there are symbolic links present with this name - remove them */
        status = stat(mesh_pos_file_path, &f_stat);
        if(status == 0){
           if(f_stat.st_mode | S_IFLNK) {
            
              /* remove the symbolic link */
              status = unlink(mesh_pos_file_path);
              if(status != 0) { 
                       fprintf(world->err_file, "File %s, Line %ld: error %d removing file link to the file %s.\n", __FILE__, (long)__LINE__, errno, mesh_pos_name);
                       return(1);
               }
 
           }
        }
 

      	if ((mesh_pos_data = fopen(mesh_pos_file_path,"wb"))==NULL) {
                fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__, mesh_pos_file_path);
                return(1);
        }


   }

   if ((viz_surf_states_flag) || (obj_to_show_number == 0)){
       mesh_states_file_path = (char *)malloc(sizeof(char) * (strlen(iteration_number_dir) + strlen("/mesh_states.bin") + 1));
        if(mesh_states_file_path == NULL){
            fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
            return (1);
        }
        sprintf(mesh_states_file_path,"%s/mesh_states.bin", iteration_number_dir);
     
        if(mesh_states_name == NULL)
        {
           mesh_states_name = (char *)malloc(sizeof(char) * (strlen("mesh_states.bin") + 1));
          if(mesh_states_name == NULL){
              fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
              return (1);
          }
     	  strcpy(mesh_states_name, "mesh_states.bin");
        }

        /* if there are symbolic links present with this name - remove them */
        status = stat(mesh_states_file_path, &f_stat);
        if(status == 0){
           if(f_stat.st_mode | S_IFLNK) {
              /* remove the symbolic link */
              status = unlink(mesh_states_file_path);
              if(status != 0) { 
                       fprintf(world->err_file, "File %s, Line %ld: error %d removing file link to the file %s.\n", __FILE__, (long)__LINE__, errno, mesh_states_name);
                       return(1);
               }
 
           }
        }

        if ((mesh_states_data=fopen(mesh_states_file_path,"wb"))==NULL) {
             fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__,filename);
             return(1);
        }
    }

    if ((viz_region_data_flag) || (obj_to_show_number == 0)) {
       region_viz_data_file_path = (char *)malloc(sizeof(char) * (strlen(iteration_number_dir) + strlen("/region_indices.bin") + 1));
        if(region_viz_data_file_path == NULL){
            fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
            return (1);
        }
         sprintf(region_viz_data_file_path,"%s/region_indices.bin", iteration_number_dir);
     
        if(region_viz_data_name == NULL)
        {
           region_viz_data_name = (char *)malloc(sizeof(char) * (strlen("region_indices.bin") + 1));
          if(region_viz_data_name == NULL){
              fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
              return (1);
          }
     	  strcpy(region_viz_data_name, "region_indices.bin");
        }

        /* if there are symbolic links present with this name - remove them */
        status = stat(region_viz_data_file_path, &f_stat);
        if(status == 0){
           if(f_stat.st_mode | S_IFLNK) {
              /* remove the symbolic link */
              status = unlink(region_viz_data_file_path);
              if(status != 0) { 
                       fprintf(world->err_file, "File %s, Line %ld: error %d removing file link to the file %s.\n", __FILE__, (long)__LINE__, errno, region_viz_data_name);
                       return(1);
               }
 
           }
        }

        if ((region_data=fopen(region_viz_data_file_path,"wb"))==NULL) {
              fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__, region_viz_data_file_path);
              return(1);
        }
    }

    if (((viz_surf_pos_flag || viz_region_data_flag)) || (obj_to_show_number == 0)) {
       meshes_header_file_path = (char *)malloc(sizeof(char) * (strlen(iteration_number_dir) + strlen("/meshes.dx") + 1));
        if(meshes_header_file_path == NULL){
            fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
            return (1);
        }
         sprintf(meshes_header_file_path,"%s/meshes.dx", iteration_number_dir);
     
        if(meshes_header_name == NULL)
        {
           meshes_header_name = (char *)malloc(sizeof(char) * (strlen("meshes.dx") + 1));
          if(meshes_header_name == NULL){
              fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
              return (1);
          }
     	  strcpy(meshes_header_name, "meshes.dx");
        }
         
        /* if there are symbolic links present with this name - remove them */
        status = stat(meshes_header_file_path, &f_stat);
        if(status == 0){
           if(f_stat.st_mode | S_IFLNK) {
              /* remove the symbolic link */
              status = unlink(meshes_header_file_path);
              if(status != 0) { 
                       fprintf(world->err_file, "File %s, Line %ld: error %d removing file link to the file %s.\n", __FILE__, (long)__LINE__, errno, meshes_header_name);
                       return(1);
               }
 
           }
        }
        
        if(count_meshes_header == 0)
        {
           if ((meshes_header=fopen(meshes_header_file_path,"w"))==NULL) {
              fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__, meshes_header_file_path);
              return(1);
           }
           count_meshes_header++;
        }else{
           if ((meshes_header=fopen(meshes_header_file_path,"a"))==NULL) {
              fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__, meshes_header_file_path);
              return(1);
           }
           count_meshes_header++;
        }
    }


   /* If MOLECULES keyword is absent or commented 
      create empty 'molecules' output files. */

   if ((viz_mol_pos_flag) || ((mol_to_show_number == 0) && (eff_to_show_number == 0))) {
     
       vol_mol_pos_file_path = (char *)malloc(sizeof(char) * (strlen(iteration_number_dir) + strlen("/volume_molecules_positions.bin") + 1));
        if(vol_mol_pos_file_path == NULL){
            fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
            return (1);
        }
        sprintf(vol_mol_pos_file_path,"%s/volume_molecules_positions.bin",iteration_number_dir);
     
        if(vol_mol_pos_name == NULL)
        {
          vol_mol_pos_name = (char *)malloc(sizeof(char) * (strlen("volume_molecules_positions.bin") + 1));
          if(vol_mol_pos_name == NULL){
              fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
              return (1);
          }
     	  strcpy(vol_mol_pos_name, "volume_molecules_positions.bin");
        }

        /* if there are symbolic links present with this name - remove them */
        status = stat(vol_mol_pos_file_path, &f_stat);
        if(status == 0){
           if(f_stat.st_mode | S_IFLNK) {
              /* remove the symbolic link */
              status = unlink(vol_mol_pos_file_path);
              if(status != 0) { 
                       fprintf(world->err_file, "File %s, Line %ld: error %d removing file link to the file %s.\n", __FILE__, (long)__LINE__, errno, vol_mol_pos_name);
                       return(1);
               }
 
           }
        }
        
      	if ((vol_mol_pos_data = fopen(vol_mol_pos_file_path,"wb"))==NULL) {
                fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__, vol_mol_pos_file_path);
                return(1);
        }

       surf_mol_pos_file_path = (char *)malloc(sizeof(char) * (strlen(iteration_number_dir) + strlen("/surface_molecules_positions.bin") + 1));
        if(surf_mol_pos_file_path == NULL){
            fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
            return (1);
        }
        sprintf(surf_mol_pos_file_path,"%s/surface_molecules_positions.bin",iteration_number_dir);
     
        if(surf_mol_pos_name == NULL)
        {
          surf_mol_pos_name = (char *)malloc(sizeof(char) * (strlen("surface_molecules_positions.bin") + 1));
          if(surf_mol_pos_name == NULL){
              fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
              return (1);
          }
     	  strcpy(surf_mol_pos_name, "surface_molecules_positions.bin");
        }

        /* if there are symbolic links present with this name - remove them */
        status = stat(surf_mol_pos_file_path, &f_stat);
        if(status == 0){
           if(f_stat.st_mode | S_IFLNK) {
              /* remove the symbolic link */
              status = unlink(surf_mol_pos_file_path);
              if(status != 0) { 
                       fprintf(world->err_file, "File %s, Line %ld: error %d removing file link to the file %s.\n", __FILE__, (long)__LINE__, errno, surf_mol_pos_name);
                       return(1);
               }
 
           }
        }
      	
        if ((surf_mol_pos_data = fopen(surf_mol_pos_file_path,"wb"))==NULL) {
                fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__, surf_mol_pos_file_path);
                return(1);
        }

   }

   if ((viz_mol_orient_flag) || ((mol_to_show_number == 0) && (eff_to_show_number == 0))){
      	 
        vol_mol_orient_file_path = (char *)malloc(sizeof(char) * (strlen(iteration_number_dir) + strlen("/volume_molecules_orientations.bin") + 1));
        if(vol_mol_orient_file_path == NULL){
            fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
            return (1);
        }
        sprintf(vol_mol_orient_file_path, "%s/volume_molecules_orientations.bin", iteration_number_dir);
     
        if(vol_mol_orient_name == NULL)
        {
          vol_mol_orient_name = (char *)malloc(sizeof(char) * (strlen("volume_molecules_orientations.bin") + 1));
          if(vol_mol_orient_name == NULL){
              fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
              return (1);
          }
     	  strcpy(vol_mol_orient_name, "volume_molecules_orientations.bin");
        }
        
        /* if there are symbolic links present with this name - remove them */
        status = stat(vol_mol_orient_file_path, &f_stat);
        if(status == 0){
           if(f_stat.st_mode | S_IFLNK) {
              /* remove the symbolic link */
              status = unlink(vol_mol_orient_file_path);
              if(status != 0) { 
                       fprintf(world->err_file, "File %s, Line %ld: error %d removing file link to the file %s.\n", __FILE__, (long)__LINE__, errno, vol_mol_orient_name);
                       return(1);
               }
 
           }
        }

      	if ((vol_mol_orient_data = fopen(vol_mol_orient_file_path,"wb"))==NULL) {
                fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__, vol_mol_orient_file_path);
                return(1);
        }


       surf_mol_orient_file_path = (char *)malloc(sizeof(char) * (strlen(iteration_number_dir) + strlen("/surface_molecules_orientations.bin") + 1));
        if(surf_mol_orient_file_path == NULL){
            fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
            return (1);
        }
      	 
        sprintf(surf_mol_orient_file_path,"%s/surface_molecules_orientations.bin",iteration_number_dir);
     
        if(surf_mol_orient_name == NULL)
        {
          surf_mol_orient_name = (char *)malloc(sizeof(char) * (strlen("surface_molecules_orientations.bin") + 1));
          if(surf_mol_orient_name == NULL){
              fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
              return (1);
          }
     	  strcpy(surf_mol_orient_name, "surface_molecules_orientations.bin");
        }

        /* if there are symbolic links present with this name - remove them */
        status = stat(surf_mol_orient_file_path, &f_stat);
        if(status == 0){
           if(f_stat.st_mode | S_IFLNK) {
              /* remove the symbolic link */
              status = unlink(surf_mol_orient_file_path);
              if(status != 0) { 
                       fprintf(world->err_file, "File %s, Line %ld: error %d removing file link to the file %s.\n", __FILE__, (long)__LINE__, errno, surf_mol_orient_name);
                       return(1);
               }
 
           }
        }
      	
        if ((surf_mol_orient_data = fopen(surf_mol_orient_file_path,"wb"))==NULL) {
                fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__, surf_mol_orient_file_path);
                return(1);
        }
   }

   if ((viz_mol_states_flag) || ((mol_to_show_number == 0) && (eff_to_show_number == 0))) {
      	 
       vol_mol_states_file_path = (char *)malloc(sizeof(char) * (strlen(iteration_number_dir) + strlen("/volume_molecules_states.bin") + 1));
        if(vol_mol_states_file_path == NULL){
            fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
            return (1);
        }
        sprintf(vol_mol_states_file_path,"%s/volume_molecules_states.bin", iteration_number_dir);
     
        if(vol_mol_states_name == NULL)
        {
          vol_mol_states_name = (char *)malloc(sizeof(char) * (strlen("volume_molecules_states.bin") + 1));
          if(vol_mol_states_name == NULL){
              fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
              return (1);
          }
     	  strcpy(vol_mol_states_name, "volume_molecules_states.bin");
        }
        
        /* if there are symbolic links present with this name - remove them */
        status = stat(vol_mol_states_file_path, &f_stat);
        if(status == 0){
           if(f_stat.st_mode | S_IFLNK) {
              /* remove the symbolic link */
              status = unlink(vol_mol_states_file_path);
              if(status != 0) { 
                       fprintf(world->err_file, "File %s, Line %ld: error %d removing file link to the file %s.\n", __FILE__, (long)__LINE__, errno, vol_mol_states_file_path);
                       return(1);
               }
 
           }
        }

      	if ((vol_mol_states_data = fopen(vol_mol_states_file_path,"wb"))==NULL) {
                fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__, vol_mol_states_file_path);
                return(1);
        }

      	 
       surf_mol_states_file_path = (char *)malloc(sizeof(char) * (strlen(iteration_number_dir) +  strlen("/surface_molecules_states.bin") + 1));
        if(surf_mol_states_file_path == NULL){
            fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
            return (1);
        }
        sprintf(surf_mol_states_file_path, "%s/surface_molecules_states.bin", iteration_number_dir);
     
        if(surf_mol_states_name == NULL)
        {
          surf_mol_states_name = (char *)malloc(sizeof(char) * (strlen("surface_molecules_states.bin") + 1));
          if(surf_mol_states_name == NULL){
              fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
              return (1);
          }
     	  strcpy(surf_mol_states_name, "surface_molecules_states.bin");
        }

        /* if there are symbolic links present with this name - remove them */
        status = stat(surf_mol_states_file_path, &f_stat);
        if(status == 0){
           if(f_stat.st_mode | S_IFLNK) {
              /* remove the symbolic link */
              status = unlink(surf_mol_states_file_path);
              if(status != 0) { 
                       fprintf(world->err_file, "File %s, Line %ld: error %d removing file link to the file %s.\n", __FILE__, (long)__LINE__, errno, surf_mol_states_name);
                       return(1);
               }
 
           }
        }
      	
        if ((surf_mol_states_data = fopen(surf_mol_states_file_path,"wb"))==NULL) {
                fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__, surf_mol_states_file_path);
                return(1);
        }

   }

    if ((viz_mol_pos_flag || viz_mol_orient_flag)  || ((mol_to_show_number == 0) && (eff_to_show_number == 0))){
       vol_mol_header_file_path = (char *)malloc(sizeof(char) * (strlen(iteration_number_dir) + strlen("/volume_molecules.dx") + 1));
        if(vol_mol_header_file_path == NULL){
            fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
            return (1);
        }
         sprintf(vol_mol_header_file_path,"%s/volume_molecules.dx", iteration_number_dir);
     
        if(vol_mol_header_name == NULL)
        {
          vol_mol_header_name = (char *)malloc(sizeof(char) * (strlen("volume_molecules.dx") + 1));
          if(vol_mol_header_name == NULL){
              fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
              return (1);
          }
     	  strcpy(vol_mol_header_name, "volume_molecules.dx");
        }
        
        /* if there are symbolic links present with this name - remove them */
        status = stat(vol_mol_header_file_path, &f_stat);
        if(status == 0){
           if(f_stat.st_mode | S_IFLNK) {
              /* remove the symbolic link */
              status = unlink(vol_mol_header_file_path);
              if(status != 0) { 
                       fprintf(world->err_file, "File %s, Line %ld: error %d removing file link to the file %s.\n", __FILE__, (long)__LINE__, errno, vol_mol_header_name);
                       return(1);
               }
 
           }
        }
        
        if(count_vol_mol_header == 0)
        {
           if ((vol_mol_header=fopen(vol_mol_header_file_path,"w"))==NULL) {
              fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__, vol_mol_header_file_path);
              return(1);
           }
           count_vol_mol_header++;
        }else{
           if ((vol_mol_header=fopen(vol_mol_header_file_path,"a"))==NULL) {
              fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__, vol_mol_header_file_path);
              return(1);
           }
           count_vol_mol_header++;
        }

       surf_mol_header_file_path = (char *)malloc(sizeof(char) * (strlen(iteration_number_dir) + strlen("/surface_molecules.dx") + 1));
        if(surf_mol_header_file_path == NULL){
            fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
            return (1);
        }
         sprintf(surf_mol_header_file_path, "%s/surface_molecules.dx", iteration_number_dir);
     
        if(surf_mol_header_name == NULL)
        {
          surf_mol_header_name = (char *)malloc(sizeof(char) * (strlen("surface_molecules.dx") + 1));
          if(surf_mol_header_name == NULL){
              fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
              return (1);
          }
     	  strcpy(surf_mol_header_name, "surface_molecules.dx");
        }
        
        /* if there are symbolic links present with this name - remove them */
        status = stat(surf_mol_header_file_path, &f_stat);
        if(status == 0){
           if(f_stat.st_mode | S_IFLNK) {
              /* remove the symbolic link */
              status = unlink(surf_mol_header_file_path);
              if(status != 0) { 
                       fprintf(world->err_file, "File %s, Line %ld: error %d removing file link to the file %s.\n", __FILE__, (long)__LINE__, errno, surf_mol_header_name);
                       return(1);
               }
 
           }
        }
        
        if(count_surf_mol_header == 0)
        {
           if ((surf_mol_header=fopen(surf_mol_header_file_path,"w"))==NULL) {
              fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__, surf_mol_header_file_path);
              return(1);
           }
           count_surf_mol_header++;
        }else{
           if ((surf_mol_header=fopen(surf_mol_header_file_path, "a"))==NULL) {
              fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__, surf_mol_header_file_path);
              return(1);
           }
           count_surf_mol_header++;
        }
    }


    /* find out the values of the current iteration steps for
       (GEOMETRY, REG_DATA),  
       (MOL_POS, MOL_ORIENT) frames combinations */
   
    fdl_ptr = world->frame_data_head;
    while(fdl_ptr != NULL){
        if(fdl_ptr->type == MESH_GEOMETRY){
             curr_surf_pos_iteration_step = fdl_ptr->viz_iterationll;
        }if(fdl_ptr->type == REG_DATA){
             curr_region_data_iteration_step = fdl_ptr->viz_iterationll;
        }if(fdl_ptr->type == ALL_MESH_DATA){
             curr_surf_pos_iteration_step = fdl_ptr->viz_iterationll;
             curr_region_data_iteration_step = fdl_ptr->viz_iterationll;
        }else if(fdl_ptr->type == MOL_POS){
             curr_mol_pos_iteration_step = fdl_ptr->viz_iterationll;
	}else if(fdl_ptr->type == MOL_ORIENT) 
        {
             curr_mol_orient_iteration_step = fdl_ptr->viz_iterationll;
        }else if(fdl_ptr->type == ALL_MOL_DATA){
             curr_mol_pos_iteration_step = fdl_ptr->viz_iterationll;
             curr_mol_orient_iteration_step = fdl_ptr->viz_iterationll;
        }	
        fdl_ptr = fdl_ptr->next;
    }
  
    /* If the values of the current iteration steps for REG_VIZ_VALUES and
       MESH_GEOMETRY are equal set this value to the 
       special_surf_iteration_step.
       Do the same for the (MOL_POS,MOL_ORIENT).
    */
     
    if(curr_region_data_iteration_step == curr_surf_pos_iteration_step){
	special_surf_iteration_step = curr_surf_pos_iteration_step;
    }
    
    if(curr_mol_orient_iteration_step == curr_mol_pos_iteration_step){
	special_mol_iteration_step = curr_mol_orient_iteration_step;
    }
    
    /* check for the special_iteration_step  */  
    if(viz_surf_pos_flag || viz_region_data_flag){	    
	    if(fdlp->viz_iterationll == special_surf_iteration_step){
		special_surf_frames_counter++;
            }
    }

    /* check for the special_iteration_step  */ 
    if(viz_mol_pos_flag || viz_mol_orient_flag){	    
	    if(fdlp->viz_iterationll == special_mol_iteration_step){
		special_mol_frames_counter++;
            }
    }

  /* dump walls */
  if((viz_surf_pos_flag) || (viz_region_data_flag)) {
     vizp = world->viz_obj_head;
     
     while(vizp!=NULL) {


    /* Traverse all visualized compartments 
       output mesh element positions and connections */
    vcp = vizp->viz_child_head;

    while(vcp!=NULL) {

      objp = vcp->obj;
      if(objp->viz_state == NULL) continue;
      pop=(struct polygon_object *)objp->contents;

      if (objp->object_type==POLY_OBJ || objp->object_type==BOX_OBJ) {
          opp=(struct ordered_poly *)pop->polygon_data;
          edp=opp->element;
          element_data_count=objp->n_walls_actual;

          if (viz_surf_pos_flag) {
            fprintf(meshes_header,
              "object %d class array type float rank 1 shape 3 items %d %s binary data file %s,%d # %s.positions #\n",
              meshes_main_index,objp->n_verts,my_byte_order,mesh_pos_name, mesh_pos_byte_offset, objp->sym->name);
            fprintf(meshes_header,
              "\tattribute \"dep\" string \"positions\"\n\n");
            surf_pos[surf_pos_index] = meshes_main_index;
            surf_pos_index++;
            meshes_main_index++;

            /* output polyhedron vertices */
            for (ii=0;ii<objp->n_verts;ii++) {
              v1 = (float)(world->length_unit*objp->verts[ii].x);
              v2 = (float)(world->length_unit*objp->verts[ii].y);
              v3 = (float)(world->length_unit*objp->verts[ii].z);
              fwrite(&v1,sizeof v1,1,mesh_pos_data);
              fwrite(&v2,sizeof v2,1,mesh_pos_data);
              fwrite(&v3,sizeof v3,1,mesh_pos_data);
              mesh_pos_byte_offset += (sizeof(v1) + sizeof(v2) + sizeof(v3));
            }

            /* output polygon element connections */
            fprintf(meshes_header,
              "object %d class array type int rank 1 shape 3 items %d %s binary data file %s,%d # %s.connections #\n",
              meshes_main_index,element_data_count,my_byte_order, mesh_pos_name, mesh_pos_byte_offset, objp->sym->name);
            fprintf(meshes_header,
              "\tattribute \"ref\" string \"positions\"\n");
            fprintf(meshes_header,
              "\tattribute \"element type\" string \"triangles\"\n\n");

            for (ii=0;ii<objp->n_walls;ii++) {
              if (!get_bit(pop->side_removed,ii)) {
                vi1=edp[ii].vertex_index[0];
                vi2=edp[ii].vertex_index[1];
                vi3=edp[ii].vertex_index[2];
	        fwrite(&vi1,sizeof vi1,1,mesh_pos_data);
	        fwrite(&vi2,sizeof vi2,1,mesh_pos_data);
	        fwrite(&vi3,sizeof vi3,1,mesh_pos_data);
                mesh_pos_byte_offset += (sizeof(vi1) + sizeof(vi2) + sizeof(vi3));
              }
            }
            surf_con[surf_con_index] = meshes_main_index;
            meshes_main_index++;
            surf_con_index++;

          } /* end viz_surf_pos_flag for POLY_OBJ */
 

          if (viz_surf_states_flag) {
            fprintf(meshes_header,
              "object %d class array type int rank 0 items %d %s binary data file %s,%d # %s.states #\n", meshes_main_index, element_data_count, my_byte_order, mesh_states_name, mesh_states_byte_offset, objp->sym->name);
            fprintf(meshes_header,"\tattribute \"dep\" string \"connections\"\n\n");
           surf_states[surf_states_index] = meshes_main_index;
           surf_states_index++;
           meshes_main_index++;

            for (ii=0;ii<objp->n_walls;ii++) {
               if (!get_bit(pop->side_removed,ii)) {
                 state=objp->viz_state[ii];
                 fwrite(&state,sizeof (state),1,mesh_states_data);
                 mesh_states_byte_offset += sizeof(state);
               }
            }

          } 	/* end viz_surf_states_flag for POLY_OBJ */


          if(viz_region_data_flag && (objp->num_regions > 1))
          {
              surf_region_values_index = 0;

              for(rlp = objp->regions; rlp != NULL; rlp = rlp->next)
              {
                  rp = rlp->reg;
                  if(strcmp(rp->region_last_name, "ALL") == 0) continue; 
                  
                  /* number of walls in the region */
                  int region_walls_number = 0; 
                  
                  for(jj = 0; jj < objp->n_walls; jj++)
                  {
                    int n = objp->wall_p[jj]->side;
                    if(get_bit(rp->membership,n))
                    {
                        fwrite(&n,sizeof (n),1,region_data);
                        region_data_byte_offset += sizeof(n);
                        region_walls_number++;
                    }

                  }

                  fprintf(meshes_header,
                        "object %d class array type int rank 0 items %d %s binary data file %s,%d # %s.region_data #\n", meshes_main_index, region_walls_number, my_byte_order, region_viz_data_name, region_data_byte_offset_prev, objp->sym->name);
                  fprintf(meshes_header,"\tattribute \"ref\" string \"connections\"\n");  
                  fprintf(meshes_header,"\tattribute \"identity\" string \"region_indices\"\n");
                  fprintf(meshes_header,"\tattribute \"name\" string \"%s\"\n", rp->region_last_name);
                  if(rp->region_viz_value > 0){
                      fprintf(meshes_header,"\tattribute \"viz_value\" number %d\n", rp->region_viz_value);
                  }

                  region_data_byte_offset_prev = region_data_byte_offset;
                  surf_region_values[surf_obj_region_values_index][surf_region_values_index] = meshes_main_index;
                  surf_region_values_index++;
                  meshes_main_index++;
                  fprintf(meshes_header, "\n\n");

              } /* end for */
              

           } /* end if(region_data_flag) */


       }	/* end POLY_OBJ */

       surf_obj_region_values_index++; 

      vcp = vcp->next;
      }
      
      vizp = vizp->next;
      }
    } /* end (viz_surf_pos_flag || viz_surf_states_flag || viz_region_data_flag) for vizp */
    

    /* build fields here */
   if(obj_to_show_number > 0)
   {
      if(fdlp->type == ALL_MESH_DATA) {
           show_meshes = 1;
      }
      else if((viz_surf_pos_flag || viz_region_data_flag) && ((special_surf_frames_counter == 0) || (special_surf_frames_counter == 2))){
		show_meshes = 1;
      }
    }

    /* create field objects */
    if(show_meshes)
    {
         for(ii = 0; ii < obj_to_show_number; ii++)
         {
             if(obj_names[ii] != NULL){
                fprintf(meshes_header,
                    "object %d field   # %s #\n",meshes_main_index, obj_names[ii]);
             }
             if(surf_pos[ii] > 0){
             	fprintf(meshes_header,
                 "\tcomponent \"positions\" value %d\n",surf_pos[ii]);
             }
             if(surf_con[ii] > 0){
             	fprintf(meshes_header,
                 "\tcomponent \"connections\" value %d\n",surf_con[ii]);
             }
             if(surf_states != NULL){
                if(surf_states[ii] > 0){
             	   fprintf(meshes_header,
                	"\tcomponent \"state_values\" value %d\n",surf_states[ii]);
	        }
             }
             if(surf_region_values[ii] != NULL)
             {
                for(jj = 0; jj < obj_num_regions[ii]; jj++)
                {
                   if(surf_region_values[ii][jj] > 0){
             	      fprintf(meshes_header,
                	"\tcomponent \"%s\" value %d\n",region_names[ii][jj], surf_region_values[ii][jj]);
	           }
                }
             }
             fprintf(meshes_header, "\n");
             surf_field_indices[ii] = meshes_main_index;

             meshes_main_index++;
           }     
     }





   /* create a group object for all meshes */       
    if(show_meshes)
    {

        vizp = world->viz_obj_head;
        if(vizp != NULL){

        	fprintf(meshes_header,"object %d group # %s #\n",meshes_main_index, "meshes");
        	meshes_main_index++;

                ii = 0;
        	while(vizp != NULL) {
         		vcp = vizp->viz_child_head;
         		while(vcp != NULL){
                          if(surf_field_indices[ii] > 0){
             			fprintf(meshes_header,"\tmember \"%s\" value %d\n",vcp->obj->sym->name,surf_field_indices[ii]);
                          }
                          ii++;
		       	  vcp = vcp->next;
         		}
         		vizp = vizp->next;
        	}       
        	fprintf(meshes_header, "\n"); 
      	}  /* end (if vizp) */
        
        /* reset meshes object indexing */
        meshes_main_index = 1;
        count_meshes_header = 0;
        special_surf_frames_counter = 0;

        /* store iteration_number for meshes */
      iteration_numbers_meshes[iteration_numbers_meshes_count] = viz_iteration;
      iteration_numbers_meshes_count++;

	/* put value of viz_iteration into the time_values array */
            if(time_values_count == 0){
               time_values[time_values_count] = viz_iteration;  
               time_values_count++;
               last_time_value_written = viz_iteration;
            }else if(viz_iteration > last_time_value_written){
               time_values[time_values_count] = viz_iteration;  
               time_values_count++;
               last_time_value_written = viz_iteration;
            } 
          
    } /* end if(show_meshes) */

    /* Visualize molecules. */

/* dump grid molecules. */

 if(viz_mol_pos_flag || viz_mol_orient_flag){	    

       /* create references to the numbers of grid molecules of each name. */
       if ((viz_grid_mol_count=(u_int *)malloc(n_species*sizeof(u_int)))==NULL) {
         return(1);
       }

       /* perform initialization */
      for (ii = 0; ii < n_species; ii++){
         spec_id = species_list[ii]->species_id;
         viz_grid_mol_count[spec_id]=0;

      }


   for (ii = 0; ii < n_species; ii++)
   {
     specp = species_list[ii];
     if((specp->flags & ON_GRID) == 0) continue; 
     if(specp->viz_state == EXCLUDE_OBJ) continue; 
 
        grid_mol_name = specp->sym->name;
        spec_id = specp->species_id; 

        if(viz_mol_pos_flag){
      	   surf_mol_pos_byte_offset_prev = surf_mol_pos_byte_offset;
        }
        if(viz_mol_orient_flag){
      	   surf_mol_orient_byte_offset_prev = surf_mol_orient_byte_offset;
        }
        if(viz_mol_states_flag){
      	   surf_mol_states_byte_offset_prev = surf_mol_states_byte_offset;
        }
        /* Write binary data files. */
        vizp = world->viz_obj_head;
        while(vizp != NULL) {
         vcp = vizp->viz_child_head;
         while(vcp != NULL){
	     objp = vcp->obj;
             wp = objp->wall_p;
             no_printf("Traversing walls in object %s\n",objp->sym->name);
             for (jj=0;jj<objp->n_walls;jj++) {
                w = wp[jj];
                if (w!=NULL) {
	           sg = w->effectors;
                   if (sg!=NULL) {
                     for (index=0;index<sg->n_tiles;index++) {
                        grid2xyz(sg,index,&p0);
	                gmol=sg->mol[index];
	                if ((gmol!=NULL) && (viz_mol_states_flag)){
	                   state=sg->mol[index]->properties->viz_state;
                        }
                        if (gmol != NULL) {
                             if(spec_id == gmol->properties->species_id){
                                 if(viz_grid_mol_count[spec_id] < specp->population){
                                     viz_grid_mol_count[spec_id]++;
                                 }else{
                                     fprintf(log_file,"MCell: molecule count disagreement!!\n");
                                     fprintf(log_file,"  Species %s  population = %d  count = %d\n",grid_mol_name,specp->population,viz_grid_mol_count[spec_id]);

                                 }

                               if(viz_mol_pos_flag){
                                   /* write positions information */
	           		   v1=(float)(world->length_unit*p0.x);
	           		   v2=(float)(world->length_unit*p0.y);
	           		   v3=(float)(world->length_unit*p0.z);
	           		   fwrite(&v1,sizeof v1,1,surf_mol_pos_data);
	               		   fwrite(&v2,sizeof v2,1,surf_mol_pos_data);
	           		   fwrite(&v3,sizeof v3,1,surf_mol_pos_data);
                                   surf_mol_pos_byte_offset += (sizeof(v1) + sizeof(v2) + sizeof(v3));
                               }
                               if(viz_mol_orient_flag){
                                   /* write orientations information */
                   		   v1=(float)((w->normal.x)*(gmol->orient));
                   		   v2=(float)((w->normal.y)*(gmol->orient));
                   		   v3=(float)((w->normal.z)*(gmol->orient));
                   		   fwrite(&v1,sizeof v1,1,surf_mol_orient_data);
                   		   fwrite(&v2,sizeof v2,1,surf_mol_orient_data);
                   		   fwrite(&v3,sizeof v3,1,surf_mol_orient_data);
                                   surf_mol_orient_byte_offset += (sizeof(v1) + sizeof(v2) + sizeof(v3));
                  		}
                          } /* end if strcmp */
                           
                        } /* end if (gmol)*/
                     } /* end for */
                   } /* end if (sg) */
                 } /* end if (w)*/
              } /* end for */
              vcp = vcp->next;
            } /* end while (vcp) */
                                              
            vizp = vizp->next;
         } /* end while (vzp) */

        num = viz_grid_mol_count[spec_id];
        
        if(viz_mol_pos_flag)
        {
           if(num > 0)
           {   
        	fprintf(surf_mol_header,"object %d class array type float rank 1 shape 3 items %d %s binary data file %s,%d # %s positions #\n", surf_mol_main_index,num,my_byte_order, surf_mol_pos_name, surf_mol_pos_byte_offset_prev, grid_mol_name);
        	fprintf(surf_mol_header,"\tattribute \"dep\" string \"positions\"\n\n");
            }else{
                /* output empty arrays for zero molecule counts here */
                fprintf(surf_mol_header,"object %d array   # %s positions #\n",surf_mol_main_index, grid_mol_name);
            }
            eff_pos[eff_pos_index] = surf_mol_main_index;
            eff_pos_index++;
            surf_mol_main_index++;
         }
         
         if(viz_mol_orient_flag)
         {
           if(num > 0)
           {
        	fprintf(surf_mol_header,"object %d class array type float rank 1 shape 3 items %d %s binary data file %s,%d   # %s orientations #\n",surf_mol_main_index,num,my_byte_order, surf_mol_orient_name, surf_mol_orient_byte_offset_prev, grid_mol_name);
        	fprintf(surf_mol_header,"\tattribute \"dep\" string \"positions\"\n\n");
            }else{
                /* output empty arrays for zero molecule counts here */
                fprintf(surf_mol_header,"object %d array   # %s orientations #\n",surf_mol_main_index, grid_mol_name);
            }
            eff_orient[eff_orient_index] = surf_mol_main_index;
            eff_orient_index++;
            surf_mol_main_index++;
        }

        if (viz_mol_states_flag) {
          if(num > 0)
          {
            /* write states information. */
            fwrite(&state,sizeof state,1,surf_mol_states_data);
            surf_mol_states_byte_offset += (sizeof state);
        
	    fprintf(surf_mol_header,"object %d class constantarray type int items %d %s binary data file %s,%d  # %s states #\n",surf_mol_main_index,num, my_byte_order,surf_mol_states_name,surf_mol_states_byte_offset_prev, grid_mol_name);

            fprintf(surf_mol_header,"\tattribute \"dep\" string \"positions\"\n\n");
	  }else{
             /* output empty arrays for zero molecule counts here */
             fprintf(surf_mol_header,"object %d array   # %s states #\n",surf_mol_main_index, grid_mol_name);
          }
          eff_states[eff_states_index] = surf_mol_main_index;
          eff_states_index++;
          surf_mol_main_index++;
       }
       if(num == 0){
         fprintf(surf_mol_header, "\n");
       }
   } /* end for loop */

 } /* end if(viz_mol_pos_flag || viz_mol_orient_flag) */

/* build fields for grid molecules here */

  if(eff_to_show_number > 0){
      if(fdlp->type == ALL_MOL_DATA) {
           show_effectors = 1;
      }
      else if((viz_mol_pos_flag || viz_mol_orient_flag) && ((special_mol_frames_counter ==0) || (special_mol_frames_counter == 2))){
		show_effectors = 1;
      }
  }
 
  if(show_effectors){

       for(ii = 0; ii < eff_to_show_number; ii++)
       {
             if(eff_names[ii] != NULL){
                fprintf(surf_mol_header,
                    "object %d field   # %s #\n",surf_mol_main_index, eff_names[ii]);
             }
             if(eff_pos[ii] > 0){
             	fprintf(surf_mol_header,
                 "\tcomponent \"positions\" value %d\n",eff_pos[ii]);
             }
             if(eff_orient[ii] > 0){     
             	fprintf(surf_mol_header,
                 	"\tcomponent \"data\" value %d # orientations #\n",eff_orient[ii]);
             }
             if(viz_mol_states_flag)
             {
                if(eff_states[ii] > 0){
             	   fprintf(surf_mol_header,
                	"\tcomponent \"state_values\" value %d\n",eff_states[ii]);
                }
             }
             fprintf(surf_mol_header, "\n");
             eff_field_indices[ii] = surf_mol_main_index;
             surf_mol_main_index++;
      }
  }




/* dump 3D molecules: */

 if(viz_mol_pos_flag || viz_mol_orient_flag){	 
   
        /* create references to the molecules. */
        if ((viz_molp=(struct volume_molecule ***)malloc(n_species*sizeof(struct volume_molecule **)))==NULL) {
                fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
      		return(1);
    	}
    	if ((viz_mol_count=(u_int *)malloc(n_species*sizeof(u_int)))==NULL) {
                fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
      		return(1);
    	}
  
    for (ii=0;ii<n_species;ii++) {
      /* perform initialization */
      spec_id=species_list[ii]->species_id;
      viz_molp[spec_id]=NULL;
      viz_mol_count[spec_id]=0;

      if (species_list[ii]->viz_state == EXCLUDE_OBJ)  continue;

      num=species_list[ii]->population;
      if ((num>0) && (species_list[ii]->flags & NOT_FREE) == 0) {
          /* create references for 3D molecules */
          if ((viz_molp[spec_id]=(struct volume_molecule **)malloc
            (num*sizeof(struct volume_molecule *)))==NULL) {
                fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                return(1);
          }
      }
    } /* end for */

    slp=world->storage_head;
    while (slp!=NULL) {
      sp=slp->store;
      shp=sp->timer;
      while (shp!=NULL) {
        
        for (ii=0;ii<shp->buf_len;ii++) {
          amp=(struct abstract_molecule *)shp->circ_buf_head[ii];
          while (amp!=NULL) {
            if ((amp->properties!=NULL) && (amp->flags & TYPE_3D) == TYPE_3D){
              molp=(struct volume_molecule *)amp;
              if (molp->properties->viz_state!=EXCLUDE_OBJ) {
                spec_id=molp->properties->species_id;
                if (viz_mol_count[spec_id]<molp->properties->population) {
                  viz_molp[spec_id][viz_mol_count[spec_id]++]=molp;
                }
                else {
                  fprintf(log_file,"MCell: molecule count disagreement!!\n");
                  fprintf(log_file,"  Species %s  population = %d  count = %d\n",molp->properties->sym->name,molp->properties->population,viz_mol_count[spec_id]);
                } /* end if/else */
              }  /* end if(molp) */
            } /* end if(amp) */
            amp=amp->next;
          } /* end while  (amp) */
        } /* end for */
        
        amp=(struct abstract_molecule *)shp->current;
        while (amp!=NULL) {
          if ((amp->properties!=NULL) && (amp->flags & TYPE_3D) == TYPE_3D) {
            molp=(struct volume_molecule *)amp;
            if (molp->properties->viz_state!=EXCLUDE_OBJ) {
              spec_id=molp->properties->species_id;
              if (viz_mol_count[spec_id]<molp->properties->population) {
                viz_molp[spec_id][viz_mol_count[spec_id]++]=molp;
              }
              else {
                fprintf(log_file,"MCell: molecule count disagreement!!\n");
                fprintf(log_file,"  Species %s  population = %d  count = %d\n",molp->properties->sym->name,molp->properties->population,viz_mol_count[spec_id]);
              }
            }

           }
          amp=amp->next;
        }
        
        shp=shp->next_scale;
      } /* end (while (shp) */

      slp=slp->next;
    } /* end (while slp) */


    for (ii=0;ii<n_species;ii++) {
      
      if(species_list[ii]->viz_state == EXCLUDE_OBJ) continue; 
      spec_id=species_list[ii]->species_id;
      
      if(viz_mol_count[spec_id] > 0)
      {
         num=viz_mol_count[spec_id];
         if(num!=species_list[ii]->population
             && ((species_list[ii]->flags & NOT_FREE)==0)) {
             fprintf(log_file,"MCell: molecule count disagreement!!\n");
             fprintf(log_file,"  Species %s  population = %d  count = %d\n",species_list[ii]->sym->name,species_list[ii]->population,num);
         }
      }

      if (viz_mol_count[spec_id]>0 && ((species_list[ii]->flags & NOT_FREE) == 0)) {
        /* here are 3D diffusing molecules */
        num = viz_mol_count[spec_id];
       if(viz_mol_pos_flag)
       { 	  
	  vol_mol_pos_byte_offset_prev = vol_mol_pos_byte_offset;
          for (jj=0;jj<num;jj++) {
            molp=viz_molp[spec_id][jj];
	    v1=(float)(world->length_unit*molp->pos.x);
	    v2=(float)(world->length_unit*molp->pos.y);
	    v3=(float)(world->length_unit*molp->pos.z);
	    fwrite(&v1,sizeof v1,1,vol_mol_pos_data);
	    fwrite(&v2,sizeof v2,1,vol_mol_pos_data);
	    fwrite(&v3,sizeof v3,1,vol_mol_pos_data);
            vol_mol_pos_byte_offset += (sizeof(v1) + sizeof(v2) + sizeof(v3));
          }
          fprintf(vol_mol_header,"object %d class array type float rank 1 shape 3 items %d %s binary data file %s,%d  # %s positions #\n",vol_mol_main_index,num,my_byte_order, vol_mol_pos_name, vol_mol_pos_byte_offset_prev, species_list[ii]->sym->name);
          fprintf(vol_mol_header,"\tattribute \"dep\" string \"positions\"\n\n");
          mol_pos[mol_pos_index] = vol_mol_main_index;
          mol_pos_index++;
          vol_mol_main_index++;
        }

        if(viz_mol_orient_flag)
        {  
          /* write molecule orientations information. 
             for 3D molecules we use default orientation [0,0,1] */
      	  vol_mol_orient_byte_offset_prev = vol_mol_orient_byte_offset;
          v1 = 0;
          v2 = 0;
          v3 = 1;
          fwrite(&v1,sizeof v1,1,vol_mol_orient_data);
          fwrite(&v2,sizeof v2,1,vol_mol_orient_data);
          fwrite(&v3,sizeof v3,1,vol_mol_orient_data);
          vol_mol_orient_byte_offset += (sizeof(v1) + sizeof(v2) + sizeof(v3));
          fprintf(vol_mol_header,"object %d class constantarray type float rank 1 shape 3 items %d %s binary data file %s,%d # %s orientations #\n",vol_mol_main_index,num, my_byte_order, vol_mol_orient_name, vol_mol_orient_byte_offset_prev, species_list[ii]->sym->name);
          fprintf(vol_mol_header,"\tattribute \"dep\" string \"positions\"\n\n");
          mol_orient[mol_orient_index] = vol_mol_main_index;
          mol_orient_index++;
          vol_mol_main_index++;
        }

        if(viz_mol_states_flag)
        {
          /* write molecule states information. */ 
      	  vol_mol_states_byte_offset_prev = vol_mol_states_byte_offset;
          fwrite(&state,sizeof state,1,vol_mol_states_data);
          vol_mol_states_byte_offset += (sizeof state);
          fprintf(vol_mol_header,"object %d class constantarray type int items %d %s binary data file %s,%d  # %s states #\n",vol_mol_main_index,num, my_byte_order,vol_mol_states_name,vol_mol_states_byte_offset_prev, species_list[ii]->sym->name);
          fprintf(vol_mol_header,"\tattribute \"dep\" string \"positions\"\n\n");
          mol_states[mol_states_index] = vol_mol_main_index;
          mol_states_index++;
          vol_mol_main_index++;
        }
      }
      /* output empty arrays for zero molecule counts here */
      else if ((viz_mol_count[spec_id]==0) && ((species_list[ii]->flags & NOT_FREE) == 0)) {
        if(viz_mol_pos_flag)
        {
          fprintf(vol_mol_header,"object %d array   # %s positions #\n",vol_mol_main_index, species_list[ii]->sym->name);
          mol_pos[mol_pos_index] = vol_mol_main_index;
          mol_pos_index++;
          vol_mol_main_index++;
        }
        if(viz_mol_orient_flag)
        {
          fprintf(vol_mol_header,"object %d array   # %s orientations #\n",vol_mol_main_index, species_list[ii]->sym->name);
          mol_orient[mol_orient_index] = vol_mol_main_index;
          mol_orient_index++;
          vol_mol_main_index++;
        }
       
       if(viz_mol_states_flag)
       {
          fprintf(vol_mol_header,"object %d array   # %s states #\n",vol_mol_main_index, species_list[ii]->sym->name);
          mol_states[mol_states_index] = vol_mol_main_index;
          mol_states_index++;
          vol_mol_main_index++;
          
       }
	 fprintf(vol_mol_header,"\n");
      } /* end else if */
    }
  } /* end if((viz_mol_pos_flag) || (viz_mol_orient_flag)) */


/* build fields here */
   if(mol_to_show_number > 0)
   {
      if(fdlp->type == ALL_MOL_DATA) {
           show_molecules = 1;
      }
      else if((viz_mol_pos_flag || viz_mol_orient_flag) && ((special_mol_frames_counter ==0) || (special_mol_frames_counter == 2))){
		show_molecules = 1;
      }
   }

    if(show_molecules)
    {
      for (ii=0; ii<mol_to_show_number; ii++) {
               if(mol_names[ii] != NULL){
                  fprintf(vol_mol_header,
                      "object %d field   # %s #\n",vol_mol_main_index, mol_names[ii]);
                }
                if(mol_pos[ii] > 0) {
             	   fprintf(vol_mol_header,
                 	"\tcomponent \"positions\" value %d\n",mol_pos[ii]);
                }
                if(mol_orient[ii] > 0){
             	   fprintf(vol_mol_header,
                 	"\tcomponent \"data\" value %d # orientations #\n",mol_orient[ii]);
                }
             if(viz_mol_states_flag)
             {
                if(mol_states[ii] > 0){ 
             	   fprintf(vol_mol_header,
                	"\tcomponent \"state_values\" value %d\n",mol_states[ii]);
                }
             }
             fprintf(vol_mol_header, "\n");
             mol_field_indices[ii] = vol_mol_main_index;
             vol_mol_main_index++;
      }
    }

   

   /* create group objects for molecules/effectors */

   if(show_molecules)
   {

         fprintf(vol_mol_header,"object %d group # %s #\n", vol_mol_main_index, "volume molecules"); 
         for (ii=0;ii<mol_to_show_number;ii++) {
             if(mol_field_indices[ii] > 0){
          	 fprintf(vol_mol_header,"\tmember \"%s\" value %d\n",mol_names[ii], mol_field_indices[ii]);
             }
         }

        /* reset volume_molecules object and binary data indexing */
        vol_mol_main_index = 1;
        count_vol_mol_header = 0;
        special_mol_frames_counter = 0;

        /* store iteration_number for volume molecules */
      iteration_numbers_vol_mols[iteration_numbers_vol_mols_count] =  viz_iteration;
      iteration_numbers_vol_mols_count++;


        /* put value of viz_iteration into the time_values array */
            if(time_values_count == 0){
               time_values[time_values_count] = viz_iteration;  
               time_values_count++;
               last_time_value_written = viz_iteration;
            }else if(viz_iteration > last_time_value_written){
               time_values[time_values_count] = viz_iteration;  
               time_values_count++;
               last_time_value_written = viz_iteration;
            } 

   }


  
  if(show_effectors)
  {
         fprintf(surf_mol_header,"object %d group # %s #\n",surf_mol_main_index, "surface molecules");
      	 for (ii=0;ii<eff_to_show_number;ii++) {
              if(eff_field_indices[ii] > 0){
                   fprintf(surf_mol_header,"\tmember \"%s\" value %d\n",eff_names[ii], eff_field_indices[ii]);
              }
          }


        /* reset surface molecules object and binary data indexing */
        surf_mol_main_index = 1;
        count_surf_mol_header = 0;
        special_mol_frames_counter = 0;
       

       /* store iteration_number for surface molecules */
    iteration_numbers_surf_mols[iteration_numbers_surf_mols_count] = viz_iteration;
    iteration_numbers_surf_mols_count++;


        /* put value of viz_iteration into the time_values array */
            if(time_values_count == 0){
               time_values[time_values_count] = viz_iteration;  
               time_values_count++;
               last_time_value_written = viz_iteration;
            }else if(viz_iteration > last_time_value_written){
               time_values[time_values_count] = viz_iteration;  
               time_values_count++;
               last_time_value_written = viz_iteration;
            } 

  }
   

  /* check whether it is time to write data into the master_header file */
     struct frame_data_list *fdlp_temp;
     long long next_iteration_step_this_frame = UINT_MAX;  /* next iteration for this frame */
     static long long next_iteration_step_previous_frame = -1;  /* next iteration for previous frame */

     if(fdlp->curr_viz_iteration->next != NULL){
	switch (fdlp->list_type) {
	  case OUTPUT_BY_ITERATION_LIST:
	  	  next_iteration_step_this_frame = (long long)fdlp->curr_viz_iteration->next->value; 
	          break;
	  case OUTPUT_BY_TIME_LIST:
	          next_iteration_step_this_frame = (long long)(fdlp->curr_viz_iteration->next->value/world->time_unit + ROUND_UP);
	          break;
          default:
                  fprintf(world->err_file,"File '%s', Line %ld: error - wrong frame_data_list list_type %d\n", __FILE__, (long)__LINE__, fdlp->list_type);
                  break;
	}
     }
    

     found = 0;
     if(world->chkpt_flag){
        /* check whether it is the last frame */
               
        if((world->it_time == world->iterations) ||
                 (world->it_time == final_iteration))
               
        { 

             /* look forward to find out whether there are 
                other frames to be output */
             for(fdlp_temp = fdlp->next; fdlp_temp != NULL; fdlp_temp = fdlp_temp->next){
                if((fdlp_temp->viz_iterationll == world->iterations) || (fdlp_temp->viz_iterationll == final_iteration)){ 
                   found = 1;
                   break;
                }
             }
             
             if(!found){
                /* this is the last frame */
                    time_to_write_master_header = 1; 
             }
        }
        
       else if(((world->iterations < next_iteration_step_this_frame) && (next_iteration_step_this_frame != UINT_MAX)) || (next_iteration_step_this_frame == UINT_MAX)){
        

          /* look forward to find out whether there are 
             other frames to be output */
        for(fdlp_temp = fdlp->next; fdlp_temp != NULL; fdlp_temp = fdlp_temp->next){
                
                if((fdlp_temp->viz_iterationll >= fdlp->viz_iterationll) &&
                    (fdlp_temp->viz_iterationll <= world->iterations)){
                       found = 1;
                       break;
                }
         }
        
        /* this allows to look backwards */
        if(next_iteration_step_this_frame != UINT_MAX){
           if(next_iteration_step_previous_frame > next_iteration_step_this_frame){
                found = 1;
           }
        }else{
                if((next_iteration_step_previous_frame >= fdlp->viz_iterationll) && (next_iteration_step_previous_frame <= world->iterations)){
                   found = 1;
                }

        }
         
         if(!found){
             /* this is the last frame */
             time_to_write_master_header = 1; 
          }

     }

    /* end if(world->chkpt_flag) */
   } else if(world->it_time == final_iteration){


        /* look forward to find out whether there are 
           other frames to be output */
        for(fdlp_temp = fdlp->next; fdlp_temp != NULL; fdlp_temp = fdlp_temp->next){
          
                if(fdlp_temp->viz_iterationll == final_iteration) {
                   found = 1;
                   break;
                }
         }
         if(!found){
             /* this is the last frame */
             time_to_write_master_header = 1; 
          }
           
    }
    else if((world->iterations < next_iteration_step_this_frame) || (next_iteration_step_this_frame == UINT_MAX)){

        /* look forward to find out whether there are 
           other frames to be output */
        for(fdlp_temp = fdlp->next; fdlp_temp != NULL; fdlp_temp = fdlp_temp->next){
                
                if((fdlp_temp->viz_iterationll >= fdlp->viz_iterationll) &&
                    (fdlp_temp->viz_iterationll <= world->iterations)){
                   found = 1;
                   break;
                }
         }
             
        /* this allows to look backwards */
        if(next_iteration_step_this_frame != UINT_MAX){
           if(next_iteration_step_previous_frame > next_iteration_step_this_frame){
                found = 1;
           }
        }else{
                if((next_iteration_step_previous_frame >= fdlp->viz_iterationll) && (next_iteration_step_previous_frame <= world->iterations)){
                   found = 1;
                }

        }

         if(!found){
             /* this is the last frame */
             time_to_write_master_header = 1; 
          }
           
    } /* end if-else(world->chkpt_flag) */


    /* this allows to look backwards to the previous frames */
    if(next_iteration_step_this_frame != UINT_MAX){
          if(next_iteration_step_this_frame > next_iteration_step_previous_frame){
             next_iteration_step_previous_frame = next_iteration_step_this_frame;         }
     }


     if(time_to_write_master_header)
     {
        double t_value;
        int extra_elems;
        int dummy = -1;
         
     	
        ch_ptr = strrchr(world->file_prefix_name, '/');
        if(ch_ptr != NULL)
        {
     	   ++ch_ptr;
           if(world->chkpt_flag){
              sprintf(chkpt_seq_num, "%d", world->chkpt_seq_num);
              iteration_numbers_name = (char *)malloc(sizeof(char) * (strlen(ch_ptr) + strlen(".iteration_numbers.") + strlen(chkpt_seq_num) + strlen(".bin") + 1));
              if(iteration_numbers_name == NULL){
                 fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                 return (1);
              }              
      	      sprintf(iteration_numbers_name,"%s.iteration_numbers.%s.bin",                                    ch_ptr, chkpt_seq_num);

              time_values_name = (char *)malloc(sizeof(char) * (strlen(ch_ptr) + strlen(".time_values.") + strlen(chkpt_seq_num) + strlen(".bin") + 1));
              if(time_values_name == NULL){
                 fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                 return (1);
              }              

      	      sprintf(time_values_name,"%s.time_values.%s.bin",                                    ch_ptr, chkpt_seq_num);
           }else{
              iteration_numbers_name = (char *)malloc(sizeof(char) * (strlen(ch_ptr) + strlen(".iteration_numbers.bin") + 1));
              if(iteration_numbers_name == NULL){
                 fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                 return (1);
              }              
      	      sprintf(iteration_numbers_name,"%s.iteration_numbers.bin",ch_ptr);
      	      
              
              time_values_name = (char *)malloc(sizeof(char) * (strlen(ch_ptr) + strlen(".time_values.bin") + 1));
              if(time_values_name == NULL){
                 fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                 return (1);
              }              
              sprintf(time_values_name,"%s.time_values.bin",ch_ptr);
           }
        }else{ /* if (ch_ptr == NULL) */
           if(world->chkpt_flag){
              sprintf(chkpt_seq_num, "%d", world->chkpt_seq_num);
              iteration_numbers_name = (char *)malloc(sizeof(char) * (strlen(world->file_prefix_name) + strlen(".iteration_numbers.") + strlen(chkpt_seq_num) + strlen(".bin") + 1));
              if(iteration_numbers_name == NULL){
                 fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                 return (1);
              }              
      	      sprintf(iteration_numbers_name,"%s.iteration_numbers.%s.bin",                                    world->file_prefix_name, chkpt_seq_num);
      	      

              time_values_name = (char *)malloc(sizeof(char) * (strlen(world->file_prefix_name) + strlen(".time_values.") + strlen(chkpt_seq_num) + strlen(".bin") + 1));
              if(time_values_name == NULL){
                 fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                 return (1);
              }              
              sprintf(time_values_name,"%s.time_values.%s.bin",                                    world->file_prefix_name, chkpt_seq_num);
           }else{
              iteration_numbers_name = (char *)malloc(sizeof(char) * (strlen(world->file_prefix_name) + strlen(".iteration_numbers.bin") + 1));
              if(iteration_numbers_name == NULL){
                 fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                 return (1);
              }              
      	      sprintf(iteration_numbers_name,"%s.iteration_numbers.bin",world->file_prefix_name);


              time_values_name = (char *)malloc(sizeof(char) * (strlen(world->file_prefix_name) + strlen(".time_values.bin") + 1));
              if(time_values_name == NULL){
                 fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                 return (1);
              }              
      	      sprintf(time_values_name,"%s.time_values.bin",world->file_prefix_name);
           }


        }

        filename = (char *)malloc(sizeof(char) * (strlen(viz_data_dir_name) +
            strlen("/") + strlen(iteration_numbers_name) + 1));
        if(filename == NULL){
                 fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                 return (1);
        }
        strcpy(filename, viz_data_dir_name);
        strcat(filename, "/");
     	strcat(filename, iteration_numbers_name);

        if ((iteration_numbers_data=fopen(filename,"wb"))==NULL) {
            fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__, iteration_numbers_name);
           return(1);
        }

        filename = (char *)malloc(sizeof(char) * (strlen(viz_data_dir_name) +
            strlen("/") + strlen(time_values_name) + 1));
        if(filename == NULL){
                 fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                 return (1);
        }
        strcpy(filename, viz_data_dir_name);
        strcat(filename, "/");
     	strcat(filename, time_values_name);

        if ((time_values_data=fopen(filename,"wb"))==NULL) {
            fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__, time_values_name);
           return(1);
        }

      if(iteration_numbers_vol_mols_count > iteration_numbers_surf_mols_count){
         iteration_numbers_count = iteration_numbers_vol_mols_count;
      }else{
         iteration_numbers_count = iteration_numbers_surf_mols_count;
      }
      if(iteration_numbers_meshes_count > iteration_numbers_count){
         iteration_numbers_count = iteration_numbers_meshes_count;
      }
      
        if(iteration_numbers_meshes_count > 0)
        {
           extra_elems = iteration_numbers_count - iteration_numbers_meshes_count;
           if(extra_elems > 0){
              /* pad the iteration_numbers_meshes array with the last 
                 element so that it will have the same number of elements 
                 as the maximum length array */
                 elem = iteration_numbers_meshes[iteration_numbers_meshes_count - 1];
              
		for(ii = 0; ii < extra_elems; ii++){
                   iteration_numbers_meshes[iteration_numbers_meshes_count + ii] =  elem;
                }
            }
            iteration_numbers_meshes_count += extra_elems;
        }

        if(iteration_numbers_vol_mols_count > 0)
        {
           extra_elems = iteration_numbers_count - iteration_numbers_vol_mols_count;
           if(extra_elems > 0){
              /* pad the iteration_numbers_vol_mols array with the last 
                 element so that it will have the same number of elements 
                 as the maximum length array */
              elem = iteration_numbers_vol_mols[iteration_numbers_vol_mols_count - 1];
              
		for(ii = 0; ii < extra_elems; ii++){
                   iteration_numbers_vol_mols[iteration_numbers_vol_mols_count + ii] = elem;
                }
           }
           iteration_numbers_vol_mols_count += extra_elems;
        }

        if(iteration_numbers_surf_mols_count > 0)
        {
           extra_elems = iteration_numbers_count - iteration_numbers_surf_mols_count;
           if(extra_elems > 0){
              /* pad the iteration_numbers_surf_mols array with the last 
                 element so that it will have the same number of elements 
                 as the maximum length array */
              elem = iteration_numbers_surf_mols[iteration_numbers_surf_mols_count - 1];
              
		for(ii = 0; ii < extra_elems; ii++){
                   iteration_numbers_surf_mols[iteration_numbers_surf_mols_count + ii] = elem;
                }
           }
           iteration_numbers_surf_mols_count += extra_elems;
        }

     /* Open master header file. */
     if(world->chkpt_flag){
       sprintf(chkpt_seq_num, "%d", world->chkpt_seq_num);
       filename = (char *)malloc(sizeof(char) * (strlen(world->file_prefix_name) + strlen(".") + strlen(chkpt_seq_num) + strlen (".") + 1));
       if(filename == NULL){
            fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
            return (1);
       }
        sprintf(filename,"%s.%u.dx",world->file_prefix_name, world->chkpt_seq_num);
     }else{
           filename = (char *)malloc(sizeof(char) * (strlen(world->file_prefix_name) + strlen(".dx") + 1));
           if(filename == NULL){
                fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                return (1);
            }

            sprintf(filename,"%s.dx",world->file_prefix_name);
     }

     ch_ptr = strrchr(filename, '/');
     if(ch_ptr != NULL)
     {
        ++ch_ptr;
        master_header_name = (char *)malloc(sizeof(char) *(strlen(viz_data_dir_name) + strlen("/") + strlen(ch_ptr) + 1));
        if(master_header_name == NULL){
                fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                return (1);
        }
        sprintf(master_header_name, "%s%s%s", viz_data_dir_name, "/", ch_ptr);
     }else{
        master_header_name = (char *)malloc(sizeof(char) *(strlen(viz_data_dir_name) + strlen("/") + strlen(filename) + 1));
        if(master_header_name == NULL){
                fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                return (1);
        }
        sprintf(master_header_name, "%s%s%s", viz_data_dir_name, "/", filename);

     }

      if ((master_header=fopen(master_header_name,"w"))==NULL) {
           fprintf(world->err_file, "File %s, Line %ld: cannot open master header file %s.\n", __FILE__, (long)__LINE__,master_header_name);
           return(1);
      }

	/* write 'iteration_numbers' object. */
      int dreamm3mode_number = 0;
      char dreamm3mode[1024];
      if(world->viz_mode == DREAMM_V3_MODE){
         dreamm3mode_number = 1;
         sprintf(dreamm3mode, "DREAMM_V3_MODE");
      }else if(world->viz_mode == DREAMM_V3_GROUPED_MODE){
         dreamm3mode_number = 2;
         sprintf(dreamm3mode, "DREAMM_V3_GROUPED_MODE");
      }      


      fprintf(master_header,"object \"iteration_numbers\" class array  type unsigned int rank 1 shape 3 items %u %s binary data file %s,%d\n",iteration_numbers_count, my_byte_order,iteration_numbers_name, iteration_numbers_byte_offset);
       fprintf(master_header,"\tattribute \"dreamm3mode\" number %d\t#%s#\n", dreamm3mode_number, dreamm3mode);
      
       for(ii = 0; ii < iteration_numbers_count; ii++){
                elem = iteration_numbers_meshes[ii];
                if(elem == UINT_MAX) 
                {
                   fwrite(&dummy, sizeof(dummy),1,iteration_numbers_data);
                }else{
                   fwrite(&elem, sizeof(elem),1,iteration_numbers_data);
                }

                elem = iteration_numbers_vol_mols[ii];
                if(elem == UINT_MAX) 
                {
                   fwrite(&dummy, sizeof(dummy),1,iteration_numbers_data);
                }else{
                   fwrite(&elem, sizeof(elem),1,iteration_numbers_data);
                }

                elem = iteration_numbers_surf_mols[ii];
                if(elem == UINT_MAX) 
                {
                   fwrite(&dummy, sizeof(dummy),1,iteration_numbers_data);
                }else{
                   fwrite(&elem, sizeof(elem),1,iteration_numbers_data);
                }

        }
     	fprintf(master_header, "\n\n");



        /* write "time_values" object. */
        if(time_values_count > 0)
        {
        	fprintf(master_header,"object \"time_values\" class array  type double rank 0 items %u %s binary data file %s,%d\n",time_values_count, my_byte_order,time_values_name, time_values_byte_offset);
                fprintf(master_header,"\tattribute \"dreamm3mode\" number %d\t#%s#\n", dreamm3mode_number, dreamm3mode);
												for(ii = 0; ii < time_values_count; ii++){
                	elem = time_values[ii];
                         {
                           t_value = elem*world->time_unit;
                           fwrite(&(t_value), sizeof(t_value),1,time_values_data);
                        }
                 }
		fprintf(master_header, "\n\n");
	}
     

       /* check here if MESHES or MOLECULES blocks
        are not supplied the corresponding files
        should be empty in order to prevent unintentional mixing of
        pre-existing and new files */

        if(obj_to_show_number == 0)
        {
           fprintf(world->log_file, "MESHES keyword is absent or commented.\nEmpty 'meshes' output files are created.\n");
        }
        if((mol_to_show_number == 0) && (eff_to_show_number == 0))
        {
           fprintf(world->log_file, "MOLECULES keyword is absent or commented.\nEmpty 'molecules' output files are created.\n");
        }


   } /* end if(time_to_write_master_header) */


   /* 
      Now check whether there is a need to create symlinks
      to the meshes/molecules files saved previously
      in the previous frame directories. E.g. if this frame
      is MOLS type check whether there are frames ahead for the same
      "viz_iteration" but MESHES type and if there are no such -
      create link in this "frame_#" folder to the last existing
      "meshes" files.  Do the similar if this frame is of MESHES type.
      In fact the links are created only when e.g. the current frame is the 
      last one of MOLS type and there are no MESHES type frames for this
      iteration.
  
   */


    int mesh_frame_found = 0;
    int mol_frame_found = 0;
    
    /* check whether this frame type is the last one for the current iteration */
    for(fdlp_temp = fdlp->next; fdlp_temp != NULL; fdlp_temp = fdlp_temp->next){
           if(fdlp_temp->viz_iterationll != fdlp->viz_iterationll){
               break;
           }else{
               if((fdlp_temp->type == ALL_MOL_DATA) ||
                  (fdlp_temp->type == MOL_POS) ||
                  (fdlp_temp->type == MOL_ORIENT))
               {
		   mol_frame_found = 1;
               }else if((fdlp_temp->type == ALL_MESH_DATA) ||
                  (fdlp_temp->type == MESH_GEOMETRY) ||
                  (fdlp_temp->type == REG_DATA))
               {
		   mesh_frame_found = 1;
               }    
           }
    }


    if(viz_surf_all_data_flag || viz_surf_pos_flag || viz_region_data_flag)
    {
       
        if((!mesh_frame_found) && (!mol_frame_found) && (last_mols_iteration >= 0) && (fdlp->viz_iterationll > last_mols_iteration))
        {
           sprintf(iteration_number, "%lld", fdlp->viz_iterationll);
           filename = (char *)malloc(sizeof(char) * (strlen(frame_data_dir_name) + strlen("/iteration_") + strlen(iteration_number) + 1));
           if(filename == NULL){
                fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                return (1);
           }
            sprintf(filename, "%s%s%s", frame_data_dir_name, "/iteration_", iteration_number); 
            
            chdir(filename);
           /* count the depth of the directory structure */
           ii = 0;
           viz_dir_depth = 0;
           while(filename[ii] != '\0') 
           {
        	if(filename[ii] == '/'){
           	   viz_dir_depth++;
                }
                ii++;
           }
          
           if(filename[0] != '.'){
               viz_dir_depth++;
           } 
            sprintf(buf, "../");
            for(ii = 0; ii < viz_dir_depth - 1; ii++)
            {
              strcat(buf, "../");
            }

  
            sprintf(path_name_1,"%s%lld%s", "../iteration_", last_mols_iteration, "/surface_molecules.dx");    
            status = stat(path_name_1, &f_stat);
            if(status == 0){
    	       sprintf(path_name_2,  "./surface_molecules.dx");   
               if (((status = symlink(path_name_1, path_name_2)) == -1) && 
                    (errno != EEXIST)) 
               { 
                   fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
                   chdir(buf); 
                   return(1);
               }else if (((status = symlink(path_name_1, path_name_2)) == -1) &&  (errno == EEXIST)) 
               {
                   /* remove the symbolic link */
                   status = unlink(path_name_2);
                   if(status != 0) { 
                       fprintf(world->err_file, "File %s, Line %ld: error %d removing symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_2);
                       chdir(buf); 
                       return(1);
                   }
                   /* create a new symbolic link */
                   if ((status = symlink(path_name_1, path_name_2)) == -1){
                      fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
                       chdir(buf); 
                      return(1);
                   }

                }  /* end else if */
            }else{  /* end if(status = stat()) */
                      fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
            }

            sprintf(path_name_1,"%s%lld%s", "../iteration_", last_mols_iteration, "/surface_molecules_orientations.bin");    
            status = stat(path_name_1, &f_stat);
            if(status == 0){
    	         sprintf(path_name_2,  "./surface_molecules_orientations.bin");    
               if (((status = symlink(path_name_1, path_name_2)) == -1) && 
                    (errno != EEXIST)) 
               { 
                   fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
                   chdir(buf); 
                   return(1);
               }else if (((status = symlink(path_name_1, path_name_2)) == -1) &&  (errno == EEXIST)) 
               {
                   /* remove the symbolic link */
                   status = unlink(path_name_2);
                   if(status != 0) { 
                       fprintf(world->err_file, "File %s, Line %ld: error %d removing symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_2);
                       chdir(buf); 
                       return(1);
                   
                   }
                   /* create a new symbolic link */
                   if ((status = symlink(path_name_1, path_name_2)) == -1){
                      fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
                       chdir(buf); 
                      return(1);
                   }

                }  /* end else if */
            
            }else{  /* end if(status = stat()) */
                      fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
            }
	   
            sprintf(path_name_1,"%s%lld%s", "../iteration_", last_mols_iteration, "/surface_molecules_positions.bin");    
            status = stat(path_name_1, &f_stat);
            if(status == 0){
    	       sprintf(path_name_2,  "./surface_molecules_positions.bin");    
               if (((status = symlink(path_name_1, path_name_2)) == -1) && 
                    (errno != EEXIST)) 
               { 
                   fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
                   chdir(buf); 
                   return(1);
               }else if (((status = symlink(path_name_1, path_name_2)) == -1) &&  (errno == EEXIST)) 
               {
                   /* remove the symbolic link */
                   status = unlink(path_name_2);
                   if(status != 0) { 
                       fprintf(world->err_file, "File %s, Line %ld: error %d removing symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_2);
                       chdir(buf); 
                       return(1);
                   
                   }
                   /* create a new symbolic link */
                   if ((status = symlink(path_name_1, path_name_2)) == -1){
                      fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
                       chdir(buf); 
                       return(1); 
                   }

                }  /* end else if */

            }else{  /* end if(status = stat()) */
                      fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
            }
 
            sprintf(path_name_1,"%s%lld%s", "../iteration_", last_mols_iteration, "/surface_molecules_states.bin");    
            status = stat(path_name_1, &f_stat);
            if(status == 0){
    	       sprintf(path_name_2,  "./surface_molecules_states.bin");  
               if (((status = symlink(path_name_1, path_name_2)) == -1) && 
                    (errno != EEXIST)) 
               { 
                   fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
                   chdir(buf); 
                   return(1);
               }else if (((status = symlink(path_name_1, path_name_2)) == -1) &&  (errno == EEXIST)) 
               {
                   /* remove the symbolic link */
                   status = unlink(path_name_2);
                   if(status != 0) { 
                       fprintf(world->err_file, "File %s, Line %ld: error %d removing symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_2);
                       chdir(buf); 
                       return(1);
                   
                   }
                   /* create a new symbolic link */
                   if ((status = symlink(path_name_1, path_name_2)) == -1){
                      fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
                       chdir(buf); 
                      return(1);
                   }

                }  /* end else if */

            }else{  /* end if(status = stat()) */
                      fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
            }
            
            sprintf(path_name_1,"%s%lld%s", "../iteration_", last_mols_iteration, "/volume_molecules.dx");    
            status = stat(path_name_1, &f_stat);
            if(status == 0){
    	       sprintf(path_name_2,  "./volume_molecules.dx");  
               if (((status = symlink(path_name_1, path_name_2)) == -1) && 
                    (errno != EEXIST)) 
               { 
                   fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
                   chdir(buf); 
                   return(1);
               }else if (((status = symlink(path_name_1, path_name_2)) == -1) &&  (errno == EEXIST)) 
               {
                   /* remove the symbolic link */
                   status = unlink(path_name_2);
                   if(status != 0) { 
                       fprintf(world->err_file, "File %s, Line %ld: error %d removing symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_2);
                       chdir(buf); 
                       return(1);
                   
                   }
                   /* create a new symbolic link */
                   if ((status = symlink(path_name_1, path_name_2)) == -1){
                      fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
                       chdir(buf); 
                      return(1);
                   }

                }  /* end else if */
                       
            }else{  /* end if(status = stat()) */
                      fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
            }
            
            sprintf(path_name_1,"%s%lld%s", "../iteration_", last_mols_iteration, "/volume_molecules_orientations.bin");    
            status = stat(path_name_1, &f_stat);
            if(status == 0){
    	       sprintf(path_name_2,  "./volume_molecules_orientations.bin");    
               if (((status = symlink(path_name_1, path_name_2)) == -1) && 
                    (errno != EEXIST)) 
               { 
                   fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
                   chdir(buf); 
                   return(1);
               }else if (((status = symlink(path_name_1, path_name_2)) == -1) &&  (errno == EEXIST)) 
               {
                   /* remove the symbolic link */
                   status = unlink(path_name_2);
                   if(status != 0) { 
                       fprintf(world->err_file, "File %s, Line %ld: error %d removing symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_2);
                       chdir(buf); 
                       return(1);
                   
                   }
                   /* create a new symbolic link */
                   if ((status = symlink(path_name_1, path_name_2)) == -1){
                      fprintf(world->err_file, "File %s, Line %ld: error %d creating file link to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
                       chdir(buf); 
                      return(1);
                   }

                }  /* end else if */

            }else{  /* end if(status = stat()) */
                      fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
            }

            sprintf(path_name_1,"%s%lld%s", "../iteration_", last_mols_iteration, "/volume_molecules_positions.bin");    
            status = stat(path_name_1, &f_stat);
            if(status == 0){
    	       sprintf(path_name_2,  "./volume_molecules_positions.bin");   
               if (((status = symlink(path_name_1, path_name_2)) == -1) && 
                    (errno != EEXIST)) 
               { 
                   fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
                   chdir(buf); 
                   return(1);
               }else if (((status = symlink(path_name_1, path_name_2)) == -1) &&  (errno == EEXIST)) 
               {
                   /* remove the symbolic link */
                   status = unlink(path_name_2);
                   if(status != 0) { 
                       fprintf(world->err_file, "File %s, Line %ld: error %d removing symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_2);
                       chdir(buf); 
                       return(1);
                   
                   }
                   /* create a new symbolic link */
                   if ((status = symlink(path_name_1, path_name_2)) == -1){
                      fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
                       chdir(buf); 
                      return(1);
                   }

                }  /* end else if */

            }else{  /* end if(status = stat()) */
                      fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
            }

            sprintf(path_name_1,"%s%lld%s", "../iteration_", last_mols_iteration, "/volume_molecules_states.bin");    
            status = stat(path_name_1, &f_stat);
            if(status == 0){
    	       sprintf(path_name_2,  "./volume_molecules_states.bin");   
               if (((status = symlink(path_name_1, path_name_2)) == -1) && 
                    (errno != EEXIST)) 
               { 
                   fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
                   chdir(buf); 
                   return(1);
               }else if (((status = symlink(path_name_1, path_name_2)) == -1) &&  (errno == EEXIST)) 
               {
                   /* remove the symbolic link */
                   status = unlink(path_name_2);
                   if(status != 0) { 
                       fprintf(world->err_file, "File %s, Line %ld: error %d removing symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_2);
                       chdir(buf); 
                       return(1);
                   
                   }
                   /* create a new symbolic link */
                   if ((status = symlink(path_name_1, path_name_2)) == -1){
                      fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
                       chdir(buf); 
                      return(1);
                   }

                }  /* end else if */

            }else{  /* end if(status = stat()) */
                      fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
            }
              
            chdir(buf);
        }

    }
    else if(viz_mol_all_data_flag || viz_mol_pos_flag || viz_mol_orient_flag)
    {
       
        if((!mesh_frame_found) && (!mol_frame_found) && (last_meshes_iteration >= 0) && (fdlp->viz_iterationll > last_meshes_iteration))
        {
           sprintf(iteration_number, "%lld", fdlp->viz_iterationll);
           filename = (char *)malloc(sizeof(char) * (strlen(frame_data_dir_name) + strlen("/iteration_") + strlen(iteration_number) + 1));
           if(filename == NULL){
                fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                return (1);
           }
            sprintf(filename, "%s%s%s", frame_data_dir_name, "/iteration_", iteration_number); 
 
            chdir(filename);
           /* count the depth of the directory structure */
           ii = 0;
           viz_dir_depth = 0;
           while(filename[ii] != '\0') 
           {
        	if(filename[ii] == '/'){
           	   viz_dir_depth++;
                }
                ii++;
           }
         
 
           if(filename[0] != '.'){
               viz_dir_depth++;
           } 
            sprintf(buf, "../");
            for(ii = 0; ii < viz_dir_depth - 1; ii++)
            {
              strcat(buf, "../");
            }
           

            sprintf(path_name_1,"%s%lld%s", "../iteration_", last_meshes_iteration, "/meshes.dx");    
            status = stat(path_name_1, &f_stat);
            if(status == 0){
    	       sprintf(path_name_2,  "./meshes.dx");   
               if (((status = symlink(path_name_1, path_name_2)) == -1) && 
                    (errno != EEXIST)) 
               { 
                   fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
                   chdir(buf); 
                   return(1);
               }else if (((status = symlink(path_name_1, path_name_2)) == -1) &&  (errno == EEXIST)) 
               {
                   /* remove the symbolic link */
                   status = unlink(path_name_2);
                   if(status != 0) { 
                       fprintf(world->err_file, "File %s, Line %ld: error %d removing symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_2);
                       chdir(buf); 
                       return(1);
                   
                   }
                   /* create a new symbolic link */
                   if ((status = symlink(path_name_1, path_name_2)) == -1){
                      fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
                       chdir(buf); 
                      return(1);
                   }

                }  /* end else if */

            }else{  /* end if(status = stat()) */
                      fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
            }

            sprintf(path_name_1,"%s%lld%s", "../iteration_", last_meshes_iteration, "/mesh_positions.bin");    
            status = stat(path_name_1, &f_stat);
            if(status == 0){
    	       sprintf(path_name_2,  "./mesh_positions.bin");    
               if (((status = symlink(path_name_1, path_name_2)) == -1) && 
                    (errno != EEXIST)) 
               { 
                   fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
                   chdir(buf); 
                   return(1);
               }else if (((status = symlink(path_name_1, path_name_2)) == -1) &&  (errno == EEXIST)) 
               {
                   /* remove the symbolic link */
                   status = unlink(path_name_2);
                   if(status != 0) { 
                       fprintf(world->err_file, "File %s, Line %ld: error %d removing symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_2);
                       chdir(buf); 
                       return(1);
                   
                   }
                   /* create a new symbolic link */
                   if ((status = symlink(path_name_1, path_name_2)) == -1){
                      fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
                       chdir(buf); 
                      return(1);
                   }

                }  /* end else if */

            }else{  /* end if(status = stat()) */
                      fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
            }

            sprintf(path_name_1,"%s%lld%s", "../iteration_", last_meshes_iteration, "/mesh_states.bin");    
            status = stat(path_name_1, &f_stat);
            if(status == 0){
    	       sprintf(path_name_2,  "./mesh_states.bin");   
               if (((status = symlink(path_name_1, path_name_2)) == -1) && 
                    (errno != EEXIST)) 
               { 
                   fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
                   chdir(buf); 
                   return(1);
               }else if (((status = symlink(path_name_1, path_name_2)) == -1) &&  (errno == EEXIST)) 
               {
                   /* remove the symbolic link */
                   status = unlink(path_name_2);
                   if(status != 0) { 
                       fprintf(world->err_file, "File %s, Line %ld: error %d removing symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_2);
                       chdir(buf); 
                       return(1);
                   
                   }
                   /* create a new symbolic link */
                   if ((status = symlink(path_name_1, path_name_2)) == -1){
                      fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
                       chdir(buf); 
                      return(1);
                   }

                }  /* end else if */

            }else{  /* end if(status = stat()) */
                      fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
            }
            
            sprintf(path_name_1,"%s%lld%s", "../iteration_", last_meshes_iteration, "/region_indices.bin");    
            status = stat(path_name_1, &f_stat);
            if(status == 0){
    	       sprintf(path_name_2,  "./region_indices.bin");  
               if (((status = symlink(path_name_1, path_name_2)) == -1) && 
                    (errno != EEXIST)) 
               { 
                   fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
                   chdir(buf); 
                   return(1);
               }else if (((status = symlink(path_name_1, path_name_2)) == -1) &&  (errno == EEXIST)) 
               {
                   /* remove the symbolic link */
                   status = unlink(path_name_2);
                   if(status != 0) { 
                       fprintf(world->err_file, "File %s, Line %ld: error %d removing symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_2);
                       chdir(buf); 
                       return(1);
                   
                   }
                   /* create a new symbolic link */
                   if ((status = symlink(path_name_1, path_name_2)) == -1){
                      fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
                       chdir(buf); 
                       return(1);  
                   }

                }  /* end else if */

            }else{  /* end if(status = stat()) */
                      fprintf(world->err_file, "File %s, Line %ld: error %d creating symlink to the file %s.\n", __FILE__, (long)__LINE__, errno, path_name_1);
            }
             
            chdir(buf);

        } /* end if(!mesh_frame_found) ...  */
    }  /* end else if */


    /* free allocated memory */
    if(viz_grid_mol_count != NULL){
      free(viz_grid_mol_count);
      viz_grid_mol_count = NULL;
    }
    if(viz_mol_count != NULL){
       free(viz_mol_count);
       viz_mol_count = NULL;
    }
    if (viz_molp != NULL) {
      for (ii=0;ii<n_species;ii++) {
         if (viz_molp[ii]!=NULL) {
           free(viz_molp[ii]);
           viz_molp[ii] = NULL;
         }
      }
    
      free(viz_molp);
      viz_molp = NULL;
    }
   

    /* close opened files */
    if(iteration_numbers_data != NULL){
        fclose(iteration_numbers_data);
    }
    if(time_values_data != NULL){
        fclose(time_values_data);
    }
    if(master_header != NULL){
        fclose(master_header);
    }
    if(meshes_header != NULL){
      fclose(meshes_header);
    }
    if(mesh_pos_data != NULL){
      fclose(mesh_pos_data);
    }
    if(mesh_states_data != NULL){
      fclose(mesh_states_data);
    }
    if(region_data != NULL){
      fclose(region_data);
    }
    if(vol_mol_header != NULL){
      fclose(vol_mol_header);
    }
    if(vol_mol_pos_data != NULL){
      fclose(vol_mol_pos_data);
    }
    if(vol_mol_states_data != NULL){
      fclose(vol_mol_states_data);
    }
    if(vol_mol_orient_data != NULL){
      fclose(vol_mol_orient_data);
    }
    if(surf_mol_header != NULL){
      fclose(surf_mol_header);
    }
    if(surf_mol_pos_data != NULL){
      fclose(surf_mol_pos_data);
    }
    if(surf_mol_states_data != NULL){
      fclose(surf_mol_states_data);
    }
    if(surf_mol_orient_data != NULL){
      fclose(surf_mol_orient_data);
    }

    if(mesh_pos_file_path != NULL) free(mesh_pos_file_path);
    mesh_pos_file_path = NULL;
    if(mesh_states_file_path != NULL) free(mesh_states_file_path);
    mesh_states_file_path = NULL;
    if(region_viz_data_file_path != NULL) free(region_viz_data_file_path);
    region_viz_data_file_path = NULL;
    if(meshes_header_file_path != NULL) free(meshes_header_file_path);
    meshes_header_file_path = NULL;
    if(vol_mol_pos_file_path != NULL) free(vol_mol_pos_file_path);
    vol_mol_pos_file_path = NULL;
    if(surf_mol_pos_file_path != NULL) free(surf_mol_pos_file_path);
    surf_mol_pos_file_path = NULL;
    if(vol_mol_orient_file_path != NULL) free(vol_mol_orient_file_path);
    vol_mol_orient_file_path = NULL;
    if(surf_mol_orient_file_path != NULL) free(surf_mol_orient_file_path);
    surf_mol_orient_file_path = NULL;
    if(vol_mol_states_file_path != NULL) free(vol_mol_states_file_path);
    vol_mol_states_file_path = NULL;
    if(vol_mol_header_file_path != NULL) free(vol_mol_header_file_path);
    vol_mol_header_file_path = NULL;
    if(surf_mol_header_file_path != NULL) free(surf_mol_header_file_path);
    surf_mol_header_file_path = NULL;

   return 0;
}
/*************************************************************************
output_dreamm_objects_grouped:
	In: struct frame_data_list *fdlp
	Out: 0 on success, 1 on error; output visualization files (*.dx)
             in dreamm  group format are written.
**************************************************************************/

int output_dreamm_objects_grouped(struct frame_data_list *fdlp)
{
  FILE *log_file = NULL;
  FILE *master_header = NULL;
  FILE *mesh_pos_data = NULL;  /* data file for wall vertices */
  FILE *mesh_states_data = NULL; /* data file for wall states */
  FILE *region_data = NULL; /* data file for region's data */
  FILE *mol_pos_data = NULL;	/* data file for molecule positions */
  FILE *mol_states_data = NULL; /* data file for molecule states */
  FILE *mol_orient_data = NULL; /* data file for molecule orientations */
  FILE *iteration_numbers_data = NULL; /* data file for iteration numbers */
  FILE *time_values_data = NULL; /* data file for time_values */
  struct viz_obj *vizp = NULL;
  struct viz_child *vcp = NULL;
  struct surface_grid *sg = NULL;
  struct wall *w = NULL, **wp = NULL;
  struct species **species_list = NULL;
  struct species *specp = NULL;
  struct object *objp = NULL;
  struct polygon_object *pop = NULL;
  struct ordered_poly *opp = NULL;
  struct element_data *edp = NULL;
  struct vector3 p0;
  struct storage_list *slp = NULL;
  struct storage *sp = NULL;
  struct schedule_helper *shp = NULL;
  struct abstract_molecule *amp = NULL;
  struct grid_molecule *gmol = NULL;
  struct volume_molecule *molp = NULL, ***viz_molp = NULL;       /* for 3D molecules */
  struct region *rp = NULL;
  struct region_list *rlp = NULL;
  float v1,v2,v3;
  u_int spec_id = 0, *viz_mol_count = NULL, *viz_grid_mol_count = NULL;
  static u_int main_index = 1;
  static u_int series_index = 0;
  int ii,jj;
  int vi1,vi2,vi3;
  u_int num;
  u_int viz_iteration;
  u_int n_viz_iterations;
  int element_data_count;
  int state;
  int viz_type;
  unsigned int index; 
  /* indices used in arrays "u_int *surf_pos", etc. */
  int surf_pos_index = 0; 
  int surf_con_index = 0;
  int surf_states_index = 0;
  int surf_region_values_index = 0;
  int surf_obj_region_values_index = 0;
  int eff_states_index = 0;
  int eff_pos_index = 0;
  int eff_orient_index = 0;
  int mol_states_index = 0;
  int mol_pos_index = 0;
  int mol_orient_index = 0;
  int word;
  static int viz_dir_depth = 0; /* points to the depth of viz_output 
                            directory structure */
  byte *word_p = NULL;
  byte viz_mol_pos_flag = 0, viz_mol_states_flag = 0;	/* flags */
  byte viz_mol_orient_flag = 0, viz_region_data_flag = 0;   /* flags */
  byte viz_mol_all_data_flag = 0, viz_surf_all_data_flag = 0;	/* flags */
  byte viz_surf_pos_flag = 0, viz_surf_states_flag = 0;	/* flags */
  char *filename = NULL;
  char chkpt_seq_num[1024];     /* holds checkpoint sequence number */
  char *ch_ptr = NULL; /* pointer used to extract data file name */
  static char *master_header_file_path = NULL;
  char *grid_mol_name = NULL; /* points to the name of the grid molecule */
  static char *mesh_pos_file_path = NULL; /* path to the meshes vertices
                                            data file */
  static char *mesh_pos_name = NULL; /* meshes vertices data file name */
  static char *mesh_states_file_path = NULL; /* path to the meshes states
                                            data file */
  static char *mesh_states_name = NULL; /* meshes states data file name */
  
  static char *region_viz_data_file_path = NULL; /* path to the region viz
                                            data file */
  static char *region_viz_data_name = NULL; /* region_viz_data file name */
  static char *mol_pos_file_path = NULL; /* path to the molecule positions
                                            data file */
  static char *mol_pos_name = NULL; /* molecule positions data file name */
  static char *mol_states_file_path = NULL; /* path to the molecule states
                                            data file */
  static char *mol_states_name = NULL; /* molecule states data file name */
  static char *mol_orient_file_path = NULL; /* path to the molecule orientations
                                            data file */
  static char *mol_orient_name = NULL;/* molecule orientations data file name */
  char *iteration_numbers_file_path = NULL; /* path to iteration numbers data file */ 
  char *iteration_numbers_name = NULL; /* iteration numbers data file name */ 
  char *time_values_file_path = NULL; /* path to time values data file */
  char *time_values_name = NULL; /* time values data file name */
  char *buf;       /* used to write 'frame_data' object information */
  /* used to write combined group information */
  static u_int member_meshes_iteration = UINT_MAX;
  static u_int member_molecules_iteration = UINT_MAX;
  static u_int member_effectors_iteration = UINT_MAX;
  /* linked list that stores data for the 'frame_data' object */
   static char **frame_data_series_list = NULL;
  static u_int frame_data_series_count = 0; /* count elements in frame_data_series_list array.*/
  /* arrays that stores data for the 'iteration_numbers' object */
  static u_int *iteration_numbers_meshes = NULL;
  static u_int *iteration_numbers_vol_mols = NULL;
  static u_int *iteration_numbers_surf_mols = NULL;

  static u_int iteration_numbers_meshes_count = 0; /* count elements in 
                                           iteration_numbers_meshes array  */
  static u_int iteration_numbers_vol_mols_count = 0; /* count elements in 
                                           iteration_numbers_vol_mols array  */
  static u_int iteration_numbers_surf_mols_count = 0; /* count elements in 
                                           iteration_numbers_surf_mols array  */
  static u_int time_values_count = 0; /* count elements in 
                                           time_values array  */
  /* keeps track of the last value written into 'time_values' array */
  static u_int last_time_value_written = 0;
  char my_byte_order[8];  /* shows binary ordering ('lsb' or 'msb') */
  static int mesh_pos_byte_offset = 0;  /* defines position of the object data
                                  in the mesh positions binary data file */
  static int mesh_states_byte_offset = 0; /* defines position of the object data
                                    in the mesh states binary file */
  static int region_data_byte_offset = 0; /* defines position of the 
                            object data in the region_data binary file */
  static int region_data_byte_offset_prev = 0; /* defines position of the 
                            object data in the region_data binary file */
  static int mol_pos_byte_offset = 0; /*defines position of the object data 
                           in the molecule positions binary file */
  static int mol_orient_byte_offset = 0; /*defines position of the object data 
                           in the molecule orientations binary file */
  static int mol_states_byte_offset = 0; /* defines position of the object data
                              in the molecule states binary file. */
  static int iteration_numbers_byte_offset = 0; /* defines position of the 
                 iteration numbers data in the iteration_numbers binary file */
  static int time_values_byte_offset = 0; /* defines position of the time 
                          values data in the time_values binary file */
  int mol_pos_byte_offset_prev = 0; /* used when defining position  
                           in the molecule positions binary file */
  int mol_orient_byte_offset_prev = 0; /* used when defining position                               in the molecule orientations binary file */
  int mol_states_byte_offset_prev = 0; /* used when defining position                                  in the molecule states binary file. */
  /* placeholders for the members of the combined group */
  static long long mesh_group_index = 0;
  static long long mol_group_index = 0;
  static long long eff_group_index = 0;
  int combined_group_index = 0;

  /* counts number of times master_header file was opened.*/
  static int count_master_header = 0;
  /* counts number of times mol_pos_data file was opened.*/
  static int count_mol_pos_data = 0;
  /* counts number of times mol_orient_data file was opened. */
  static int count_mol_orient_data = 0;
  /* counts number of times mol_states_data file was opened. */
  static int count_mol_states_data = 0;
  /* counts number of times mesh_pos_data file was opened.*/
  static int count_mesh_pos_data = 0;
  /* counts number of times mesh_states_data file was opened. */
  static int count_mesh_states_data = 0;
  /* counts number of times region_values_data file was opened. */
  static int count_region_data = 0;

  /* points to the values of the current iteration steps
     for certain frame types. */
  static long long curr_surf_pos_iteration_step = -1;
  static long long curr_region_data_iteration_step = -1;
  static long long curr_mol_pos_iteration_step = -1;
  static long long curr_mol_orient_iteration_step = -1;

  /* points to the special case in iteration steps
     when both values for GEOMETRY and REGION_VIZ_VALUES
     are equal.  E.g. for the cases when GEOMETRY = [0,200], 
     REGION_VIZ_VALUES = [0,100, 200,300] 
     special_surf_iteration_step = [0,200].Same for (MOL_POS,MOL_ORIENT). 
  */
   
  static long long special_surf_iteration_step = -1;
   
  static long long special_mol_iteration_step = -1;
  
  /* counts number of this function executions
     for special_surf_iteration_step/special_mol_iteration_step cases. */
  
  static int special_surf_frames_counter = 0;
  
  static int special_mol_frames_counter = 0;

  struct frame_data_list * fdl_ptr; 
  u_int n_species = world->n_species;
  species_list=world->species_list;

  log_file=world->log_file;
  no_printf("Viz output in DREAMM_V3_GROUPED mode...\n");
  
  if(world->file_prefix_name == NULL) {
   	fprintf(world->err_file, "File %s, Line %ld: Inside VIZ_OUTPUT block the required keyword FILENAME is missing.\n", __FILE__, (long)__LINE__);
   	exit(1);
  }

  word_p=(unsigned char *)&word;
  word=0x04030201;

  if (word_p[0]==1) {
    sprintf(my_byte_order,"lsb");
  }
  else {
    sprintf(my_byte_order,"msb");
  }
  
  /*initialize arrays. */

  if(time_values == NULL){
  
     time_values = (u_int *)malloc(sizeof(u_int) * time_values_total);
     if(time_values == NULL){
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
      }
      
      for(ii = 0; ii < time_values_total; ii++){
        time_values[ii] = UINT_MAX;
      }
     
   }

  if(iteration_numbers_meshes == NULL){
  
     iteration_numbers_meshes = (u_int *)malloc(sizeof(u_int) * time_values_total);
     if(iteration_numbers_meshes == NULL){
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
      }
      
      for(ii = 0; ii < time_values_total; ii++){
        iteration_numbers_meshes[ii] = UINT_MAX;
      }
     
   }

  if(iteration_numbers_vol_mols == NULL){
  
     iteration_numbers_vol_mols = (u_int *)malloc(sizeof(u_int) * time_values_total);
     if(iteration_numbers_vol_mols == NULL){
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
      }
      
      for(ii = 0; ii < time_values_total; ii++){
        iteration_numbers_vol_mols[ii] = UINT_MAX;
      }
     
   }

  if(iteration_numbers_surf_mols == NULL){
  
     iteration_numbers_surf_mols = (u_int *)malloc(sizeof(u_int) * time_values_total);
     if(iteration_numbers_surf_mols == NULL){
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
      }
      
      for(ii = 0; ii < time_values_total; ii++){
        iteration_numbers_surf_mols[ii] = UINT_MAX;
      }
     
   }

   if(frame_data_series_list == NULL){
     frame_data_series_list = (char **)malloc(sizeof(char *) *time_values_total);     
     if(frame_data_series_list == NULL){
            fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
            return (1);
      }
      
      for(ii = 0; ii < time_values_total; ii++){
        frame_data_series_list[ii] = NULL;
      }

   }

  viz_iteration = (u_int)(fdlp->viz_iterationll);
  n_viz_iterations = (u_int)(fdlp->n_viz_iterations);

  viz_type=fdlp->type;

  /* here is the check to prevent writing twice the same info 
    - at the end of one checkpoint and the beginning of the next checkpoint.
   */
  if((world->chkpt_flag) && (world->start_time > 0)){
     if (world->it_time == world->start_time){
         if(viz_iteration % (world->chkpt_iterations) == 0){
             return 0;
         }
     }
  }

  /* initialize flags */
  viz_mol_pos_flag = (viz_type==MOL_POS);
  viz_mol_orient_flag = (viz_type==MOL_ORIENT);
  viz_mol_all_data_flag = (viz_type==ALL_MOL_DATA);
  if(viz_mol_all_data_flag){
       viz_mol_pos_flag = viz_mol_orient_flag = 1;
  }
  if(viz_mol_pos_flag){
     if((world->viz_output_flag & VIZ_MOLECULES_STATES) != 0){
     	viz_mol_states_flag = 1;
     }
  } 
  
  viz_surf_pos_flag = (viz_type==MESH_GEOMETRY);
  viz_region_data_flag = (viz_type==REG_DATA);
  viz_surf_all_data_flag = (viz_type==ALL_MESH_DATA);
  if(viz_surf_all_data_flag){
     viz_surf_pos_flag = viz_region_data_flag = 1;
  }
  if(viz_surf_pos_flag){
     if((world->viz_output_flag & VIZ_SURFACE_STATES) != 0){
     	viz_surf_states_flag = 1;
     }
  }


  /* these arrays are used to hold values of main_index for objects */
  if((viz_surf_pos_flag) || (viz_region_data_flag))
  {
     if(obj_to_show_number > 0)
     {
  	if((surf_states == NULL) && (viz_surf_states_flag)){ 
     		if ((surf_states=(u_int *)malloc(obj_to_show_number*sizeof(u_int)))==NULL)      {
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
     		}
        	for(ii = 0; ii < obj_to_show_number; ii++){
			surf_states[ii] = 0;
     		}
   	}
   	if((surf_pos == NULL) && (viz_surf_pos_flag)){
     		if ((surf_pos=(u_int *)malloc(obj_to_show_number*sizeof(u_int)))==NULL) {
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
     		}
     		for(ii = 0; ii < obj_to_show_number; ii++){
			surf_pos[ii] = 0;
     		}
   	}
   	if((surf_con == NULL) && (viz_surf_pos_flag)){
      		if ((surf_con=(u_int *)malloc(obj_to_show_number*sizeof(u_int)))==NULL) {
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
      		}
      		for(ii = 0; ii < obj_to_show_number; ii++){
			surf_con[ii] = 0;
      		}
        }
   	if(surf_field_indices == NULL){
      		if ((surf_field_indices=(u_int *)malloc(obj_to_show_number*sizeof(u_int)))==NULL) {
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
      		}
      		for(ii = 0; ii < obj_to_show_number; ii++){
			surf_field_indices[ii] = 0;
      		}
        }
    

   	/* initialize array of viz_objects names */
   	if(obj_names == NULL){
      		if ((obj_names = (char **)malloc(obj_to_show_number*sizeof(char *)))==NULL) {
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
      		}
      		for(ii = 0; ii < obj_to_show_number; ii++){
			obj_names[ii] = NULL;
      		}
                /* create an array of region names */
   	        if(region_names == NULL){
      	            if ((region_names = (char ***)malloc(obj_to_show_number*sizeof(char **)))==NULL) { 
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         	        return (1);
      	            }
                    /* initialize it */
                    for(ii = 0; ii < obj_to_show_number; ii++)
                    {
                       region_names[ii] = NULL;
                    }
                 }          
      
     	        ii = 0;
     	        vizp = world->viz_obj_head;
     	        while(vizp != NULL){
		   vcp = vizp->viz_child_head;
        	   while(vcp != NULL){
         	       obj_names[ii] = my_strdup(vcp->obj->sym->name);
                       if(obj_names[ii] == NULL){
                            fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		    return (1);
         	        }
                
                        int n_regs;   /* number of regions in the object */
                        /* subtract the default region ALL */
                        n_regs = vcp->obj->num_regions - 1;
                        if(n_regs > 0){
		            region_names[ii] = (char **)malloc(n_regs*sizeof(char *));
                            if(region_names[ii] == NULL){ 
                               fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		       return (1);
      		            }
                        }

                        jj = 0;
 
                        for(rlp = vcp->obj->regions; rlp != NULL; rlp = rlp->next){
                            rp = rlp->reg;
                            if(strcmp(rp->region_last_name, "ALL") == 0) continue;
                            
                            region_names[ii][jj] = my_strdup(rp->region_last_name);
                            if(region_names[ii][jj] == NULL)
                            { 
                               fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		       return (1);
      		            }
                            jj++;
                             
                        }
                        
                        ii++;
         		vcp = vcp->next;
       		    } /* end while (vcp) */
       		    vizp = vizp->next;
    	         } /* end while (vizp) */

   	}
       
       } /* end if (obj_to_show_number > 0) */
    }  /* end if (viz_surf_pos_flag  || viz_region_data_flag) */



     /* these arrays are used to hold values of main_index for effectors 
      and 3D molecules */
   if(viz_mol_pos_flag || viz_mol_orient_flag)
   {
     if(eff_to_show_number > 0)
     {
     	if((eff_states == NULL) && (viz_mol_states_flag)){ 
     		if((eff_states=(u_int *)malloc(eff_to_show_number*sizeof(u_int)))==NULL)        {
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
     		}
     		for(ii = 0; ii < eff_to_show_number; ii++){
			eff_states[ii] = 0;
     		}
     	}
     	if((eff_pos == NULL) && (viz_mol_pos_flag)){
     		if ((eff_pos=(u_int *)malloc(eff_to_show_number*sizeof(u_int)))==NULL) 	{
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
     		}
     		for(ii = 0; ii < eff_to_show_number; ii++){
			eff_pos[ii] = 0;
     		}
     	}
     	if((eff_orient == NULL) && (viz_mol_orient_flag)){
     		if ((eff_orient = (u_int *)malloc(eff_to_show_number*sizeof(u_int)))==NULL) 	{
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
     		}
     		for(ii = 0; ii < eff_to_show_number; ii++){
			eff_orient[ii] = 0;
     		}
     	}
     	if(eff_field_indices == NULL){
     		if ((eff_field_indices = (u_int *)malloc(eff_to_show_number*sizeof(u_int)))==NULL) 	{
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
     		}
     		for(ii = 0; ii < eff_to_show_number; ii++){
			eff_field_indices[ii] = 0;
     		}
     	}
   	/* initialize array of grid_mol's names */
   	if(eff_names == NULL){

      		if ((eff_names = (char **)malloc(eff_to_show_number*sizeof(char *)))==NULL) {
                        fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
      		}
      		for(ii = 0; ii < eff_to_show_number; ii++){
			eff_names[ii] = NULL;
      		}
   	
                index = 0;
        
                for(ii = 0; ii < world->n_species; ii++)
                {
     	           specp = world->species_list[ii];
     	           if((specp->flags & IS_SURFACE) != 0) continue;
     	           if(strcmp(specp->sym->name, "GENERIC_MOLECULE") == 0) continue;
                   if(((specp->flags & ON_GRID) == ON_GRID) && (specp->viz_state > 0)){ 
                      eff_names[index] = my_strdup(specp->sym->name);
                      if(eff_names[index] == NULL){
                         fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                         return (1);
                      }
                      index++;
                    } 
                 }
        }
        index = 0;
     } /* end if (eff_to_show_number > 0) */
   }  /* end if(viz_eff_pos_flag  */
   
   if(viz_mol_pos_flag || viz_mol_orient_flag)
   {
     if(mol_to_show_number > 0)
     {
     	if((mol_states == NULL) && (viz_mol_states_flag)){ 
     		if((mol_states=(u_int *)malloc(mol_to_show_number*sizeof(u_int)))==NULL)        {
                         fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
     		}
     		for(ii = 0; ii < mol_to_show_number; ii++){
			mol_states[ii] = 0;
     		}
     	}
     	if((mol_pos == NULL) && (viz_mol_pos_flag)) {
     		if ((mol_pos=(u_int *)malloc(mol_to_show_number*sizeof(u_int)))==NULL) 	{
                         fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
     		}
     		for(ii = 0; ii < mol_to_show_number; ii++){
			mol_pos[ii] = 0;
     		}
     	}
     	if((mol_orient == NULL) && (viz_mol_orient_flag)){
     		if ((mol_orient = (u_int *)malloc(mol_to_show_number*sizeof(u_int)))==NULL) 	{
                         fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
     		}
     		for(ii = 0; ii < mol_to_show_number; ii++){
			mol_orient[ii] = 0;
     		}
     	}
     	if(mol_field_indices == NULL){
     		if ((mol_field_indices = (u_int *)malloc(mol_to_show_number*sizeof(u_int)))==NULL) 	{
                         fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
     		}
     		for(ii = 0; ii < mol_to_show_number; ii++){
			mol_field_indices[ii] = 0;
     		}
     	}
        
   	/* initialize array of mol's names */
   	if(mol_names == NULL){
      		if ((mol_names = (char **)malloc(mol_to_show_number*sizeof(char *)))==NULL) {
                         fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
         		return (1);
      		}
      		for(ii = 0; ii < mol_to_show_number; ii++){
			mol_names[ii] = NULL;
      		}
   	
        	index = 0;
        	for(ii = 0; ii < world->n_species; ii++)
        	{
     	   	   specp = world->species_list[ii];
     	   	   if((specp->flags & IS_SURFACE) != 0) continue;
     	   	   if(strcmp(specp->sym->name, "GENERIC_MOLECULE") == 0) continue;
           	   if(((specp->flags & NOT_FREE) == 0) && (specp->viz_state > 0)){ 
                       mol_names[index] = my_strdup(specp->sym->name);
                       if(mol_names[index] == NULL){
                         fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                         return (1);
                       }
                       index++;
                   } 
                }
        }
        index = 0;

     } /* end if (mol_to_show_number > 0) */
  }  /* end if(viz_mol_pos_flag || viz_mol_orient_flag) */

     /* test whether a directory structure created by the user exists */
     /* count the number of '/' */
     if(viz_dir_depth == 0)
     {
        ii = 0;
        while(world->file_prefix_name[ii] != '\0') 
        {
           if(world->file_prefix_name[ii] == '/'){
              viz_dir_depth++;
           }
           ii++;
        }
     }


  /* Open master header file. */
  if(master_header_file_path == NULL)
  {
    if(world->chkpt_flag){
       sprintf(chkpt_seq_num, "%d", world->chkpt_seq_num);
       master_header_file_path = (char *)malloc(sizeof(char) * (strlen(world->file_prefix_name) +  strlen(".") +  strlen(chkpt_seq_num) + 1));
       if(master_header_file_path == NULL){
            fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
            return (1);
       }
    
       sprintf(master_header_file_path,"%s.%s.dx",world->file_prefix_name, chkpt_seq_num);
     }else{

       master_header_file_path = (char *)malloc(sizeof(char) * (strlen(world->file_prefix_name) + strlen(".dx")  + 1));
       if(master_header_file_path == NULL){
            fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
            return (1);
       }
       sprintf(master_header_file_path, "%s.dx",world->file_prefix_name);
     }
  }

  if(count_master_header == 0){
      if ((master_header=fopen(master_header_file_path, "w"))==NULL) {
           fprintf(world->err_file, "File %s, Line %ld: cannot open master header file %s.\n", __FILE__, (long)__LINE__, master_header_file_path);
           return(1);
      }
      count_master_header++;

  }else{
      if ((master_header=fopen(master_header_file_path,"a"))==NULL) {
           fprintf(world->err_file, "File %s, Line %ld: cannot open master header file %s.\n", __FILE__, (long)__LINE__, filename);
           return(1);
      }
      count_master_header++;

  }
    

  if ((viz_mol_pos_flag)|| ((mol_to_show_number == 0) && (eff_to_show_number == 0))) {
   if(mol_pos_file_path == NULL){
     if(world->chkpt_flag){
        sprintf(chkpt_seq_num, "%d", world->chkpt_seq_num);
        mol_pos_file_path = (char *)malloc(sizeof(char) * (strlen(world->file_prefix_name) + strlen(".molecule_positions.") +  strlen(chkpt_seq_num) + strlen(".bin") + 1));
        if(mol_pos_file_path == NULL){
            fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
            return (1);
        }
         sprintf(mol_pos_file_path,"%s.molecule_positions.%s.bin",world->file_prefix_name, chkpt_seq_num);
     }else{

        mol_pos_file_path = (char *)malloc(sizeof(char) * (strlen(world->file_prefix_name) + strlen(".molecule_positions.bin") + 1));
        if(mol_pos_file_path == NULL){
            fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
            return (1);
        }
        sprintf(mol_pos_file_path,"%s.molecule_positions.bin",world->file_prefix_name);
     }

     /* remove the folder name from the molecule_positions data file name */
     if(viz_dir_depth > 1){
        ch_ptr = strrchr(mol_pos_file_path, '/');
        ++ch_ptr;
        mol_pos_name = malloc(sizeof(char) * (strlen(ch_ptr) + 1));
        if(mol_pos_name == NULL){
            fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
            return (1);
        }
        strcpy(mol_pos_name, ch_ptr);
     }else{
        mol_pos_name = malloc(sizeof(char) * (strlen(mol_pos_file_path) + 1));
        if(mol_pos_name == NULL){
            fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
            return (1);
        }

        strcpy(mol_pos_name, mol_pos_file_path);
     }
   } /* end if(mol_pos_file_path == NULL) */

     if (count_mol_pos_data == 0){
        if ((mol_pos_data=fopen(mol_pos_file_path,"wb"))==NULL) {
           fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__, mol_pos_file_path);
           return(1);
        }else{}
        count_mol_pos_data++;
     }else{
        if ((mol_pos_data=fopen(mol_pos_file_path,"ab"))==NULL) {
              fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__, mol_pos_file_path);
              return(1);
        }
        count_mol_pos_data++;
    }
     

  } /* end if(viz_mol_pos_flag) */



  if((viz_mol_orient_flag) || ((mol_to_show_number == 0) && (eff_to_show_number == 0))) 
  {
    if(mol_orient_file_path == NULL)
    {
       if(world->chkpt_flag){
           sprintf(chkpt_seq_num, "%d", world->chkpt_seq_num);
           mol_orient_file_path = (char *)malloc(sizeof(char) * (strlen(world->file_prefix_name) + strlen(".molecule_orientations.") +  strlen(chkpt_seq_num) + strlen(".bin") + 1));
           if(mol_orient_file_path == NULL){
               fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
               return (1);
           }
           sprintf(mol_orient_file_path,"%s.molecule_orientations.%s.bin",world->file_prefix_name, chkpt_seq_num);
        }else{
           mol_orient_file_path = (char *)malloc(sizeof(char) * (strlen(world->file_prefix_name) + strlen(".molecule_orientations.bin") + 1));
           if(mol_orient_file_path == NULL){
               fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
               return (1);
           }
           sprintf(mol_orient_file_path,"%s.molecule_orientations.bin",world->file_prefix_name);
        }

         /* remove the folder name from the molecule_orientations data file name */
         if(viz_dir_depth > 1) {
           ch_ptr = strrchr(mol_orient_file_path, '/');
           ++ch_ptr;
           mol_orient_name = malloc(sizeof(char) * (strlen(ch_ptr) + 1));
           if(mol_orient_name == NULL){
               fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
               return (1);
           }
           strcpy(mol_orient_name, ch_ptr);
         }else{
           mol_orient_name = malloc(sizeof(char) * (strlen(mol_orient_file_path) + 1));
           if(mol_orient_name == NULL){
               fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
               return (1);
           }
           strcpy(mol_orient_name, mol_orient_file_path);
         }
       } /* end if(mol_orient_file_path == NULL) */

     if (count_mol_orient_data == 0){
        if ((mol_orient_data=fopen(mol_orient_file_path,"wb"))==NULL) {
           fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__, mol_orient_file_path);
           return(1);
        }
        count_mol_orient_data++;
     }else{
        if ((mol_orient_data=fopen(mol_orient_file_path,"ab"))==NULL) {
              fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__, mol_orient_file_path);
              return(1);
        }
        count_mol_orient_data++;

     }
     
   
  } /* end if(viz_mol_orient_flag) */


    if ((viz_mol_states_flag) || ((mol_to_show_number == 0) && (eff_to_show_number =0))) {
      if(mol_states_file_path == NULL)
      {  
         if(world->chkpt_flag){
           sprintf(chkpt_seq_num, "%d", world->chkpt_seq_num);
           mol_states_file_path = (char *)malloc(sizeof(char) * (strlen(world->file_prefix_name) + strlen(".molecule_states.") +  strlen(chkpt_seq_num) + strlen(".bin") + 1));
           if(mol_states_file_path == NULL){
               fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
               return (1);
           }
           sprintf(mol_states_file_path,"%s.molecule_states.%s.bin",world->file_prefix_name, chkpt_seq_num);
         }else{
           mol_states_file_path = (char *)malloc(sizeof(char) * (strlen(world->file_prefix_name) + strlen(".molecule_states.bin") + 1));
           if(mol_states_file_path == NULL){
               fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
               return (1);
           }
           sprintf(mol_states_file_path,"%s.molecule_states.bin",world->file_prefix_name);
         }
          /* remove the folder name from the molecule_states data file name */
         if(viz_dir_depth > 1)
         {
           ch_ptr = strrchr(mol_states_file_path, '/');
           ++ch_ptr;
           mol_states_name = malloc(sizeof(char) * (strlen(ch_ptr) + 1));
           if(mol_states_name == NULL){
               fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
               return (1);
           }
           strcpy(mol_states_name, ch_ptr);
         }else{
           mol_states_name = malloc(sizeof(char) * (strlen(mol_states_file_path) + 1));
           if(mol_states_name == NULL){
               fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
               return (1);
           }
           strcpy(mol_states_name, mol_states_file_path);
         }
       } /* end if(mol_states_file_path == NULL) */
     
       if (count_mol_states_data == 0){
            if ((mol_states_data = fopen(mol_states_file_path,"wb"))==NULL) {
                   fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__, mol_states_file_path);
           	   return(1);
            }
            count_mol_states_data++;
       }else{
           if ((mol_states_data = fopen(mol_states_file_path, "ab"))==NULL) {
                   fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__, mol_states_file_path);
           	   return(1);
            }
            count_mol_states_data++;
       }
    } /* end if(viz_mol_states_flag) */
    
          /* check here if MESHES or MOLECULES blocks
             are not supplied. In such case the corresponding files
             should be empty in order to prevent unintentional mixing of
             pre-existing and new files */
     if((mol_to_show_number == 0) && (eff_to_show_number == 0))
     {
          fprintf(world->log_file, "MOLECULES keyword is absent or commented.\nEmpty 'molecules' output files are created.\n");
     }


      if ((viz_surf_pos_flag) || (obj_to_show_number == 0)) {
        if(mesh_pos_file_path == NULL)
        {
           if(world->chkpt_flag){
              sprintf(chkpt_seq_num, "%d", world->chkpt_seq_num);
              mesh_pos_file_path = (char *)malloc(sizeof(char) * (strlen(world->file_prefix_name) + strlen(".mesh_positions.") +  strlen(chkpt_seq_num) + strlen(".bin") + 1));
              if(mesh_pos_file_path == NULL){
                  fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                  return (1);
              }
              sprintf(mesh_pos_file_path,"%s.mesh_positions.%s.bin",world->file_prefix_name, chkpt_seq_num);
           }else{
              mesh_pos_file_path = (char *)malloc(sizeof(char) * (strlen(world->file_prefix_name) + strlen(".mesh_positions.bin") + 1));
              if(mesh_pos_file_path == NULL){
                  fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                  return (1);
              }
              sprintf(mesh_pos_file_path,"%s.mesh_positions.bin",world->file_prefix_name);
           }
     
     	   /* remove the folder name from the mesh_positions data file name */
           if(viz_dir_depth > 1)
           {
              ch_ptr = strrchr(mesh_pos_file_path, '/');
              ++ch_ptr;
              mesh_pos_name = malloc(sizeof(char) * (strlen(ch_ptr) + 1));
              if(mesh_pos_name == NULL){
                  fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                  return (1);
              }
              strcpy(mesh_pos_name, ch_ptr);
           }else{
              mesh_pos_name = malloc(sizeof(char) * (strlen(mesh_pos_file_path) + 1));
              if(mesh_pos_name == NULL){
                  fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                  return (1);
              }
              strcpy(mesh_pos_name, mesh_pos_file_path);
           }
        } /* end if(mesh_pos_file_path == NULL) */

       if (count_mesh_pos_data == 0){
      	 if ((mesh_pos_data=fopen(mesh_pos_file_path,"wb"))==NULL) {
                fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__, mesh_pos_file_path);
                return(1);
          }
          count_mesh_pos_data++;
       }else{
      	   if ((mesh_pos_data=fopen(mesh_pos_file_path,"ab"))==NULL) {
               fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__, mesh_pos_file_path);
               return(1);
           }
           count_mesh_pos_data++;
       }

      } /* end if(viz_surf_pos_flag) */


      if ((viz_surf_states_flag) || (obj_to_show_number == 0)) {
         if(mesh_states_file_path == NULL)
         {
            if(world->chkpt_flag){
              sprintf(chkpt_seq_num, "%d", world->chkpt_seq_num);
              mesh_states_file_path = (char *)malloc(sizeof(char) * (strlen(world->file_prefix_name) + strlen(".mesh_states.") +  strlen(chkpt_seq_num) + strlen(".bin") + 1));
              if(mesh_states_file_path == NULL){
                  fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                  return (1);
              }
              sprintf(mesh_states_file_path,"%s.mesh_states.%s.bin",world->file_prefix_name, chkpt_seq_num);
            }else{
              mesh_states_file_path = (char *)malloc(sizeof(char) * (strlen(world->file_prefix_name) + strlen(".mesh_states.bin") + 1));
              if(mesh_states_file_path == NULL){
                  fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                  return (1);
              }
              sprintf(mesh_states_file_path,"%s.mesh_states.bin",world->file_prefix_name);
            }
     
           /* remove the folder name from the mesh_states data file name */
           if(viz_dir_depth > 1){
              ch_ptr = strrchr(mesh_states_file_path, '/');
              ++ch_ptr;
              mesh_states_name = malloc(sizeof(char) * (strlen(ch_ptr) + 1));
              if(mesh_states_name == NULL){
                  fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                  return (1);
              }
              strcpy(mesh_states_name, ch_ptr);
           }else{
              mesh_states_name = malloc(sizeof(char) * (strlen(mesh_states_file_path) + 1));
              if(mesh_states_name == NULL){
                  fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                  return (1);
              }
              strcpy(mesh_states_name, mesh_states_file_path);
           }
        } /* end if(mesh_states_file_path == NULL) */

       if (count_mesh_states_data == 0){
           if ((mesh_states_data=fopen(mesh_states_file_path,"wb"))==NULL) {
                  fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__,mesh_states_file_path);
                  return(1);
            }
            count_mesh_states_data++;
       }else{
          if ((mesh_states_data=fopen(mesh_states_file_path,"ab"))==NULL) {
              fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__,mesh_states_file_path);
              return(1);
          }
          count_mesh_states_data++;
       }
      }  /* end if(viz_surf_states_flag) */

      if ((viz_region_data_flag) || (obj_to_show_number == 0)){
         if(region_viz_data_file_path == NULL)
         {         
            if(world->chkpt_flag){
              sprintf(chkpt_seq_num, "%d", world->chkpt_seq_num);
              region_viz_data_file_path = (char *)malloc(sizeof(char) * (strlen(world->file_prefix_name) + strlen(".region_indices.") +  strlen(chkpt_seq_num) + strlen(".bin") + 1));
              if(region_viz_data_file_path == NULL){
                  fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                  return (1);
              }
              sprintf(region_viz_data_file_path,"%s.region_indices.%s.bin",world->file_prefix_name, chkpt_seq_num);
            }else{
              region_viz_data_file_path = (char *)malloc(sizeof(char) * (strlen(world->file_prefix_name) + strlen(".region_indices.bin") + 1));
              if(region_viz_data_file_path == NULL){
                  fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                  return (1);
              }
              sprintf(region_viz_data_file_path,"%s.region_indices.bin",world->file_prefix_name);
            }
     
            /* remove the folder name from the region values data file name */
            if(viz_dir_depth > 1){
              ch_ptr = strrchr(region_viz_data_file_path, '/');
              ++ch_ptr;
              region_viz_data_name = malloc(sizeof(char) * (strlen(ch_ptr) + 1));
              if(region_viz_data_name == NULL){
                  fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                  return (1);
              }
              strcpy(region_viz_data_name, ch_ptr);
            }else{
              region_viz_data_name = malloc(sizeof(char) * (strlen(region_viz_data_file_path) + 1));
              if(region_viz_data_name == NULL){
                  fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                  return (1);
              }
              strcpy(region_viz_data_name, region_viz_data_file_path);
            }
        } /* end if(region_viz_data_file_path == NULL) */

       if (count_region_data == 0){
          if ((region_data=fopen(region_viz_data_file_path,"wb"))==NULL) {
              fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__, region_viz_data_file_path);
              return(1);
           }
           count_region_data++;
       }else{
          if ((region_data=fopen(region_viz_data_file_path,"ab"))==NULL) {
                fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__, region_viz_data_file_path);
                return(1);
          }
          count_region_data++;
       }
      } /* end if(viz_region_data_flag) */

       if(obj_to_show_number == 0)
       {
         fprintf(world->log_file, "MESHES keyword is absent or commented.\nEmpty 'meshes' output files are created.\n");
       }

    /* find out the values of the current iteration steps for
       (GEOMETRY, REG_DATA),  
       (MOL_POS, MOL_ORIENT) frames combinations */
   
    fdl_ptr = world->frame_data_head;
    while(fdl_ptr != NULL){
        if(fdl_ptr->type == MESH_GEOMETRY){
             curr_surf_pos_iteration_step = fdl_ptr->viz_iterationll;
        }if(fdl_ptr->type == REG_DATA){
             curr_region_data_iteration_step = fdl_ptr->viz_iterationll;
        }if(fdl_ptr->type == ALL_MESH_DATA){
             curr_surf_pos_iteration_step = fdl_ptr->viz_iterationll;
             curr_region_data_iteration_step = fdl_ptr->viz_iterationll;
        }else if(fdl_ptr->type == MOL_POS){
             curr_mol_pos_iteration_step = fdl_ptr->viz_iterationll;
	}else if(fdl_ptr->type == MOL_ORIENT) 
        {
             curr_mol_orient_iteration_step = fdl_ptr->viz_iterationll;
        }else if(fdl_ptr->type == ALL_MOL_DATA){
             curr_mol_pos_iteration_step = fdl_ptr->viz_iterationll;
             curr_mol_orient_iteration_step = fdl_ptr->viz_iterationll;
        }	
        fdl_ptr = fdl_ptr->next;
    }
  
    /* If the values of the current iteration steps for REG_VIZ_VALUES and
       MESH_GEOMETRY are equal set this value to the 
       special_surf_iteration_step.
       Do the same for the (MOL_POS,MOL_ORIENT).
    */
     
    if(curr_region_data_iteration_step == curr_surf_pos_iteration_step){
	special_surf_iteration_step = curr_surf_pos_iteration_step;
    }
    
    if(curr_mol_orient_iteration_step == curr_mol_pos_iteration_step){
	special_mol_iteration_step = curr_mol_orient_iteration_step;
    }
    
    /* check for the special_iteration_step  */  
    if(viz_surf_pos_flag || viz_region_data_flag){	    
	    if(fdlp->viz_iterationll == special_surf_iteration_step){
		special_surf_frames_counter++;
            }
    }


/* dump walls */
  if((viz_surf_pos_flag) || (viz_region_data_flag)) {
     vizp = world->viz_obj_head;
     
     while(vizp!=NULL) {

    /* Traverse all visualized compartments 
       output mesh element positions and connections */
    vcp = vizp->viz_child_head;

    while(vcp!=NULL) {
      objp = vcp->obj;
      if(objp->viz_state == NULL) continue;
      pop=(struct polygon_object *)objp->contents;

      if (objp->object_type==POLY_OBJ || objp->object_type==BOX_OBJ) {
          opp=(struct ordered_poly *)pop->polygon_data;
          edp=opp->element;
          element_data_count=objp->n_walls_actual;

          if (viz_surf_pos_flag) {
            fprintf(master_header,
              "object %d class array type float rank 1 shape 3 items %d %s binary data file %s,%d # %s.positions #\n",
              main_index,objp->n_verts,my_byte_order,mesh_pos_name, mesh_pos_byte_offset, objp->sym->name);
            fprintf(master_header,
              "\tattribute \"dep\" string \"positions\"\n\n");
            surf_pos[surf_pos_index] = main_index;
            surf_pos_index++;
            main_index++;

            /* output polyhedron vertices */
            for (ii=0;ii<objp->n_verts;ii++) {
              v1 = (float)(world->length_unit*objp->verts[ii].x);
              v2 = (float)(world->length_unit*objp->verts[ii].y);
              v3 = (float)(world->length_unit*objp->verts[ii].z);
              fwrite(&v1,sizeof v1,1,mesh_pos_data);
              fwrite(&v2,sizeof v2,1,mesh_pos_data);
              fwrite(&v3,sizeof v3,1,mesh_pos_data);
              mesh_pos_byte_offset += (sizeof(v1) + sizeof(v2) + sizeof(v3));
            }

            /* output polygon element connections */
            fprintf(master_header,
              "object %d class array type int rank 1 shape 3 items %d %s binary data file %s,%d # %s.connections #\n",
              main_index,element_data_count,my_byte_order, mesh_pos_name, mesh_pos_byte_offset, objp->sym->name);
            fprintf(master_header,
              "\tattribute \"ref\" string \"positions\"\n");
            fprintf(master_header,
              "\tattribute \"element type\" string \"triangles\"\n\n");

            for (ii=0;ii<objp->n_walls;ii++) {
              if (!get_bit(pop->side_removed,ii)) {
                vi1=edp[ii].vertex_index[0];
                vi2=edp[ii].vertex_index[1];
                vi3=edp[ii].vertex_index[2];
	        fwrite(&vi1,sizeof vi1,1,mesh_pos_data);
	        fwrite(&vi2,sizeof vi2,1,mesh_pos_data);
	        fwrite(&vi3,sizeof vi3,1,mesh_pos_data);
                mesh_pos_byte_offset += (sizeof(vi1) + sizeof(vi2) + sizeof(vi3));
              }
            }
            surf_con[surf_con_index] = main_index;
            main_index++;
            surf_con_index++;

          } /* end viz_surf_pos_flag for POLY_OBJ */
 
          if (viz_surf_states_flag) {
            fprintf(master_header,
              "object %d class array type int rank 0 items %d %s binary data file %s,%d # %s.states #\n", main_index, element_data_count, my_byte_order, mesh_states_name, mesh_states_byte_offset, objp->sym->name);
            fprintf(master_header,"\tattribute \"dep\" string \"connections\"\n\n");
           surf_states[surf_states_index] = main_index;
           surf_states_index++;
           main_index++;

            for (ii=0;ii<objp->n_walls;ii++) {
               if (!get_bit(pop->side_removed,ii)) {
                 state=objp->viz_state[ii];
                 fwrite(&state,sizeof (state),1,mesh_states_data);
                 mesh_states_byte_offset += sizeof(state);
               }
            }

          } 	/* end viz_surf_states_flag for POLY_OBJ */

          if(viz_region_data_flag && (objp->num_regions > 1))
          {
              surf_region_values_index = 0;

              for(rlp = objp->regions; rlp != NULL; rlp = rlp->next)
              {
                  rp = rlp->reg;
                  if(strcmp(rp->region_last_name, "ALL") == 0) continue; 
                  
                  /* number of walls in the region */
                  int region_walls_number = 0; 
                  
                  for(jj = 0; jj < objp->n_walls; jj++)
                  {
                    int n = objp->wall_p[jj]->side;
                    if(get_bit(rp->membership,n))
                    {
                        fwrite(&n,sizeof (n),1,region_data);
                        region_data_byte_offset += sizeof(n);
                        region_walls_number++;
                    }

                  }

                  fprintf(master_header,
                        "object %d class array type int rank 0 items %d %s binary data file %s,%d # %s.region_data #\n", main_index, region_walls_number, my_byte_order, region_viz_data_name, region_data_byte_offset_prev, objp->sym->name);
                  fprintf(master_header,"\tattribute \"ref\" string \"connections\"\n");  
                  fprintf(master_header,"\tattribute \"identity\" string \"region_indices\"\n");
                  fprintf(master_header,"\tattribute \"name\" string \"%s\"\n", rp->region_last_name);
                  if(rp->region_viz_value > 0){
                      fprintf(master_header,"\tattribute \"viz_value\" number %d\n", rp->region_viz_value);
                  }

                  region_data_byte_offset_prev = region_data_byte_offset;
                  surf_region_values[surf_obj_region_values_index][surf_region_values_index] = main_index;
                  surf_region_values_index++;
                  main_index++;
                  fprintf(master_header, "\n\n");

              } /* end for */
              

           } /* end if(region_data_flag) */


       }	/* end POLY_OBJ */

       surf_obj_region_values_index++; 

      vcp = vcp->next;
      }
      
      vizp = vizp->next;
      }
    } /* end (viz_surf_pos_flag || viz_surf_states_flag || viz_region_data_flag) for vizp */
    
    /* build fields here */
      int show_meshes = 0;
   if(obj_to_show_number > 0)
   {
      if(fdlp->type == ALL_MESH_DATA) {
           show_meshes = 1;
      }
      else if((viz_surf_pos_flag || viz_region_data_flag) && ((special_surf_frames_counter == 0) || (special_surf_frames_counter == 2))){
		show_meshes = 1;
      }
    }


    if(show_meshes)
    {
         for(ii = 0; ii < obj_to_show_number; ii++)
         {
             if(obj_names[ii] != NULL){
                fprintf(master_header,
                    "object %d field   # %s #\n",main_index, obj_names[ii]);
             }
             if(surf_pos[ii] > 0){
             	fprintf(master_header,
                 "\tcomponent \"positions\" value %d\n",surf_pos[ii]);
             }
             if(surf_con[ii] > 0){
             	fprintf(master_header,
                 "\tcomponent \"connections\" value %d\n",surf_con[ii]);
             }
             if(surf_states != NULL){
                if(surf_states[ii] > 0){
             	   fprintf(master_header,
                	"\tcomponent \"state_values\" value %d\n",surf_states[ii]);
	        }
             }
             if(surf_region_values[ii] != NULL)
             {
                for(jj = 0; jj < obj_num_regions[ii]; jj++)
                {
                   if(surf_region_values[ii][jj] > 0){
             	      fprintf(master_header,
                	"\tcomponent \"%s\" value %d\n",region_names[ii][jj], surf_region_values[ii][jj]);
	           }
                }
             }
             fprintf(master_header, "\n");
             surf_field_indices[ii] = main_index;

             main_index++;
           }     
     }
    
    /* create a group object for all meshes. */
    if(show_meshes)
    {
        vizp = world->viz_obj_head;
        if(vizp != NULL){

        	fprintf(master_header,"object \"%s%u\" group # %s #\n","meshes_", viz_iteration, "meshes");
        	mesh_group_index = main_index;
        	main_index++;
                member_meshes_iteration = viz_iteration;

                ii = 0;
        	while(vizp != NULL) {
         		vcp = vizp->viz_child_head;
         		while(vcp != NULL){
                          if(surf_field_indices[ii] > 0){
             			fprintf(master_header,"\tmember \"%s\" value %d\n",vcp->obj->sym->name,surf_field_indices[ii]);
                          }
                          ii++;
		       	  vcp = vcp->next;
         		}
         		vizp = vizp->next;
        	}       
        	fprintf(master_header, "\n"); 
      	}  /* end (if vizp) */

        /* store iteration_number for meshes */
      iteration_numbers_meshes[iteration_numbers_meshes_count] = viz_iteration;
      iteration_numbers_meshes_count++;
  
   } 

      
    /* Visualize molecules. */



/* dump grid molecules. */
    /* check for the special_iteration_step  */ 
    if(viz_mol_pos_flag || viz_mol_orient_flag){	    
	    if(fdlp->viz_iterationll == special_mol_iteration_step){
		special_mol_frames_counter++;
            }
    }


 if(viz_mol_pos_flag || viz_mol_orient_flag){	    

       /* create references to the numbers of grid molecules of each name. */
       if ((viz_grid_mol_count=(u_int *)malloc(n_species*sizeof(u_int)))==NULL) {
         return(1);
       }

       /* perform initialization */
      for (ii = 0; ii < n_species; ii++){
         spec_id = species_list[ii]->species_id;
         viz_grid_mol_count[spec_id]=0;

      } 


   for (ii = 0; ii < n_species; ii++)
   {
     specp = species_list[ii];
     if((specp->flags & ON_GRID) == 0) continue; 
     if(specp->viz_state == EXCLUDE_OBJ) continue; 
 
        grid_mol_name = specp->sym->name;
        spec_id = specp->species_id; 

        if(viz_mol_pos_flag){
      	   mol_pos_byte_offset_prev = mol_pos_byte_offset;
        }
        if(viz_mol_orient_flag){
      	   mol_orient_byte_offset_prev = mol_orient_byte_offset;
        }
        if(viz_mol_states_flag){
      	   mol_states_byte_offset_prev = mol_states_byte_offset;
        }
        /* Write binary data files. */
        vizp = world->viz_obj_head;
        while(vizp != NULL) {
         vcp = vizp->viz_child_head;
         while(vcp != NULL){
	     objp = vcp->obj;
             wp = objp->wall_p;
             no_printf("Traversing walls in object %s\n",objp->sym->name);
             for (jj=0;jj<objp->n_walls;jj++) {
                w = wp[jj];
                if (w!=NULL) {
	           sg = w->effectors;
                   if (sg!=NULL) {
                     for (index=0;index<sg->n_tiles;index++) {
                        grid2xyz(sg,index,&p0);
	                gmol=sg->mol[index];
	                if ((gmol!=NULL) && (viz_mol_states_flag)){
	                   state=sg->mol[index]->properties->viz_state;
                        }
                        if (gmol != NULL) {
                             if(spec_id == gmol->properties->species_id){
                                 if(viz_grid_mol_count[spec_id] < specp->population){
                                     viz_grid_mol_count[spec_id]++;
                                 }else{
                                     fprintf(log_file,"MCell: molecule count disagreement!!\n");
                                     fprintf(log_file,"  Species %s  population = %d  count = %d\n",grid_mol_name,specp->population,viz_grid_mol_count[spec_id]);

                                 }

                               if(viz_mol_pos_flag){
                                   /* write positions information */
	           		   v1=(float)(world->length_unit*p0.x);
	           		   v2=(float)(world->length_unit*p0.y);
	           		   v3=(float)(world->length_unit*p0.z);
	           		   fwrite(&v1,sizeof v1,1,mol_pos_data);
	               		   fwrite(&v2,sizeof v2,1,mol_pos_data);
	           		   fwrite(&v3,sizeof v3,1,mol_pos_data);
                                   mol_pos_byte_offset += (sizeof(v1) + sizeof(v2) + sizeof(v3));
                               }
                               if(viz_mol_orient_flag){
                                   /* write orientations information */
                   		   v1=(float)((w->normal.x)*(gmol->orient));
                   		   v2=(float)((w->normal.y)*(gmol->orient));
                   		   v3=(float)((w->normal.z)*(gmol->orient));
                   		   fwrite(&v1,sizeof v1,1,mol_orient_data);
                   		   fwrite(&v2,sizeof v2,1,mol_orient_data);
                   		   fwrite(&v3,sizeof v3,1,mol_orient_data);
                                   mol_orient_byte_offset += (sizeof(v1) + sizeof(v2) + sizeof(v3));
                  		}
                          } /* end if strcmp */
                           
                        } /* end if (gmol)*/
                     } /* end for */
                   } /* end if (sg) */
                 } /* end if (w)*/
              } /* end for */
              vcp = vcp->next;
            } /* end while (vcp) */
                                              
            vizp = vizp->next;
         } /* end while (vzp) */

        num = viz_grid_mol_count[spec_id];
        
        if(viz_mol_pos_flag)
        {
           if(num > 0)
           {   
        	fprintf(master_header,"object %d class array type float rank 1 shape 3 items %d %s binary data file %s,%d # %s positions #\n",main_index,num,my_byte_order, mol_pos_name, mol_pos_byte_offset_prev, grid_mol_name);
        	fprintf(master_header,"\tattribute \"dep\" string \"positions\"\n\n");
            }else{
                /* output empty arrays for zero molecule counts here */
                fprintf(master_header,"object %d array   # %s positions #\n",main_index, grid_mol_name);
            }
            eff_pos[eff_pos_index] = main_index;
            eff_pos_index++;
            main_index++;
         }
         
         if(viz_mol_orient_flag)
         {
           if(num > 0)
           {
        	fprintf(master_header,"object %d class array type float rank 1 shape 3 items %d %s binary data file %s,%d   # %s orientations #\n",main_index,num,my_byte_order, mol_orient_name, mol_orient_byte_offset_prev, grid_mol_name);
        	fprintf(master_header,"\tattribute \"dep\" string \"positions\"\n\n");
            }else{
                /* output empty arrays for zero molecule counts here */
                fprintf(master_header,"object %d array   # %s orientations #\n",main_index, grid_mol_name);
            }
            eff_orient[eff_orient_index] = main_index;
            eff_orient_index++;
            main_index++;
        }

        if (viz_mol_states_flag) {
          if(num > 0)
          {
            /* write states information. */
            fwrite(&state,sizeof state,1,mol_states_data);
            mol_states_byte_offset += (sizeof state);
        
	    fprintf(master_header,"object %d class constantarray type int items %d %s binary data file %s,%d  # %s states #\n",main_index,num, my_byte_order,mol_states_name,mol_states_byte_offset_prev, grid_mol_name);

            fprintf(master_header,"\tattribute \"dep\" string \"positions\"\n\n");
	  }else{
             /* output empty arrays for zero molecule counts here */
             fprintf(master_header,"object %d array   # %s states #\n",main_index, grid_mol_name);
          }
          eff_states[eff_states_index] = main_index;
          eff_states_index++;
          main_index++;
       }
       if(num == 0){
         fprintf(master_header, "\n");
       }
   } /* end for loop */

 } /* end if(viz_mol_pos_flag || viz_mol_orient_flag) */

/* build fields for grid molecules here */
  int show_effectors = 0;

  if(eff_to_show_number > 0){
      if(fdlp->type == ALL_MOL_DATA) {
           show_effectors = 1;
      }
      else if((viz_mol_pos_flag || viz_mol_orient_flag) && ((special_mol_frames_counter ==0) || (special_mol_frames_counter == 2))){
		show_effectors = 1;
      }
  }
 
  if(show_effectors){


       for(ii = 0; ii < eff_to_show_number; ii++)
       {
             if(eff_names[ii] != NULL){
                fprintf(master_header,
                    "object %d field   # %s #\n",main_index, eff_names[ii]);
             }
             if(eff_pos[ii] > 0){
             	fprintf(master_header,
                 "\tcomponent \"positions\" value %d\n",eff_pos[ii]);
             }
             if(eff_orient[ii] > 0){     
             	fprintf(master_header,
                 	"\tcomponent \"data\" value %d # orientations #\n",eff_orient[ii]);
             }
             if(viz_mol_states_flag)
             {
                if(eff_states[ii] > 0){
             	   fprintf(master_header,
                	"\tcomponent \"state_values\" value %d\n",eff_states[ii]);
                }
             }
             fprintf(master_header, "\n");
             eff_field_indices[ii] = main_index;
             main_index++;
      }
  }




/* dump 3D molecules: */

 if(viz_mol_pos_flag || viz_mol_orient_flag){	 
   
        /* create references to the molecules. */
        if ((viz_molp=(struct volume_molecule ***)malloc(n_species*sizeof(struct volume_molecule **)))==NULL) {
                fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
      		return(1);
    	}
    	if ((viz_mol_count=(u_int *)malloc(n_species*sizeof(u_int)))==NULL) {
                fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
      		return(1);
    	}
  
    for (ii=0;ii<n_species;ii++) {
      /* perform initialization */
      spec_id=species_list[ii]->species_id;
      viz_molp[spec_id]=NULL;
      viz_mol_count[spec_id]=0;

      if (species_list[ii]->viz_state == EXCLUDE_OBJ)  continue;

      num=species_list[ii]->population;
      if ((num>0) && (species_list[ii]->flags & NOT_FREE) == 0) {
          /* create references for 3D molecules */
          if ((viz_molp[spec_id]=(struct volume_molecule **)malloc
            (num*sizeof(struct volume_molecule *)))==NULL) {
                fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
                return(1);
          }
      }
    } /* end for */

    slp=world->storage_head;
    while (slp!=NULL) {
      sp=slp->store;
      shp=sp->timer;
      while (shp!=NULL) {
        
        for (ii=0;ii<shp->buf_len;ii++) {
          amp=(struct abstract_molecule *)shp->circ_buf_head[ii];
          while (amp!=NULL) {
            if ((amp->properties!=NULL) && (amp->flags & TYPE_3D) == TYPE_3D){
              molp=(struct volume_molecule *)amp;
              if (molp->properties->viz_state!=EXCLUDE_OBJ) {
                spec_id=molp->properties->species_id;
                if (viz_mol_count[spec_id]<molp->properties->population) {
                  viz_molp[spec_id][viz_mol_count[spec_id]++]=molp;
                }
                else {
                  fprintf(log_file,"MCell: molecule count disagreement!!\n");
                  fprintf(log_file,"  Species %s  population = %d  count = %d\n",molp->properties->sym->name,molp->properties->population,viz_mol_count[spec_id]);
                } /* end if/else */
              }  /* end if(molp) */
            } /* end if(amp) */
            amp=amp->next;
          } /* end while  (amp) */
        } /* end for */
        
        amp=(struct abstract_molecule *)shp->current;
        while (amp!=NULL) {
          if ((amp->properties!=NULL) && (amp->flags & TYPE_3D) == TYPE_3D) {
            molp=(struct volume_molecule *)amp;
            if (molp->properties->viz_state!=EXCLUDE_OBJ) {
              spec_id=molp->properties->species_id;
              if (viz_mol_count[spec_id]<molp->properties->population) {
                viz_molp[spec_id][viz_mol_count[spec_id]++]=molp;
              }
              else {
                fprintf(log_file,"MCell: molecule count disagreement!!\n");
                fprintf(log_file,"  Species %s  population = %d  count = %d\n",molp->properties->sym->name,molp->properties->population,viz_mol_count[spec_id]);
              }
            }

           }
          amp=amp->next;
        }
        
        shp=shp->next_scale;
      } /* end (while (shp) */

      slp=slp->next;
    } /* end (while slp) */


    for (ii=0;ii<n_species;ii++) {
      
      if(species_list[ii]->viz_state == EXCLUDE_OBJ) continue; 
      spec_id=species_list[ii]->species_id;
      
      if(viz_mol_count[spec_id] > 0)
      {
         num=viz_mol_count[spec_id];
         if(num!=species_list[ii]->population
             && ((species_list[ii]->flags & NOT_FREE)==0)) {
             fprintf(log_file,"MCell: molecule count disagreement!!\n");
             fprintf(log_file,"  Species %s  population = %d  count = %d\n",species_list[ii]->sym->name,species_list[ii]->population,num);
         }
      }

      if (viz_mol_count[spec_id]>0 && ((species_list[ii]->flags & NOT_FREE) == 0)) {
        /* here are 3D diffusing molecules */
        num = viz_mol_count[spec_id];
       if(viz_mol_pos_flag)
       { 	  
	  mol_pos_byte_offset_prev = mol_pos_byte_offset;
          for (jj=0;jj<num;jj++) {
            molp=viz_molp[spec_id][jj];
	    v1=(float)(world->length_unit*molp->pos.x);
	    v2=(float)(world->length_unit*molp->pos.y);
	    v3=(float)(world->length_unit*molp->pos.z);
	    fwrite(&v1,sizeof v1,1,mol_pos_data);
	    fwrite(&v2,sizeof v2,1,mol_pos_data);
	    fwrite(&v3,sizeof v3,1,mol_pos_data);
            mol_pos_byte_offset += (sizeof(v1) + sizeof(v2) + sizeof(v3));
          }
          fprintf(master_header,"object %d class array type float rank 1 shape 3 items %d %s binary data file %s,%d  # %s positions #\n",main_index,num,my_byte_order, mol_pos_name, mol_pos_byte_offset_prev, species_list[ii]->sym->name);
          fprintf(master_header,"\tattribute \"dep\" string \"positions\"\n\n");
          mol_pos[mol_pos_index] = main_index;
          mol_pos_index++;
          main_index++;
        }

        if(viz_mol_orient_flag)
        {  
          /* write molecule orientations information. 
             for 3D molecules we use default orientation [0,0,1] */
      	  mol_orient_byte_offset_prev = mol_orient_byte_offset;
          v1 = 0;
          v2 = 0;
          v3 = 1;
          fwrite(&v1,sizeof v1,1,mol_orient_data);
          fwrite(&v2,sizeof v2,1,mol_orient_data);
          fwrite(&v3,sizeof v3,1,mol_orient_data);
          mol_orient_byte_offset += (sizeof(v1) + sizeof(v2) + sizeof(v3));
          fprintf(master_header,"object %d class constantarray type float rank 1 shape 3 items %d %s binary data file %s,%d # %s orientations #\n",main_index,num, my_byte_order, mol_orient_name, mol_orient_byte_offset_prev, species_list[ii]->sym->name);
          fprintf(master_header,"\tattribute \"dep\" string \"positions\"\n\n");
          mol_orient[mol_orient_index] = main_index;
          mol_orient_index++;
          main_index++;
        }

        if(viz_mol_states_flag)
        {
          /* write molecule states information. */ 
      	  mol_states_byte_offset_prev = mol_states_byte_offset;
          fwrite(&state,sizeof state,1,mol_states_data);
          mol_states_byte_offset += (sizeof state);
          fprintf(master_header,"object %d class constantarray type int items %d %s binary data file %s,%d  # %s states #\n",main_index,num, my_byte_order,mol_states_name,mol_states_byte_offset_prev, species_list[ii]->sym->name);
          fprintf(master_header,"\tattribute \"dep\" string \"positions\"\n\n");
          mol_states[mol_states_index] = main_index;
          mol_states_index++;
          main_index++;
        }
      }
      /* output empty arrays for zero molecule counts here */
      else if ((viz_mol_count[spec_id]==0) && ((species_list[ii]->flags & NOT_FREE) == 0)) {
        if(viz_mol_pos_flag)
        {
          fprintf(master_header,"object %d array   # %s positions #\n",main_index, species_list[ii]->sym->name);
          mol_pos[mol_pos_index] = main_index;
          mol_pos_index++;
          main_index++;
        }
        if(viz_mol_orient_flag)
        {
          fprintf(master_header,"object %d array   # %s orientations #\n",main_index, species_list[ii]->sym->name);
          mol_orient[mol_orient_index] = main_index;
          mol_orient_index++;
          main_index++;
        }
       
       if(viz_mol_states_flag)
       {
          fprintf(master_header,"object %d array   # %s states #\n",main_index, species_list[ii]->sym->name);
          mol_states[mol_states_index] = main_index;
          mol_states_index++;
          main_index++;
          
       }
	 fprintf(master_header,"\n");
      } /* end else if */
    }
  } /* end if((viz_mol_pos_flag) || (viz_mol_orient_flag)) */
 

/* build fields here */
      int show_molecules = 0;
   if(mol_to_show_number > 0)
   {
      if(fdlp->type == ALL_MOL_DATA) {
           show_molecules = 1;
      }
      else if((viz_mol_pos_flag || viz_mol_orient_flag) && ((special_mol_frames_counter ==0) || (special_mol_frames_counter == 2))){
		show_molecules = 1;
      }
   }

    if(show_molecules)
    {
      for (ii=0; ii<mol_to_show_number; ii++) {
               if(mol_names[ii] != NULL){
                  fprintf(master_header,
                      "object %d field   # %s #\n",main_index, mol_names[ii]);
                }
                if(mol_pos[ii] > 0) {
             	   fprintf(master_header,
                 	"\tcomponent \"positions\" value %d\n",mol_pos[ii]);
                }
                if(mol_orient[ii] > 0){
             	   fprintf(master_header,
                 	"\tcomponent \"data\" value %d # orientations #\n",mol_orient[ii]);
                }
             if(viz_mol_states_flag)
             {
                if(mol_states[ii] > 0){ 
             	   fprintf(master_header,
                	"\tcomponent \"state_values\" value %d\n",mol_states[ii]);
                }
             }
             fprintf(master_header, "\n");
             mol_field_indices[ii] = main_index;
             main_index++;
      }
    }

      /* create group objects for molecules/effectors */

        if(show_molecules)
        {
                fprintf(master_header,"object \"%s%u\" group # %s #\n", "volume_molecules_", viz_iteration, "volume molecules"); 
        	mol_group_index = main_index;
                member_molecules_iteration = viz_iteration;
      		for (ii=0;ii<mol_to_show_number;ii++) {
                   if(mol_field_indices[ii] > 0){
          		fprintf(master_header,"\tmember \"%s\" value %d\n",mol_names[ii], mol_field_indices[ii]);
                    }
          	}
                main_index++;
            /* store iteration_number for volume molecules */
            iteration_numbers_vol_mols[iteration_numbers_vol_mols_count] = viz_iteration;
            iteration_numbers_vol_mols_count++;
      	}

        fprintf(master_header, "\n"); 

 
        if(show_effectors)
        {
                fprintf(master_header,"object \"%s%u\" group # %s #\n","surface_molecules_", viz_iteration, "surface molecules");
               eff_group_index = main_index;
               member_effectors_iteration = viz_iteration;
      	       for (ii=0;ii<eff_to_show_number;ii++) {
                   if(eff_field_indices[ii] > 0){
                       fprintf(master_header,"\tmember \"%s\" value %d\n",eff_names[ii], eff_field_indices[ii]);
                   }
               }
               main_index++;
            /* store iteration_number for surface molecules */
            iteration_numbers_surf_mols[iteration_numbers_surf_mols_count] = viz_iteration;
            iteration_numbers_surf_mols_count++;
      	}
        fprintf(master_header, "\n"); 

      /* create combined group object for meshes and molecules */
      int show_combined_group = 1;
      if(((fdlp->viz_iterationll == curr_surf_pos_iteration_step) ||
	  (fdlp->viz_iterationll == curr_region_data_iteration_step)) && 
        (mesh_group_index == 0)) 
      {
            	show_combined_group = 0;
      }
      if(((fdlp->viz_iterationll == curr_mol_pos_iteration_step) ||
	 (fdlp->viz_iterationll == curr_mol_orient_iteration_step))
             && ((mol_group_index == 0) && (eff_group_index == 0))){ 
            	show_combined_group = 0;
      }

      if(show_combined_group)
      {
        fprintf(master_header,"object %d group\n",main_index);
        combined_group_index = main_index;
        if(member_meshes_iteration != UINT_MAX){  
          	fprintf(master_header,"\tmember \"meshes\" value \"%s%u\"\n", "meshes_",member_meshes_iteration);
	     	mesh_group_index = 0; 

      	}
        if(member_molecules_iteration != UINT_MAX) {  
          	fprintf(master_header,"\tmember \"volume_molecules\" value \"%s%u\"\n",  "volume_molecules_", member_molecules_iteration); 
		mol_group_index = 0; 
        }
       if(member_effectors_iteration != UINT_MAX) {  
          	fprintf(master_header,"\tmember \"surface_molecules\" value \"%s%u\"\n",  "surface_molecules_", member_effectors_iteration);
		eff_group_index = 0; 
        }
      	
        fprintf(master_header,"\n");
      }


     /* create entry into "frame_data" object. */
      if(show_combined_group){

        /* create an entry into a 'frame_data' object. */
        buf = (char *)malloc(1024*sizeof(char));
        if(buf == NULL){
               fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
               return (1);
        }
      	sprintf(buf, "\tmember %d value %d position %u\n", series_index, combined_group_index, viz_iteration);
        frame_data_series_list[frame_data_series_count] = buf;
        frame_data_series_count++;
      	series_index++;
      	main_index++;

      	fprintf(master_header, "\n\n");

        if(special_surf_frames_counter == 2){
		special_surf_frames_counter = 0;
        }
        if(special_mol_frames_counter == 2){
		special_mol_frames_counter = 0;
        }
          

	/* put value of viz_iteration into the time_values array */ 
            if(time_values_count == 0){
               time_values[time_values_count] = viz_iteration;  
               time_values_count++;
               last_time_value_written = viz_iteration;
            }else if(viz_iteration > last_time_value_written){
               time_values[time_values_count] = viz_iteration;  
               time_values_count++;
               last_time_value_written = viz_iteration;
            }
     }

     /* flag that signals the need to write "footers" for 
        the master file */
     int time_to_write_footers = 0;
     /* flag pointing to another frame with certain condition */
     int found = 0;
     struct frame_data_list *fdlp_temp = NULL;
     long long next_iteration_step_this_frame = UINT_MAX;  /* next iteration for this frame */
     static long long next_iteration_step_previous_frame = -1;  /* next iteration for previous frame */

     if(fdlp->curr_viz_iteration->next != NULL){
	switch (fdlp->list_type) {
	  case OUTPUT_BY_ITERATION_LIST:
	  	  next_iteration_step_this_frame = (long long)fdlp->curr_viz_iteration->next->value; 
	          break;
	  case OUTPUT_BY_TIME_LIST:
	          next_iteration_step_this_frame = (long long)(fdlp->curr_viz_iteration->next->value/world->time_unit + ROUND_UP);
	          break;
          default:
                  fprintf(world->err_file,"File '%s', Line %ld: error - wrong frame_data_list list_type %d\n", __FILE__, (long)__LINE__, fdlp->list_type);
                  break;
	}
     }
    

     found = 0;
     if(world->chkpt_flag){
        /* check whether it is the last frame */
               
        if((world->it_time == world->iterations) ||
                 (world->it_time == final_iteration))
               
        { 

             /* look forward to find out whether there are 
                other frames to be output */
             for(fdlp_temp = fdlp->next; fdlp_temp != NULL; fdlp_temp = fdlp_temp->next){
                if((fdlp_temp->viz_iterationll == world->iterations) || (fdlp_temp->viz_iterationll == final_iteration)){ 
                   found = 1;
                   break;
                }
             }
             
             if(!found){
                /* this is the last frame */
                    time_to_write_footers = 1; 
             }
        }
        
       else if(((world->iterations < next_iteration_step_this_frame) && (next_iteration_step_this_frame != UINT_MAX)) || (next_iteration_step_this_frame == UINT_MAX)){
        

          /* look forward to find out whether there are 
             other frames to be output */
        for(fdlp_temp = fdlp->next; fdlp_temp != NULL; fdlp_temp = fdlp_temp->next){
                
                if((fdlp_temp->viz_iterationll >= fdlp->viz_iterationll) &&
                    (fdlp_temp->viz_iterationll <= world->iterations)){
                       found = 1;
                       break;
                }
         }
        
        /* this allows to look backwards */
        if(next_iteration_step_this_frame != UINT_MAX){
           if(next_iteration_step_previous_frame > next_iteration_step_this_frame){
                found = 1;
           }
        }else{
                if((next_iteration_step_previous_frame >= fdlp->viz_iterationll) && (next_iteration_step_previous_frame <= world->iterations)){
                   found = 1;
                }

        }
         
         if(!found){
             /* this is the last frame */
             time_to_write_footers = 1; 
          }

     }

    /* end if(world->chkpt_flag) */
   } else if(world->it_time == final_iteration){


        /* look forward to find out whether there are 
           other frames to be output */
        for(fdlp_temp = fdlp->next; fdlp_temp != NULL; fdlp_temp = fdlp_temp->next){
          
                if(fdlp_temp->viz_iterationll == final_iteration) {
                   found = 1;
                   break;
                }
         }
         if(!found){
             /* this is the last frame */
             time_to_write_footers = 1; 
          }
           
    }
    else if((world->iterations < next_iteration_step_this_frame) || (next_iteration_step_this_frame == UINT_MAX)){

        /* look forward to find out whether there are 
           other frames to be output */
        for(fdlp_temp = fdlp->next; fdlp_temp != NULL; fdlp_temp = fdlp_temp->next){
                
                if((fdlp_temp->viz_iterationll >= fdlp->viz_iterationll) &&
                    (fdlp_temp->viz_iterationll <= world->iterations)){
                   found = 1;
                   break;
                }
         }
             
        /* this allows to look backwards */
        if(next_iteration_step_this_frame != UINT_MAX){
           if(next_iteration_step_previous_frame > next_iteration_step_this_frame){
                found = 1;
           }
        }else{
                if((next_iteration_step_previous_frame >= fdlp->viz_iterationll) && (next_iteration_step_previous_frame <= world->iterations)){
                   found = 1;
                }

        }

         if(!found){
             /* this is the last frame */
             time_to_write_footers = 1; 
          }
           
    } /* end if-else(world->chkpt_flag) */


    /* this allows to look backwards to the previous frames */
    if(next_iteration_step_this_frame != UINT_MAX){
          if(next_iteration_step_this_frame > next_iteration_step_previous_frame){
             next_iteration_step_previous_frame = next_iteration_step_this_frame;         }
     }

     if(time_to_write_footers)
     {
       
        u_int elem1;
        double t_value;
        int extra_elems;
        int iteration_numbers_count;
        int dummy = -1;

	/* write 'iteration_numbers' object. */
        if(world->chkpt_flag){
           sprintf(chkpt_seq_num, "%d", world->chkpt_seq_num);
           iteration_numbers_file_path = (char *)malloc(sizeof(char) * (strlen(world->file_prefix_name) + strlen(".iteration_numbers.") +  strlen(chkpt_seq_num) + strlen(".bin") + 1));
           if(iteration_numbers_file_path == NULL){
               fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
               return (1);
           }
            sprintf(iteration_numbers_file_path,"%s.iteration_numbers.%s.bin",world->file_prefix_name, chkpt_seq_num);
        }else{
           iteration_numbers_file_path = (char *)malloc(sizeof(char) * (strlen(world->file_prefix_name) + strlen(".iteration_numbers.bin") + 1));
           if(iteration_numbers_file_path == NULL){
               fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
               return (1);
           }
           sprintf(iteration_numbers_file_path,"%s.iteration_numbers.bin",world->file_prefix_name);
        }

     /* remove the folder name from the iteration_numbers data file name */
        if(viz_dir_depth > 1){
           ch_ptr = strrchr(iteration_numbers_file_path, '/');
           ++ch_ptr;
           iteration_numbers_name = malloc(sizeof(char) * (strlen(ch_ptr) + 1));
           if(iteration_numbers_name == NULL){
               fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
               return (1);
           }
           strcpy(iteration_numbers_name, ch_ptr);
        }else{
           iteration_numbers_name = malloc(sizeof(char) * (strlen(iteration_numbers_file_path) + 1));
           if(iteration_numbers_name == NULL){
               fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
               return (1);
           }

           strcpy(iteration_numbers_name, world->file_prefix_name);
        }

      	if ((iteration_numbers_data=fopen(iteration_numbers_file_path,"wb"))==NULL) {
            fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__, iteration_numbers_file_path);
           return(1);
        }

        /* write "time_values" object. */
        if(world->chkpt_flag){
           sprintf(chkpt_seq_num, "%d", world->chkpt_seq_num);
           time_values_file_path = (char *)malloc(sizeof(char) * (strlen(world->file_prefix_name) + strlen(".time_values.") +  strlen(chkpt_seq_num) + strlen(".bin") + 1));
           if(time_values_file_path == NULL){
               fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
               return (1);
           }
            sprintf(time_values_file_path,"%s.time_values.%s.bin",world->file_prefix_name, chkpt_seq_num);
        }else{
           time_values_file_path = (char *)malloc(sizeof(char) * (strlen(world->file_prefix_name) + strlen(".time_values.bin") + 1));
           if(time_values_file_path == NULL){
               fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
               return (1);
           }
           sprintf(time_values_file_path,"%s.time_values.bin",world->file_prefix_name);
        }

     	/* remove the folder name from the time_values data file name */
        if(viz_dir_depth > 1){
           ch_ptr = strrchr(time_values_file_path, '/');
           ++ch_ptr;
           time_values_name = malloc(sizeof(char) * (strlen(ch_ptr) + 1));
           if(time_values_name == NULL){
               fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
               return (1);
           }
           strcpy(time_values_name, ch_ptr);
        }else{
           time_values_name = malloc(sizeof(char) * (strlen(time_values_file_path) + 1));
           if(time_values_name == NULL){
               fprintf(world->err_file, "File %s, Line %ld: memory allocation error.\n", __FILE__, (long)__LINE__);
               return (1);
           }
           strcpy(time_values_name, world->file_prefix_name);
        }

      	if ((time_values_data=fopen(time_values_file_path,"wb"))==NULL) {
            fprintf(world->err_file, "File %s, Line %ld: cannot open file %s.\n", __FILE__, (long)__LINE__, time_values_file_path);
            return(1);
        }

      if(iteration_numbers_vol_mols_count > iteration_numbers_surf_mols_count){
         iteration_numbers_count = iteration_numbers_vol_mols_count;
      }else{
         iteration_numbers_count = iteration_numbers_surf_mols_count;
      }
      if(iteration_numbers_meshes_count > iteration_numbers_count){
         iteration_numbers_count = iteration_numbers_meshes_count;
      }
      
        if(iteration_numbers_meshes_count > 0)
        {
           extra_elems = iteration_numbers_count - iteration_numbers_meshes_count;
           if(extra_elems > 0){
              /* pad the iteration_numbers_meshes array with the last 
                 element so that it will have the same number of elements 
                 as the maximum length array */
              elem1 = iteration_numbers_meshes[iteration_numbers_meshes_count - 1];
              
		for(ii = 0; ii < extra_elems; ii++){
                   iteration_numbers_meshes[iteration_numbers_meshes_count + ii] =  elem1;
                }
            }
            iteration_numbers_meshes_count += extra_elems;
          
        }else{
            /* pad the 'iteration_numbers_meshes' array with UINT_MAX */
            for(ii = 0; ii < iteration_numbers_count; ii++){
               iteration_numbers_meshes[ii] = UINT_MAX;
            }
            iteration_numbers_meshes_count = iteration_numbers_count;
        }

        if(iteration_numbers_vol_mols_count > 0)
        {
           extra_elems = iteration_numbers_count - iteration_numbers_vol_mols_count;
           if(extra_elems > 0){
              /* pad the iteration_numbers_vol_mols array with the last 
                 element so that it will have the same number of elements 
                 as the maximum length array */
              elem1 = iteration_numbers_vol_mols[iteration_numbers_vol_mols_count - 1];
              
		for(ii = 0; ii < extra_elems; ii++){
                   iteration_numbers_vol_mols[iteration_numbers_vol_mols_count + ii] = elem1;
                }
           }
           iteration_numbers_vol_mols_count += extra_elems;
        }else{
            /* pad the 'iteration_numbers_vol_mols' array with UINT_MAX */
            for(ii = 0; ii < iteration_numbers_count; ii++){
               iteration_numbers_vol_mols[ii] = UINT_MAX;
            }
            iteration_numbers_vol_mols_count = iteration_numbers_count;
        }

        if(iteration_numbers_surf_mols_count > 0)
        {
           extra_elems = iteration_numbers_count - iteration_numbers_surf_mols_count;
           if(extra_elems > 0){
              /* pad the iteration_numbers_surf_mols array with the last 
                 element so that it will have the same number of elements 
                 as the maximum length array */
              elem1 = iteration_numbers_surf_mols[iteration_numbers_surf_mols_count - 1];
              
		for(ii = 0; ii < extra_elems; ii++){
                   iteration_numbers_surf_mols[iteration_numbers_surf_mols_count + ii] = elem1;
                }
           }
           iteration_numbers_surf_mols_count += extra_elems;
        }else{
            /* pad the 'iteration_numbers_surf_mols' array with UINT_MAX */
            for(ii = 0; ii < iteration_numbers_count; ii++){
               iteration_numbers_surf_mols[ii] = UINT_MAX;
            }
            iteration_numbers_surf_mols_count = iteration_numbers_count;

        }

      int dreamm3mode_number = 0;
      char dreamm3mode[1024];
      if(world->viz_mode == DREAMM_V3_MODE){
         dreamm3mode_number = 1;
         sprintf(dreamm3mode, "DREAMM_V3_MODE");
      }else if(world->viz_mode == DREAMM_V3_GROUPED_MODE){
         dreamm3mode_number = 2;
         sprintf(dreamm3mode, "DREAMM_V3_GROUPED_MODE");
      }      

      fprintf(master_header,"object \"iteration_numbers\" class array  type unsigned int rank 1 shape 3 items %u %s binary data file %s,%d\n",iteration_numbers_count, my_byte_order,iteration_numbers_name, iteration_numbers_byte_offset);
       fprintf(master_header,"\tattribute \"dreamm3mode\" number %d\t#%s#\n", dreamm3mode_number, dreamm3mode);
        for(ii = 0; ii < iteration_numbers_count; ii++){
                elem1 = iteration_numbers_meshes[ii];
                if(elem1 == UINT_MAX) 
                {
                   fwrite(&dummy, sizeof(dummy),1,iteration_numbers_data);
                }else{
                   fwrite(&elem1, sizeof(elem1),1,iteration_numbers_data);
                }
                elem1 = iteration_numbers_vol_mols[ii];
                if(elem1 == UINT_MAX) 
                {
                   fwrite(&dummy, sizeof(dummy),1,iteration_numbers_data);
                }else{
                   fwrite(&elem1, sizeof(elem1),1,iteration_numbers_data);
                }

                elem1 = iteration_numbers_surf_mols[ii];
                if(elem1 == UINT_MAX) 
                {
                   fwrite(&dummy, sizeof(dummy),1,iteration_numbers_data);
                }else{
                   fwrite(&elem1, sizeof(elem1),1,iteration_numbers_data);
                }

        }
     	fprintf(master_header, "\n\n");

        if(time_values_count > 0)
        {
        	fprintf(master_header,"object \"time_values\" class array  type double rank 0 items %u %s binary data file %s,%d\n",time_values_count, my_byte_order,time_values_name, time_values_byte_offset);
                fprintf(master_header,"\tattribute \"dreamm3mode\" number %d\t#%s#\n", dreamm3mode_number, dreamm3mode);
												for(ii = 0; ii < time_values_count; ii++){
                	elem1 = time_values[ii];
                        if(elem1 == UINT_MAX) 
                        {
                           fwrite(&(dummy), sizeof(dummy),1,time_values_data);
                        }else{
                           t_value = elem1*world->time_unit;
                           fwrite(&(t_value), sizeof(t_value),1,time_values_data);
                        }

                }
		fprintf(master_header, "\n\n");
	}



                /* write 'frame_data' object. */
    		fprintf(master_header,"object \"%s\" class series\n", "frame_data");
        	if(frame_data_series_count > 0)
        	{
        	   char *elem;
		   for(ii = 0; ii < frame_data_series_count; ii++){
                      elem = frame_data_series_list[ii];
                       if(elem == NULL){
                          fprintf(world->err_file, "File %s, Line %ld:  attempt of access to uninitialized data.\n", __FILE__, (long)__LINE__);
                        }
			fprintf(master_header, "\t%s", elem);
        	    }
                 }
	         fprintf(master_header, "\n\n");


    } /* end if(time_to_write_footers) */

 
   /* free allocated memory */
   if (viz_molp != NULL) {
    for (ii=0;ii<n_species;ii++) {
      if (viz_molp[ii]!=NULL) {
        free(viz_molp[ii]);
        viz_molp[ii] = NULL;
      }
    }
    
    free(viz_molp);
    viz_molp = NULL;
  }
   
  if (viz_mol_count != NULL) {
    free (viz_mol_count);
    viz_mol_count = NULL;
  }


  if (viz_grid_mol_count != NULL) {
    free (viz_grid_mol_count);
    viz_grid_mol_count = NULL;
  }
  
    if(master_header != NULL){
    	fclose(master_header);
    }
    if(mol_pos_data != NULL){
    	fclose(mol_pos_data);
    }
    if(mol_orient_data != NULL){
    	fclose(mol_orient_data);
    }
    if(mol_states_data != NULL){
    	fclose(mol_states_data);
    }
    if(mesh_pos_data != NULL){
    	fclose(mesh_pos_data);
    }
    if(mesh_states_data != NULL){
    	fclose(mesh_states_data);
    }
    if(region_data != NULL){
    	fclose(region_data);
    }
    if(iteration_numbers_data != NULL){
        fclose(iteration_numbers_data);
    }
    if(time_values_data != NULL){
        fclose(time_values_data);
    }
             
    if(time_to_write_footers)
    {
      if(master_header_file_path != NULL) free(master_header_file_path); 
      if(mol_pos_name != NULL) free(mol_pos_name);
      mol_pos_name = NULL;
      if(mol_pos_file_path != NULL) free(mol_pos_file_path);
      mol_pos_file_path = NULL;
      if(mol_orient_name != NULL) free(mol_orient_name);
      mol_orient_name = NULL;
      if(mol_orient_file_path != NULL) free(mol_orient_file_path);
      mol_orient_file_path = NULL;
      if(mol_states_name != NULL) free(mol_states_name);
      mol_states_name = NULL;
      if(mol_states_file_path != NULL) free(mol_states_file_path);
      mol_states_file_path = NULL;
      if(mesh_pos_name != NULL) free(mesh_pos_name);
      mesh_pos_name = NULL;
      if(mesh_pos_file_path != NULL) free(mesh_pos_file_path);
      mesh_pos_file_path = NULL;
      if(mesh_states_name != NULL) free(mesh_states_name);
      mesh_states_name = NULL;
      if(mesh_states_file_path != NULL) free(mesh_states_file_path);
      mesh_states_file_path = NULL;
      if(region_viz_data_name != NULL) free(region_viz_data_name);
      region_viz_data_name = NULL;
      if(region_viz_data_file_path != NULL) free(region_viz_data_file_path);
      region_viz_data_file_path = NULL;
      if(iteration_numbers_name != NULL) free(iteration_numbers_name);
      iteration_numbers_name = NULL;
      if(iteration_numbers_file_path != NULL) free(iteration_numbers_file_path);
      iteration_numbers_file_path = NULL;
      if(time_values_name != NULL) free(time_values_name);
      time_values_name = NULL;
      if(time_values_file_path != NULL) free(time_values_file_path);
      time_values_file_path = NULL;
                
      if(time_values != NULL){
         free(time_values);
         time_values = NULL;
      }
      if(iteration_numbers_meshes != NULL){
         free(iteration_numbers_meshes);
         iteration_numbers_meshes = NULL;
      }
      if(iteration_numbers_vol_mols != NULL){
         free(iteration_numbers_vol_mols);
         iteration_numbers_vol_mols = NULL;
      }
      if(iteration_numbers_surf_mols != NULL){
         free(iteration_numbers_surf_mols);
         iteration_numbers_surf_mols = NULL;
      }
      if(surf_states != NULL){
         free(surf_states);
         surf_states = NULL;
      }
      if(surf_pos != NULL){
         free(surf_pos);
         surf_pos = NULL;
      }
      if(surf_con != NULL){
         free(surf_con);
         surf_con = NULL;
      }
      if(surf_field_indices != NULL){
         free(surf_field_indices);
         surf_field_indices = NULL;
      }
      if(eff_pos != NULL){
         free(eff_pos);
         eff_pos = NULL;
         
      }
      if(eff_orient != NULL){
         free(eff_orient);
         eff_orient = NULL;
      }
      if(eff_states != NULL){
         free(eff_states);
         eff_states = NULL;
      }
      if(eff_field_indices != NULL){
         free(eff_field_indices);
         eff_field_indices = NULL;
      }
      if(mol_pos != NULL){
         free(mol_pos);
         mol_pos = NULL;
      }
      if(mol_orient != NULL){
         free(mol_orient);
         mol_orient = NULL;
      }
      if(mol_states != NULL){
         free(mol_states);
         mol_states = NULL;
      }
      if(mol_field_indices != NULL){
         free(mol_field_indices);
         mol_field_indices = NULL;
      }

      if (eff_names != NULL) {
        for (ii = 0; ii < eff_to_show_number; ii++) {
          if (eff_names[ii] != NULL) {
            free(eff_names[ii]);
            eff_names[ii] = NULL;
          }
        }
    
        free(eff_names);
        eff_names = NULL;
      }
      if (mol_names != NULL) {
        for (ii = 0; ii < mol_to_show_number; ii++) {
          if (mol_names[ii] != NULL) {
            free(mol_names[ii]);
            mol_names[ii] = NULL;
          }
        }
    
        free(mol_names);
        mol_names = NULL;
      }
      if (obj_names != NULL) {
        for (ii = 0; ii < obj_to_show_number; ii++) {
          if (obj_names[ii] != NULL) {
            free(obj_names[ii]);
            obj_names[ii] = NULL;
          }
        }
    
        free(obj_names);
        obj_names = NULL;
      }
      if (surf_region_values != NULL) {
        for(ii = 0; ii < obj_to_show_number; ii++){
           if (surf_region_values[ii] != NULL) {
              free(surf_region_values[ii]);
              surf_region_values[ii] = NULL;
           }
        }
    
        free(surf_region_values);
        surf_region_values = NULL;
      }
      if (region_names != NULL) {
        for(ii = 0; ii < obj_to_show_number; ii++){
           if (region_names[ii] != NULL) {
              free(region_names[ii]);
              region_names[ii] = NULL;
           }
        } 
        free(region_names);
        region_names = NULL;
      }
      if(obj_num_regions != NULL){
         free(obj_num_regions);
         obj_num_regions = NULL;
      }

   }



  return(0);

}


/************************************************************************ 
output_rk_custom:
Rex Kerr's personal visualization mode output function 
*************************************************************************/

int output_rk_custom(struct frame_data_list *fdlp)
{
  FILE *log_file;
  FILE *custom_file;
  char cf_name[1024];
  struct storage_list *slp;
  struct schedule_helper *shp;
  struct abstract_element *aep;
  struct abstract_molecule *amp;
  struct volume_molecule *mp;
  struct grid_molecule *gmp;
  short orient = 0;
  struct species *target;
  
  int i,j,k;
  double d;

  int id;
  struct vector3 where;
  
  no_printf("Output in CUSTOM_RK mode...\n");
  log_file = world->log_file;
  
  if (world->rk_mode_var==NULL) return output_ascii_molecules(fdlp);
  
  if ((fdlp->type==ALL_FRAME_DATA) || (fdlp->type==MOL_POS) || (fdlp->type==MOL_STATES))
  {
    sprintf(cf_name,"%s.rk.dat",world->molecule_prefix_name);
    custom_file = fopen(cf_name,(world->rk_mode_var->n_written)?"a+":"w");
    if (!custom_file)
    {
      fprintf(log_file,"Couldn't open file %s for viz output.\n",cf_name);
      return 1;
    }
    else no_printf("Writing to file %s\n",cf_name);
    
    world->rk_mode_var->n_written++;
    fprintf(custom_file,"%lld",fdlp->viz_iterationll);
    for (j=0;j<world->n_species;j++)
    {
      target = world->species_list[j];
      if (target==NULL) continue;
      if (target->viz_state==EXCLUDE_OBJ) continue;
      for (k=0;k<world->rk_mode_var->n_bins;k++) world->rk_mode_var->bins[k] = 0;
      id = target->viz_state;
      
      fprintf(custom_file,"\t%d",id);
      
      for (slp = world->storage_head ; slp != NULL ; slp = slp->next)
      {
	for (shp = slp->store->timer ; shp != NULL ; shp = shp->next_scale)
	{
	  for (i=-1;i<shp->buf_len;i++)
	  {
	    if (i<0) aep = shp->current;
	    else aep = shp->circ_buf_head[i];
	    
	    for (aep=(i<0)?shp->current:shp->circ_buf_head[i] ; aep!=NULL ; aep=aep->next)
	    {
	      amp = (struct abstract_molecule*)aep;
	      if (amp->properties != target) continue;
  
	      if ((amp->properties->flags & NOT_FREE)==0)
	      {
		mp = (struct volume_molecule*)amp;
		where.x = mp->pos.x;
		where.y = mp->pos.y;
		where.z = mp->pos.z;
	      }
	      else if ((amp->properties->flags & ON_GRID)!=0)
	      {
		gmp = (struct grid_molecule*)amp;
		uv2xyz(&(gmp->s_pos),gmp->grid->surface,&where);
		orient = gmp->orient;
	      }
	      else continue;
	      
	      d = dot_prod(&where , world->rk_mode_var->direction);
	      
	      k = bin(world->rk_mode_var->parts,world->rk_mode_var->n_bins-1,d);
	      world->rk_mode_var->bins[k]++;
	    }
	  }
	}
      }
      for (i=k=0 ; k<world->rk_mode_var->n_bins ; k++) i+=world->rk_mode_var->bins[k];
      if (i!=target->population) printf("Wanted to bin %d but found %d instead\n",target->population,i);
      for (k=0 ; k<world->rk_mode_var->n_bins ; k++) fprintf(custom_file," %d",world->rk_mode_var->bins[k]);
    }
    fprintf(custom_file,"\n");
    fclose(custom_file);
  }
  
  return 0;
}


/************************************************************************ 
output_ascii_molecules:
In: a frame data list (internal viz output data structure)
Out: 0 on success, 1 on failure.  The positions of molecules are output
     in exponential floating point notation (with 8 decimal places)
*************************************************************************/

int output_ascii_molecules(struct frame_data_list *fdlp)
{
  FILE *log_file;
  FILE *custom_file;
  char cf_name[1024];
  char cf_format[256];
  struct storage_list *slp;
  struct schedule_helper *shp;
  struct abstract_element *aep;
  struct abstract_molecule *amp;
  struct volume_molecule *mp;
  struct grid_molecule *gmp;
  short orient = 0;
  
  int ndigits,i;
  long long lli;

  int id;
  struct vector3 where;
  
  no_printf("Output in ASCII mode (molecules only)...\n");
  log_file = world->log_file;
  
  if ((fdlp->type==ALL_FRAME_DATA) || (fdlp->type==MOL_POS) || (fdlp->type==MOL_STATES))
  {
    lli = 10;
    for (ndigits = 1 ; lli <= world->iterations && ndigits<20 ; lli*=10 , ndigits++) {}
    sprintf(cf_format,"%%s.ascii.%%0%dlld.dat",ndigits);
    sprintf(cf_name,cf_format,world->molecule_prefix_name,fdlp->viz_iterationll);
    custom_file = fopen(cf_name,"w");
    if (!custom_file)
    {
      fprintf(log_file,"Couldn't open file %s for viz output.\n",cf_name);
      return 1;
    }
    else no_printf("Writing to file %s\n",cf_name);
    
    for (slp = world->storage_head ; slp != NULL ; slp = slp->next)
    {
      for (shp = slp->store->timer ; shp != NULL ; shp = shp->next_scale)
      {
        for (i=-1;i<shp->buf_len;i++)
        {
          if (i<0) aep = shp->current;
          else aep = shp->circ_buf_head[i];
          
          for (aep=(i<0)?shp->current:shp->circ_buf_head[i] ; aep!=NULL ; aep=aep->next)
          {
            amp = (struct abstract_molecule*)aep;
            if (amp->properties == NULL) continue;
            if (amp->properties->viz_state == EXCLUDE_OBJ) continue;

            id = amp->properties->viz_state;
            
            if ((amp->properties->flags & NOT_FREE)==0)
            {
              mp = (struct volume_molecule*)amp;
              where.x = mp->pos.x;
              where.y = mp->pos.y;
              where.z = mp->pos.z;
            }
            else if ((amp->properties->flags & ON_GRID)!=0)
            {
              gmp = (struct grid_molecule*)amp;
              uv2xyz(&(gmp->s_pos),gmp->grid->surface,&where);
              orient = gmp->orient;
            }
            else continue;
            
            where.x *= world->length_unit;
            where.y *= world->length_unit;
            where.z *= world->length_unit;
            fprintf(custom_file,"%d %15.8e %15.8e %15.8e %2d\n",id,where.x,where.y,where.z,orient);
          }
        }
      }
    }
    fclose(custom_file);
  }
  
  return 0;
}


