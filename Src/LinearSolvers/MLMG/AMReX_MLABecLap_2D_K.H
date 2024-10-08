#ifndef AMREX_MLABECLAP_2D_K_H_
#define AMREX_MLABECLAP_2D_K_H_
#include <AMReX_Config.H>

namespace amrex {

template <typename T>
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlabeclap_adotx (int i, int j, int, int n, Array4<T> const& y,
                      Array4<T const> const& x,
                      Array4<T const> const& a,
                      Array4<T const> const& bX,
                      Array4<T const> const& bY,
                      GpuArray<T,AMREX_SPACEDIM> const& dxinv,
                      T alpha, T beta) noexcept
{
    const T dhx = beta*dxinv[0]*dxinv[0];
    const T dhy = beta*dxinv[1]*dxinv[1];
    y(i,j,0,n) = alpha*a(i,j,0)*x(i,j,0,n)
            - dhx * (bX(i+1,j,0,n)*(x(i+1,j,0,n) - x(i  ,j,0,n))
                   - bX(i  ,j,0,n)*(x(i  ,j,0,n) - x(i-1,j,0,n)))
            - dhy * (bY(i,j+1,0,n)*(x(i,j+1,0,n) - x(i,j  ,0,n))
                   - bY(i,j  ,0,n)*(x(i,j  ,0,n) - x(i,j-1,0,n)));
}

template <typename T>
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlabeclap_adotx_os (int i, int j, int, int n, Array4<T> const& y,
                         Array4<T const> const& x,
                         Array4<T const> const& a,
                         Array4<T const> const& bX,
                         Array4<T const> const& bY,
                         Array4<int const> const& osm,
                         GpuArray<T,AMREX_SPACEDIM> const& dxinv,
                         T alpha, T beta) noexcept
{
    const T dhx = beta*dxinv[0]*dxinv[0];
    const T dhy = beta*dxinv[1]*dxinv[1];
    if (osm(i,j,0) == 0) {
        y(i,j,0,n) = T(0.0);
    } else {
        y(i,j,0,n) = alpha*a(i,j,0)*x(i,j,0,n)
            - dhx * (bX(i+1,j,0,n)*(x(i+1,j,0,n) - x(i  ,j,0,n))
                   - bX(i  ,j,0,n)*(x(i  ,j,0,n) - x(i-1,j,0,n)))
            - dhy * (bY(i,j+1,0,n)*(x(i,j+1,0,n) - x(i,j  ,0,n))
                   - bY(i,j  ,0,n)*(x(i,j  ,0,n) - x(i,j-1,0,n)));
    }
}

template <typename T>
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlabeclap_normalize (int i, int j, int, int n, Array4<T> const& x,
                          Array4<T const> const& a,
                          Array4<T const> const& bX,
                          Array4<T const> const& bY,
                          GpuArray<T,AMREX_SPACEDIM> const& dxinv,
                          T alpha, T beta) noexcept
{
    const T dhx = beta*dxinv[0]*dxinv[0];
    const T dhy = beta*dxinv[1]*dxinv[1];
    x(i,j,0,n) /= alpha*a(i,j,0)
        + dhx*(bX(i,j,0,n)+bX(i+1,j,0,n))
        + dhy*(bY(i,j,0,n)+bY(i,j+1,0,n));
}

template <typename T>
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlabeclap_flux_x (Box const& box, Array4<T> const& fx, Array4<T const> const& sol,
                       Array4<T const> const& bx, T fac, int ncomp) noexcept
{
    const auto lo = amrex::lbound(box);
    const auto hi = amrex::ubound(box);

    for (int n = 0; n < ncomp; ++n) {
    for     (int j = lo.y; j <= hi.y; ++j) {
        AMREX_PRAGMA_SIMD
        for (int i = lo.x; i <= hi.x; ++i) {
            fx(i,j,0,n) = -fac*bx(i,j,0,n)*(sol(i,j,0,n)-sol(i-1,j,0,n));
        }
    }
    }
}

template <typename T>
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlabeclap_flux_xface (Box const& box, Array4<T> const& fx, Array4<T const> const& sol,
                           Array4<T const> const& bx, T fac, int xlen, int ncomp) noexcept
{
    const auto lo = amrex::lbound(box);
    const auto hi = amrex::ubound(box);

    for (int n = 0; n < ncomp; ++n) {
    for     (int j = lo.y; j <= hi.y; ++j) {
        int i = lo.x;
        fx(i,j,0,n) = -fac*bx(i,j,0,n)*(sol(i,j,0,n)-sol(i-1,j,0,n));
        i += xlen;
        fx(i,j,0,n) = -fac*bx(i,j,0,n)*(sol(i,j,0,n)-sol(i-1,j,0,n));
    }
    }
}

template <typename T>
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlabeclap_flux_y (Box const& box, Array4<T> const& fy, Array4<T const> const& sol,
                       Array4<T const> const& by, T fac, int ncomp) noexcept
{
    const auto lo = amrex::lbound(box);
    const auto hi = amrex::ubound(box);

    for (int n = 0; n < ncomp; ++n) {
    for     (int j = lo.y; j <= hi.y; ++j) {
        AMREX_PRAGMA_SIMD
        for (int i = lo.x; i <= hi.x; ++i) {
            fy(i,j,0,n) = -fac*by(i,j,0,n)*(sol(i,j,0,n)-sol(i,j-1,0,n));
        }
    }
    }
}

template <typename T>
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlabeclap_flux_yface (Box const& box, Array4<T> const& fy, Array4<T const> const& sol,
                           Array4<T const> const& by, T fac, int ylen, int ncomp) noexcept
{
    const auto lo = amrex::lbound(box);
    const auto hi = amrex::ubound(box);

    for (int n = 0; n < ncomp; ++n) {
    int j = lo.y;
    AMREX_PRAGMA_SIMD
    for (int i = lo.x; i <= hi.x; ++i) {
        fy(i,j,0,n) = -fac*by(i,j,0,n)*(sol(i,j,0,n)-sol(i,j-1,0,n));
    }
    j += ylen;
    AMREX_PRAGMA_SIMD
    for (int i = lo.x; i <= hi.x; ++i) {
        fy(i,j,0,n) = -fac*by(i,j,0,n)*(sol(i,j,0,n)-sol(i,j-1,0,n));
    }
    }
}

template <typename T>
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void abec_gsrb (int i, int j, int, int n, Array4<T> const& phi, Array4<T const> const& rhs,
                T alpha, Array4<T const> const& a,
                T dhx, T dhy,
                Array4<T const> const& bX, Array4<T const> const& bY,
                Array4<int const> const& m0, Array4<int const> const& m2,
                Array4<int const> const& m1, Array4<int const> const& m3,
                Array4<T const> const& f0, Array4<T const> const& f2,
                Array4<T const> const& f1, Array4<T const> const& f3,
                Box const& vbox, int redblack) noexcept
{
    if ((i+j+redblack)%2 == 0) {
        const auto vlo = amrex::lbound(vbox);
        const auto vhi = amrex::ubound(vbox);

        T cf0 = (i == vlo.x && m0(vlo.x-1,j,0) > 0)
            ? f0(vlo.x,j,0,n) : T(0.0);
        T cf1 = (j == vlo.y && m1(i,vlo.y-1,0) > 0)
            ? f1(i,vlo.y,0,n) : T(0.0);
        T cf2 = (i == vhi.x && m2(vhi.x+1,j,0) > 0)
            ? f2(vhi.x,j,0,n) : T(0.0);
        T cf3 = (j == vhi.y && m3(i,vhi.y+1,0) > 0)
            ? f3(i,vhi.y,0,n) : T(0.0);

        T delta = dhx*(bX(i,j,0,n)*cf0 + bX(i+1,j,0,n)*cf2)
            +  dhy*(bY(i,j,0,n)*cf1 + bY(i,j+1,0,n)*cf3);

        T gamma = alpha*a(i,j,0)
            +   dhx*( bX(i,j,0,n) + bX(i+1,j,0,n) )
            +   dhy*( bY(i,j,0,n) + bY(i,j+1,0,n) );

        T rho = dhx*(bX(i  ,j  ,0,n)*phi(i-1,j  ,0,n)
                   + bX(i+1,j  ,0,n)*phi(i+1,j  ,0,n))
               +dhy*(bY(i  ,j  ,0,n)*phi(i  ,j-1,0,n)
                   + bY(i  ,j+1,0,n)*phi(i  ,j+1,0,n));

        phi(i,j,0,n) = (rhs(i,j,0,n) + rho - phi(i,j,0,n)*delta)
            / (gamma - delta);
    }
}

template <typename T>
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void abec_gsrb_os (int i, int j, int, int n, Array4<T> const& phi, Array4<T const> const& rhs,
                   T alpha, Array4<T const> const& a,
                   T dhx, T dhy,
                   Array4<T const> const& bX, Array4<T const> const& bY,
                   Array4<int const> const& m0, Array4<int const> const& m2,
                   Array4<int const> const& m1, Array4<int const> const& m3,
                   Array4<T const> const& f0, Array4<T const> const& f2,
                   Array4<T const> const& f1, Array4<T const> const& f3,
                   Array4<int const> const& osm,
                   Box const& vbox, int redblack) noexcept
{
    if ((i+j+redblack)%2 == 0) {
        if (osm(i,j,0) == 0) {
            phi(i,j,0,n) = T(0.0);
        } else {
            const auto vlo = amrex::lbound(vbox);
            const auto vhi = amrex::ubound(vbox);

            T cf0 = (i == vlo.x && m0(vlo.x-1,j,0) > 0)
                ? f0(vlo.x,j,0,n) : T(0.0);
            T cf1 = (j == vlo.y && m1(i,vlo.y-1,0) > 0)
                ? f1(i,vlo.y,0,n) : T(0.0);
            T cf2 = (i == vhi.x && m2(vhi.x+1,j,0) > 0)
                ? f2(vhi.x,j,0,n) : T(0.0);
            T cf3 = (j == vhi.y && m3(i,vhi.y+1,0) > 0)
                ? f3(i,vhi.y,0,n) : T(0.0);

            T delta = dhx*(bX(i,j,0,n)*cf0 + bX(i+1,j,0,n)*cf2)
                   +  dhy*(bY(i,j,0,n)*cf1 + bY(i,j+1,0,n)*cf3);

            T gamma = alpha*a(i,j,0)
                +   dhx*( bX(i,j,0,n) + bX(i+1,j,0,n) )
                +   dhy*( bY(i,j,0,n) + bY(i,j+1,0,n) );

            T rho = dhx*(bX(i  ,j  ,0,n)*phi(i-1,j  ,0,n)
                       + bX(i+1,j  ,0,n)*phi(i+1,j  ,0,n))
                   +dhy*(bY(i  ,j  ,0,n)*phi(i  ,j-1,0,n)
                      + bY(i  ,j+1,0,n)*phi(i  ,j+1,0,n));

            phi(i,j,0,n) = (rhs(i,j,0,n) + rho - phi(i,j,0,n)*delta)
                / (gamma - delta);
        }
    }
}

template <typename T>
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void abec_jacobi (int i, int j, int, int n, Array4<T> const& phi,
                  Array4<T const> const& rhs, Array4< T const> const& Ax,
                  T alpha, Array4<T const> const& a,
                  T dhx, T dhy,
                  Array4<T const> const& bX, Array4<T const> const& bY,
                  Array4<int const> const& m0, Array4<int const> const& m2,
                  Array4<int const> const& m1, Array4<int const> const& m3,
                  Array4<T const> const& f0, Array4<T const> const& f2,
                  Array4<T const> const& f1, Array4<T const> const& f3,
                  Box const& vbox) noexcept
{
    const auto vlo = amrex::lbound(vbox);
    const auto vhi = amrex::ubound(vbox);

    T cf0 = (i == vlo.x && m0(vlo.x-1,j,0) > 0)
        ? f0(vlo.x,j,0,n) : T(0.0);
    T cf1 = (j == vlo.y && m1(i,vlo.y-1,0) > 0)
        ? f1(i,vlo.y,0,n) : T(0.0);
    T cf2 = (i == vhi.x && m2(vhi.x+1,j,0) > 0)
        ? f2(vhi.x,j,0,n) : T(0.0);
    T cf3 = (j == vhi.y && m3(i,vhi.y+1,0) > 0)
        ? f3(i,vhi.y,0,n) : T(0.0);

    T delta = dhx*(bX(i,j,0,n)*cf0 + bX(i+1,j,0,n)*cf2)
           +  dhy*(bY(i,j,0,n)*cf1 + bY(i,j+1,0,n)*cf3);

    T gamma = alpha*a(i,j,0)
        +   dhx*( bX(i,j,0,n) + bX(i+1,j,0,n) )
        +   dhy*( bY(i,j,0,n) + bY(i,j+1,0,n) );

    phi(i,j,0,n) += T(2.0/3.0) * (rhs(i,j,0,n) - Ax(i,j,0,n)) / (gamma - delta);
}

template <typename T>
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void abec_jacobi_os (int i, int j, int, int n, Array4<T> const& phi,
                     Array4<T const> const& rhs, Array4<T const> const& Ax,
                     T alpha, Array4<T const> const& a,
                     T dhx, T dhy,
                     Array4<T const> const& bX, Array4<T const> const& bY,
                     Array4<int const> const& m0, Array4<int const> const& m2,
                     Array4<int const> const& m1, Array4<int const> const& m3,
                     Array4<T const> const& f0, Array4<T const> const& f2,
                     Array4<T const> const& f1, Array4<T const> const& f3,
                     Array4<int const> const& osm,
                     Box const& vbox) noexcept
{
    if (osm(i,j,0) == 0) {
        phi(i,j,0,n) = T(0.0);
    } else {
        const auto vlo = amrex::lbound(vbox);
        const auto vhi = amrex::ubound(vbox);

        T cf0 = (i == vlo.x && m0(vlo.x-1,j,0) > 0)
            ? f0(vlo.x,j,0,n) : T(0.0);
        T cf1 = (j == vlo.y && m1(i,vlo.y-1,0) > 0)
            ? f1(i,vlo.y,0,n) : T(0.0);
        T cf2 = (i == vhi.x && m2(vhi.x+1,j,0) > 0)
            ? f2(vhi.x,j,0,n) : T(0.0);
        T cf3 = (j == vhi.y && m3(i,vhi.y+1,0) > 0)
            ? f3(i,vhi.y,0,n) : T(0.0);

        T delta = dhx*(bX(i,j,0,n)*cf0 + bX(i+1,j,0,n)*cf2)
               +  dhy*(bY(i,j,0,n)*cf1 + bY(i,j+1,0,n)*cf3);

        T gamma = alpha*a(i,j,0)
            +   dhx*( bX(i,j,0,n) + bX(i+1,j,0,n) )
            +   dhy*( bY(i,j,0,n) + bY(i,j+1,0,n) );

        phi(i,j,0,n) += T(2.0/3.0) * (rhs(i,j,0,n) - Ax(i,j,0,n)) / (gamma - delta);
    }
}

template <typename T>
AMREX_FORCE_INLINE
void abec_gsrb_with_line_solve (
                Box const& box, Array4<T> const& phi, Array4<T const> const& rhs,
                T alpha, Array4<T const> const& a,
                T dhx, T dhy,
                Array4<T const> const& bX, Array4<T const> const& bY,
                Array4<int const> const& m0, Array4<int const> const& m2,
                Array4<int const> const& m1, Array4<int const> const& m3,
                Array4<T const> const& f0, Array4<T const> const& f2,
                Array4<T const> const& f1, Array4<T const> const& f3,
                Box const& vbox, int redblack, int nc) noexcept
{

    const auto lo = amrex::lbound(box);
    const auto hi = amrex::ubound(box);
    const auto vlo = amrex::lbound(vbox);
    const auto vhi = amrex::ubound(vbox);

    // idir is the direction in which we will do the tridiagonal solve --
    // it should be the direction in which the mesh spacing is much larger
    // than in the other directions
    // int idir = 1;

    // This should be moved outside the kernel!
    if (dhy <= dhx) { amrex::Abort("dhy is supposed to be much larger than dhx"); }

    int ilen = hi.y - lo.y + 1;

    // This should be moved outside the kernel!
    if (ilen > 32) { amrex::Abort("abec_gsrb_with_line_solve is hard-wired to be no longer than 32"); }

    Array1D<T,0,31> a_ls;
    Array1D<T,0,31> b_ls;
    Array1D<T,0,31> c_ls;
    Array1D<T,0,31> r_ls;
    Array1D<T,0,31> u_ls;
    Array1D<T,0,31> gam;

    for (int n = 0; n < nc; ++n) {
        AMREX_PRAGMA_SIMD
        for (int i = lo.x; i <= hi.x; ++i) {
            if ((i+redblack)%2 == 0) {
                for (int j = lo.y; j <= hi.y; ++j) {
                    T gamma = alpha*a(i,j,0)
                        +   dhx*(bX(i,j,0,n)+bX(i+1,j,0,n))
                        +   dhy*(bY(i,j,0,n)+bY(i,j+1,0,n));

                    T cf0 = (i == vlo.x && m0(vlo.x-1,j,0) > 0)
                        ? f0(vlo.x,j,0,n) : T(0.0);
                    T cf1 = (j == vlo.y && m1(i,vlo.y-1,0) > 0)
                        ? f1(i,vlo.y,0,n) : T(0.0);
                    T cf2 = (i == vhi.x && m2(vhi.x+1,j,0) > 0)
                        ? f2(vhi.x,j,0,n) : T(0.0);
                    T cf3 = (j == vhi.y && m3(i,vhi.y+1,0) > 0)
                        ? f3(i,vhi.y,0,n) : T(0.0);

                    T g_m_d = gamma
                        - (dhx*(bX(i,j,0,n)*cf0 + bX(i+1,j,0,n)*cf2)
                        +  dhy*(bY(i,j,0,n)*cf1 + bY(i,j+1,0,n)*cf3));

                    T rho =  dhx*( bX(i  ,j,0,n)*phi(i-1,j,0,n)
                           +       bX(i+1,j,0,n)*phi(i+1,j,0,n) );

                    // We have already accounted for this external boundary in the coefficient of phi(i,j,k,n)
                    if (i == vlo.x && m0(vlo.x-1,j,0) > 0) {
                        rho -= dhx*bX(i  ,j,0,n)*phi(i-1,j,0,n);
                    }
                    if (i == vhi.x && m3(vhi.x+1,j,0) > 0) {
                        rho -= dhx*bX(i+1,j,0,n)*phi(i+1,j,0,n);
                    }

                    a_ls(j-lo.y) = -dhy*bY(i,j,0,n);
                    b_ls(j-lo.y) =  g_m_d;
                    c_ls(j-lo.y) = -dhy*bY(i,j+1,0,n);
                    u_ls(j-lo.y) = T(0.);
                    r_ls(j-lo.y) = rhs(i,j,0,n) + rho;

                    if (j == lo.y) {
                        a_ls(j-lo.y) = T(0.);
                        if (!(m1(i,vlo.y-1,0) > 0)) { r_ls(j-lo.y) += dhy*bY(i,j,0,n)*phi(i,j-1,0,n); }
                    }
                    if (j == hi.y) {
                        c_ls(j-lo.y) = T(0.);
                        if (!(m3(i,vhi.y+1,0) > 0)) { r_ls(j-lo.y) += dhy*bY(i,j+1,0,n)*phi(i,j+1,0,n); }
                    }
                }
//                      This is the tridiagonal solve
                {
                    T bet = b_ls(0);
                    u_ls(0) = r_ls(0) / bet;

                    for (int jj = 1; jj <= ilen-1; jj++) {
                        gam(jj) = c_ls(jj-1) / bet;
                        bet = b_ls(jj) - a_ls(jj)*gam(jj);
                        if (bet == 0) { amrex::Abort(">>>TRIDIAG FAILED"); }
                        u_ls(jj) = (r_ls(jj)-a_ls(jj)*u_ls(jj-1)) / bet;
                    }

                    for (int jj = ilen-2; jj >= 0; jj--) {
                                u_ls(jj) = u_ls(jj) - gam(jj+1)*u_ls(jj+1);
                    }
                }

                for (int j = lo.y; j <= hi.y; ++j) {
                            phi(i,j,0,n) = u_ls(j-lo.y);
                }
            }
        }
    }
}

template <typename T>
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void overset_rescale_bcoef_x (Box const& box, Array4<T> const& bX, Array4<int const> const& osm,
                              int ncomp, T osfac) noexcept
{
    const auto lo = amrex::lbound(box);
    const auto hi = amrex::ubound(box);
    for (int n = 0; n < ncomp; ++n) {
        for (int j = lo.y; j <= hi.y; ++j) {
        for (int i = lo.x; i <= hi.x; ++i) {
            if ((osm(i-1,j,0)+osm(i,j,0)) == 1) {
                bX(i,j,0,n) *= osfac;
            }
        }}
    }
}

template <typename T>
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void overset_rescale_bcoef_y (Box const& box, Array4<T> const& bY, Array4<int const> const& osm,
                              int ncomp, T osfac) noexcept
{
    const auto lo = amrex::lbound(box);
    const auto hi = amrex::ubound(box);
    for (int n = 0; n < ncomp; ++n) {
        for (int j = lo.y; j <= hi.y; ++j) {
        for (int i = lo.x; i <= hi.x; ++i) {
            if ((osm(i,j-1,0)+osm(i,j,0)) == 1) {
                bY(i,j,0,n) *= osfac;
            }
        }}
    }
}

}
#endif
