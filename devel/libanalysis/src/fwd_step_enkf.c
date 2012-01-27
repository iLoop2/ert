/*
   Copyright (C) 2011  Statoil ASA, Norway. 
    
   The file 'fwd_step_enkf.c' is part of ERT - Ensemble based Reservoir Tool. 
    
   ERT is free software: you can redistribute it and/or modify 
   it under the terms of the GNU General Public License as published by 
   the Free Software Foundation, either version 3 of the License, or 
   (at your option) any later version. 
    
   ERT is distributed in the hope that it will be useful, but WITHOUT ANY 
   WARRANTY; without even the implied warranty of MERCHANTABILITY or 
   FITNESS FOR A PARTICULAR PURPOSE.   
    
   See the GNU General Public License at <http://www.gnu.org/licenses/gpl.html> 
   for more details. 
*/

#include <stdlib.h>
#include <string.h>
#include <util.h>
#include <rng.h>
#include <matrix.h>
#include <matrix_blas.h>
#include <stdio.h>
#include <analysis_table.h>
#include <analysis_module.h>
#include <enkf_linalg.h>
#include <stepwise.h>
#include <fwd_step_enkf.h>
#include <math.h>

#define FWD_STEP_ENKF_TYPE_ID 765524

#define DEFAULT_NFOLDS              5
#define DEFAULT_R2_LIMIT            0.99
#define NFOLDS_KEY                  "CV_NFOLDS"
#define R2_LIMIT_KEY                "FWD_STEP_R2_LIMIT"


struct fwd_step_enkf_data_struct {
  UTIL_TYPE_ID_DECLARATION;
  stepwise_type        * stepwise_data; 
  rng_type             * rng;
  int                    nfolds;
  long                   option_flags;
  double                 r2_limit;
};



static UTIL_SAFE_CAST_FUNCTION( fwd_step_enkf_data , FWD_STEP_ENKF_TYPE_ID )


void fwd_step_enkf_set_nfolds( fwd_step_enkf_data_type * data , int nfolds ) {
  data->nfolds = nfolds;
}

void fwd_step_enkf_set_r2_limit( fwd_step_enkf_data_type * data , double limit ) {
  data->r2_limit = limit;
}


void * fwd_step_enkf_data_alloc( rng_type * rng ) {
  fwd_step_enkf_data_type * data = util_malloc( sizeof * data , __func__ );
  UTIL_TYPE_ID_INIT( data , FWD_STEP_ENKF_TYPE_ID );

  data->rng          = rng;
  data->nfolds       = DEFAULT_NFOLDS;
  data->r2_limit     = DEFAULT_R2_LIMIT;
  data->option_flags = ANALYSIS_NEED_ED + ANALYSIS_UPDATE_A;

  return data;
}



/*Main function: */
void fwd_step_enkf_updateA(void * module_data , 
			   matrix_type * A , 
			   matrix_type * S , 
			   matrix_type * R , 
			   matrix_type * dObs , 
			   matrix_type * E ,
			   matrix_type * D ,
			   matrix_type * randrot) {

  
  printf("Inside fwd_step_enkf_updateA\n");
  fwd_step_enkf_data_type * fwd_step_data = fwd_step_enkf_data_safe_cast( module_data );
  printf("Running Forward Stepwise regression:\n");
  {
        
    int ens_size = matrix_get_columns( S );
    int nx = matrix_get_rows( A );
    int nd = matrix_get_rows( S );
    
    {
      /*Start of the actual update */
      matrix_type * y = matrix_alloc( ens_size , 1 );

      stepwise_type * stepwise_data = stepwise_alloc1(ens_size, nd , fwd_step_data->rng);
      matrix_type * workS = matrix_alloc( ens_size , nd  );

      
      /*workS = S' */
      for (int i = 0; i < nd; i++) {
	for (int j = 0; j < ens_size; j++) {
	  matrix_iset( workS , j , i , matrix_iget( S , i , j ) );
	}
      }
						      
      /*This might be illigal???? */
      stepwise_set_X0( stepwise_data , workS );
      
      double xHat;

      matrix_type * di = matrix_alloc( 1 , nd );


      for (int i = 0; i < nx; i++) {
	/*Update values of y */
	for (int j = 0; j < ens_size; j++) {
	  matrix_iset(y , j , 1 , matrix_iget( A, i , j ) );
	}
	
	/*This might be illigal???? */
	stepwise_set_Y0( stepwise_data , y );
	
	stepwise_estimate(stepwise_data , fwd_step_data->r2_limit , fwd_step_data->nfolds );
	
	/*manipulate A directly*/
	for (int j = 0; j < ens_size; j++) {
	  for (int k = 0; k < nd; k++) {
	    matrix_iset(di , 1 , k , matrix_iget( D , k , j ) );
	  }
	  
	  xHat = stepwise_eval(stepwise_data , di );
	  matrix_iset(A , i , j , xHat);
	}
	
	
	
      }
						      
      
      stepwise_free( stepwise_data );
      matrix_free( di );
      matrix_free( workS );
      matrix_free( y );
      
	
    }
    
    
    
  }
  
  
}




void fwd_step_enkf_data_free( void * arg ) {
  fwd_step_enkf_data_type * fwd_step_data = fwd_step_enkf_data_safe_cast( arg );
  {
    stepwise_free( fwd_step_data->stepwise_data );
  }
}


bool fwd_step_enkf_set_double( void * arg , const char * var_name , double value) {
  fwd_step_enkf_data_type * module_data = fwd_step_enkf_data_safe_cast( arg );
  {
    bool name_recognized = true;

    if (strcmp( var_name , R2_LIMIT_KEY ) == 0)
      fwd_step_enkf_set_r2_limit( module_data , value );
    else
      name_recognized = false;

    return name_recognized;
  }
}


bool fwd_step_enkf_set_int( void * arg , const char * var_name , int value) {
  fwd_step_enkf_data_type * module_data = fwd_step_enkf_data_safe_cast( arg );
  {
    bool name_recognized = true;
    
    /*Set number of CV folds */
    if (strcmp( var_name , NFOLDS_KEY) == 0)
      fwd_step_enkf_set_nfolds( module_data , value);
    else
      name_recognized = false;
    
    return name_recognized;
   }
}

long fwd_step_enkf_get_options( void * arg , long flag) {
  fwd_step_enkf_data_type * fwd_step_data = fwd_step_enkf_data_safe_cast( arg );
  {
    return fwd_step_data->option_flags;
  }
}




#ifdef INTERNAL_LINK
#define SYMBOL_TABLE fwd_step_enkf_symbol_table
#else
#define SYMBOL_TABLE EXTERNAL_MODULE_SYMBOL
#endif


analysis_table_type SYMBOL_TABLE = {
  .alloc           = fwd_step_enkf_data_alloc,
  .freef           = fwd_step_enkf_data_free,
  .set_int         = fwd_step_enkf_set_int , 
  .set_double      = fwd_step_enkf_set_double , 
  .set_bool        = NULL , 
  .set_string      = NULL , 
  .get_options     = fwd_step_enkf_get_options , 
  .initX           = NULL , 
  .updateA         = fwd_step_enkf_updateA,
  .init_update     = NULL ,
  .complete_update = NULL ,
};


