/*
 * @BEGIN LICENSE
 *
 * Psi4: an open-source quantum chemistry software package
 *
 * Copyright (c) 2007-2025 The Psi4 Developers.
 *
 * The copyrights for code used from other parties are included in
 * the corresponding files.
 *
 * This file is part of Psi4.
 *
 * Psi4 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * Psi4 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along
 * with Psi4; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * @END LICENSE
 */

/*! \file
    \ingroup DPD
    \brief Enter brief description of file here
*/
#include <cstdio>
#include <cstdlib>
#include "psi4/libqt/qt.h"
#include "dpd.h"
#include "psi4/libpsi4util/exception.h"

namespace psi {

/* dpd_buf4_mat_irrep_rd(): Reads an entire irrep from disk into a dpd
** four-index buffer using the "rules" specified when the buffer was
** initialized by dpd_buf4_init().
**
** Arguments:
**   dpdbuf4 *Buf: A pointer to the dpdbuf4 where the data will
**                 be stored.
**   int irrep: The irrep number to be read.
**
** Tested methods: 11,12,22,31,44. (12/3/96)
**
** There are definitely some problems with the read routines here.
** Mainly in the case of unpacking bra indices, there is a danger that
** the input row buffer won't be filled with new values or zeros.  See,
** e.g., method 21.
**
** T. Daniel Crawford
** December 1996
**
** Minor modifications for newest dpd version.
** TDC
** October 1997
**
** Converted to latest version.
** TDC
** September 1999
*/

int DPD::buf4_mat_irrep_rd(dpdbuf4 *Buf, int irrep) {
    int method=0;
    int filerow, all_buf_irrep;
    int pq, rs;                 /* dpdbuf row and column indices */
    int p, q, r, s;             /* orbital indices */
    int filepq, filers, filesr; /* Input dpdfile row and column indices */
    int rowtot, coltot;         /* dpdbuf row and column dimensions */
    int pq_permute, permute;
    double value;
    long int size;

#ifdef DPD_TIMER
    timer_on("buf_rd");
#endif

    all_buf_irrep = Buf->file.my_irrep;

    rowtot = Buf->params->rowtot[irrep];
    coltot = Buf->params->coltot[irrep ^ all_buf_irrep];
    size = ((long)rowtot) * ((long)coltot);

    const auto& b_perm_pq = Buf->params->perm_pq;
    const auto& b_perm_rs = Buf->params->perm_rs;
    const auto& f_perm_pq = Buf->file.params->perm_pq;
    const auto& f_perm_rs = Buf->file.params->perm_rs;
    const auto& b_peq = Buf->params->peq;
    const auto& b_res = Buf->params->res;
    const auto& f_peq = Buf->file.params->peq;
    const auto& f_res = Buf->file.params->res;

    const auto& AllPolicy = dpdparams4::DiagPolicy::All;

    if ((b_perm_pq == f_perm_pq) && (b_perm_rs == f_perm_rs) && (b_peq == f_peq) && (b_res == f_res)) {
        if (Buf->anti)
            method = 11;
        else
            method = 12;
    } else if ((b_perm_pq != f_perm_pq) && (b_perm_rs == f_perm_rs) && (b_res == f_res)) {
        if (b_perm_pq == AllPolicy) {
            if (Buf->anti) {
                printf("\n\tUnpack pq and antisymmetrize?\n");
                throw PSIEXCEPTION("Unpack pq and antisymmetrize?");
            }
            method = 21;
        } else if (f_perm_pq == AllPolicy) {
            if (Buf->anti)
                method = 22;
            else
                method = 23;
        } else {
            printf("\n\tInvalid second-level method!\n");
            throw PSIEXCEPTION("Invalid second-level method!");
        }
    } else if ((b_perm_pq == f_perm_pq) && (b_perm_rs != f_perm_rs) && (b_peq == f_peq)) {
        if (b_perm_rs == AllPolicy) {
            if (Buf->anti) {
                printf("\n\tUnpack rs and antisymmetrize?\n");
                throw PSIEXCEPTION("Unpack rs and antisymmetrize?");
            }
            method = 31;
        } else if (f_perm_rs == AllPolicy) {
            if (Buf->anti)
                method = 32;
            else
                method = 33;
        } else {
            printf("\n\tInvalid third-level method!\n");
            throw PSIEXCEPTION("Invalid third-level method!");
        }
    } else if ((b_perm_pq != f_perm_pq) && (b_perm_rs != f_perm_rs)) {
        if (b_perm_pq == AllPolicy) {
            if (b_perm_rs == AllPolicy) {
                if (Buf->anti) {
                    printf("\n\tUnpack pq and rs and antisymmetrize?\n");
                    throw PSIEXCEPTION("Unpack pq and rs and antisymmetrize?");
                } else
                    method = 41;
            } else if (f_perm_rs == AllPolicy) {
                if (Buf->anti) {
                    printf("\n\tUnpack pq and antisymmetrize?\n");
                    throw PSIEXCEPTION("Unpack pq and antisymmetrize?");
                } else
                    method = 42;
            }
        } else if (f_perm_pq == AllPolicy) {
            if (b_perm_rs == AllPolicy) {
                if (Buf->anti) {
                    printf("\n\tUnpack rs and antisymmetrize?\n");
                    throw PSIEXCEPTION("Unpack rs and antisymmetrize?");
                } else
                    method = 43;
            } else if (f_perm_rs == AllPolicy) {
                if (Buf->anti)
                    method = 44;
                else
                    method = 45;
            }
        } else {
            printf("\n\tInvalid fourth-level method!\n");
            throw PSIEXCEPTION("Invalid fourth-level method!");
        }
    } else {
        printf("\n\tInvalid method in dpd_buf_mat_irrep_rd!\n");
        throw PSIEXCEPTION("Invalid method in dpd_buf_mat_irrep_rd!");
    }

    switch (method) {
        case 11: /* No change in pq or rs; antisymmetrize */

#ifdef DPD_TIMER
            timer_on("buf_rd_11");
#endif

            /* Prepare the input buffer from the input file */
            file4_mat_irrep_row_init(&(Buf->file), irrep);

            /* Loop over rows in the dpdbuf/dpdfile */
            for (pq = 0; pq < rowtot; pq++) {
                /* Fill the buffer */
                file4_mat_irrep_row_rd(&(Buf->file), irrep, pq);

                filerow = Buf->file.incore ? pq : 0;

                /* Loop over the columns in the dpdbuf */
                for (rs = 0; rs < coltot; rs++) {
                    r = Buf->params->colorb[irrep ^ all_buf_irrep][rs][0];
                    s = Buf->params->colorb[irrep ^ all_buf_irrep][rs][1];

                    /* Column indices in the dpdfile */
                    filers = rs;
                    filesr = Buf->file.params->colidx[s][r];

                    value = Buf->file.matrix[irrep][filerow][filers];

                    value -= Buf->file.matrix[irrep][filerow][filesr];

                    /* Assign the value */
                    Buf->matrix[irrep][pq][rs] = value;
                }
            }

            /* Close the input buffer */
            file4_mat_irrep_row_close(&(Buf->file), irrep);

#ifdef DPD_TIMER
            timer_off("buf_rd_11");
#endif

            break;

        case 12: /* No change in pq or rs */

#ifdef DPD_TIMER
            timer_on("buf_rd_12");
#endif

            //      if(Buf->file.incore && size) {

            //          /* We shouldn't actually have to do anything here since the
            //             pointer to the data should already have been copied in
            //             buf4_mat_irrep_init(). */
            //          continue;
            //      }
            //      else {
            //          Buf->file.matrix[irrep] = Buf->matrix[irrep];
            //          dpd_file4_mat_irrep_rd(&(Buf->file), irrep);
            //      }

            if (!(Buf->file.incore && size)) {
                Buf->file.matrix[irrep] = Buf->matrix[irrep];
                file4_mat_irrep_rd(&(Buf->file), irrep);
            }

#ifdef DPD_TIMER
            timer_off("buf_rd_12");
#endif

            break;
        case 21: /* Unpack pq; no change in rs */

#ifdef DPD_TIMER
            timer_on("buf_rd_21");
#endif

            /* Prepare the input buffer from the input file */
            file4_mat_irrep_row_init(&(Buf->file), irrep);

            /* Loop over rows in the dpdbuf */
            for (pq = 0; pq < rowtot; pq++) {
                p = Buf->params->roworb[irrep][pq][0];
                q = Buf->params->roworb[irrep][pq][1];
                filepq = Buf->file.params->rowidx[p][q];

                filerow = Buf->file.incore ? filepq : 0;

                /* Set the permutation operator's value */
                permute = ((p < q) && (f_perm_pq == dpdparams4::DiagPolicy::AntiSymm) ? -1 : 1);

                /* Fill the buffer */
                if (filepq >= 0)
                    file4_mat_irrep_row_rd(&(Buf->file), irrep, filepq);
                else
                    file4_mat_irrep_row_zero(&(Buf->file), irrep, filepq);

                /* Loop over the columns in the dpdbuf */
                for (rs = 0; rs < coltot; rs++) {
                    filers = rs;

                    if (filepq >= 0)
                        value = Buf->file.matrix[irrep][filerow][filers];
                    else
                        value = 0;

                    /* Assign the value, keeping track of the sign */
                    Buf->matrix[irrep][pq][rs] = permute * value;
                }
            }

            /* Close the input buffer */
            file4_mat_irrep_row_close(&(Buf->file), irrep);

#ifdef DPD_TIMER
            timer_off("buf_rd_21");
#endif

            break;
        case 22: /* Pack pq; no change in rs; antisymmetrize */

#ifdef DPD_TIMER
            timer_on("buf_rd_22");
#endif

            /* Prepare the input buffer from the input file */
            file4_mat_irrep_row_init(&(Buf->file), irrep);

            /* Loop over rows in the dpdbuf */
            for (pq = 0; pq < rowtot; pq++) {
                p = Buf->params->roworb[irrep][pq][0];
                q = Buf->params->roworb[irrep][pq][1];
                filepq = Buf->file.params->rowidx[p][q];

                filerow = Buf->file.incore ? filepq : 0;

                /* Fill the buffer */
                file4_mat_irrep_row_rd(&(Buf->file), irrep, filepq);

                /* Loop over the columns in the dpdbuf */
                for (rs = 0; rs < coltot; rs++) {
                    r = Buf->params->colorb[irrep ^ all_buf_irrep][rs][0];
                    s = Buf->params->colorb[irrep ^ all_buf_irrep][rs][1];

                    /* Column indices in the dpdfile */
                    filers = rs;
                    filesr = Buf->file.params->colidx[s][r];

                    value = Buf->file.matrix[irrep][filerow][filers];

                    value -= Buf->file.matrix[irrep][filerow][filesr];

                    /* Assign the value */
                    Buf->matrix[irrep][pq][rs] = value;
                }
            }

            /* Close the input buffer */
            file4_mat_irrep_row_close(&(Buf->file), irrep);

#ifdef DPD_TIMER
            timer_off("buf_rd_22");
#endif

            break;
        case 23: /* Pack pq; no change in rs */

#ifdef DPD_TIMER
            timer_on("buf_rd_23");
#endif

            /* Prepare the input buffer from the input file */
            file4_mat_irrep_row_init(&(Buf->file), irrep);

            /* Loop over rows in the dpdbuf */
            for (pq = 0; pq < rowtot; pq++) {
                p = Buf->params->roworb[irrep][pq][0];
                q = Buf->params->roworb[irrep][pq][1];
                filepq = Buf->file.params->rowidx[p][q];

                filerow = Buf->file.incore ? filepq : 0;

                file4_mat_irrep_row_rd(&(Buf->file), irrep, filepq);

                /* Loop over the columns in the dpdbuf */
                for (rs = 0; rs < coltot; rs++) {
                    filers = rs;

                    value = Buf->file.matrix[irrep][filerow][filers];

                    /* Assign the value */
                    Buf->matrix[irrep][pq][rs] = value;
                }
            }

            /* Close the input buffer */
            file4_mat_irrep_row_close(&(Buf->file), irrep);

#ifdef DPD_TIMER
            timer_off("buf_rd_23");
#endif

            break;
        case 31: /* No change in pq; unpack rs */

#ifdef DPD_TIMER
            timer_on("buf_rd_31");
#endif

            /* Prepare the input buffer from the input file */
            file4_mat_irrep_row_init(&(Buf->file), irrep);

            /* Loop over rows in the dpdbuf/dpdfile */
            for (pq = 0; pq < rowtot; pq++) {
                filepq = pq;

                filerow = Buf->file.incore ? filepq : 0;

                /* Fill the buffer */
                file4_mat_irrep_row_rd(&(Buf->file), irrep, filepq);

                /* Loop over the columns in the dpdbuf */
                for (rs = 0; rs < coltot; rs++) {
                    r = Buf->params->colorb[irrep ^ all_buf_irrep][rs][0];
                    s = Buf->params->colorb[irrep ^ all_buf_irrep][rs][1];
                    filers = Buf->file.params->colidx[r][s];

                    /* rs permutation operator */
                    permute = ((r < s) && (f_perm_rs == dpdparams4::DiagPolicy::AntiSymm) ? -1 : 1);

                    /* Is this fast enough? */
                    value = ((filers < 0) ? 0 : Buf->file.matrix[irrep][filerow][filers]);

                    /* Assign the value */
                    Buf->matrix[irrep][pq][rs] = permute * value;
                }
            }

            /* Close the input buffer */
            file4_mat_irrep_row_close(&(Buf->file), irrep);

#ifdef DPD_TIMER
            timer_off("buf_rd_31");
#endif

            break;
        case 32: /* No change in pq; pack rs; antisymmetrize */

#ifdef DPD_TIMER
            timer_on("buf_rd_32");
#endif

            /* Prepare the input buffer from the input file */
            file4_mat_irrep_row_init(&(Buf->file), irrep);

            /* Loop over rows in the dpdbuf/dpdfile */
            for (pq = 0; pq < rowtot; pq++) {
                filepq = pq;

                filerow = Buf->file.incore ? filepq : 0;

                /* Fill the buffer */
                file4_mat_irrep_row_rd(&(Buf->file), irrep, filepq);

                /* Loop over the columns in the dpdbuf */
                for (rs = 0; rs < coltot; rs++) {
                    r = Buf->params->colorb[irrep ^ all_buf_irrep][rs][0];
                    s = Buf->params->colorb[irrep ^ all_buf_irrep][rs][1];

                    /* Column indices in the dpdfile */
                    filers = Buf->file.params->colidx[r][s];
                    filesr = Buf->file.params->colidx[s][r];

                    value = Buf->file.matrix[irrep][filerow][filers];
                    value -= Buf->file.matrix[irrep][filerow][filesr];

                    /* Assign the value */
                    Buf->matrix[irrep][pq][rs] = value;
                }
            }

            /* Close the input buffer */
            file4_mat_irrep_row_close(&(Buf->file), irrep);

#ifdef DPD_TIMER
            timer_off("buf_rd_32");
#endif

            break;
        case 33: /* No change in pq; pack rs */

#ifdef DPD_TIMER
            timer_on("buf_rd_33");
#endif

            /* Prepare the input buffer from the input file */
            file4_mat_irrep_row_init(&(Buf->file), irrep);

            /* Loop over rows in the dpdbuf/dpdfile */
            for (pq = 0; pq < rowtot; pq++) {
                filepq = pq;

                filerow = Buf->file.incore ? filepq : 0;

                /* Fill the buffer */
                file4_mat_irrep_row_rd(&(Buf->file), irrep, filepq);

                /* Loop over the columns in the dpdbuf */
                for (rs = 0; rs < coltot; rs++) {
                    r = Buf->params->colorb[irrep ^ all_buf_irrep][rs][0];
                    s = Buf->params->colorb[irrep ^ all_buf_irrep][rs][1];
                    filers = Buf->file.params->colidx[r][s];

                    value = Buf->file.matrix[irrep][filerow][filers];

                    /* Assign the value */
                    Buf->matrix[irrep][pq][rs] = value;
                }
            }

            /* Close the input buffer */
            file4_mat_irrep_row_close(&(Buf->file), irrep);

#ifdef DPD_TIMER
            timer_off("buf_rd_33");
#endif

            break;
        case 41: /* Unpack pq and rs */

#ifdef DPD_TIMER
            timer_on("buf_rd_41");
#endif

            /* Prepare the input buffer from the input file */
            file4_mat_irrep_row_init(&(Buf->file), irrep);

            /* Loop over rows in the dpdbuf */
            for (pq = 0; pq < rowtot; pq++) {
                p = Buf->params->roworb[irrep][pq][0];
                q = Buf->params->roworb[irrep][pq][1];
                filepq = Buf->file.params->rowidx[p][q];

                filerow = Buf->file.incore ? filepq : 0;

                /* Set the value of the pq permutation operator */
                pq_permute = ((p < q) && (f_perm_pq == dpdparams4::DiagPolicy::AntiSymm) ? -1 : 1);

                /* Fill the buffer */
                if (filepq >= 0)
                    file4_mat_irrep_row_rd(&(Buf->file), irrep, filepq);
                else
                    file4_mat_irrep_row_zero(&(Buf->file), irrep, filepq);

                /* Loop over the columns in the dpdbuf */
                for (rs = 0; rs < coltot; rs++) {
                    r = Buf->params->colorb[irrep ^ all_buf_irrep][rs][0];
                    s = Buf->params->colorb[irrep ^ all_buf_irrep][rs][1];
                    filers = Buf->file.params->colidx[r][s];

                    /* Set the value of the pqrs permutation operator */
                    permute = ((r < s) && (f_perm_rs == dpdparams4::DiagPolicy::AntiSymm) ? -1 : 1) * pq_permute;

                    value = 0;

                    if (filers >= 0 && filepq >= 0) value = Buf->file.matrix[irrep][filerow][filers];

                    /* Assign the value */
                    Buf->matrix[irrep][pq][rs] = permute * value;
                }
            }

            /* Close the input buffer */
            file4_mat_irrep_row_close(&(Buf->file), irrep);

#ifdef DPD_TIMER
            timer_off("buf_rd_41");
#endif
            break;
        case 42: /* Pack pq; unpack rs */
            printf("\n\tHaven't programmed method 42 yet!\n");
            throw PSIEXCEPTION("Haven't programmed method 42 yet!");

            break;
        case 43: /* Unpack pq; pack rs */
            printf("\n\tHaven't programmed method 43 yet!\n");
            throw PSIEXCEPTION("Haven't programmed method 43 yet!");

            break;
        case 44: /* Pack pq; pack rs; antisymmetrize */

#ifdef DPD_TIMER
            timer_on("buf_rd_44");
#endif

            /* Prepare the input buffer from the input file */
            file4_mat_irrep_row_init(&(Buf->file), irrep);

            /* Loop over rows in the dpdbuf */
            for (pq = 0; pq < rowtot; pq++) {
                p = Buf->params->roworb[irrep][pq][0];
                q = Buf->params->roworb[irrep][pq][1];
                filepq = Buf->file.params->rowidx[p][q];

                filerow = Buf->file.incore ? filepq : 0;

                /* Fill the buffer */
                file4_mat_irrep_row_rd(&(Buf->file), irrep, filepq);

                /* Loop over the columns in the dpdbuf */
                for (rs = 0; rs < coltot; rs++) {
                    r = Buf->params->colorb[irrep ^ all_buf_irrep][rs][0];
                    s = Buf->params->colorb[irrep ^ all_buf_irrep][rs][1];

                    /* Column indices in the dpdfile */
                    filers = Buf->file.params->colidx[r][s];
                    filesr = Buf->file.params->colidx[s][r];

                    value = Buf->file.matrix[irrep][filerow][filers];
                    value -= Buf->file.matrix[irrep][filerow][filesr];

                    /* Assign the value */
                    Buf->matrix[irrep][pq][rs] = value;
                }
            }

            /* Close the input buffer */
            file4_mat_irrep_row_close(&(Buf->file), irrep);

#ifdef DPD_TIMER
            timer_off("buf_rd_44");
#endif

            break;
        case 45: /* Pack pq and rs */

#ifdef DPD_TIMER
            timer_on("buf_rd_45");
#endif

            /* Prepare the input buffer from the input file */
            file4_mat_irrep_row_init(&(Buf->file), irrep);

            /* Loop over rows in the dpdbuf */
            for (pq = 0; pq < rowtot; pq++) {
                p = Buf->params->roworb[irrep][pq][0];
                q = Buf->params->roworb[irrep][pq][1];
                filepq = Buf->file.params->rowidx[p][q];

                filerow = Buf->file.incore ? filepq : 0;

                file4_mat_irrep_row_rd(&(Buf->file), irrep, filepq);

                /* Loop over the columns in the dpdbuf */
                for (rs = 0; rs < coltot; rs++) {
                    r = Buf->params->colorb[irrep ^ all_buf_irrep][rs][0];
                    s = Buf->params->colorb[irrep ^ all_buf_irrep][rs][1];
                    filers = Buf->file.params->colidx[r][s];

                    if (filers < 0) {
                        printf("\n\tNegative colidx in method 44?\n");
                        throw PSIEXCEPTION("Negative colidx in method 44?");
                    }

                    value = Buf->file.matrix[irrep][filerow][filers];

                    /* Assign the value */
                    Buf->matrix[irrep][pq][rs] = value;
                }
            }

            /* Close the input buffer */
            file4_mat_irrep_row_close(&(Buf->file), irrep);

#ifdef DPD_TIMER
            timer_off("buf_rd_45");
#endif

            break;
        default: /* Error trapping */
            printf("\n\tInvalid switch case in dpd_buf_mat_irrep_rd!\n");
            throw PSIEXCEPTION("Invalid switch case in dpd_buf_mat_irrep_rd!");
            break;
    }

#ifdef DPD_TIMER
    timer_off("buf_rd");
#endif

    return 0;
}

}  // namespace psi
