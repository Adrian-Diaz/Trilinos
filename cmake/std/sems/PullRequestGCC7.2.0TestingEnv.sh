# This script can be used to load the appropriate environment for the
# GCC 7.2.0 Pull Request testing build on a Linux machine that has access to
# the SEMS NFS mount.

# usage: $ source PullRequestGCC7.2.0TestingEnv.sh

module purge

source /projects/sems/modulefiles/utils/sems-modules-init.sh

module load sems-gcc/7.2.0
module load sems-openmpi/1.10.1
module load sems-boost/1.63.0/base
module load sems-zlib/1.2.8/base
module load sems-hdf5/1.10.6/parallel
module load sems-netcdf/4.7.3/parallel
module load sems-parmetis/4.0.3/parallel
module load sems-scotch/6.0.3/nopthread_64bit_parallel
module load sems-superlu/4.3/base

module load sems-cmake/3.10.3
module load sems-ninja_fortran/1.8.2

module load sems-git/2.10.1

# Note: sems-boost also loads sems-python/2.7.x, we need to
#       unload it here.
module unload sems-python
module load sems-python/3.5.2

# add the OpenMP environment variable we need
export OMP_NUM_THREADS=2

