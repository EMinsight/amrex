#include <AMReX_HypreABecLap2.H>

#include <AMReX_Habec_K.H>

#include <cmath>
#include <numeric>
#include <algorithm>
#include <type_traits>

namespace amrex
{

HypreABecLap2::HypreABecLap2 (const BoxArray& grids, const DistributionMapping& dmap,
                              const Geometry& geom_, MPI_Comm comm_)
    : Hypre(grids, dmap, geom_, comm_)
{
}

HypreABecLap2::~HypreABecLap2 ()
{
    HYPRE_BoomerAMGDestroy(solver);
    solver = nullptr;
    HYPRE_SStructMatrixDestroy(A);
    A = nullptr;
//    HYPRE_SStructVectorDestroy(b);  // done in solve()
//    b = nullptr;
//    HYPRE_SStructVectorDestroy(x);
//    x = nullptr;
    HYPRE_SStructGraphDestroy(graph);
    graph = nullptr;
    HYPRE_SStructStencilDestroy(stencil);
    stencil = nullptr;
    HYPRE_SStructGridDestroy(hgrid);
    hgrid = nullptr;
}

void
HypreABecLap2::solve (MultiFab& soln, const MultiFab& rhs, Real reltol, Real abstol,
                      int maxiter, const BndryData& bndry, int max_bndry_order)
{
    if (solver == nullptr || m_bndry != &bndry || m_maxorder != max_bndry_order)
    {
        m_bndry = &bndry;
        m_maxorder = max_bndry_order;
        m_factory = &(rhs.Factory());
        prepareSolver();
    }
    else
    {
        m_factory = &(rhs.Factory());
    }

    // We have to do this repeatedly to avoid memory leak due to Hypre bug
    HYPRE_SStructVectorCreate(comm, hgrid, &b);
    HYPRE_SStructVectorSetObjectType(b, HYPRE_PARCSR);
    HYPRE_SStructVectorInitialize(b);
    //
    HYPRE_SStructVectorCreate(comm, hgrid, &x);
    HYPRE_SStructVectorSetObjectType(x, HYPRE_PARCSR);
    HYPRE_SStructVectorInitialize(x);
    //
    loadVectors(soln, rhs);
    //
    HYPRE_SStructVectorAssemble(b);
    HYPRE_SStructVectorAssemble(x);

    HYPRE_BoomerAMGSetMinIter(solver, 1);
    HYPRE_BoomerAMGSetMaxIter(solver, maxiter);
    HYPRE_BoomerAMGSetTol(solver, reltol);
    if (abstol > 0.0)
    {
        Real bnorm;
        hypre_SStructInnerProd((hypre_SStructVector *) b,
                               (hypre_SStructVector *) b,
                               &bnorm);
        bnorm = std::sqrt(bnorm);

        const BoxArray& grids = acoefs.boxArray();
        Real volume = grids.d_numPts();
        Real reltol_new = bnorm > 0.0 ? abstol / (bnorm+1.e-100) * std::sqrt(volume) : reltol;

        if (reltol_new > reltol) {
            HYPRE_BoomerAMGSetTol(solver, reltol_new);
        }
    }

    HYPRE_ParCSRMatrix par_A;
    HYPRE_ParVector par_b;
    HYPRE_ParVector par_x;

    HYPRE_SStructMatrixGetObject(A, (void**) &par_A);
    HYPRE_SStructVectorGetObject(b, (void**) &par_b);
    HYPRE_SStructVectorGetObject(x, (void**) &par_x);

    HYPRE_BoomerAMGSolve(solver, par_A, par_b, par_x);

    if (verbose >= 2)
    {
        HYPRE_Int num_iterations;
        Real res;
        HYPRE_BoomerAMGGetNumIterations(solver, &num_iterations);
        HYPRE_BoomerAMGGetFinalRelativeResidualNorm(solver, &res);

        amrex::Print() << "\n" << num_iterations
                       << " Hypre SS BoomerAMG Iterations, Relative Residual "
                       << res << '\n';
    }

    getSolution(soln);

    // We have to do this repeatedly to avoid memory leak due to Hypre bug
    HYPRE_SStructVectorDestroy(b);
    b = nullptr;
    HYPRE_SStructVectorDestroy(x);
    x = nullptr;
}

void
HypreABecLap2::getSolution (MultiFab& a_soln)
{
    MultiFab* soln = &a_soln;
    MultiFab tmp;
    if (a_soln.nGrowVect() != 0) {
        tmp.define(a_soln.boxArray(), a_soln.DistributionMap(), 1, 0);
        soln = &tmp;
    }

    HYPRE_SStructVectorGather(x);

    const HYPRE_Int part = 0;

    for (MFIter mfi(*soln); mfi.isValid(); ++mfi)
    {
        const Box &reg = mfi.validbox();
        auto reglo = Hypre::loV(reg);
        auto reghi = Hypre::hiV(reg);
        HYPRE_SStructVectorGetBoxValues(x, part, reglo.data(), reghi.data(),
                                        0, (*soln)[mfi].dataPtr());
    }
    Gpu::hypreSynchronize();

    if (a_soln.nGrowVect() != 0) {
        MultiFab::Copy(a_soln, tmp, 0, 0, 1, 0);
    }
}

void
HypreABecLap2::prepareSolver ()
{
    BL_PROFILE("HypreABecLap2::prepareSolver()");

    HYPRE_SStructGridCreate(comm, AMREX_SPACEDIM, 1, &hgrid);

    Array<HYPRE_Int,AMREX_SPACEDIM> is_periodic {AMREX_D_DECL(0,0,0)};
    for (int i = 0; i < AMREX_SPACEDIM; i++) {
        if (geom.isPeriodic(i)) {
            is_periodic[i] = geom.period(i);
            AMREX_ASSERT(Hypre::ispow2(is_periodic[i]));
            AMREX_ASSERT(geom.Domain().smallEnd(i) == 0);
        }
    }
    if (geom.isAnyPeriodic()) {
        HYPRE_SStructGridSetPeriodic(hgrid, 0, is_periodic.data());
    }

    for (MFIter mfi(acoefs); mfi.isValid(); ++mfi)
    {
        const Box& bx = mfi.validbox();
        auto lo = Hypre::loV(bx);
        auto hi = Hypre::hiV(bx);
        HYPRE_SStructGridSetExtents(hgrid, 0, lo.data(), hi.data());
    }

    // All variables are cell-centered
    HYPRE_SStructVariable vars[1] = {HYPRE_SSTRUCT_VARIABLE_CELL};
    HYPRE_SStructGridSetVariables(hgrid, 0, 1, vars);

    HYPRE_SStructGridAssemble(hgrid);

    // Setup stencils
#if (AMREX_SPACEDIM == 2)
    HYPRE_Int offsets[regular_stencil_size][2] = {{ 0,  0},
                                                  {-1,  0},
                                                  { 1,  0},
                                                  { 0, -1},
                                                  { 0,  1}};
#elif (AMREX_SPACEDIM == 3)
    HYPRE_Int offsets[regular_stencil_size][3] = {{ 0,  0,  0},
                                                  {-1,  0,  0},
                                                  { 1,  0,  0},
                                                  { 0, -1,  0},
                                                  { 0,  1,  0},
                                                  { 0,  0, -1},
                                                  { 0,  0,  1}};
#endif

    HYPRE_SStructStencilCreate(AMREX_SPACEDIM, regular_stencil_size, &stencil);

    for (int i = 0; i < regular_stencil_size; i++) {
        HYPRE_SStructStencilSetEntry(stencil, i, offsets[i], 0);
    }

    HYPRE_SStructGraphCreate(comm, hgrid, &graph);
    HYPRE_SStructGraphSetObjectType(graph, HYPRE_PARCSR);

    HYPRE_SStructGraphSetStencil(graph, 0, 0, stencil);

    HYPRE_SStructGraphAssemble(graph);

    HYPRE_SStructMatrixCreate(comm, graph, &A);
    HYPRE_SStructMatrixSetObjectType(A, HYPRE_PARCSR);
    HYPRE_SStructMatrixInitialize(A);

    // A.SetValues() & A.assemble()
    Array<HYPRE_Int,regular_stencil_size> stencil_indices;
    std::iota(stencil_indices.begin(), stencil_indices.end(), 0);
    const HYPRE_Int part = 0;
    const auto dx = geom.CellSizeArray();
    const int bho = (m_maxorder > 2) ? 1 : 0;
    BaseFab<GpuArray<Real, regular_stencil_size> > rfab;
    for (MFIter mfi(acoefs); mfi.isValid(); ++mfi)
    {
        const Box &reg = mfi.validbox();
        rfab.resize(reg);

        Array4<Real const> const& afab = acoefs.const_array(mfi);
        GpuArray<Array4<Real const>, AMREX_SPACEDIM> bfabs {
            AMREX_D_DECL(bcoefs[0].const_array(mfi),
                         bcoefs[1].const_array(mfi),
                         bcoefs[2].const_array(mfi))};
        Array4<Real> const& diaginvfab = diaginv.array(mfi);
        GpuArray<int,AMREX_SPACEDIM*2> bctype;
        GpuArray<Real,AMREX_SPACEDIM*2> bcl;
        GpuArray<Array4<int const>, AMREX_SPACEDIM*2> msk;
        for (OrientationIter oit; oit; oit++)
        {
            Orientation ori = oit();
            int cdir(ori);
            bctype[cdir] = m_bndry->bndryConds(mfi)[cdir][0];
            bcl[cdir] = m_bndry->bndryLocs(mfi)[cdir];
            msk[cdir] = m_bndry->bndryMasks(ori)[mfi].const_array();
        }

        Real sa = scalar_a;
        Real sb = scalar_b;
        const auto boxlo = amrex::lbound(reg);
        const auto boxhi = amrex::ubound(reg);

        amrex::fill(rfab,
        [=] AMREX_GPU_HOST_DEVICE (GpuArray<Real,regular_stencil_size>& sten,
                                   int i, int j, int k)
        {
            habec_mat(sten, i, j, k, boxlo, boxhi, sa, afab, sb, dx, bfabs,
                      bctype, bcl, bho, msk, diaginvfab);
        });

        Real* mat = (Real*) rfab.dataPtr();
        Gpu::streamSynchronize();

        auto reglo = Hypre::loV(reg);
        auto reghi = Hypre::hiV(reg);
        HYPRE_SStructMatrixSetBoxValues(A, part, reglo.data(), reghi.data(),
                                        0, regular_stencil_size, stencil_indices.data(),
                                        mat);
        Gpu::hypreSynchronize();
    }
    HYPRE_SStructMatrixAssemble(A);

    // create solver
    HYPRE_BoomerAMGCreate(&solver);

    HYPRE_BoomerAMGSetOldDefault(solver); // Falgout coarsening with modified classical interpolation
//    HYPRE_BoomerAMGSetCoarsenType(solver, 6);
//    HYPRE_BoomerAMGSetCycleType(solver, 1);
    HYPRE_BoomerAMGSetRelaxType(solver, 6);   /* G-S/Jacobi hybrid relaxation */
    HYPRE_BoomerAMGSetRelaxOrder(solver, 1);   /* uses C/F relaxation */
    HYPRE_BoomerAMGSetNumSweeps(solver, 2);   /* Sweeeps on each level */
//    HYPRE_BoomerAMGSetStrongThreshold(solver, 0.6); // default is 0.25

    int logging = (verbose >= 2) ? 1 : 0;
    HYPRE_BoomerAMGSetLogging(solver, logging);

    HYPRE_ParCSRMatrix par_A;
    HYPRE_SStructMatrixGetObject(A, (void**) &par_A);
    HYPRE_BoomerAMGSetup(solver, par_A, nullptr, nullptr);
}

void
HypreABecLap2::loadVectors (MultiFab& soln, const MultiFab& rhs)
{
    BL_PROFILE("HypreABecLap2::loadVectors()");

    soln.setVal(0.0);

    MultiFab rhs_diag(rhs.boxArray(), rhs.DistributionMap(), 1, 0);

#ifdef AMREX_USE_GPU
    if (Gpu::inLaunchRegion() && rhs_diag.isFusingCandidate()) {
        auto const& rhs_diag_ma = rhs_diag.arrays();
        auto const& rhs_ma = rhs.const_arrays();
        auto const& diaginv_ma = diaginv.const_arrays();
        ParallelFor(rhs_diag,
        [=] AMREX_GPU_DEVICE (int box_no, int i, int j, int k) noexcept
        {
            rhs_diag_ma[box_no](i,j,k) = rhs_ma[box_no](i,j,k) * diaginv_ma[box_no](i,j,k);
        });
        Gpu::streamSynchronize();
    } else
#endif
    {
#ifdef AMREX_USE_OMP
#pragma omp parallel if (Gpu::notInLaunchRegion())
#endif
        for (MFIter mfi(rhs_diag,TilingIfNotGPU()); mfi.isValid(); ++mfi)
        {
            const Box& bx = mfi.tilebox();
            Array4<Real> const& rhs_diag_a = rhs_diag.array(mfi);
            Array4<Real const> const& rhs_a = rhs.const_array(mfi);
            Array4<Real const> const& diaginv_a = diaginv.const_array(mfi);
            AMREX_HOST_DEVICE_PARALLEL_FOR_3D(bx, i, j, k,
            {
                rhs_diag_a(i,j,k) = rhs_a(i,j,k) * diaginv_a(i,j,k);
            });
        }
    }

    const HYPRE_Int part = 0;
    for (MFIter mfi(soln); mfi.isValid(); ++mfi)
    {
        const Box &reg = mfi.validbox();
        auto reglo = Hypre::loV(reg);
        auto reghi = Hypre::hiV(reg);
        HYPRE_SStructVectorSetBoxValues(x, part, reglo.data(), reghi.data(),
                                        0, soln[mfi].dataPtr());
        HYPRE_SStructVectorSetBoxValues(b, part, reglo.data(), reghi.data(),
                                        0, rhs_diag[mfi].dataPtr());
    }
    Gpu::hypreSynchronize();
}

}
