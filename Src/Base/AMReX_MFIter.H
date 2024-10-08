#ifndef BL_MFITER_H_
#define BL_MFITER_H_
#include <AMReX_Config.H>

#include <AMReX_FabArrayBase.H>

#include <memory>

namespace amrex {

#ifdef AMREX_USE_GPU
    inline bool TilingIfNotGPU () noexcept { return Gpu::notInLaunchRegion(); }
#else
    inline constexpr bool TilingIfNotGPU () noexcept { return true; }
#endif

template<class T> class FabArray;

struct MFItInfo
{
    bool do_tiling{false};
    bool dynamic{false};
    bool device_sync;
    int  num_streams;
    IntVect tilesize;
    MFItInfo () noexcept
        :  device_sync(!Gpu::inNoSyncRegion()), num_streams(Gpu::numGpuStreams()),
          tilesize(IntVect::TheZeroVector()) {}
    MFItInfo& EnableTiling (const IntVect& ts = FabArrayBase::mfiter_tile_size) noexcept {
        do_tiling = true;
        tilesize = ts;
        return *this;
    }
    MFItInfo& SetDynamic (bool f) noexcept {
        dynamic = f;
        return *this;
    }
    MFItInfo& DisableDeviceSync () noexcept {
        device_sync = false;
        return *this;
    }
    MFItInfo& SetDeviceSync (bool f) noexcept {
        device_sync = f;
        return *this;
    }
    MFItInfo& SetNumStreams (int n) noexcept {
        num_streams = n;
        return *this;
    }
    MFItInfo& UseDefaultStream () noexcept {
        num_streams = 1;
        return *this;
    }
};

class MFIter
{
public:
    //! Flags that specify tiling modes. All these flags are off by default.
    enum Flags {
        //!Tiling: Enabling the tiling mode
        Tiling        = 0x01,
        /**
        * \brief AllBoxes: If on, all threads/workers loop over all boxes without tiling.
        * This essentially loops over indexMap.
        * Note that many functions won't work with this.
        */
        AllBoxes      = 0x02,
        //! NoTeamBarrier: This option is for Team only. If on, there is no barrier in MFIter dtor.
        NoTeamBarrier = 0x04
    };

    /**
    * \brief The default constructor does not enable tiling (flags_ is set to 0 by default).
    * However, the tiling mode can be enabled by explicitly turning the tiling flag on (i.e. flags_ = Tiling).
    * The tile size in this case is defined by FabArrayBase::mfiter_tile_size
    */
    explicit MFIter (const FabArrayBase& fabarray,
                     unsigned char       flags_=0);

    //! Enable tiling with the default tile size, which is defined by FabArrayBase::mfiter_tile_size
    MFIter (const FabArrayBase& fabarray,
            bool                do_tiling);

    //! Enable tiling with explicit tile size and flags (See type Flags for more information)
    MFIter (const FabArrayBase& fabarray,
            const IntVect&      tilesize,
            unsigned char       flags_=0);

    MFIter (const BoxArray& ba, const DistributionMapping& dm, unsigned char flags_=0);

    MFIter (const BoxArray& ba, const DistributionMapping& dm, bool do_tiling);

    MFIter (const BoxArray& ba, const DistributionMapping& dm,
            const IntVect& tilesize, unsigned char flags_=0);

    MFIter (const FabArrayBase& fabarray, const MFItInfo& info);

    MFIter (const BoxArray& ba, const DistributionMapping& dm, const MFItInfo& info);

    MFIter (MFIter&& rhs) = default;
    MFIter (MFIter const&) = delete;
    MFIter& operator= (MFIter const&) = delete;
    MFIter& operator= (MFIter &&) = delete;

    // dtor
    ~MFIter ();

    //! Return the tile Box at the current index.
    [[nodiscard]] Box tilebox () const noexcept;

    //! Return the tilebox with provided nodal flag
    [[nodiscard]] Box tilebox (const IntVect& nodal) const noexcept;

    //! Return the tilebox with provided nodal flag and grown cells
    [[nodiscard]] Box tilebox (const IntVect& nodal, const IntVect& ngrow) const noexcept;

    //! Return the dir-nodal (or all nodal if dir<0) Box at the current index.
    [[nodiscard]] Box nodaltilebox (int dir=-1) const noexcept;

    //! Return the tile box at the current index grown to include ghost cells.
    [[nodiscard]] Box growntilebox (int ng=-1000000) const noexcept;

    [[nodiscard]] Box growntilebox (const IntVect& ng) const noexcept;

    //! Return the dir-nodal (or all nodal if dir<0) box grown to include ghost cells.
    [[nodiscard]] Box grownnodaltilebox (int dir=-1, int ng=-1000000) const noexcept;

    [[nodiscard]] Box grownnodaltilebox (int dir, const IntVect& ng) const noexcept;

    //! Return the valid Box in which the current tile resides.
    [[nodiscard]] Box validbox () const noexcept { return fabArray->box((*index_map)[currentIndex]); }

    //! Return the Box of the FAB at which we currently point.
    [[nodiscard]] Box fabbox () const noexcept { return fabArray->fabbox((*index_map)[currentIndex]); }

    //! Increment iterator to the next tile we own.
    void operator++ () noexcept;

    //! Is the iterator valid i.e. is it associated with a FAB?
    [[nodiscard]] bool isValid () const noexcept { return currentIndex < endIndex; }

    //! The index into the underlying BoxArray of the current FAB.
    [[nodiscard]] int index () const noexcept { return (*index_map)[currentIndex]; }

    //! The number of indices.
    [[nodiscard]] int length () const noexcept { return (endIndex - beginIndex); }

    //! The current local tile index in the current grid;
    [[nodiscard]] int LocalTileIndex () const noexcept {return local_tile_index_map ? (*local_tile_index_map)[currentIndex] : 0;}

    //! The the number of tiles in the current grid;
    [[nodiscard]] int numLocalTiles() const noexcept {return num_local_tiles ? (*num_local_tiles)[currentIndex] : 1;}

    /**
    * \brief Return local index into the vector of fab pointers, m_fabs_v
    * When AllBoxes is on, local_index_map is a nullptr and local index is current index.
    */
    [[nodiscard]] int LocalIndex () const noexcept { return local_index_map ? (*local_index_map)[currentIndex] : currentIndex; }

    //! Constant reference to FabArray over which we're iterating.
    [[nodiscard]] const FabArrayBase& theFabArrayBase () const noexcept { return *fabArray; }

    [[nodiscard]] int tileIndex () const noexcept {return currentIndex;}

    [[nodiscard]] const DistributionMapping& DistributionMap () const noexcept { return fabArray->DistributionMap(); }

    static int allowMultipleMFIters (int allow);

    static int currentDepth ();

    void Finalize ();

protected:

    std::unique_ptr<FabArrayBase> m_fa;  //!< This must be the first member!

    const FabArrayBase* fabArray;

    IntVect tile_size;

    unsigned char flags;
    int           currentIndex;
    int           beginIndex;
    int           endIndex;
    int           streams;
    IndexType     typ;

    bool          dynamic;
    bool          finalized = false;

    struct DeviceSync {
        DeviceSync (bool f) : flag(f) {}
        DeviceSync (DeviceSync&& rhs)  noexcept : flag(std::exchange(rhs.flag,false)) {}
        ~DeviceSync () = default;
        DeviceSync (DeviceSync const&) = delete;
        DeviceSync& operator= (DeviceSync const&) = delete;
        DeviceSync& operator= (DeviceSync &&) = delete;
        explicit operator bool() const noexcept { return flag; }
        bool flag = true;
    };
    DeviceSync device_sync;

    const Vector<int>* index_map;
    const Vector<int>* local_index_map;
    const Vector<Box>* tile_array;
    const Vector<int>* local_tile_index_map;
    const Vector<int>* num_local_tiles;

    static AMREX_EXPORT int nextDynamicIndex;
    static AMREX_EXPORT int depth;
    static AMREX_EXPORT int allow_multiple_mfiters;

    void Initialize ();
};

//! Is it safe to have these two MultiFabs in the same MFiter?
//! True means safe; false means maybe.
inline bool isMFIterSafe (const FabArrayBase& x, const FabArrayBase& y) {
    return x.DistributionMap() == y.DistributionMap()
        && BoxArray::SameRefs(x.boxArray(), y.boxArray());
}

}

#endif
