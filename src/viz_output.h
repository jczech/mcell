#ifndef VIZ_OUTPUT_H
#define VIZ_OUTPUT_H

/* Header file for visualization output routines */

void update_frame_data_list(struct frame_data_list *fdlp);
void init_frame_data_list(struct frame_data_list *fdlp);
int output_dx_objects(struct frame_data_list *fdlp);

/*
int output_radiance_objects(struct frame_data_list *fdlp);
int output_rayshade_objects(struct frame_data_list *fdlp);
int output_povray_objects(struct frame_data_list *fdlp);
int output_renderman_objects(struct frame_data_list *fdlp);
int output_irit_objects(struct frame_data_list *fdlp);
int output_mcell_objects(struct frame_data_list *fdlp);
int output_voxel_image(struct frame_data_list *fdlp);
int output_voxel_volume(struct frame_data_list *fdlp);
*/


#endif