/* 
   Description
*/


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <debugtrace.h>
#include <mrilib.h>    
#include <3ddata.h>    
//#include <rsfc.h>    
//#include <gsl/gsl_rng.h>
#include "DoTrackit.h"


int check_make_rai( THD_3dim_dataset *A, 
                    char *dset_or );

int find_bad_slices_streak( float **slipar,
                            int *Nmskd,
                            int **slibad,
                            int *Dim,
                            int   MIN_STREAK_LEN,
                            float MIN_STREAK_WARN );

int find_bad_slices_drop( float **slipar,
                          int *Nmskd,
                          int **slibad,
                          int *Dim,
                          float MIN_DROP_DIFF,
                          float MIN_DROP_FRAC );

/* maybe come back to later...
int do_calc_entrop( float **diffarr,
                    int *Nmskd2,
                    int *mskd2,
                    float **slient,
                    int *Dim);
*/


void usage_ZipperZapper(int detail) 
{
   printf(
" # ------------------------------------------------------------------------\n"
" \n"
" This is a basic program to help highlight problematic volumes in data\n"
" sets, specifically in EPI/DWI data sets with interleaved acquisition.\n"
" \n"
" Intra-volume subject motion can be quite problematic, potentially\n"
" bad-ifying the data values in the volume so much that it is basically\n"
" useless for analysis.  In FMRI analysis, outlier counts might be\n"
" useful to find ensuing badness (e.g., via 3dToutcount). However, with\n"
" DWI data, we might want to find it without aligning the volumes\n"
" (esp. due to the necessarily differing contrasts) and without tensor\n"
" fitting.\n"
" \n"
" Therefore, this program will look through axial slices of a data set\n"
" for brightness fluctuations and/or dropout slices.  It will build a\n"
" list of volumes indices that it identifies as bad, and the user can\n"
" then use something like the 'fat_proc_filter_dwis' program after to\n"
" apply the filtration to the volumetric dset *as well as* to any\n"
" accompanying b-value, gradient vector, b-matrix, etc., text files.\n"
" \n"
" The program works by looking for alternating brightness patterns in\n"
" the data (again, specifically in axial slices, so if your data was\n"
" acquired differently, this program ain't for you! (weeellll, some\n"
" tricks with changing header info miiiight be able to work then)).  It\n"
" should be run *before* any processing, particularly alignments or\n"
" unwarping things, because those could change the slice locations.\n"
" Additionally, it has mainly been tested on 3T data of humans; it is\n"
" possible that it will work equally well on 7T or non-humans, but be\n"
" sure to check results carefully in the latter cases (well, *always*\n"
" check your data carefully!).\n"
" \n"
" Note that there is also 'fat_proc_select_vols' program for\n"
" interactively selecting out bad volumes, by looking at a sheet of\n"
" sagittal images from the DWI set.  That might be useful for amending\n"
" or altering the output from this program, if necessary.\n"
" \n"
" written by PA Taylor (started Jan, 2018)\n"
" \n"
" * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\n"
" \n"
" USAGE:\n"
" \n"
"     Input: + a 3D+time data set of DWI or EPI volumes,\n"
"            + a mask of the brain-ish region.\n"
"    \n"
"    Output: + a map of potentially bad slices across the input dset,\n"
"            + a list of the bad volumes,\n"
"            + a 1D file of the per-volume parameters used to detect\n"
"              badness,\n"
"            + a text file with the selector string of *good* volumes\n"
"              in the dset (for easy use with fat_proc_filter_dwis, \n"
"              for example).\n"
" \n"
" * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\n"
" \n"
" RUNNING: \n"
"  \n"
" \n"
" \n"
" # ------------------------------------------------------------------\n"
" \n"
" # ------------------------------------------------------------------------\n"
          );
	return;
}

int main(int argc, char *argv[]) {
   int i,j,k,m,n,mm;
   int idx=0;
   int iarg;

   THD_3dim_dataset *insetA = NULL;
   THD_3dim_dataset *MASK=NULL;
   char *prefix="PREFIX" ;
   char tprefixx[THD_MAX_PREFIX];
   char tprefixy[THD_MAX_PREFIX];
   char bprefix[THD_MAX_PREFIX];
   char lprefix[THD_MAX_PREFIX];

   // char in_name[300];

   FILE *fout0, *fout1;

   int Nvox=-1;   // tot number vox
   int *Dim=NULL;
   byte ***mskd=NULL; // define mask of where time series are nonzero
   int *mskd2=NULL;   // another format of mask: hold sli num, and dilate

   int TEST_OK = 0;
   double checksum = 0.;


   int *Nmskd=NULL;                            // num of vox in slice inp mask
   int *Nmskd2=NULL;                           // num of vox in slice dil mask
   int MIN_NMSKD = -1;                         // calc how many vox/sli
                                             // are needed for calc
   int mink = -1, maxk = -1, upk = -1, delsli = -1;     // some loop pars
   
   // for vox counting and fracs
   float **slipar=NULL;      // per sli, per vol par
   int **slinvox=NULL;       // per sli, per vol count
   // for entropy/zipping
   //float **slient=NULL;        // per sli, per vol par

   int **slibad=NULL;        // per sli, badness marker
   int *volbad=NULL;         // per vol, badness marker
   int   MIN_STREAK_LEN  = 4;           // alternating streak
   float MIN_STREAK_WARN = 0.3;    // seq of diffs of this mag -> BAD
   float MIN_DROP_DIFF   = 0.7;    // any diff of this mag -> BAD 
   float MIN_DROP_FRAC   = 0.05;   // any frac outside this edge -> BAD
   THD_3dim_dataset *baddset=NULL;          // output dset of diffs

   float **diffarr=NULL;
   THD_3dim_dataset *diffdset=NULL;          // output dset of diffs

	char dset_or[4] = "RAI";
	THD_3dim_dataset *dsetn=NULL;

   mainENTRY("3dZipperZapper"); machdep(); 
  
   // ****************************************************************
   // ****************************************************************
   //                    load AFNI stuff
   // ****************************************************************
   // ****************************************************************

   INFO_message("version: 2018_01_06");
	
   /** scan args **/
   if (argc == 1) { usage_ZipperZapper(1); exit(0); }
   iarg = 1; 
   while( iarg < argc && argv[iarg][0] == '-' ){
      if( strcmp(argv[iarg],"-help") == 0 || 
          strcmp(argv[iarg],"-h") == 0 ) {
         usage_ZipperZapper(strlen(argv[iarg])>3 ? 2:1);
         exit(0);
      }
		
      // NO ARG:
      if( strcmp(argv[iarg],"-TESTING") == 0) {
         TEST_OK=1;
         iarg++ ; continue ;
      }

      if( strcmp(argv[iarg],"-prefix") == 0 ){
         iarg++ ; if( iarg >= argc ) 
                     ERROR_exit("Need argument after '-prefix'");
         prefix = strdup(argv[iarg]) ;
         if( !THD_filename_ok(prefix) ) 
            ERROR_exit("Illegal name after '-prefix'");
         iarg++ ; continue ;
      }
	 
      if( strcmp(argv[iarg],"-insetA") == 0 ){
         iarg++ ; if( iarg >= argc ) 
                     ERROR_exit("Need argument after '-insetA'");

         insetA = THD_open_dataset(argv[iarg]);
         if( (insetA == NULL ))
            ERROR_exit("Can't open time series dataset '%s'.",
                       argv[iarg]);

         Dim = (int *)calloc(4,sizeof(int));
         DSET_load(insetA); CHECK_LOAD_ERROR(insetA);
         Nvox = DSET_NVOX(insetA) ;
         Dim[0] = DSET_NX(insetA); Dim[1] = DSET_NY(insetA); 
         Dim[2] = DSET_NZ(insetA); Dim[3] = DSET_NVALS(insetA); 

         iarg++ ; continue ;
      }

      if( strcmp(argv[iarg],"-mask") == 0 ){
         iarg++ ; if( iarg >= argc ) 
                     ERROR_exit("Need argument after '-mask'");

         MASK = THD_open_dataset(argv[iarg]);
         if( MASK == NULL )
            ERROR_exit("Can't open time series dataset '%s'.",
                       argv[iarg]);

         DSET_load(MASK); CHECK_LOAD_ERROR(MASK);
			
         iarg++ ; continue ;
      }

      //  EXAMPLE: option with numerical input: ATOF
      /*if( strcmp(argv[iarg],"-neigh_Y") == 0 ){
        iarg++ ; if( iarg >= argc ) 
        ERROR_exit("Need argument after '-nneigh'");
      
        NEIGH_R[1] = atof(argv[iarg]);

        iarg++ ; continue ;
        }*/

      ERROR_message("Bad option '%s'\n",argv[iarg]) ;
      suggest_best_prog_option(argv[0], argv[iarg]);
      exit(1);
   }
	
   // TEST BASIC INPUT PROPERTIES
   if (iarg < 3) {
      ERROR_message("Too few options. Try -help for details.\n");
      exit(1);
   }
	
   if( !TEST_OK ) {
      ERROR_message("HEY! Just testing/building mode right now!\n");
      exit(5);
   }

   // ****************************************************************
   // ****************************************************************
   //                    pre-stuff, make storage
   // ****************************************************************
   // ****************************************************************

   mskd = (byte ***) calloc( Dim[0], sizeof(byte **) );
   for ( i = 0 ; i < Dim[0] ; i++ ) 
      mskd[i] = (byte **) calloc( Dim[1], sizeof(byte *) );
   for ( i = 0 ; i < Dim[0] ; i++ ) 
      for ( j = 0 ; j < Dim[1] ; j++ ) 
         mskd[i][j] = (byte *) calloc( Dim[2], sizeof(byte) );

   // Store number of voxels in ipnut axial slice mask: 
   Nmskd = (int *)calloc(Dim[2], sizeof(int)); 
   
   mskd2  = (int *)calloc(Nvox, sizeof(int)); // dilated form
   Nmskd2 = (int *)calloc(Dim[2], sizeof(int)); 

   if( (mskd == NULL) || (Nmskd == NULL) || 
       (mskd2 == NULL) || (Nmskd2 == NULL) ) { 
      fprintf(stderr, "\n\n MemAlloc failure (masks).\n\n");
      exit(12);
   }

   MIN_NMSKD = (int) (0.1 * Dim[0]*Dim[1]); // min num of vox in mask per sli
   if( MIN_NMSKD <= 0 )
      ERROR_exit("Min num of vox per slice was somehow %d?? No thanks!", 
                 MIN_NMSKD);

   // array for diffs; later, maybe just do one by one!
   diffarr = calloc( Dim[3], sizeof(diffarr) );
   for(i=0 ; i<Dim[3] ; i++) 
      diffarr[i] = calloc( Nvox, sizeof(float) ); 

   // summary pars and count for slices
   slipar = calloc( Dim[3], sizeof(slipar) );
   for(i=0 ; i<Dim[3] ; i++) 
      slipar[i] = calloc( Dim[2], sizeof(float) );
   slinvox = calloc( Dim[3], sizeof(slinvox) );
   for(i=0 ; i<Dim[3] ; i++) 
      slinvox[i] = calloc( Dim[2], sizeof(int) ); 

   // keep track of bad slices
   slibad = calloc( Dim[3], sizeof(slibad) );
   for(i=0 ; i<Dim[3] ; i++) 
      slibad[i] = calloc( Dim[2], sizeof(int) );
   // list of bad vols
   volbad = (int *)calloc( Dim[3], sizeof(int) ); 

   if( (diffarr == NULL) || (slipar == NULL) || (slinvox == NULL) ||
       (slibad == NULL) || (volbad == NULL) ) { 
      fprintf(stderr, "\n\n MemAlloc failure (arrs).\n\n");
      exit(13);
   }

   // ===================== resamp, if nec =======================

   // !!! lazy way-- make function different later...
   
   if (check_make_rai( insetA, dset_or ) ) {
      dsetn = r_new_resam_dset( insetA, NULL, 0.0, 0.0, 0.0,
                                dset_or, RESAM_NN_TYPE, NULL, 1, 0);
      DSET_delete(insetA); 
      insetA=dsetn;
      dsetn=NULL;
   }

   if( MASK )
      if (check_make_rai( MASK, dset_or ) ) {
         dsetn = r_new_resam_dset( MASK, NULL, 0.0, 0.0, 0.0,
                                   dset_or, RESAM_NN_TYPE, NULL, 1, 0);
         DSET_delete(MASK); 
         MASK=dsetn;
         dsetn=NULL;
      }
   
   // *************************************************************
   // *************************************************************
   //                    Beginning of main loops
   // *************************************************************
   // *************************************************************
	
   INFO_message("Masking and counting.");

   // go through once: define data vox
   idx = 0;
   for( k=0 ; k<Dim[2] ; k++ )  
      for( j=0 ; j<Dim[1] ; j++ ) 
         for( i=0 ; i<Dim[0] ; i++ ) {
            if( MASK ) {
               if( THD_get_voxel(MASK,idx,0)>0 ) {
                  mskd[i][j][k] = 1;
                  //mskd2[idx] = k+1;  // !! just do later now in values used!
                  Nmskd[k]++;
               }
            }
            else{
               checksum = 0.;
               for( m=0 ; m<Dim[3] ; m++ ) 
                  checksum+= fabs(THD_get_voxel(insetA,idx,m)); 
               if( checksum > EPS_V ) {
                  mskd[i][j][k] = 1;
                  //mskd2[idx] = k+1;  // !! just do later now in values used!
                  Nmskd[k]++;
               }
            }
            idx++; 
         }
   
   // will only look at slices with "enough" vox.  Also, ignore top
   // level, because we can't take diff above it.
   for( k=0 ; k<Dim[2] ; k++ ) 
      if( Nmskd[k] < MIN_NMSKD )
         Nmskd[k] = 0;
   Nmskd[Dim[2]-1]=0; 

   // **************************************************************
   // **************************************************************
   //                 Calculate stuff
   // **************************************************************
   // **************************************************************

   INFO_message("Start processing.");
   
   // ---------------- Calc (scaled) diff values ------------------

   // Single pass through.  
   idx = 0;
   delsli = Dim[0]*Dim[1];
   maxk = Dim[2] - 1;
   for( k=0 ; k<maxk ; k++ ) 
      for( j=0 ; j<Dim[1] ; j++ ) 
         for( i=0 ; i<Dim[0] ; i++ ) {
            if( mskd[i][j][k] && Nmskd[k] ) {
               if( mskd[i][j][k+1] ) {
                  upk = idx + delsli; // index one slice up
                  mskd2[idx] = k+1;   // record sli as k+1 bc of zeros.
                  Nmskd2[k]+= 1;        // count nvox per sli in dil mask
                  for( m=0 ; m<Dim[3] ; m++ ) {
                     diffarr[m][idx] = 0.5*(THD_get_voxel(insetA, idx, m) -
                                            THD_get_voxel(insetA, upk, m) );
                     diffarr[m][idx]/= fabs(THD_get_voxel(insetA, idx, m)) +
                        fabs(THD_get_voxel(insetA, upk, m)) +
                        0.0000001; // guard against double-zero val badness
                  }
               }
            }
            idx++;
         }
   
   // ---------------- counts of slice vals ------------------

   // Count 'em
   for( idx=0 ; idx<Nvox ; idx++)
      if( mskd2[idx] ) {
         for( m=0 ; m<Dim[3] ; m++ ) {
            slinvox[m][ mskd2[idx]-1 ] +=1;  // count this
            if( diffarr[m][idx] > 0 )
               slipar[m][ mskd2[idx]-1 ] +=1;
         }
      }

   // Calc fraction per slice of uppers
   for( k=0 ; k<maxk ; k++)
      for( m=0 ; m<Dim[3] ; m++ ) 
         if( slinvox[m][k] ) {
            slipar[m][k]/= slinvox[m][k];
            slipar[m][k]-= 0.5; // center around 0
         }

   // ---------------- linear entropy of slice vals -----------------

   /*
   slient = calloc( Dim[3], sizeof(slient) );
   for(i=0 ; i<Dim[3] ; i++) 
      slient[i] = calloc( Dim[2], sizeof(float) );
   if( (slient == NULL)  ) { 
      fprintf(stderr, "\n\n MemAlloc failure (slient).\n\n");
      exit(13);
   }
   
   i = do_calc_entrop( diffarr,
                       Nmskd2,
                       mskd2,
                       slient,
                       Dim);
   */

   // --------------- identify bad slices from counts ---------------


   i = find_bad_slices_streak( slipar,
                               Nmskd,
                               slibad,
                               Dim,
                               MIN_STREAK_LEN,
                               MIN_STREAK_WARN );
   
   i = find_bad_slices_drop( slipar,
                             Nmskd,
                             slibad,
                             Dim,
                             MIN_DROP_DIFF,
                             MIN_DROP_FRAC );
   
   // keep track of badness
   for( m=0 ; m<Dim[3] ; m++ )
      for( k=0 ; k<maxk ; k++)
         if( slibad[m][k] )
            volbad[m]+= slibad[m][k];

   // **************************************************************
   // **************************************************************
   //                 Write stuff
   // **************************************************************
   // **************************************************************

   if ( 1 ) {

      sprintf(tprefixx,"%s_sli.1D", prefix);
      sprintf(tprefixy,"%s_val.1D", prefix);
      
      if( (fout0 = fopen(tprefixx, "w")) == NULL) {
         fprintf(stderr, "Error opening file %s.", tprefixx);
         exit(19);
      }
      if( (fout1 = fopen(tprefixy, "w")) == NULL) {
         fprintf(stderr, "Error opening file %s.", tprefixy);
         exit(19);
      }
      
      for( k=0 ; k<maxk ; k++) {
         if (Nmskd[k]) {
            fprintf(fout0, " %5d\n", k);
            for( m=0 ; m<Dim[3] ; m++) {
               fprintf(fout1, " %8.5f ", fabs(slipar[m][k]-slipar[m][k+1]));
               //fprintf(fout1, " %8.5f ", slipar[m][k]); // ORIG!
            }
            fprintf(fout1, "\n");
         }
      }
      fclose(fout0);
      fclose(fout1);
      INFO_message("Wrote text file: %s", tprefixx);
      INFO_message("Wrote text file: %s", tprefixy);
   }

   if ( 1 ) {
      INFO_message("Preparing output of diffs.");
      
      diffdset = EDIT_empty_copy( insetA );

      EDIT_dset_items( diffdset,
                       ADN_prefix    , prefix,
                       ADN_datum_all , MRI_float,
                       ADN_brick_fac , NULL,
                       ADN_nvals     , Dim[3],
                       ADN_none );

      for( m=0 ; m<Dim[3] ; m++) {
         EDIT_substitute_brick(diffdset, m, MRI_float, diffarr[m]);
         diffarr[m]=NULL; // to not get into trouble...
         free(diffarr[m]);
      }

      THD_load_statistics( diffdset );
      tross_Copy_History( insetA, diffdset );
      tross_Make_History( "3dZipperZapper", argc, argv, diffdset );
      if( !THD_ok_overwrite() && 
          THD_is_ondisk(DSET_HEADNAME(diffdset)) )
         ERROR_exit("Can't overwrite existing dataset '%s'",
                    DSET_HEADNAME(diffdset));
      THD_write_3dim_dataset(NULL, NULL, diffdset, True);
      INFO_message("Wrote dataset: %s\n", DSET_BRIKNAME(diffdset));
   }


   // !!! maybe just temp:  output map of badness
   if ( 1 ) {
      
      float **badarr=NULL;

      // array for diffs; later, maybe just do one by one!
      badarr = calloc( Dim[3], sizeof(badarr) );
      for(i=0 ; i<Dim[3] ; i++) 
         badarr[i] = calloc( Nvox, sizeof(float) ); 
      
      if( (badarr == NULL) ) { 
         fprintf(stderr, "\n\n MemAlloc failure (arrs).\n\n");
         exit(13);
      }

      // write out array of "bad sli" map
      idx = 0;
      for( k=0 ; k<Dim[2] ; k++ ) 
         for( j=0 ; j<Dim[1] ; j++ ) 
            for( i=0 ; i<Dim[0] ; i++ ) {
               if( mskd[i][j][k] && Nmskd[k] ) 
                  for( m=0 ; m<Dim[3] ; m++ ) 
                     if( slibad[m][k] ) 
                        badarr[m][idx] = slibad[m][k];
               idx++;
            }
      
      INFO_message("Preparing map of bad slices.");
      
      sprintf(bprefix,"%s_badmask", prefix);
      baddset = EDIT_empty_copy( insetA );

      EDIT_dset_items( baddset,
                       ADN_prefix    , bprefix,
                       ADN_datum_all , MRI_float,
                       ADN_brick_fac , NULL,
                       ADN_nvals     , Dim[3],
                       ADN_none );

      for( m=0 ; m<Dim[3] ; m++) {
         EDIT_substitute_brick(baddset, m, MRI_float, badarr[m]);
         badarr[m]=NULL; // to not get into trouble...
         free(badarr[m]);
      }

      THD_load_statistics( baddset );
      tross_Copy_History( insetA, baddset );
      tross_Make_History( "3dZipperZapper", argc, argv, baddset );
      if( !THD_ok_overwrite() && 
          THD_is_ondisk(DSET_HEADNAME(baddset)) )
         ERROR_exit("Can't overwrite existing dataset '%s'",
                    DSET_HEADNAME(baddset));
      THD_write_3dim_dataset(NULL, NULL, baddset, True);
      INFO_message("Wrote dataset: %s\n", DSET_BRIKNAME(baddset));
      
      if(badarr) {
         for( i=0 ; i<Dim[3] ; i++) 
            free(badarr[i]);
         free(badarr);
      }
      

      // ------- write out list of bads, single col file
      sprintf(lprefix,"%s_badlist.txt", prefix);
      if( (fout0 = fopen(lprefix, "w")) == NULL) {
         fprintf(stderr, "Error opening file %s.", lprefix);
         exit(19);
      }

      fprintf(stderr, "++ The following were identified as bad volumes:\n");
      fprintf(stderr, "%15s %15s\n", "Vol index", "N bad slices");

      for( m=0 ; m<Dim[3] ; m++) {
         if( volbad[m] ) {
            fprintf(fout0, " %8d %8d\n", m, volbad[m]);
            fprintf(stderr, "%15d %15d\n", m, volbad[m]);
         }
      }
      fclose(fout0);

      INFO_message("Wrote text file listing bads:  %s", lprefix);
   }

   // ************************************************************
   // ************************************************************
   //                    Free dsets, arrays, etc.
   // ************************************************************
   // ************************************************************
	
   if(insetA){
      DSET_delete(insetA);
      free(insetA);
   }

   if( MASK ) {
      DSET_delete(MASK);
      free(MASK);
   }
      
   if(mskd) {
      for( i=0 ; i<Dim[0] ; i++) 
         for( j=0 ; j<Dim[1] ; j++) {
            free(mskd[i][j]);
         }
      for( i=0 ; i<Dim[0] ; i++) {
         free(mskd[i]);
      }
      free(mskd);
   }
   if(mskd2)
      free(mskd2);

   if(diffarr) {
      for( i=0 ; i<Dim[3] ; i++) 
         free(diffarr[i]);
      free(diffarr);
   }

   free(Nmskd);
   free(Nmskd2);

   for( i=0 ; i<Dim[3] ; i++) {
      free(slipar[i]);
      free(slinvox[i]);
      free(slibad[i]);
      // free(slient[i]);
   }
   free(slipar);
   free(slinvox);
   free(slibad);
   // free(slient);

   free(volbad);
      
   if(prefix)
      free(prefix);

   if(Dim)
      free(Dim);
	
   return 0;
}

// ---------------------------------------------------------

int check_make_rai( THD_3dim_dataset *A, 
                    char *dset_or )
{
   if( ORIENT_typestr[A->daxes->xxorient][0] != dset_or[0] )
      return 1;
   if( ORIENT_typestr[A->daxes->yyorient][0] != dset_or[1] )
      return 1;
   if( ORIENT_typestr[A->daxes->zzorient][0] != dset_or[2] )
      return 1;

   INFO_message("No need to resample.");
   return 0;
}

// ---------------------------------------------------------
int find_bad_slices_streak( float **slipar,
                            int *Nmskd,
                            int **slibad,
                            int *Dim,
                            int   MIN_STREAK_LEN,
                            float MIN_STREAK_WARN )
{
   int k,m,i;
   int topk = Dim[2] - MIN_STREAK_LEN; // max sli to check iteratively
   int THIS_SLI = 1;                   // switch to check or not
   int IS_BAD = 0;

   for( m=0 ; m<Dim[3] ; m++ ) 
      for( k=0 ; k<topk ; k++ ) {

         // long enough streak to check?
         THIS_SLI = 1;
         for( i=0 ; i<MIN_STREAK_LEN ; i++ )
            if ( Nmskd[k+i] == 0 ) {
               THIS_SLI = 0;
               break;
            }
         if( THIS_SLI ) {
            IS_BAD = 1;
            for( i=0 ; i<MIN_STREAK_LEN ; i++ )
               if( fabs(slipar[m][k+i]-slipar[m][k+i+1]) < MIN_STREAK_WARN )
                  IS_BAD = 0; // -> the streak is broken
            if( IS_BAD ) 
               for( i=0 ; i<MIN_STREAK_LEN ; i++ ) {
                  slibad[m][k+i]+=1;
                  INFO_message("Hey! %f:  %f -%f",
                               fabs(slipar[m][k+i]-slipar[m][k+i+1]),slipar[m][k+i],
                               slipar[m][k+i+1] );
               }
         }
      }
   
   return 0;
}

// -------------------------------------------------------

int find_bad_slices_drop( float **slipar, // shape: Dim[3] x Dim[2]
                        int *Nmskd,
                        int **slibad,
                        int *Dim,
                        float MIN_DROP_DIFF,
                        float MIN_DROP_FRAC)
{
   int k,m,i;
   float BOUND = 0.5 - MIN_DROP_FRAC;
   int topk = Dim[2] - 1; // max sli to check iteratively

   for( m=0 ; m<Dim[3] ; m++ ) 
      for( k=0 ; k<Dim[2] ; k++ ) 
         if( Nmskd[k] ) {
            if( fabs(slipar[m][k]) >= BOUND ) {
               slibad[m][k]+=1;
            }
            else if( k < topk ) {
               if( fabs(slipar[m][k]-slipar[m][k+1]) >= MIN_DROP_DIFF )
                  slibad[m][k]+=1;
            }
         }
  
   return 0;
}

// ---------------------------------------------------------
/*
int do_calc_entrop( float **diffarr,
                    int *Nmskd2,
                    int *mskd2,
                    float **slient,
                    int *Dim)
{
   int i,j,k,m,n;
   int idx;
   char *slisrc=NULL;
   float scaler = 0.;

   // Single pass through.  
   for( m=0 ; m<Dim[3] ; m++ ) {
      idx = 0;
      for( k=0 ; k<Dim[2] ; k++ ) {
         if( Nmskd2[k] ) {
            if( slisrc )
               realloc(slisrc, Nmskd2[k] * sizeof(byte) ); 
            else
               slisrc = (byte *)calloc( Nmskd2[k], sizeof(byte) ); 
            if( (slisrc == NULL)  ) { 
               fprintf(stderr, "\n\n MemAlloc failure (slisrc).\n\n");
               exit(13);
            }
            
            // set scale
            for( n=0 ; n<Nmskd2[k] ; n++ )
               if( n % 2 ) 
                  slisrc[n] = 1;
               else
                  slisrc[n] = 0;
            scaler = zz_compress_all( Nmskd2[k], slisrc, NULL );
            n = 0;
         }

         for( j=0 ; j<Dim[1] ; j++ ) 
            for( i=0 ; i<Dim[0] ; i++ ) {
               if( Nmskd2[k] && mskd2[idx] ) {
                  if( diffarr[m][idx] > 0 )
                     slisrc[n] = 1;
                  else
                     slisrc[n] = 0;
                  n++;
               }
               idx++;
            }
         
         if( slisrc ) {
            slient[m][k] = zz_compress_all( Nmskd2[k], slisrc, NULL );
            // HOW TO SCALE????????????????
            slient[m][k]/= scaler*scaler; //log10(Nmskd2[k]) * log10(Nmskd2[k]);
            INFO_message("%5d    %f", Nmskd2[k], scaler);
            slisrc=NULL; //free(slisrc);
         }
      }
   }
   free(slisrc);

   return 0;
}
*/
