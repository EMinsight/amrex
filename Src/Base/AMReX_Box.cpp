
#include <AMReX_BLassert.H>
#include <AMReX.H>
#include <AMReX_Box.H>
#include <AMReX_Print.H>
#include <AMReX_ParallelDescriptor.H>

#include <iostream>
#include <limits>

namespace amrex {

namespace detail {

//
// I/O functions.
//

std::ostream&
box_write (std::ostream& os,
           const int * smallend,
           const int * bigend,
           const int * type,
           int dim)
{
    os << '(';
    int_vector_write(os, smallend, dim) << ' ';
    int_vector_write(os, bigend, dim) << ' ';
    int_vector_write(os, type, dim) << ')';

    if (os.fail()) {
        amrex::Error("operator<<(ostream&,Box&) failed");
    }

    return os;
}

//
// Moved out of Utility.H
//
#define BL_IGNORE_MAX 100000

std::istream&
box_read (std::istream& is,
          int * smallend,
          int * bigend,
          int * type,
          int dim)
{
    is >> std::ws;
    char c;
    is >> c;

    for (int i=0; i<dim; ++i) {
        type[i] = 0;
    }

    if (c == '(')
    {
        int_vector_read(is, smallend, dim);
        int_vector_read(is, bigend, dim);
        is >> c;
        // Read an optional IndexType
        is.putback(c);
        if ( c == '(' )
        {
            int_vector_read(is, type, dim);
        }
        is.ignore(BL_IGNORE_MAX,')');
    }
    else if (c == '<')
    {
        is.putback(c);
        int_vector_read(is, smallend, dim);
        int_vector_read(is, bigend, dim);
        is >> c;
        // Read an optional IndexType
        is.putback(c);
        if ( c == '<' )
        {
            int_vector_read(is, type, dim);
        }
        //is.ignore(BL_IGNORE_MAX,'>');
    }
    else
    {
        amrex::Error("operator>>(istream&,Box&): expected \'(\'");
    }

    if (is.fail()) {
        amrex::Error("operator>>(istream&,Box&) failed");
    }

    return is;
}

} // namespace detail

BoxCommHelper::BoxCommHelper (const Box& bx, int* p_)
    : p(p_)
{
    if (p == nullptr) {
        v.resize(3*AMREX_SPACEDIM);
        p = v.data();
    }

    AMREX_D_EXPR(p[0]                = bx.smallend[0],
                 p[1]                = bx.smallend[1],
                 p[2]                = bx.smallend[2]);
    AMREX_D_EXPR(p[0+AMREX_SPACEDIM] = bx.bigend[0],
                 p[1+AMREX_SPACEDIM] = bx.bigend[1],
                 p[2+AMREX_SPACEDIM] = bx.bigend[2]);
    const IntVect& typ = bx.btype.ixType();
    AMREX_D_EXPR(p[0+AMREX_SPACEDIM*2] = typ[0],
                 p[1+AMREX_SPACEDIM*2] = typ[1],
                 p[2+AMREX_SPACEDIM*2] = typ[2]);
}

void
AllGatherBoxes (Vector<Box>& bxs, int n_extra_reserve)
{
#ifdef BL_USE_MPI

#if 0
    // In principle, MPI_Allgather/MPI_Allgatherv should not be slower than
    // MPI_Gather/MPI_Gatherv followed by MPI_Bcast.  But that's not true on Summit.
    MPI_Comm comm = ParallelContext::CommunicatorSub();
    const int count = bxs.size();
    Vector<int> countvec(ParallelContext::NProcsSub());
    MPI_Allgather(&count, 1, MPI_INT, countvec.data(), 1, MPI_INT, comm);

    Vector<int> offset(countvec.size(),0);
    Long count_tot = countvec[0];
    for (int i = 1, N = offset.size(); i < N; ++i) {
        offset[i] = offset[i-1] + countvec[i-1];
        count_tot += countvec[i];
    }

    if (count_tot == 0) { return; }

    if (count_tot > static_cast<Long>(std::numeric_limits<int>::max())) {
        amrex::Abort("AllGatherBoxes: too many boxes");
    }

    Vector<Box> recv_buffer;
    recv_buffer.reserve(count_tot+n_extra_reserve);
    recv_buffer.resize(count_tot);
    MPI_Allgatherv(bxs.data(), count, ParallelDescriptor::Mpi_typemap<Box>::type(),
                   recv_buffer.data(), countvec.data(), offset.data(),
                   ParallelDescriptor::Mpi_typemap<Box>::type(), comm);

    std::swap(bxs,recv_buffer);
#else
    MPI_Comm comm = ParallelContext::CommunicatorSub();
    const int root = ParallelContext::IOProcessorNumberSub();
    const int myproc = ParallelContext::MyProcSub();
    const int nprocs = ParallelContext::NProcsSub();
    const int count = static_cast<int>(bxs.size());
    Vector<int> countvec(nprocs);
    MPI_Gather(&count, 1, MPI_INT, countvec.data(), 1, MPI_INT, root, comm);

    Long count_tot = 0L;
    Vector<int> offset(countvec.size(),0);
    if (myproc == root) {
        count_tot = countvec[0];
        for (int i = 1, N = static_cast<int>(offset.size()); i < N; ++i) {
            offset[i] = offset[i-1] + countvec[i-1];
            count_tot += countvec[i];
        }
    }

    MPI_Bcast(&count_tot, 1, MPI_INT, root, comm);

    if (count_tot == 0) { return; }

    if (count_tot > static_cast<Long>(std::numeric_limits<int>::max())) {
        amrex::Abort("AllGatherBoxes: too many boxes");
    }

    Vector<Box> recv_buffer;
    recv_buffer.reserve(count_tot+n_extra_reserve);
    recv_buffer.resize(count_tot);
    MPI_Gatherv(bxs.data(), count, ParallelDescriptor::Mpi_typemap<Box>::type(),
                recv_buffer.data(), countvec.data(), offset.data(),
                ParallelDescriptor::Mpi_typemap<Box>::type(), root, comm);
    MPI_Bcast(recv_buffer.data(), static_cast<int>(count_tot), ParallelDescriptor::Mpi_typemap<Box>::type(),
              root, comm);

    std::swap(bxs,recv_buffer);
#endif

#else
    amrex::ignore_unused(bxs,n_extra_reserve);
#endif
}

}
