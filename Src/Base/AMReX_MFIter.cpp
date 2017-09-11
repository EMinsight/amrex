#include <iostream>

#include <AMReX_MFIter.H>
#include <AMReX_FabArray.H>
#include <AMReX_FArrayBox.H>
#include <AMReX_Device.H>

#ifdef CUDA
#include <cuda_runtime_api.h>
#include <AMReX_CUDA_helper.H>
#include <nvToolsExt.h>
#include <cmath>
#endif

namespace amrex {


MFIter::MFIter (const FabArrayBase& fabarray_, 
		unsigned char       flags_
#ifdef CUDA
                ,bool                use_device_
#endif
                )
    :
    fabArray(fabarray_),
    tile_size((flags_ & Tiling) ? FabArrayBase::mfiter_tile_size : IntVect::TheZeroVector()),
    flags(flags_),
    index_map(nullptr),
    local_index_map(nullptr),
    tile_array(nullptr),
    local_tile_index_map(nullptr),
#ifdef CUDA
    use_device(use_device_),
#endif
    num_local_tiles(nullptr)
{
    Initialize();
}

MFIter::MFIter (const FabArrayBase& fabarray_, 
		bool                do_tiling_)
    :
    fabArray(fabarray_),
    tile_size((do_tiling_) ? FabArrayBase::mfiter_tile_size : IntVect::TheZeroVector()),
    flags(do_tiling_ ? Tiling : 0),
    index_map(nullptr),
    local_index_map(nullptr),
    tile_array(nullptr),
    local_tile_index_map(nullptr),
#ifdef CUDA
    use_device(false),
#endif
    num_local_tiles(nullptr)
{
    Initialize();
}

MFIter::MFIter (const FabArrayBase& fabarray_, 
		const IntVect&      tilesize_, 
		unsigned char       flags_)
    :
    fabArray(fabarray_),
    tile_size(tilesize_),
    flags(flags_ | Tiling),
    index_map(nullptr),
    local_index_map(nullptr),
    tile_array(nullptr),
    local_tile_index_map(nullptr),
#ifdef CUDA
    use_device(false),
#endif
    num_local_tiles(nullptr)
{
    Initialize();
}

MFIter::MFIter (const BoxArray& ba, const DistributionMapping& dm, unsigned char flags_)
    :
    m_fa(new FabArray<FArrayBox>(ba, dm, 1, 0, MFInfo().SetAlloc(false))),
    fabArray(*m_fa),
    tile_size((flags_ & Tiling) ? FabArrayBase::mfiter_tile_size : IntVect::TheZeroVector()),
    flags(flags_),
    index_map(nullptr),
    local_index_map(nullptr),
    tile_array(nullptr),
    local_tile_index_map(nullptr),
#ifdef CUDA
    use_device(false),
#endif
    num_local_tiles(nullptr)
{
    Initialize();
}

MFIter::MFIter (const BoxArray& ba, const DistributionMapping& dm, bool do_tiling_)
    :
    m_fa(new FabArray<FArrayBox>(ba, dm, 1, 0, MFInfo().SetAlloc(false))),
    fabArray(*m_fa),
    tile_size((do_tiling_) ? FabArrayBase::mfiter_tile_size : IntVect::TheZeroVector()),
    flags(do_tiling_ ? Tiling : 0),
    index_map(nullptr),
    local_index_map(nullptr),
    tile_array(nullptr),
    local_tile_index_map(nullptr),
#ifdef CUDA
    use_device(false),
#endif
    num_local_tiles(nullptr)
{
    Initialize();
}


MFIter::MFIter (const BoxArray& ba, const DistributionMapping& dm,
                const IntVect& tilesize_, unsigned char flags_)
    :
    m_fa(new FabArray<FArrayBox>(ba, dm, 1, 0, MFInfo().SetAlloc(false))),
    fabArray(*m_fa),
    tile_size(tilesize_),
    flags(flags_ | Tiling),
    index_map(nullptr),
    local_index_map(nullptr),
    tile_array(nullptr),
    local_tile_index_map(nullptr),
#ifdef CUDA
    use_device(false),
#endif
    num_local_tiles(nullptr)
{
    Initialize();
}


MFIter::~MFIter ()
{
#if BL_USE_TEAM
    if ( ! (flags & NoTeamBarrier) )
	ParallelDescriptor::MyTeam().MemoryBarrier();
#endif

#ifdef CUDA
#ifdef _OPENMP
#pragma omp barrier
#pragma omp single
#endif
    if (use_device) {
        // synchronize all devices
        int n_dev = ParallelDescriptor::get_num_devices_used();
        for (int i = 0; i < n_dev; ++i) {
            checkCudaErrors(cudaSetDevice(i));
            checkCudaErrors(cudaDeviceSynchronize());
        }
    }
#endif
    // releaseDeviceData();
}

void 
MFIter::Initialize ()
{
    if (flags & SkipInit) {
	return;
    }
    else if (flags & AllBoxes)  // a very special case
    {
	index_map    = &(fabArray.IndexArray());
	currentIndex = 0;
	beginIndex   = 0;
	endIndex     = index_map->size();
    }
    else
    {
	const FabArrayBase::TileArray* pta = fabArray.getTileArray(tile_size);
	
	index_map            = &(pta->indexMap);
	local_index_map      = &(pta->localIndexMap);
	tile_array           = &(pta->tileArray);
	local_tile_index_map = &(pta->localTileIndexMap);
	num_local_tiles      = &(pta->numLocalTiles);

	{
	    int rit = 0;
	    int nworkers = 1;
#ifdef BL_USE_TEAM
	    if (ParallelDescriptor::TeamSize() > 1) {
		if ( tile_size == IntVect::TheZeroVector() ) {
		    // In this case the TileArray contains only boxes owned by this worker.
		    // So there is no sharing going on.
		    rit = 0;
		    nworkers = 1;
		} else {
		    rit = ParallelDescriptor::MyRankInTeam();
		    nworkers = ParallelDescriptor::TeamSize();
		}
	    }
#endif

	    int ntot = index_map->size();
	    
	    if (nworkers == 1)
	    {
		beginIndex = 0;
		endIndex = ntot;
	    }
	    else
	    {
		int nr   = ntot / nworkers;
		int nlft = ntot - nr * nworkers;
		if (rit < nlft) {  // get nr+1 items
		    beginIndex = rit * (nr + 1);
		    endIndex = beginIndex + nr + 1;
		} else {           // get nr items
		    beginIndex = rit * nr + nlft;
		    endIndex = beginIndex + nr;
		}
	    }
	}
	
#ifdef _OPENMP
	int nthreads = omp_get_num_threads();
	if (nthreads > 1)
	{
#ifndef CUDA
	    int tid = omp_get_thread_num();
	    int ntot = endIndex - beginIndex;
	    int nr   = ntot / nthreads;
	    int nlft = ntot - nr * nthreads;
	    if (tid < nlft) {  // get nr+1 items
		beginIndex += tid * (nr + 1);
		endIndex = beginIndex + nr + 1;
	    } else {           // get nr items
		beginIndex += tid * nr + nlft;
		endIndex = beginIndex + nr;
	    }	    
#else
            if (use_device) {
                Real gpu_portion = 1; // change this to decide how many works should be assigned to GPU
                int tid = omp_get_thread_num();
                int ntot = endIndex - beginIndex;
                int gpu_endIndex = std::floor(ntot*gpu_portion);
                if ( 0 == tid) {
                    beginIndex = 0;
                    endIndex = gpu_endIndex;
                }
                else {
                    // the rest of the OMP threads do it on CPU
                    ntot = endIndex - gpu_endIndex;
                    nthreads = nthreads - 1;
                    tid = tid - 1;
                    int nr   = ntot / nthreads;
                    int nlft = ntot - nr * nthreads;
                    if (tid < nlft) {  // get nr+1 items
                        beginIndex += tid * (nr + 1) + gpu_endIndex;
                        endIndex = beginIndex + nr + 1;
                    } else {           // get nr items
                        beginIndex += tid * nr + nlft + gpu_endIndex;
                        endIndex = beginIndex + nr;
                    }	    
                }
            } else {
                int tid = omp_get_thread_num();
                int ntot = endIndex - beginIndex;
                int nr   = ntot / nthreads;
                int nlft = ntot - nr * nthreads;
                if (tid < nlft) {  // get nr+1 items
                    beginIndex += tid * (nr + 1);
                    endIndex = beginIndex + nr + 1;
                } else {           // get nr items
                    beginIndex += tid * nr + nlft;
                    endIndex = beginIndex + nr;
                }	    
            }

#endif
	}
#endif

	currentIndex = beginIndex;
#ifdef CUDA
        // evenly assigned FABs to difference devices if more than one exists
        if (isValid()) {
            int device_used = fabArray.deviceArray[currentIndex];
            checkCudaErrors(cudaSetDevice(device_used));
        }
#endif

	typ = fabArray.boxArray().ixType();
    }
}

Box 
MFIter::tilebox () const
{ 
    BL_ASSERT(tile_array != 0);
    Box bx((*tile_array)[currentIndex]);
    if (! typ.cellCentered())
    {
	bx.convert(typ);
	const Box& vbx = validbox();
	const IntVect& Big = vbx.bigEnd();
	for (int d=0; d<BL_SPACEDIM; ++d) {
	    if (typ.nodeCentered(d)) { // validbox should also be nodal in d-direction.
		if (bx.bigEnd(d) < Big[d]) {
		    bx.growHi(d,-1);
		}
	    }
	}
    }
    return bx;
}

Box
MFIter::tilebox (const IntVect& nodal) const
{
    BL_ASSERT(tile_array != 0);
    Box bx((*tile_array)[currentIndex]);
    const IndexType new_typ {nodal};
    if (! new_typ.cellCentered())
    {
	bx.setType(new_typ);
	const Box& valid_cc_box = amrex::enclosedCells(validbox());
	const IntVect& Big = valid_cc_box.bigEnd();
	for (int d=0; d<BL_SPACEDIM; ++d) {
	    if (new_typ.nodeCentered(d)) { // validbox should also be nodal in d-direction.
		if (bx.bigEnd(d) == Big[d]) {
		    bx.growHi(d,1);
		}
	    }
	}
    }
    return bx;
}

Box
MFIter::nodaltilebox (int dir) const 
{ 
    BL_ASSERT(dir < BL_SPACEDIM);
    BL_ASSERT(tile_array != 0);
    Box bx((*tile_array)[currentIndex]);
    bx.convert(typ);
    const Box& vbx = validbox();
    const IntVect& Big = vbx.bigEnd();
    int d0, d1;
    if (dir < 0) {
	d0 = 0;
	d1 = BL_SPACEDIM-1;
    } else {
	d0 = d1 = dir;
    }
    for (int d=d0; d<=d1; ++d) {
	if (typ.cellCentered(d)) { // validbox should also be cell-centered in d-direction.
	    bx.surroundingNodes(d);
	    if (bx.bigEnd(d) <= Big[d]) {
		bx.growHi(d,-1);
	    }
	}
    }
    return bx;
}

// Note that a small negative ng is supported.
Box 
MFIter::growntilebox (int ng) const 
{
    Box bx = tilebox();
    if (ng < -100) ng = fabArray.nGrow();
    const Box& vbx = validbox();
    for (int d=0; d<BL_SPACEDIM; ++d) {
	if (bx.smallEnd(d) == vbx.smallEnd(d)) {
	    bx.growLo(d, ng);
	}
	if (bx.bigEnd(d) == vbx.bigEnd(d)) {
	    bx.growHi(d, ng);
	}
    }
    return bx;
}

Box
MFIter::grownnodaltilebox (int dir, int ng) const
{
    BL_ASSERT(dir < BL_SPACEDIM);
    Box bx = nodaltilebox(dir);
    if (ng < -100) ng = fabArray.nGrow();
    const Box& vbx = validbox();
    for (int d=0; d<BL_SPACEDIM; ++d) {
	if (bx.smallEnd(d) == vbx.smallEnd(d)) {
	    bx.growLo(d, ng);
	}
	if (bx.bigEnd(d) >= vbx.bigEnd(d)) {
	    bx.growHi(d, ng);
	}
    }
    return bx;
}

void
MFIter::operator++ () {

    ++currentIndex;
#ifdef CUDA
    if (isValid()) {
        int pre_use_device = fabArray.deviceArray[currentIndex-1];
        int device_used = fabArray.deviceArray[currentIndex];
        if (pre_use_device != device_used)
            checkCudaErrors(cudaSetDevice(device_used));
    }
#endif

    // releaseDeviceData();

}

void
MFIter::releaseDeviceData() {
    if (Device::inDeviceLaunchRegion()) {
	for (int i = 0; i < registered_fabs.size(); ++i)
	    registered_fabs[i]->toHost(registered_fabs_indices[i]);
	registered_fabs.clear();
	registered_fabs_indices.clear();
    }
}

MFGhostIter::MFGhostIter (const FabArrayBase& fabarray)
    :
    MFIter(fabarray, (unsigned char)(SkipInit|Tiling))
{
    Initialize();
}

void
MFGhostIter::Initialize ()
{
    int rit = 0;
    int nworkers = 1;
#ifdef BL_USE_TEAM
    if (ParallelDescriptor::TeamSize() > 1) {
	rit = ParallelDescriptor::MyRankInTeam();
	nworkers = ParallelDescriptor::TeamSize();
    }
#endif

    int tid = 0;
    int nthreads = 1;
#ifdef _OPENMP
    nthreads = omp_get_num_threads();
    if (nthreads > 1)
	tid = omp_get_thread_num();
#endif

    int npes = nworkers*nthreads;
    int pid = rit*nthreads+tid;

    BoxList alltiles;
    Array<int> allindex;
    Array<int> alllocalindex;

    for (int i=0; i < fabArray.IndexArray().size(); ++i) {
	int K = fabArray.IndexArray()[i];
	const Box& vbx = fabArray.box(K);
	const Box& fbx = fabArray.fabbox(K);

	const BoxList& diff = amrex::boxDiff(fbx, vbx);
	
	for (BoxList::const_iterator bli = diff.begin(); bli != diff.end(); ++bli) {
	    BoxList tiles(*bli, FabArrayBase::mfghostiter_tile_size);
	    int nt = tiles.size();
	    for (int it=0; it<nt; ++it) {
		allindex.push_back(K);
		alllocalindex.push_back(i);
	    }
	    alltiles.catenate(tiles);
	}
    }

    int n_tot_tiles = alltiles.size();
    int navg = n_tot_tiles / npes;
    int nleft = n_tot_tiles - navg*npes;
    int ntiles = navg;
    if (pid < nleft) ntiles++;

    // how many tiles should we skip?
    int nskip = pid*navg + std::min(pid,nleft);
    BoxList::const_iterator bli = alltiles.begin();
    for (int i=0; i<nskip; ++i) ++bli;

    lta.indexMap.reserve(ntiles);
    lta.localIndexMap.reserve(ntiles);
    lta.tileArray.reserve(ntiles);

    for (int i=0; i<ntiles; ++i) {
	lta.indexMap.push_back(allindex[i+nskip]);
	lta.localIndexMap.push_back(alllocalindex[i+nskip]);
	lta.tileArray.push_back(*bli++);
    }

    currentIndex = beginIndex = 0;
    endIndex = lta.indexMap.size();

    lta.nuse = 0;
    index_map       = &(lta.indexMap);
    local_index_map = &(lta.localIndexMap);
    tile_array      = &(lta.tileArray);
}

}
