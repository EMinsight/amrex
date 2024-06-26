#ifndef AMREX_SUNDIALS_H_
#define AMREX_SUNDIALS_H_

#include <AMReX.H>
#include <AMReX_NVector_MultiFab.H>
#include <AMReX_Sundials_Core.H>
#include <AMReX_SUNMemory.H>

static_assert(std::is_same_v<amrex::Real,sunrealtype>,
              "amrex::Real must be the same as SUNDIALS sunrealtype");

#endif
