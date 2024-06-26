
#ifndef BL_FABFACTORY_H_
#define BL_FABFACTORY_H_
#include <AMReX_Config.H>

#include <AMReX_Box.H>
#include <AMReX_Print.H>
#include <AMReX_MakeType.H>
#include <AMReX_Vector.H>
#include <AMReX_Arena.H>
#include <AMReX_TypeTraits.H>

namespace amrex
{

class FArrayBox;

enum class FabType : int {
    covered = -1,
    regular = 0,
    singlevalued = 1,
    multivalued = 2,
    undefined = 100
};

struct FabInfo
{
    bool alloc = true;
    bool shared = false;
    Arena* arena = nullptr;

    FabInfo& SetAlloc (bool a) noexcept {
        alloc = a;
        return *this;
    }

    FabInfo& SetShared (bool s) noexcept {
        shared = s;
        return *this;
    }

    FabInfo& SetArena (Arena* ar) noexcept {
        arena = ar;
        return *this;
    }
};

template <class FAB>
class FabFactory
{
public:
    FabFactory () noexcept = default;
    FabFactory (FabFactory const&) noexcept = default;
    FabFactory (FabFactory &&) noexcept = default;
    FabFactory& operator= (FabFactory const&) noexcept = default;
    FabFactory& operator= (FabFactory &&) noexcept = default;
    virtual ~FabFactory () noexcept = default;
    AMREX_NODISCARD
    virtual FAB* create (const Box& box, int ncomps, const FabInfo& info, int box_index) const = 0;
    AMREX_NODISCARD
    virtual FAB* create_alias (FAB const& /*rhs*/, int /*scomp*/, int /*ncomp*/) const { return nullptr; }
    virtual void destroy (FAB* fab) const = 0;
    AMREX_NODISCARD virtual FabFactory<FAB>* clone () const = 0;
    AMREX_NODISCARD virtual Long nBytes (const Box& box, int ncomps, int /*box_index*/) const {
        if constexpr (IsBaseFab_v<FAB>) {
            return box.numPts() * ncomps * Long(sizeof(typename FAB::value_type));
        } else {
            return -1;
        }
    }
};

template <class FAB>
class DefaultFabFactory
    : public FabFactory<FAB>
{
public:
    AMREX_NODISCARD
    FAB* create (const Box& box, int ncomps, const FabInfo& info, int /*box_index*/) const override
    {
        return new FAB(box, ncomps, info.alloc, info.shared, info.arena);
    }

    AMREX_NODISCARD
    FAB* create_alias (FAB const& rhs, int scomp, int ncomp) const override
    {
        return new FAB(rhs, amrex::make_alias, scomp, ncomp);
    }

    void destroy (FAB* fab) const override
    {
        delete fab;
    }

    AMREX_NODISCARD
    DefaultFabFactory<FAB>* clone () const override {
        return new DefaultFabFactory<FAB>();
    }
};

}

#endif
