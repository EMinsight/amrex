#ifndef _HGPARALLEL_H_
#define _HGPARALLEL_H_

#include <MultiFab.H>

#ifdef BL_USE_MPI
#include <mpi.h>
#endif

inline int processor_number(const MultiFab&r, int igrid)
{
    return r.DistributionMap()[igrid];
}

inline bool is_remote(const MultiFab& r, int igrid)
{
    return ParallelDescriptor::MyProc() != processor_number(r, igrid);
}

inline bool is_local(const MultiFab& r, int igrid)
{
    return ParallelDescriptor::MyProc() == processor_number(r, igrid);
}

#ifdef BL_USE_NEW_HFILES
#include <list>
using namespace std;
#else
#include <list.h>
#endif

class task
{
public:
    typedef unsigned int sequence_number;
    virtual ~task() {}
    virtual bool ready() = 0;
    virtual void init(sequence_number sno, MPI_Comm comm) = 0;
    virtual bool is_off_processor() const = 0;
};

class task_copy : public task
{
public:
    task_copy(MultiFab& mf, int dgrid,                const MultiFab& smf, int sgrid, const Box& bx);
    task_copy(MultiFab& mf, int dgrid, const Box& db, const MultiFab& smf, int sgrid, const Box& sb);
    virtual ~task_copy();
    virtual bool ready();
    virtual bool is_off_processor() const;
    virtual void init(sequence_number sno, MPI_Comm comm);
private:
#ifdef BL_USE_MPI
    MPI_Request m_request;
    FArrayBox* d_tmp;
    FArrayBox* s_tmp;
#endif
    MultiFab& m_mf;
    const MultiFab& m_smf;
    int m_dgrid;
    const int m_sgrid;
    const Box m_bx;
    const Box s_bx;
    bool m_ready;
};

class task_copy_local : public task
{
public:
    task_copy_local(FArrayBox& fab_, const MultiFab& mf_, int grid_, const Box& bx);
    virtual ~task_copy_local();
    virtual bool ready();
    virtual bool is_off_processor() const;
    virtual void init(sequence_number sno, MPI_Comm comm);
private:
    FArrayBox& m_fab;
    const MultiFab& m_smf;
    const int m_sgrid;
    const Box m_bx;
    const Box s_bx;
    bool m_ready;
};

class task_fab : public task
{
public:
    virtual const FArrayBox& fab() = 0;
    virtual bool is_off_processor() const;
    virtual void init(sequence_number sno, MPI_Comm comm);
};

class task_fab_get : public task_fab
{
public:
    task_fab_get(const MultiFab& r_, int grid_);
    task_fab_get(const MultiFab& r_, int grid_, const Box& bx);
    virtual ~task_fab_get();
    virtual const FArrayBox& fab();
    virtual bool ready();
    virtual bool is_off_processor() const;
    virtual void init(sequence_number sno, MPI_Comm comm);
private:
    const MultiFab& r;
    const int grid;
    const Box bx;
};

class task_list
{
public:
    task_list(MPI_Comm comm = MPI_COMM_WORLD);
    ~task_list();
    void add_task(task* t);
    void execute();
private:
    list<task*> tasks;
    MPI_Comm comm;
    task::sequence_number seq_no;
};

class level_interface;
class amr_boundary_class;

class task_fill_patch : public task_fab
{
public:
    task_fill_patch(const Box& region_,
	const MultiFab& r_, const level_interface& lev_interface_, const amr_boundary_class* bdy_, int idim_ = 0, int index_ = 0);
    virtual ~task_fill_patch();
    virtual const FArrayBox& fab();
    virtual bool ready();
    virtual bool is_off_processor() const;
    virtual void init(sequence_number sno, MPI_Comm comm);
private:
    bool fill_patch_blindly();
    bool fill_exterior_patch_blindly();
    void fill_patch();
    bool newed;
    FArrayBox* target;
    const Box region;
    const MultiFab& r;
    const level_interface& lev_interface;
    const amr_boundary_class* bdy;
    const int idim;
    const int index;
    task_list tl;
};


class task_linked_task : public task
{
public:
    task_linked_task(task* t_) : lcpt(t_) {}
    virtual bool ready() { return lcpt->ready(); }
    virtual void init(sequence_number no, MPI_Comm comm) { lcpt->init(no, comm); }
    virtual bool is_off_processor() const { return lcpt->is_off_processor(); }
private:
    LnClassPtr<task> lcpt;
};

class task_copy_link : public task
{
public:
    task_copy_link(MultiFab& m_, int jgrid_, int igrid_, const Box& freg_, const task_linked_task& t_)
	: m(m_), jgrid(jgrid_), igrid(igrid_), freg(freg_), t(t_) {}
    virtual bool ready()
    {
	if ( t.ready() )
	{
	    m[jgrid].copy(m[igrid], freg);
	    return true;
	}
	return false;
    }
    virtual bool is_off_processor() const;
    virtual void init(sequence_number sno, MPI_Comm comm);

private:
    MultiFab& m;
    const int igrid;
    const int jgrid;
    const Box freg;
    task_linked_task t;
};

#endif

