#ifndef AMREX_MLEBNODEFDLAP_2D_K_H_
#define AMREX_MLEBNODEFDLAP_2D_K_H_

namespace amrex {

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlebndfdlap_scale_rhs (int i, int j, int, Array4<Real> const& rhs,
                            Array4<int const> const& dmsk, Array4<Real const> const& ecx,
                            Array4<Real const> const& ecy) noexcept
{
    if (!dmsk(i,j,0)) {
        Real hmx = (ecx(i-1,j  ,0) == Real(1.)) ? Real(1.) : Real(1.) - Real(2.)*ecx(i-1,j  ,0);
        Real hpx = (ecx(i  ,j  ,0) == Real(1.)) ? Real(1.) : Real(1.) + Real(2.)*ecx(i  ,j  ,0);
        Real hmy = (ecy(i  ,j-1,0) == Real(1.)) ? Real(1.) : Real(1.) - Real(2.)*ecy(i  ,j-1,0);
        Real hpy = (ecy(i  ,j  ,0) == Real(1.)) ? Real(1.) : Real(1.) + Real(2.)*ecy(i  ,j  ,0);
        Real const s = amrex::min(hmx,hpx,hmy,hpy);
        rhs(i,j,0) *= s;
    }
}

template <typename F>
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlebndfdlap_adotx_eb_doit (int i, int j, int k, Array4<Real> const& y,
                                Array4<Real const> const& x, Array4<Real const> const& levset,
                                Array4<int const> const& dmsk,
                                Array4<Real const> const& ecx, Array4<Real const> const& ecy,
                                F const& xeb, Real bx, Real by) noexcept
{
    if (dmsk(i,j,k)) {
        y(i,j,k) = Real(0.0);
    } else {
        Real tmp;
        Real hp, hm, scale, out;

        hp = (ecx(i,j,k) == Real(1.0)) ? Real(1.0) : (Real(1.0)+Real(2.)*ecx(i,j,k));
        if (levset(i+1,j,k) < Real(0.0)) {
            tmp = x(i+1,j,k) - x(i,j,k);
        } else {
            tmp = (xeb(i+1,j,k) - x(i,j,k)) / hp;
        }

        hm = (ecx(i-1,j,k) == Real(1.0)) ? Real(1.0) : (Real(1.0)-Real(2.)*ecx(i-1,j,k));
        if (levset(i-1,j,k) < Real(0.0)) {
            tmp += x(i-1,j,k) - x(i,j,k);
        } else {
            tmp += (xeb(i-1,j,k) - x(i,j,k)) / hm;
        }

        out = tmp * bx * Real(2.0) / (hp+hm);
        scale = amrex::min(hm, hp);

        hp = (ecy(i,j,k) == Real(1.0)) ? Real(1.0) : (Real(1.0)+Real(2.)*ecy(i,j,k));
        if (levset(i,j+1,k) < Real(0.0)) {
            tmp = x(i,j+1,k) - x(i,j,k);
        } else {
            tmp = (xeb(i,j+1,k) - x(i,j,k)) / hp;
        }

        hm = (ecy(i,j-1,k) == Real(1.0)) ? Real(1.0) : (Real(1.0)-Real(2.)*ecy(i,j-1,k));
        if (levset(i,j-1,k) < Real(0.0)) {
            tmp += x(i,j-1,k) - x(i,j,k);
        } else {
            tmp += (xeb(i,j-1,k) - x(i,j,k)) / hm;
        }

        out += tmp * by * Real(2.0) / (hp+hm);
        scale = amrex::min(scale, hm, hp);

        y(i,j,k) = out*scale;
    }
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlebndfdlap_adotx_eb (int i, int j, int k, Array4<Real> const& y,
                           Array4<Real const> const& x, Array4<Real const> const& levset,
                           Array4<int const> const& dmsk,
                           Array4<Real const> const& ecx, Array4<Real const> const& ecy,
                           Real xeb, Real bx, Real by) noexcept
{
    mlebndfdlap_adotx_eb_doit(i, j, k, y, x, levset, dmsk, ecx, ecy,
                              [=] (int, int, int) -> Real { return xeb; },
                              bx, by);
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlebndfdlap_adotx_eb (int i, int j, int k, Array4<Real> const& y,
                           Array4<Real const> const& x, Array4<Real const> const& levset,
                           Array4<int const> const& dmsk,
                           Array4<Real const> const& ecx, Array4<Real const> const& ecy,
                           Array4<Real const> const& xeb, Real bx, Real by) noexcept
{
    mlebndfdlap_adotx_eb_doit(i, j, k, y, x, levset, dmsk, ecx, ecy,
                              [=] (int i1, int i2, int i3) -> Real {
                                  return xeb(i1,i2,i3); },
                              bx, by);
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlebndfdlap_adotx (int i, int j, int k, Array4<Real> const& y,
                        Array4<Real const> const& x, Array4<int const> const& dmsk,
                        Real bx, Real by) noexcept
{
    if (dmsk(i,j,k)) {
        y(i,j,k) = Real(0.0);
    } else {
        y(i,j,k) = bx * (x(i-1,j,k) + x(i+1,j,k))
            +      by * (x(i,j-1,k) + x(i,j+1,k))
            - (Real(2.0)*(bx+by)) * x(i,j,k);
    }
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlebndfdlap_gsrb_eb (int i, int j, int k, Array4<Real> const& x,
                          Array4<Real const> const& rhs, Array4<Real const> const& levset,
                          Array4<int const> const& dmsk,
                          Array4<Real const> const& ecx, Array4<Real const> const& ecy,
                          Real bx, Real by, int redblack) noexcept
{
    if ((i+j+k+redblack)%2 == 0) {
        if (dmsk(i,j,k)) {
            x(i,j,k) = Real(0.);
        } else {
            Real tmp0, tmp1;
            Real hp, hm, scale;

            hp = (ecx(i,j,k) == Real(1.0)) ? Real(1.0) : (Real(1.0)+Real(2.)*ecx(i,j,k));
            if (levset(i+1,j,k) < Real(0.0)) { // regular
                tmp0 = Real(-1.0);
                tmp1 = x(i+1,j,k);
            } else {
                tmp0 = Real(-1.0) / hp;
                tmp1 = Real(0.0);
            }

            hm = (ecx(i-1,j,k) == Real(1.0)) ? Real(1.0) : (Real(1.0)-Real(2.)*ecx(i-1,j,k));
            if (levset(i-1,j,k) < Real(0.0)) {
                tmp0 += Real(-1.0);
                tmp1 += x(i-1,j,k);
            } else {
                tmp0 += Real(-1.0) / hm;
            }

            Real gamma = tmp0 * (bx * Real(2.0) / (hp+hm));
            Real rho   = tmp1 * (bx * Real(2.0) / (hp+hm));
            scale = amrex::min(hm, hp);

            hp = (ecy(i,j,k) == Real(1.0)) ? Real(1.0) : (Real(1.0)+Real(2.)*ecy(i,j,k));
            if (levset(i,j+1,k) < Real(0.0)) {
                tmp0 = Real(-1.0);
                tmp1 = x(i,j+1,k);
            } else {
                tmp0 = Real(-1.0) / hp;
                tmp1 = Real(0.0);
            }

            hm = (ecy(i,j-1,k) == Real(1.0)) ? Real(1.0) : (Real(1.0)-Real(2.)*ecy(i,j-1,k));
            if (levset(i,j-1,k) < Real(0.0)) {
                tmp0 += Real(-1.0);
                tmp1 += x(i,j-1,k);
            } else {
                tmp0 += Real(-1.0) / hm;
            }

            gamma += tmp0 * (by * Real(2.0) / (hp+hm));
            rho   += tmp1 * (by * Real(2.0) / (hp+hm));
            scale = amrex::min(scale, hm, hp);

            Real Ax = rho + gamma*x(i,j,k);
            constexpr Real omega = Real(1.25);
            x(i,j,k) += (rhs(i,j,k) - Ax*scale) * (omega / (gamma*scale));
        }
    }
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlebndfdlap_gsrb (int i, int j, int k, Array4<Real> const& x,
                       Array4<Real const> const& rhs, Array4<int const> const& dmsk,
                       Real bx, Real by, int redblack) noexcept
{
    if ((i+j+k+redblack)%2 == 0) {
        if (dmsk(i,j,k)) {
            x(i,j,k) = Real(0.);
        } else {
            Real gamma = Real(-2.0)*(bx+by);
            Real Ax = bx * (x(i-1,j,k) + x(i+1,j,k))
                +     by * (x(i,j-1,k) + x(i,j+1,k))
                + gamma * x(i,j,k);
            constexpr Real omega = Real(1.25);
            x(i,j,k) += (rhs(i,j,k) - Ax) * (omega / gamma);
        }
    }
}

// RZ

template <typename F>
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlebndfdlap_adotx_rz_eb_doit (int i, int j, int k, Array4<Real> const& y,
                                   Array4<Real const> const& x, Array4<Real const> const& levset,
                                   Array4<int const> const& dmsk,
                                   Array4<Real const> const& ecx, Array4<Real const> const& ecy,
                                   F const& xeb, Real sigr, Real dr, Real dz, Real rlo, Real alpha) noexcept
{
    Real const r = rlo + Real(i) * dr;
    if (dmsk(i,j,k) || (r == Real(0.0) && alpha != Real(0.0))) {
        y(i,j,k) = Real(0.0);
    } else {
        Real tmp;
        Real hp, hm, scale, out;

        if (r == Real(0.0)) {
            if (levset(i+1,j,k) < Real(0.0)) { // regular
                out = Real(4.0) * sigr * (x(i+1,j,k)-x(i,j,k)) / (dr*dr);
                scale = Real(1.0);
            } else {
                hp = (ecx(i,j,k) == Real(1.0)) ? Real(1.0) : (Real(1.0)+Real(2.)*ecx(i,j,k));
                out = Real(4.0) * sigr * (xeb(i+1,j,k)-x(i,j,k)) / (dr*dr*hp*hp);
                scale = hp;
            }
        } else {
            hp = (ecx(i,j,k) == Real(1.0)) ? Real(1.0) : (Real(1.0)+Real(2.)*ecx(i,j,k));
            if (levset(i+1,j,k) < Real(0.0)) { // regular
                tmp = (x(i+1,j,k) - x(i,j,k)) * (r + Real(0.5) * dr);
            } else {
                tmp = (xeb(i+1,j,k) - x(i,j,k)) / hp * (r + Real(0.5) * hp * dr);
            }

            hm = (ecx(i-1,j,k) == Real(1.0)) ? Real(1.0) : (Real(1.0)-Real(2.)*ecx(i-1,j,k));
            if (levset(i-1,j,k) < Real(0.0)) {
                tmp += (x(i-1,j,k) - x(i,j,k)) * (r - Real(0.5) * dr);
            } else {
                tmp += (xeb(i-1,j,k) - x(i,j,k)) / hm * (r - Real(0.5) * hm * dr);
            }

            out = tmp * Real(2.0) * sigr / ((hp+hm) * r * dr * dr);
            scale = amrex::min(hm, hp);
        }

        hp = (ecy(i,j,k) == Real(1.0)) ? Real(1.0) : (Real(1.0)+Real(2.)*ecy(i,j,k));
        if (levset(i,j+1,k) < Real(0.0)) {
            tmp = x(i,j+1,k) - x(i,j,k);
        } else {
            tmp = (xeb(i,j+1,k) - x(i,j,k)) / hp;
        }


        hm = (ecy(i,j-1,k) == Real(1.0)) ? Real(1.0) : (Real(1.0)-Real(2.)*ecy(i,j-1,k));
        if (levset(i,j-1,k) < Real(0.0)) {
            tmp += x(i,j-1,k) - x(i,j,k);
        } else {
            tmp += (xeb(i,j-1,k) - x(i,j,k)) / hm;
        }

        out += tmp * Real(2.0) / ((hp+hm) * dz * dz);
        scale = amrex::min(scale, hm, hp);

        if (r != Real(0.0)) {
            out -= alpha/(r*r) * x(i,j,k);
        }

        y(i,j,k) = out*scale;
    }
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlebndfdlap_adotx_rz_eb (int i, int j, int k, Array4<Real> const& y,
                              Array4<Real const> const& x, Array4<Real const> const& levset,
                              Array4<int const> const& dmsk,
                              Array4<Real const> const& ecx, Array4<Real const> const& ecy,
                              Real xeb, Real sigr, Real dr, Real dz, Real rlo, Real alpha) noexcept
{
    mlebndfdlap_adotx_rz_eb_doit(i, j, k, y, x, levset, dmsk, ecx, ecy,
                                 [=] (int, int, int) -> Real { return xeb; },
                                 sigr, dr, dz, rlo, alpha);
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlebndfdlap_adotx_rz_eb (int i, int j, int k, Array4<Real> const& y,
                              Array4<Real const> const& x, Array4<Real const> const& levset,
                              Array4<int const> const& dmsk,
                              Array4<Real const> const& ecx, Array4<Real const> const& ecy,
                              Array4<Real const> const& xeb, Real sigr, Real dr, Real dz, Real rlo,
                              Real alpha) noexcept
{
    mlebndfdlap_adotx_rz_eb_doit(i, j, k, y, x, levset, dmsk, ecx, ecy,
                                 [=] (int i1, int i2, int i3) -> Real {
                                     return xeb(i1,i2,i3); },
                                 sigr, dr, dz, rlo, alpha);
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlebndfdlap_adotx_rz (int i, int j, int k, Array4<Real> const& y,
                           Array4<Real const> const& x, Array4<int const> const& dmsk,
                           Real sigr, Real dr, Real dz, Real rlo, Real alpha) noexcept
{
    Real const r = rlo + Real(i)*dr;
    if (dmsk(i,j,k) || (r == Real(0.0) && alpha != Real(0.0))) {
        y(i,j,k) = Real(0.0);
    } else {
        Real Ax = (x(i,j-1,k) - Real(2.0)*x(i,j,k) + x(i,j+1,k)) / (dz*dz);
        if (r == Real(0.0)) {
            Ax += Real(4.0) * sigr * (x(i+1,j,k)-x(i,j,k)) / (dr*dr);
        } else {
            Real const rp = r + Real(0.5)*dr;
            Real const rm = r - Real(0.5)*dr;
            Ax += sigr * (rp*x(i+1,j,k) - (rp+rm)*x(i,j,k) + rm*x(i-1,j,k)) / (r*dr*dr);
            Ax -= alpha/(r*r) * x(i,j,k);
        }
        y(i,j,k) = Ax;
    }
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlebndfdlap_gsrb_rz_eb (int i, int j, int k, Array4<Real> const& x,
                             Array4<Real const> const& rhs, Array4<Real const> const& levset,
                             Array4<int const> const& dmsk,
                             Array4<Real const> const& ecx, Array4<Real const> const& ecy,
                             Real sigr, Real dr, Real dz, Real rlo, int redblack, Real alpha) noexcept
{
    if ((i+j+k+redblack)%2 == 0) {
        Real const r = rlo + Real(i) * dr;
        if (dmsk(i,j,k) || (r == Real(0.0) && alpha != Real(0.0))) {
            x(i,j,k) = Real(0.);
        } else {
            Real tmp, tmp0;
            Real hp, hm, scale, Ax, gamma;

            if (r == Real(0.0)) {
                if (levset(i+1,j,k) < Real(0.0)) { // regular
                    Ax = (Real(4.0) * sigr / (dr*dr)) * (x(i+1,j,k)-x(i,j,k));
                    gamma = -(Real(4.0) * sigr / (dr*dr));
                    scale = Real(1.0);
                } else {
                    hp = (ecx(i,j,k) == Real(1.0)) ? Real(1.0) : (Real(1.0)+Real(2.)*ecx(i,j,k));
                    gamma = -(Real(4.0) * sigr / (dr*dr*hp*hp));
                    Ax = gamma * x(i,j,k);
                    scale = hp;
                }
            } else {
                hp = (ecx(i,j,k) == Real(1.0)) ? Real(1.0) : (Real(1.0)+Real(2.)*ecx(i,j,k));
                if (levset(i+1,j,k) < Real(0.0)) { // regular
                    tmp = (x(i+1,j,k) - x(i,j,k)) * (r + Real(0.5) * dr);
                    tmp0 = -(r + Real(0.5) * dr);
                } else {
                    tmp0 = Real(-1.0) / hp * (r + Real(0.5) * hp * dr);
                    tmp = tmp0 * x(i,j,k);
                }

                hm = (ecx(i-1,j,k) == Real(1.0)) ? Real(1.0) : (Real(1.0)-Real(2.)*ecx(i-1,j,k));
                if (levset(i-1,j,k) < Real(0.0)) {
                    tmp += (x(i-1,j,k) - x(i,j,k)) * (r - Real(0.5) * dr);
                    tmp0 += -(r - Real(0.5) * dr);
                } else {
                    tmp += -x(i,j,k) / hm * (r - Real(0.5) * hm * dr);
                    tmp0 += Real(-1.0) / hm * (r - Real(0.5) * hm * dr);
                }

                Ax = tmp * Real(2.0) * sigr / ((hp+hm) * r * dr * dr);
                gamma = tmp0 * Real(2.0) * sigr / ((hp+hm) * r * dr * dr);
                scale = amrex::min(hm, hp);
            }

            hp = (ecy(i,j,k) == Real(1.0)) ? Real(1.0) : (Real(1.0)+Real(2.)*ecy(i,j,k));
            if (levset(i,j+1,k) < Real(0.0)) {
                tmp = x(i,j+1,k) - x(i,j,k);
                tmp0 = Real(-1.0);
            } else {
                tmp0 = Real(-1.0) / hp;
                tmp = tmp0 * x(i,j,k);
            }

            hm = (ecy(i,j-1,k) == Real(1.0)) ? Real(1.0) : (Real(1.0)-Real(2.)*ecy(i,j-1,k));
            if (levset(i,j-1,k) < Real(0.0)) {
                tmp += x(i,j-1,k) - x(i,j,k);
                tmp0 += Real(-1.0);
            } else {
                tmp += -x(i,j,k) / hm;
                tmp0 += Real(-1.0) / hm;
            }

            Ax += tmp * Real(2.0) / ((hp+hm) * dz * dz);
            gamma += tmp0 * Real(2.0) / ((hp+hm) * dz * dz);
            scale = amrex::min(scale, hm, hp);

            if (r != Real(0.0)) {
                Ax -= alpha/(r*r) * x(i,j,k);
                gamma -= alpha/(r*r);
            }

            constexpr Real omega = Real(1.25);
            x(i,j,k) += (rhs(i,j,k) - Ax*scale) * (omega / (gamma*scale));
        }
    }
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlebndfdlap_gsrb_rz (int i, int j, int k, Array4<Real> const& x,
                          Array4<Real const> const& rhs, Array4<int const> const& dmsk,
                          Real sigr, Real dr, Real dz, Real rlo, int redblack, Real alpha) noexcept
{
    if ((i+j+k+redblack)%2 == 0) {
        Real const r = rlo + Real(i)*dr;
        if (dmsk(i,j,k) || (r == Real(0.0) && alpha != Real(0.0))) {
            x(i,j,k) = Real(0.);
        } else {
            Real Ax = (x(i,j-1,k) - Real(2.0)*x(i,j,k) + x(i,j+1,k)) / (dz*dz);
            Real gamma = -Real(2.0) / (dz*dz);
            if (r == Real(0.0)) {
                Ax += (Real(4.0)*sigr/(dr*dr)) * (x(i+1,j,k)-x(i,j,k));
                gamma += -(Real(4.0)*sigr/(dr*dr));
            } else {
                Real const rp = r + Real(0.5)*dr;
                Real const rm = r - Real(0.5)*dr;
                Ax += sigr*(rp*x(i+1,j,k) - (rp+rm)*x(i,j,k) + rm*x(i-1,j,k)) / (r*dr*dr);
                gamma += -sigr*(rp+rm) / (r*dr*dr);
                Ax -= alpha/(r*r) * x(i,j,k);
                gamma -= alpha/(r*r);
            }
            constexpr Real omega = Real(1.25);
            x(i,j,k) += (rhs(i,j,k) - Ax) * (omega / gamma);
        }
    }
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlebndfdlap_sig_adotx (int i, int j, int k, Array4<Real> const& y,
                            Array4<Real const> const& x,
                            Array4<int const> const& dmsk,
                            Array4<Real const> const& sig,
                            Real bx, Real by) noexcept
{
    if (dmsk(i,j,k)) {
        y(i,j,k) = Real(0.0);
    } else {
        Real sigxm = Real(0.5)*(sig(i-1,j-1,k)+sig(i-1,j  ,k));
        Real sigxp = Real(0.5)*(sig(i  ,j-1,k)+sig(i  ,j  ,k));
        Real sigym = Real(0.5)*(sig(i-1,j-1,k)+sig(i  ,j-1,k));
        Real sigyp = Real(0.5)*(sig(i-1,j  ,k)+sig(i  ,j  ,k));
        y(i,j,k) = bx * (sigxm*x(i-1,j,k) + sigxp*x(i+1,j,k))
            +      by * (sigym*x(i,j-1,k) + sigyp*x(i,j+1,k))
            - (bx*(sigxm+sigxp) + by*(sigym+sigyp)) * x(i,j,k);
    }
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlebndfdlap_sig_gsrb (int i, int j, int k, Array4<Real> const& x,
                           Array4<Real const> const& rhs,
                           Array4<int const> const& dmsk,
                           Array4<Real const> const& sig,
                           Real bx, Real by, int redblack) noexcept
{
    if ((i+j+k+redblack)%2 == 0) {
        if (dmsk(i,j,k)) {
            x(i,j,k) = Real(0.);
        } else {
            Real sigxm = Real(0.5)*(sig(i-1,j-1,k)+sig(i-1,j  ,k));
            Real sigxp = Real(0.5)*(sig(i  ,j-1,k)+sig(i  ,j  ,k));
            Real sigym = Real(0.5)*(sig(i-1,j-1,k)+sig(i  ,j-1,k));
            Real sigyp = Real(0.5)*(sig(i-1,j  ,k)+sig(i  ,j  ,k));
            Real gamma = -(bx*(sigxm+sigxp) + by*(sigym+sigyp));
            Real Ax = bx * (sigxm*x(i-1,j,k) + sigxp*x(i+1,j,k))
                +     by * (sigym*x(i,j-1,k) + sigyp*x(i,j+1,k))
                + gamma * x(i,j,k);
            constexpr Real omega = Real(1.25);
            x(i,j,k) += (rhs(i,j,k) - Ax) * (omega / gamma);
        }
    }
}

template <typename F>
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlebndfdlap_sig_adotx_eb_doit (int i, int j, int k, Array4<Real> const& y,
                                    Array4<Real const> const& x, Array4<Real const> const& levset,
                                    Array4<int const> const& dmsk,
                                    Array4<Real const> const& ecx, Array4<Real const> const& ecy,
                                    Array4<Real const> const& sig, Array4<Real const> const& vfrc,
                                    F const& xeb, Real bx, Real by) noexcept
{
    if (dmsk(i,j,k)) {
        y(i,j,k) = Real(0.0);
    } else {
        Real tmp, sigma;
        Real hp, hm, scale, out;

        sigma = (sig(i,j-1,k)*vfrc(i,j-1,k) + sig(i,j,k)*vfrc(i,j,k))
            /  (vfrc(i,j-1,k)              + vfrc(i,j,k));
        hp = (ecx(i,j,k) == Real(1.0)) ? Real(1.0) : (Real(1.0)+Real(2.)*ecx(i,j,k));
        if (levset(i+1,j,k) < Real(0.0)) {
            tmp = sigma*(x(i+1,j,k) - x(i,j,k));
        } else {
            tmp = sigma*((xeb(i+1,j,k) - x(i,j,k)) / hp);
        }

        sigma = (sig(i-1,j-1,k)*vfrc(i-1,j-1,k) + sig(i-1,j,k)*vfrc(i-1,j,k))
            /  (vfrc(i-1,j-1,k)                + vfrc(i-1,j,k));
        hm = (ecx(i-1,j,k) == Real(1.0)) ? Real(1.0) : (Real(1.0)-Real(2.)*ecx(i-1,j,k));
        if (levset(i-1,j,k) < Real(0.0)) {
            tmp += sigma*(x(i-1,j,k) - x(i,j,k));
        } else {
            tmp += sigma*((xeb(i-1,j,k) - x(i,j,k)) / hm);
        }

        out = tmp * bx * Real(2.0) / (hp+hm);
        scale = amrex::min(hm, hp);

        sigma = (sig(i-1,j,k)*vfrc(i-1,j,k) + sig(i,j,k)*vfrc(i,j,k))
            /  (vfrc(i-1,j,k)              + vfrc(i,j,k));
        hp = (ecy(i,j,k) == Real(1.0)) ? Real(1.0) : (Real(1.0)+Real(2.)*ecy(i,j,k));
        if (levset(i,j+1,k) < Real(0.0)) {
            tmp = sigma*(x(i,j+1,k) - x(i,j,k));
        } else {
            tmp = sigma*((xeb(i,j+1,k) - x(i,j,k)) / hp);
        }

        sigma = (sig(i-1,j-1,k)*vfrc(i-1,j-1,k) + sig(i,j-1,k)*vfrc(i,j-1,k))
            /  (vfrc(i-1,j-1,k)                + vfrc(i,j-1,k));
        hm = (ecy(i,j-1,k) == Real(1.0)) ? Real(1.0) : (Real(1.0)-Real(2.)*ecy(i,j-1,k));
        if (levset(i,j-1,k) < Real(0.0)) {
            tmp += sigma*(x(i,j-1,k) - x(i,j,k));
        } else {
            tmp += sigma*((xeb(i,j-1,k) - x(i,j,k)) / hm);
        }

        out += tmp * by * Real(2.0) / (hp+hm);
        scale = amrex::min(scale, hm, hp);

        y(i,j,k) = out*scale;
    }
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlebndfdlap_sig_adotx_eb (int i, int j, int k, Array4<Real> const& y,
                               Array4<Real const> const& x, Array4<Real const> const& levset,
                               Array4<int const> const& dmsk,
                               Array4<Real const> const& ecx, Array4<Real const> const& ecy,
                               Array4<Real const> const& sig, Array4<Real const> const& vfrc,
                               Real xeb, Real bx, Real by) noexcept
{
    mlebndfdlap_sig_adotx_eb_doit(i, j, k, y, x, levset, dmsk, ecx, ecy, sig, vfrc,
                                  [=] (int, int, int) -> Real { return xeb; },
                                  bx, by);
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlebndfdlap_sig_adotx_eb (int i, int j, int k, Array4<Real> const& y,
                               Array4<Real const> const& x, Array4<Real const> const& levset,
                               Array4<int const> const& dmsk,
                               Array4<Real const> const& ecx, Array4<Real const> const& ecy,
                               Array4<Real const> const& sig, Array4<Real const> const& vfrc,
                               Array4<Real const> const& xeb, Real bx, Real by) noexcept
{
    mlebndfdlap_sig_adotx_eb_doit(i, j, k, y, x, levset, dmsk, ecx, ecy, sig, vfrc,
                                  [=] (int i1, int i2, int i3) -> Real {
                                      return xeb(i1,i2,i3); },
                                  bx, by);
}

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void mlebndfdlap_sig_gsrb_eb (int i, int j, int k, Array4<Real> const& x,
                              Array4<Real const> const& rhs,
                              Array4<Real const> const& levset,
                              Array4<int const> const& dmsk,
                              Array4<Real const> const& ecx,
                              Array4<Real const> const& ecy,
                              Array4<Real const> const& sig,
                              Array4<Real const> const& vfrc,
                              Real bx, Real by, int redblack) noexcept
{
    if ((i+j+k+redblack)%2 == 0) {
        if (dmsk(i,j,k)) {
            x(i,j,k) = Real(0.);
        } else {
            Real tmp0, tmp1, sigma;
            Real hp, hm, scale;

            sigma = (sig(i,j-1,k)*vfrc(i,j-1,k) + sig(i,j,k)*vfrc(i,j,k))
                /  (vfrc(i,j-1,k)              + vfrc(i,j,k));
            hp = (ecx(i,j,k) == Real(1.0)) ? Real(1.0) : (Real(1.0)+Real(2.)*ecx(i,j,k));
            if (levset(i+1,j,k) < Real(0.0)) { // regular
                tmp0 = sigma*Real(-1.0);
                tmp1 = sigma*x(i+1,j,k);
            } else {
                tmp0 = sigma*Real(-1.0) / hp;
                tmp1 = Real(0.0);
            }

            sigma = (sig(i-1,j-1,k)*vfrc(i-1,j-1,k) + sig(i-1,j,k)*vfrc(i-1,j,k))
                /  (vfrc(i-1,j-1,k)                + vfrc(i-1,j,k));
            hm = (ecx(i-1,j,k) == Real(1.0)) ? Real(1.0) : (Real(1.0)-Real(2.)*ecx(i-1,j,k));
            if (levset(i-1,j,k) < Real(0.0)) {
                tmp0 += sigma*Real(-1.0);
                tmp1 += sigma*x(i-1,j,k);
            } else {
                tmp0 += sigma*Real(-1.0) / hm;
            }

            Real gamma = tmp0 * (bx * Real(2.0) / (hp+hm));
            Real rho   = tmp1 * (bx * Real(2.0) / (hp+hm));
            scale = amrex::min(hm, hp);

            sigma = (sig(i-1,j,k)*vfrc(i-1,j,k) + sig(i,j,k)*vfrc(i,j,k))
                /  (vfrc(i-1,j,k)              + vfrc(i,j,k));
            hp = (ecy(i,j,k) == Real(1.0)) ? Real(1.0) : (Real(1.0)+Real(2.)*ecy(i,j,k));
            if (levset(i,j+1,k) < Real(0.0)) {
                tmp0 = sigma*Real(-1.0);
                tmp1 = sigma*x(i,j+1,k);
            } else {
                tmp0 = sigma*Real(-1.0) / hp;
                tmp1 = Real(0.0);
            }

            sigma = (sig(i-1,j-1,k)*vfrc(i-1,j-1,k) + sig(i,j-1,k)*vfrc(i,j-1,k))
                /  (vfrc(i-1,j-1,k)                + vfrc(i,j-1,k));
            hm = (ecy(i,j-1,k) == Real(1.0)) ? Real(1.0) : (Real(1.0)-Real(2.)*ecy(i,j-1,k));
            if (levset(i,j-1,k) < Real(0.0)) {
                tmp0 += sigma*Real(-1.0);
                tmp1 += sigma*x(i,j-1,k);
            } else {
                tmp0 += sigma*Real(-1.0) / hm;
            }

            gamma += tmp0 * (by * Real(2.0) / (hp+hm));
            rho   += tmp1 * (by * Real(2.0) / (hp+hm));
            scale = amrex::min(scale, hm, hp);

            Real Ax = rho + gamma*x(i,j,k);
            constexpr Real omega = Real(1.25);
            x(i,j,k) += (rhs(i,j,k) - Ax*scale) * (omega / (gamma*scale));
        }
    }
}

}

#endif
