//------------------------------------------------------------------------------------------------------------------------------
// Samuel Williams
// SWWilliams@lbl.gov
// Lawrence Berkeley National Lab
//------------------------------------------------------------------------------------------------------------------------------
//#define GSRB_STRIDE2
//#define GSRB_FP
//------------------------------------------------------------------------------------------------------------------------------
void smooth(level_type * level, int phi_id, int rhs_id, double a, double b){
  int box,s;
  int ghosts = level->box_ghosts;
  int radius     = STENCIL_RADIUS;
  int starShaped = STENCIL_STAR_SHAPED;
  int communicationAvoiding = ghosts > radius;  

  // if communication-avoiding, need updated RHS for stencils in ghost zones
  if(communicationAvoiding)exchange_boundary(level,rhs_id,0); 

  for(s=0;s<2*NUM_SMOOTHS;s+=ghosts){ // there are two sweeps per GSRB smooth
    exchange_boundary(level,phi_id,starShaped && !communicationAvoiding);
            apply_BCs(level,phi_id);

    // now do ghosts communication-avoiding smooths on each box...
    uint64_t _timeStart = CycleTime();
    #pragma omp parallel for private(box) num_threads(level->concurrent_boxes)
    for(box=0;box<level->num_my_boxes;box++){
      int i,j,k,ss;
      const int jStride = level->my_boxes[box].jStride;
      const int kStride = level->my_boxes[box].kStride;
      const int     dim = level->my_boxes[box].dim;
      const double h2inv = 1.0/(level->h*level->h);
      const double * __restrict__ phi      = level->my_boxes[box].components[        phi_id] + ghosts*(1+jStride+kStride); // i.e. [0] = first non ghost zone point
            double * __restrict__ phi_new  = level->my_boxes[box].components[        phi_id] + ghosts*(1+jStride+kStride); // i.e. [0] = first non ghost zone point
      const double * __restrict__ rhs      = level->my_boxes[box].components[        rhs_id] + ghosts*(1+jStride+kStride);
      const double * __restrict__ alpha    = level->my_boxes[box].components[STENCIL_ALPHA ] + ghosts*(1+jStride+kStride);
      const double * __restrict__ beta_i   = level->my_boxes[box].components[STENCIL_BETA_I] + ghosts*(1+jStride+kStride);
      const double * __restrict__ beta_j   = level->my_boxes[box].components[STENCIL_BETA_J] + ghosts*(1+jStride+kStride);
      const double * __restrict__ beta_k   = level->my_boxes[box].components[STENCIL_BETA_K] + ghosts*(1+jStride+kStride);
      const double * __restrict__ Dinv     = level->my_boxes[box].components[STENCIL_DINV  ] + ghosts*(1+jStride+kStride);
      const double * __restrict__ valid    = level->my_boxes[box].components[STENCIL_VALID ] + ghosts*(1+jStride+kStride); // cell is inside the domain
      const double * __restrict__ RedBlack[2] = {level->RedBlack_FP[0] + ghosts*(1+jStride), 
                                                 level->RedBlack_FP[1] + ghosts*(1+jStride)};
          

      int ghostsToOperateOn=ghosts-1;
      for(ss=s;ss<s+ghosts;ss++,ghostsToOperateOn--){
        #if defined(GSRB_FP)
        #warning GSRB using pre-computed 1.0/0.0 FP array for Red-Black
        #pragma omp parallel for private(k,j,i) num_threads(level->threads_per_box) OMP_COLLAPSE
        for(k=0-ghostsToOperateOn;k<dim+ghostsToOperateOn;k++){
        for(j=0-ghostsToOperateOn;j<dim+ghostsToOperateOn;j++){
        for(i=0-ghostsToOperateOn;i<dim+ghostsToOperateOn;i++){
              int EvenOdd = (k^ss)&1;
              int ij  = i + j*jStride;
              int ijk = i + j*jStride + k*kStride;
              double Ax     = apply_op_ijk(phi);
              double lambda =     Dinv_ijk();
              phi_new[ijk] = phi[ijk] + RedBlack[EvenOdd][ij]*lambda*(rhs[ijk]-Ax); // compiler seems to get confused unless there are disjoint read/write pointers
        }}}
        #elif defined(GSRB_STRIDE2)
        #warning GSRB using stride-2 accesses
        #pragma omp parallel for private(k,j,i) num_threads(level->threads_per_box) OMP_COLLAPSE
        for(k=0-ghostsToOperateOn;k<dim+ghostsToOperateOn;k++){
        for(j=0-ghostsToOperateOn;j<dim+ghostsToOperateOn;j++){
        for(i=((j^k^ss)&1)+1-ghosts;i<dim+ghostsToOperateOn;i+=2){ // stride-2 GSRB
              int ijk = i + j*jStride + k*kStride; 
              double Ax     = apply_op_ijk(phi);
              double lambda =     Dinv_ijk();
              phi_new[ijk] = phi[ijk] + lambda*(rhs[ijk]-Ax);
        }}}
        #else
        #warning GSRB using if-then-else on loop indices for Red-Black
        #pragma omp parallel for private(k,j,i) num_threads(level->threads_per_box) OMP_COLLAPSE
        for(k=0-ghostsToOperateOn;k<dim+ghostsToOperateOn;k++){
        for(j=0-ghostsToOperateOn;j<dim+ghostsToOperateOn;j++){
        for(i=0-ghostsToOperateOn;i<dim+ghostsToOperateOn;i++){
        if((i^j^k^ss^1)&1){ // looks very clean when [0] is i,j,k=0,0,0 
              int ijk = i + j*jStride + k*kStride;
              double Ax     = apply_op_ijk(phi);
              double lambda =     Dinv_ijk();
              phi_new[ijk] = phi[ijk] + lambda*(rhs[ijk]-Ax);
        }}}}
        #endif
      } // ss-loop
    } // boxes
    level->cycles.smooth += (uint64_t)(CycleTime()-_timeStart);
  } // s-loop
}


//------------------------------------------------------------------------------------------------------------------------------