#include <AMReX_BLProfiler.H>
#include <AMReX_CArena.H>
#include <AMReX_MemPool.H>
#include <AMReX_Vector.H>
#include <AMReX_OpenMP.H>

#ifdef AMREX_MEM_PROFILING
#include <AMReX_MemProfiler.H>
#endif

#include <AMReX_ParmParse.H>

#include <iostream>
#include <limits>
#include <algorithm>
#include <new>
#include <memory>
#include <cstring>
#include <cstdint>

using namespace amrex;

namespace
{
    Vector<std::unique_ptr<CArena> > the_memory_pool;
    bool initialized = false;
}

extern "C" {

void amrex_mempool_init ()
{
    if (!initialized)
    {
        BL_PROFILE("amrex_mempool_init()");

        initialized = true;

        int nthreads = OpenMP::get_max_threads();

        the_memory_pool.resize(nthreads);
        for (int i=0; i<nthreads; ++i) {
            the_memory_pool[i] = std::make_unique<CArena>(0, ArenaInfo().SetCpuMemory());
        }

#ifdef AMREX_USE_OMP
#pragma omp parallel num_threads(nthreads)
#endif
        {
            size_t N = 1024*1024*sizeof(double);
            void *p = amrex_mempool_alloc(N);
            memset(p, 0, N);
            amrex_mempool_free(p);
        }

#ifdef AMREX_MEM_PROFILING
        MemProfiler::add("MemPool", std::function<MemProfiler::MemInfo()>
                         ([] () -> MemProfiler::MemInfo {
                             int MB_min, MB_max, MB_tot;
                             amrex_mempool_get_stats(MB_min, MB_max, MB_tot);
                             Long b = MB_tot * (1024L*1024L);
                             return {b, b};
                         }));
#endif
    }
}

void amrex_mempool_finalize ()
{
    initialized = false;
    the_memory_pool.clear();
}

void* amrex_mempool_alloc (size_t nbytes)
{
  int tid = OpenMP::get_thread_num();
  return the_memory_pool[tid]->alloc(nbytes);
}

void amrex_mempool_free (void* p)
{
  int tid = OpenMP::get_thread_num();
  the_memory_pool[tid]->free(p);
}

void amrex_mempool_get_stats (int& mp_min, int& mp_max, int& mp_tot) // min, max & tot in MB
{
  size_t hsu_min=std::numeric_limits<size_t>::max();
  size_t hsu_max=0;
  size_t hsu_tot=0;
  for (const auto& mp : the_memory_pool) {
    size_t hsu = mp->heap_space_used();
    hsu_min = std::min(hsu, hsu_min);
    hsu_max = std::max(hsu, hsu_max);
    hsu_tot += hsu;
  }
  mp_min = static_cast<int>(hsu_min/(1024*1024));
  mp_max = static_cast<int>(hsu_max/(1024*1024));
  mp_tot = static_cast<int>(hsu_tot/(1024*1024));
}

void amrex_real_array_init (Real* p, size_t nelems)
{
    if (amrex::InitSNaN()) { amrex_array_init_snan(p, nelems); }
}

void amrex_array_init_snan (Real* p, size_t nelems)
{
    amrex::fill_snan<RunOn::Host>(p, nelems);
}

}
