/*****************************************************************************
 * Zoltan Library for Parallel Applications                                  *
 * Copyright (c) 2000,2001,2002, Sandia National Laboratories.               *
 * For more info, see the README file in the top-level Zoltan directory.     *  
 *****************************************************************************/
/*****************************************************************************
 * CVS File Information :
 *    $RCSfile$
 *    $Author$
 *    $Date$
 *    $Revision$
 ****************************************************************************/

#ifdef __cplusplus
/* if C++, define the rest of this header file as extern C */
extern "C" {
#endif

#include "zz_const.h"

/* Routines to handle mapping of partitions to processors. */

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

int Zoltan_LB_Part_To_Proc(ZZ *zz, int part)
{
/* Routine that maps partitions to processors.
 * If a partition is entirely within a processor, that processor's rank is
 * returned.
 * If a partition is spread across several processors, find the range of
 * processors.  If zz->Proc is one of them, return zz->Proc.  Otherwise,
 * return a random processor in the range of processors.
 * (This random feature should be replaced by something smarter to reduce
 * data movement. KDD)
 */
char *yo = "Zoltan_LB_Part_To_Proc";
int proc;
int *pdist = zz->LB.PartDist;    /* Temporary variable */
static int first_time = 1;
int num_procs_for_part;

  if (zz->LB.Num_Global_Parts == zz->Num_Proc) {
    /*  number of parts == number of procs. return input part. */
    proc = part;
  }
  else {
    /*  number of parts != number of procs */
    if (part < 0 || part >= zz->LB.Num_Global_Parts) {
      ZOLTAN_PRINT_ERROR(zz->Proc, yo, "Invalid partition number.");
      proc = -1;
    }
    else {

      num_procs_for_part = pdist[part+1] - pdist[part];
      if (num_procs_for_part <= 1)
        proc = pdist[part];
      else if (zz->Proc >= pdist[part] && zz->Proc < pdist[part+1])
        proc = zz->Proc;
      else {
        if (first_time) {
          srand(zz->Proc);
          first_time = 0;
        }
        proc = (rand() % num_procs_for_part) + pdist[part];
      }
    }
  }
  return proc;
}


/*****************************************************************************/


#ifdef __cplusplus
} /* closing bracket for extern "C" */
#endif
